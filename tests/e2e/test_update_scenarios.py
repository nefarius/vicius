"""
End-to-end scenario matrix for the vicius updater.

Each test:
  1. Configures the mock server with a specific manifest and/or setup.
  2. Launches the updater headlessly via --silent-update.
  3. Asserts the process exit code against the expected constant from Common.h.

All tests use:
  --force-local-version <version>   pin the "installed" version
  --ignore-busy-state               skip the SHQueryUserNotificationState loop
  --skip-self-update                skip the self-updater branch
"""

from __future__ import annotations

import json
import shutil
import socket
import subprocess
from pathlib import Path

import pytest

from helpers import (
    REPO_ROOT,
    UPDATER_CONFIG_NAME,
    UPDATER_EXE_NAME,
    NV_E_DOWNLOAD_FAILED,
    NV_E_PRODUCT_DETECTION,
    NV_E_SERVER_RESPONSE,
    NV_E_SIGNATURE_INVALID,
    NV_S_UP_TO_DATE,
    NV_S_UPDATE_FINISHED,
    make_manifest,
    make_workdir,
    run_updater,
    sha256_of,
)
from mock_server import MockUpdateServer


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _setup_update_scenario(
    mock_server: MockUpdateServer,
    fake_setup_exe: Path,
    release_version: str = "2.0.0",
    local_version: str = "1.0.0",
    **manifest_kwargs,
) -> dict:
    """Configure the mock server and return kwargs for run_updater."""
    manifest = make_manifest(
        release_version,
        mock_server.setup_url,
        **manifest_kwargs,
    )
    mock_server.set_scenario(manifest=manifest, fake_setup_path=str(fake_setup_exe))
    return {"local_version": local_version}


# ===========================================================================
# Scenario 1: Happy update
#   local 1.0.0 < release 2.0.0, valid HTTPS setup, no checksum
#   → updater downloads, verifies (no checksum to check), runs fake_setup (exits 0)
#   → exit 203 (NV_S_UPDATE_FINISHED)
# ===========================================================================

@pytest.mark.timeout(120)
def test_happy_update(workdir, mock_server, fake_setup_exe):
    kwargs = _setup_update_scenario(mock_server, fake_setup_exe)
    result = run_updater(workdir, **kwargs)
    assert result.returncode == NV_S_UPDATE_FINISHED, (
        f"Expected NV_S_UPDATE_FINISHED ({NV_S_UPDATE_FINISHED}), "
        f"got {result.returncode}"
    )


# ===========================================================================
# Scenario 2: Up-to-date
#   local 2.0.0 >= release 1.0.0
#   → exit 202 (NV_S_UP_TO_DATE)
# ===========================================================================

@pytest.mark.timeout(60)
def test_up_to_date(workdir, mock_server, fake_setup_exe):
    manifest = make_manifest("1.0.0", mock_server.setup_url)
    mock_server.set_scenario(manifest=manifest, fake_setup_path=str(fake_setup_exe))

    result = run_updater(workdir, local_version="2.0.0")
    assert result.returncode == NV_S_UP_TO_DATE, (
        f"Expected NV_S_UP_TO_DATE ({NV_S_UP_TO_DATE}), got {result.returncode}"
    )


# ===========================================================================
# Scenario 3: Checksum present and correct
#   SHA256 of fake_setup.exe is computed and embedded in the manifest.
#   → exit 203 (NV_S_UPDATE_FINISHED)
# ===========================================================================

@pytest.mark.timeout(120)
def test_checksum_correct(workdir, mock_server, fake_setup_exe):
    correct_hash = sha256_of(fake_setup_exe)
    kwargs = _setup_update_scenario(
        mock_server, fake_setup_exe,
        checksum=correct_hash, checksum_alg="SHA256",
    )
    result = run_updater(workdir, **kwargs)
    assert result.returncode == NV_S_UPDATE_FINISHED, (
        f"Expected NV_S_UPDATE_FINISHED ({NV_S_UPDATE_FINISHED}), "
        f"got {result.returncode}"
    )


# ===========================================================================
# Scenario 4: Checksum present but wrong
#   A deliberately wrong checksum triggers VerifyReleaseIntegrity failure.
#   In silent mode any integrity failure maps to NV_E_SIGNATURE_INVALID.
#   → exit 116 (NV_E_SIGNATURE_INVALID)
# ===========================================================================

@pytest.mark.timeout(120)
def test_checksum_mismatch(workdir, mock_server, fake_setup_exe):
    kwargs = _setup_update_scenario(
        mock_server, fake_setup_exe,
        checksum="deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef",
        checksum_alg="SHA256",
    )
    result = run_updater(workdir, **kwargs)
    assert result.returncode == NV_E_SIGNATURE_INVALID, (
        f"Expected NV_E_SIGNATURE_INVALID ({NV_E_SIGNATURE_INVALID}), "
        f"got {result.returncode}"
    )


# ===========================================================================
# Scenario 5: Strict verification with no checksum in manifest
#   --strict-verification requires a checksum; absence is treated as failure.
#   → exit 116 (NV_E_SIGNATURE_INVALID)
# ===========================================================================

@pytest.mark.timeout(120)
def test_strict_no_checksum(workdir, mock_server, fake_setup_exe):
    # No checksum in manifest; --strict-verification flag on the CLI
    kwargs = _setup_update_scenario(mock_server, fake_setup_exe)
    result = run_updater(workdir, extra_args=["--strict-verification"], **kwargs)
    assert result.returncode == NV_E_SIGNATURE_INVALID, (
        f"Expected NV_E_SIGNATURE_INVALID ({NV_E_SIGNATURE_INVALID}), "
        f"got {result.returncode}"
    )


# ===========================================================================
# Scenario 6: Download 404
#   Manifest served correctly; mock server returns 404 for the setup binary.
#   The updater returns immediately on 404 (no retry) → NV_E_DOWNLOAD_FAILED.
#   → exit 107 (NV_E_DOWNLOAD_FAILED)
# ===========================================================================

@pytest.mark.timeout(60)
def test_download_404(workdir, mock_server, fake_setup_exe):
    manifest = make_manifest("2.0.0", mock_server.setup_url)
    mock_server.set_scenario(
        manifest=manifest,
        fake_setup_path=str(fake_setup_exe),
        setup_status=404,           # setup endpoint returns 404
    )
    result = run_updater(workdir)
    assert result.returncode == NV_E_DOWNLOAD_FAILED, (
        f"Expected NV_E_DOWNLOAD_FAILED ({NV_E_DOWNLOAD_FAILED}), "
        f"got {result.returncode}"
    )


# ===========================================================================
# Scenario 7: Server unreachable
#   The config JSON points at a port nothing is listening on.
#   Each of 5 attempts gets ECONNREFUSED immediately; sleeps ~1-5s between
#   retries (total ≤ ~20s).  Outer loop exhausted → NV_E_SERVER_RESPONSE.
#   → exit 104 (NV_E_SERVER_RESPONSE)
# ===========================================================================

@pytest.mark.timeout(120)
def test_server_unreachable(tmp_path, updater_exe):
    # Grab an ephemeral port number then immediately release the socket so
    # nothing is listening on it.
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    dead_port = s.getsockname()[1]
    s.close()

    template = f"https://127.0.0.1:{dead_port}/api/{{}}/updates.json"
    workdir = make_workdir(tmp_path, updater_exe, template)

    result = run_updater(workdir, timeout=120)
    assert result.returncode == NV_E_SERVER_RESPONSE, (
        f"Expected NV_E_SERVER_RESPONSE ({NV_E_SERVER_RESPONSE}), "
        f"got {result.returncode}"
    )


# ===========================================================================
# Scenario 8: Malformed / non-JSON manifest
#   Server returns HTTP 200 with a body that is not valid JSON.
#   The JSON parse exception bubbles up as NV_E_SERVER_RESPONSE.
#   → exit 104 (NV_E_SERVER_RESPONSE)
# ===========================================================================

@pytest.mark.timeout(60)
def test_malformed_json(workdir, mock_server):
    mock_server.set_scenario(
        manifest=None,
        manifest_status=200,
        manifest_content_type="application/json",
        manifest_body_override=b"{ this is : not valid json !!!",
    )
    result = run_updater(workdir)
    assert result.returncode == NV_E_SERVER_RESPONSE, (
        f"Expected NV_E_SERVER_RESPONSE ({NV_E_SERVER_RESPONSE}), "
        f"got {result.returncode}"
    )


# ===========================================================================
# Scenario 9: Non-HTTPS downloadUrl rejected
#   Manifest contains a valid JSON response but downloadUrl starts with http://.
#   IsAllowedDownloadUrl() in Release builds rejects it → NV_E_SERVER_RESPONSE.
#   → exit 104 (NV_E_SERVER_RESPONSE)
# ===========================================================================

@pytest.mark.timeout(60)
def test_non_https_download_url(workdir, mock_server, fake_setup_exe):
    # Build the manifest with a plain-HTTP download URL
    http_url = mock_server.setup_url.replace("https://", "http://", 1)
    manifest = make_manifest("2.0.0", http_url)
    mock_server.set_scenario(manifest=manifest, fake_setup_path=str(fake_setup_exe))

    result = run_updater(workdir)
    assert result.returncode == NV_E_SERVER_RESPONSE, (
        f"Expected NV_E_SERVER_RESPONSE ({NV_E_SERVER_RESPONSE}), "
        f"got {result.returncode}"
    )


# ===========================================================================
# Scenario 10: All releases disabled
#   All releases have disabled=true; after erase_if the list is empty.
#   IsInstalledVersionOutdated() returns unexpected → NV_E_PRODUCT_DETECTION.
#   → exit 105 (NV_E_PRODUCT_DETECTION)
# ===========================================================================

@pytest.mark.timeout(60)
def test_all_releases_disabled(workdir, mock_server, fake_setup_exe):
    manifest = make_manifest("2.0.0", mock_server.setup_url, disabled=True)
    mock_server.set_scenario(manifest=manifest, fake_setup_path=str(fake_setup_exe))

    result = run_updater(workdir)
    assert result.returncode == NV_E_PRODUCT_DETECTION, (
        f"Expected NV_E_PRODUCT_DETECTION ({NV_E_PRODUCT_DETECTION}), "
        f"got {result.returncode}"
    )


# ===========================================================================
# Smoke: packed artifact still boots (optional; only runs if *_packed.exe exists)
#   If the UPX-packed build artefact is present next to the updater, verify it
#   can at least start and fail gracefully (no server running) → 104.
#   This exercises that UPX didn't corrupt the binary.
# ===========================================================================

@pytest.mark.timeout(120)
def test_packed_binary_smoke(tmp_path, updater_exe):
    """Run the UPX-packed variant against a dead port to confirm it boots."""
    packed = updater_exe.parent / (updater_exe.stem + "_packed.exe")
    if not packed.is_file():
        pytest.skip(f"Packed binary not found at {packed}; skipping smoke test")

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    dead_port = s.getsockname()[1]
    s.close()

    # For the packed binary we copy it under the expected tenant name so that
    # the filename regex can still extract manufacturer/product.
    dest = tmp_path / UPDATER_EXE_NAME
    shutil.copy2(str(packed), str(dest))

    config = {
        "instance": {
            "serverUrlTemplate": f"https://127.0.0.1:{dead_port}/api/{{}}/updates.json"
        }
    }
    (tmp_path / UPDATER_CONFIG_NAME).write_text(json.dumps(config), encoding="utf-8")

    result = subprocess.run(
        [str(dest), "--silent-update", "--force-local-version", "1.0.0",
         "--ignore-busy-state", "--skip-self-update"],
        cwd=str(tmp_path),
        timeout=120,
    )
    assert result.returncode == NV_E_SERVER_RESPONSE, (
        f"Expected NV_E_SERVER_RESPONSE ({NV_E_SERVER_RESPONSE}) from packed binary, "
        f"got {result.returncode}"
    )
