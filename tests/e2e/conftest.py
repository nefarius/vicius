"""
pytest session fixtures for the vicius E2E test harness.

Session-scoped fixtures (created once per test run):
  tls_cert       – self-signed cert/key files + cert imported into Windows Root store
  fake_setup_exe – path to a compiled fake_setup.exe (valid PE, exits 0)
  mock_server    – running MockUpdateServer instance
  updater_exe    – path to the Release x64 updater binary

Function-scoped fixtures (reset for every test):
  workdir        – isolated temp dir with renamed exe + config JSON pointing at mock_server
"""

from __future__ import annotations

import ctypes
import ctypes.wintypes
import ipaddress
import os
import shutil
import socket
import subprocess
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Generator

import pytest
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID

from helpers import E2E_DIR, REPO_ROOT, UPDATER_EXE_NAME, make_workdir
from mock_server import MockUpdateServer


# ===========================================================================
# TLS certificate helpers
# ===========================================================================

def _generate_self_signed_cert(cert_path: Path, key_path: Path) -> None:
    """Write a self-signed RSA-2048 cert with SAN localhost/127.0.0.1."""
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)

    subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "vicius-e2e-test")])

    cert = (
        x509.CertificateBuilder()
        .subject_name(subject)
        .issuer_name(subject)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(datetime.now(timezone.utc) - timedelta(days=1))
        .not_valid_after(datetime.now(timezone.utc) + timedelta(days=365))
        .add_extension(
            x509.SubjectAlternativeName([
                x509.DNSName("localhost"),
                x509.IPAddress(ipaddress.IPv4Address("127.0.0.1")),
            ]),
            critical=False,
        )
        .add_extension(
            x509.BasicConstraints(ca=True, path_length=None),
            critical=True,
        )
        .sign(key, hashes.SHA256())
    )

    cert_path.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    key_path.write_bytes(
        key.private_bytes(
            serialization.Encoding.PEM,
            serialization.PrivateFormat.TraditionalOpenSSL,
            serialization.NoEncryption(),
        )
    )


def _cert_sha1_thumbprint(cert_path: Path) -> str:
    """Return the uppercase hex SHA-1 thumbprint of a PEM cert."""
    from cryptography.hazmat.primitives import hashes as _h
    from cryptography import x509 as _x509

    cert = _x509.load_pem_x509_certificate(cert_path.read_bytes())
    return cert.fingerprint(_h.SHA1()).hex().upper()


def _load_cert_der(cert_path: Path) -> bytes:
    """Convert a PEM certificate file to DER bytes."""
    from cryptography import x509 as _cx509
    from cryptography.hazmat.primitives import serialization as _ser
    return _cx509.load_pem_x509_certificate(cert_path.read_bytes()).public_bytes(
        _ser.Encoding.DER
    )


def _trust_cert_in_root_store(cert_path: Path) -> str:
    """
    Import cert into CurrentUser\\Root via CertAddEncodedCertificateToStore.

    Calls the raw Windows CryptAPI directly (no certutil, no PowerShell).
    The raw API has no UI layer — Windows security dialogs are added by higher-level
    callers (certutil, MMC) and never appear at this level, so CI never hangs.
    Schannel trusts both LocalMachine and CurrentUser root stores; the user store
    requires no administrator rights.

    Returns the SHA-1 thumbprint for later removal.
    """
    _crypt32 = ctypes.windll.crypt32
    _crypt32.CertOpenStore.restype = ctypes.c_void_p
    _crypt32.CertAddEncodedCertificateToStore.restype = ctypes.c_bool
    _crypt32.CertCloseStore.restype = ctypes.c_bool

    CERT_STORE_PROV_SYSTEM      = 10
    X509_ASN_ENCODING           = 0x00000001
    CERT_SYSTEM_STORE_CURRENT_USER = 0x00010000
    CERT_STORE_ADD_REPLACE_EXISTING = 3

    der = _load_cert_der(cert_path)
    buf = (ctypes.c_ubyte * len(der))(*der)

    h_store = _crypt32.CertOpenStore(
        CERT_STORE_PROV_SYSTEM,
        X509_ASN_ENCODING,
        None,
        CERT_SYSTEM_STORE_CURRENT_USER,
        ctypes.c_wchar_p("Root"),
    )
    if not h_store:
        err = ctypes.GetLastError()
        raise RuntimeError(
            f"CertOpenStore(CurrentUser\\Root) failed: error {err:#010x}.\n"
            "Import the test CA manually:\n"
            "  $cert = New-Object Security.Cryptography.X509Certificates.X509Certificate2"
            f" '{cert_path}'\n"
            "  $store = New-Object Security.Cryptography.X509Certificates.X509Store"
            "('Root','CurrentUser')\n"
            "  $store.Open('ReadWrite'); $store.Add($cert); $store.Close()"
        )
    try:
        ok = _crypt32.CertAddEncodedCertificateToStore(
            ctypes.c_void_p(h_store),
            X509_ASN_ENCODING,
            buf,
            ctypes.c_ulong(len(der)),
            CERT_STORE_ADD_REPLACE_EXISTING,
            None,
        )
        if not ok:
            err = ctypes.GetLastError()
            raise RuntimeError(
                f"CertAddEncodedCertificateToStore failed: error {err:#010x}."
            )
    finally:
        _crypt32.CertCloseStore(ctypes.c_void_p(h_store), 0)

    return _cert_sha1_thumbprint(cert_path)


def _remove_cert_from_root_store(thumbprint: str) -> None:
    """
    Remove cert by SHA-1 thumbprint from CurrentUser\\Root via CryptoAPI.
    Raises RuntimeError on failure so teardown errors are always visible.
    """
    _crypt32 = ctypes.windll.crypt32
    _crypt32.CertOpenStore.restype = ctypes.c_void_p
    _crypt32.CertFindCertificateInStore.restype = ctypes.c_void_p
    _crypt32.CertDeleteCertificateFromStore.restype = ctypes.c_bool
    _crypt32.CertCloseStore.restype = ctypes.c_bool

    CERT_STORE_PROV_SYSTEM      = 10
    X509_ASN_ENCODING           = 0x00000001
    CERT_SYSTEM_STORE_CURRENT_USER = 0x00010000
    CERT_FIND_SHA1_HASH         = 0x00010000  # CERT_COMPARE_SHA1_HASH(1) << 16

    class _BLOB(ctypes.Structure):
        _fields_ = [
            ("cbData", ctypes.c_ulong),
            ("pbData", ctypes.POINTER(ctypes.c_ubyte)),
        ]

    sha1_bytes = bytes.fromhex(thumbprint)
    sha1_buf   = (ctypes.c_ubyte * len(sha1_bytes))(*sha1_bytes)
    blob       = _BLOB(len(sha1_bytes), sha1_buf)

    h_store = _crypt32.CertOpenStore(
        CERT_STORE_PROV_SYSTEM,
        X509_ASN_ENCODING,
        None,
        CERT_SYSTEM_STORE_CURRENT_USER,
        ctypes.c_wchar_p("Root"),
    )
    if not h_store:
        err = ctypes.GetLastError()
        raise RuntimeError(
            f"CertOpenStore(CurrentUser\\Root) for deletion failed: error {err:#010x}.\n"
            f"Remove manually: Remove-Item 'Cert:\\CurrentUser\\Root\\{thumbprint}'"
        )
    try:
        p_cert = _crypt32.CertFindCertificateInStore(
            ctypes.c_void_p(h_store),
            X509_ASN_ENCODING,
            0,
            CERT_FIND_SHA1_HASH,
            ctypes.byref(blob),
            None,
        )
        if not p_cert:
            return  # cert was already removed; not an error
        # CertDeleteCertificateFromStore always frees p_cert even on failure.
        ok = _crypt32.CertDeleteCertificateFromStore(ctypes.c_void_p(p_cert))
        if not ok:
            err = ctypes.GetLastError()
            raise RuntimeError(
                f"CertDeleteCertificateFromStore failed: error {err:#010x}.\n"
                f"Remove manually: Remove-Item 'Cert:\\CurrentUser\\Root\\{thumbprint}'"
            )
    finally:
        _crypt32.CertCloseStore(ctypes.c_void_p(h_store), 0)


# ===========================================================================
# Fake setup compilation helpers
# ===========================================================================

def _find_cl_exe() -> str | None:
    """
    Locate cl.exe for x64.
    Resolution order:
    1. Already in PATH (e.g. after running vcvars64.bat or setup-msbuild in CI).
    2. Latest VS installation via vswhere.exe.
    """
    cl_in_path = shutil.which("cl")
    if cl_in_path:
        return cl_in_path

    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    if vswhere.exists():
        result = subprocess.run(
            [
                str(vswhere),
                "-latest",
                "-find",
                r"VC\Tools\MSVC\**\bin\Hostx64\x64\cl.exe",
            ],
            capture_output=True,
            text=True,
        )
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip().splitlines()[0].strip()

    return None


def _compile_fake_setup(src: Path, out: Path) -> None:
    """Compile fake_setup.c to a native Win32 PE using cl.exe."""
    cl = _find_cl_exe()
    if not cl:
        raise RuntimeError(
            "cl.exe not found. Options:\n"
            "  • Run from a Visual Studio Developer Command Prompt, or\n"
            "  • Pre-compile tests\\e2e\\fake_setup.c and set FAKE_SETUP_EXE env var."
        )

    result = subprocess.run(
        [cl, "/nologo", "/MT", str(src), f"/Fe:{out}"],
        capture_output=True,
        text=True,
        cwd=str(src.parent),
    )
    if result.returncode != 0 or not out.exists():
        raise RuntimeError(
            f"Compilation of fake_setup.c failed:\n"
            f"stdout: {result.stdout.strip()}\n"
            f"stderr: {result.stderr.strip()}"
        )


# ===========================================================================
# Session-scoped fixtures
# ===========================================================================

@pytest.fixture(scope="session")
def tls_cert(tmp_path_factory: pytest.TempPathFactory) -> Generator[dict, None, None]:
    """
    Generate a self-signed TLS cert, trust it in CurrentUser\\Root,
    and remove it during teardown.

    Yields a dict with keys: cert_file (str), key_file (str), thumbprint (str).
    """
    cert_dir = tmp_path_factory.mktemp("certs")
    cert_path = cert_dir / "server.crt"
    key_path  = cert_dir / "server.key"

    _generate_self_signed_cert(cert_path, key_path)
    thumbprint = _trust_cert_in_root_store(cert_path)

    yield {
        "cert_file":  str(cert_path),
        "key_file":   str(key_path),
        "thumbprint": thumbprint,
    }

    _remove_cert_from_root_store(thumbprint)


@pytest.fixture(scope="session")
def fake_setup_exe(tmp_path_factory: pytest.TempPathFactory) -> Path:
    """
    Return a path to a compiled fake_setup.exe (valid PE, exits 0).

    If FAKE_SETUP_EXE env var is set and points to an existing file that path
    is used directly (CI sets this after compiling in a dedicated step).
    Otherwise fake_setup.c is compiled on the fly using cl.exe.
    """
    env_path = os.environ.get("FAKE_SETUP_EXE")
    if env_path:
        p = Path(env_path)
        if p.is_file():
            return p
        raise FileNotFoundError(f"FAKE_SETUP_EXE={env_path} does not exist")

    build_dir = tmp_path_factory.mktemp("fake_setup_build")
    src = E2E_DIR / "fake_setup.c"
    out = build_dir / "fake_setup.exe"
    _compile_fake_setup(src, out)
    return out


@pytest.fixture(scope="session")
def mock_server(tls_cert: dict) -> Generator[MockUpdateServer, None, None]:
    """Start the HTTPS mock update server; stop it after the session ends."""
    server = MockUpdateServer(
        cert_file=tls_cert["cert_file"],
        key_file=tls_cert["key_file"],
    )
    server.start()
    yield server
    server.stop()


@pytest.fixture(scope="session")
def updater_exe() -> Path:
    """
    Locate the Release x64 updater binary.

    Resolution order:
    1. VICIUS_EXE env var (set by CI after artifact download).
    2. bin\\x64\\example_Demo_Updater.exe  (CI/local build with UpdaterName override).
    3. bin\\x64\\Updater.exe               (local build with default name).
    """
    env_path = os.environ.get("VICIUS_EXE")
    if env_path:
        p = Path(env_path)
        if not p.is_file():
            raise FileNotFoundError(f"VICIUS_EXE={env_path} does not exist")
        return p

    candidates = [
        REPO_ROOT / "bin" / "x64" / "example_Demo_Updater.exe",
        REPO_ROOT / "bin" / "x64" / "Updater.exe",
    ]
    for c in candidates:
        if c.is_file():
            return c

    raise FileNotFoundError(
        "Updater exe not found. Build a Release x64 binary, or set VICIUS_EXE.\n"
        f"Searched: {[str(c) for c in candidates]}"
    )


# ===========================================================================
# Function-scoped fixture
# ===========================================================================

@pytest.fixture()
def workdir(tmp_path: Path, updater_exe: Path, mock_server: MockUpdateServer) -> dict:
    """
    Create an isolated working dir for one test scenario:
      • Copy the updater exe as example_Demo_Updater.exe
      • Write example_Demo_Updater.json pointing at the live mock server

    Yields a dict with keys: exe (Path), dir (Path).
    """
    template = f"https://127.0.0.1:{mock_server.port}/api/{{}}/updates.json"
    return make_workdir(tmp_path, updater_exe, template)
