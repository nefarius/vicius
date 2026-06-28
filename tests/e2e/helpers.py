"""
Shared constants, exit-code names, and helper functions for the E2E harness.
Imported by conftest.py and test_update_scenarios.py.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

# ---------------------------------------------------------------------------
# Repository layout
# ---------------------------------------------------------------------------

E2E_DIR   = Path(__file__).parent
REPO_ROOT = E2E_DIR.parent.parent

UPDATER_EXE_NAME    = "example_Demo_Updater.exe"
UPDATER_CONFIG_NAME = "example_Demo_Updater.json"

# ---------------------------------------------------------------------------
# Exit-code constants (mirrors src/Common.h)
# ---------------------------------------------------------------------------

NV_E_SERVER_RESPONSE      = 104
NV_E_PRODUCT_DETECTION    = 105
NV_E_DOWNLOAD_FAILED      = 107
NV_E_SIGNATURE_INVALID    = 116
NV_S_UP_TO_DATE           = 202
NV_S_UPDATE_FINISHED      = 203

# ---------------------------------------------------------------------------
# Manifest helpers
# ---------------------------------------------------------------------------

def make_manifest(
    release_version: str,
    download_url: str,
    *,
    checksum: str | None = None,
    checksum_alg: str = "SHA256",
    disabled: bool = False,
    product_name: str = "E2E Test Product",
) -> dict:
    """Return a minimal valid updates.json dict for one release."""
    release: dict = {
        "name":         f"Test Update {release_version}",
        "version":      release_version,
        "publishedAt":  "2025-01-01T00:00:00Z",
        "downloadUrl":  download_url,
    }
    if checksum is not None:
        release["checksum"] = {"checksum": checksum, "checksumAlg": checksum_alg}
    if disabled:
        release["disabled"] = True

    return {
        "shared": {"productName": product_name},
        "releases": [release],
    }


def sha256_of(path: Path | str) -> str:
    """Return lowercase hex SHA256 of the given file."""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


# ---------------------------------------------------------------------------
# Updater invocation
# ---------------------------------------------------------------------------

def run_updater(
    workdir: dict,
    local_version: str = "1.0.0",
    extra_args: list[str] | None = None,
    timeout: int = 90,
) -> subprocess.CompletedProcess:
    """
    Launch the renamed updater in headless mode and return the completed process.

    Standard flags applied to every invocation:
      --silent-update
      --force-local-version <local_version>
      --ignore-busy-state
      --skip-self-update
    """
    exe = workdir["exe"]
    cmd = [
        str(exe),
        "--silent-update",
        "--force-local-version", local_version,
        "--ignore-busy-state",
        "--skip-self-update",
    ]
    if extra_args:
        cmd.extend(extra_args)

    return subprocess.run(cmd, cwd=str(workdir["dir"]), timeout=timeout)


# ---------------------------------------------------------------------------
# Workdir factory (used by fixture and by tests that need a custom server URL)
# ---------------------------------------------------------------------------

def make_workdir(
    dest_dir: Path,
    updater_exe: Path,
    server_url_template: str,
) -> dict:
    """
    Copy the updater exe into dest_dir, write the config JSON, and return
    a dict suitable for run_updater().

    server_url_template must contain '{}' which the updater replaces with the
    tenant sub-path, e.g. "https://127.0.0.1:PORT/api/{}/updates.json".
    """
    exe_dest = dest_dir / UPDATER_EXE_NAME
    shutil.copy2(str(updater_exe), str(exe_dest))

    config = {
        "instance": {
            "serverUrlTemplate": server_url_template,
        }
    }
    (dest_dir / UPDATER_CONFIG_NAME).write_text(json.dumps(config), encoding="utf-8")

    return {"exe": exe_dest, "dir": dest_dir}
