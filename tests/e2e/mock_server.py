"""
Ephemeral HTTPS mock update server for vicius E2E tests.

Paths handled:
  GET /api/example/Demo/updates.json  -> the per-scenario manifest
  GET /files/setup.exe                -> the fake setup PE binary

The caller mutates `server.scenario` (a plain dict) between tests to
control what each request returns.  The dict keys are documented in
the SCENARIO_DEFAULTS constant below.
"""

from __future__ import annotations

import json
import ssl
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from typing import Any


MANIFEST_PATH = "/api/example/Demo/updates.json"
SETUP_PATH = "/files/setup.exe"

# Keys and their defaults used by the request handler.
SCENARIO_DEFAULTS: dict[str, Any] = {
    # JSON dict to serialise as the manifest; None -> return manifest_status with empty body
    "manifest": None,
    # HTTP status to return for the manifest request (200 / 404 / 500 / …)
    "manifest_status": 200,
    # Content-Type for the manifest response
    "manifest_content_type": "application/json",
    # Raw bytes to override what the body of a non-200 manifest response contains
    "manifest_body_override": None,
    # HTTP status for the setup binary (200 / 404 / …)
    "setup_status": 200,
    # Path to the actual fake_setup.exe to stream; None -> 404
    "fake_setup_path": None,
}


class _Handler(BaseHTTPRequestHandler):
    """HTTP request handler that reads behaviour from server.scenario."""

    # ------------------------------------------------------------------ logging
    def log_message(self, fmt: str, *args: object) -> None:  # noqa: D102
        pass  # suppress default stderr logging; pytest captures anyway

    # ------------------------------------------------------------------ routing
    def do_GET(self) -> None:  # noqa: N802
        path = self.path.split("?")[0]  # strip any query string

        if path == MANIFEST_PATH:
            self._serve_manifest()
        elif path == SETUP_PATH:
            self._serve_setup()
        else:
            self._send(404, b"Not found", "text/plain")

    # ------------------------------------------------------------------ handlers
    def _serve_manifest(self) -> None:
        sc = self.server.scenario  # type: ignore[attr-defined]
        status: int = sc.get("manifest_status", 200)
        content_type: str = sc.get("manifest_content_type", "application/json")
        body_override: bytes | None = sc.get("manifest_body_override")

        # Raw body override wins regardless of status (used for malformed-JSON tests).
        if body_override is not None:
            self._send(status, body_override, content_type)
            return

        if status != 200:
            self._send(status, b"", content_type)
            return

        manifest = sc.get("manifest")
        if manifest is None:
            self._send(404, b"No manifest configured", "text/plain")
            return

        body = json.dumps(manifest).encode()
        self._send(200, body, content_type)

    def _serve_setup(self) -> None:
        sc = self.server.scenario  # type: ignore[attr-defined]
        status: int = sc.get("setup_status", 200)

        if status != 200:
            self._send(status, b"", "application/octet-stream")
            return

        setup_path: str | None = sc.get("fake_setup_path")
        if not setup_path or not Path(setup_path).is_file():
            self._send(404, b"fake_setup not configured", "text/plain")
            return

        data = Path(setup_path).read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Content-Disposition", 'attachment; filename="setup.exe"')
        self.end_headers()
        self.wfile.write(data)

    # ------------------------------------------------------------------ helpers
    def _send(self, status: int, body: bytes, content_type: str) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body:
            self.wfile.write(body)


class MockUpdateServer:
    """Thread-backed HTTPS server with a mutable scenario dict."""

    def __init__(self, cert_file: str, key_file: str) -> None:
        self._cert_file = cert_file
        self._key_file = key_file
        self._server: HTTPServer | None = None
        self._thread: threading.Thread | None = None
        self.scenario: dict[str, Any] = dict(SCENARIO_DEFAULTS)

    # ------------------------------------------------------------------ lifecycle
    def start(self) -> None:
        """Bind to an ephemeral port, wrap with TLS, and start serving."""
        self._server = HTTPServer(("127.0.0.1", 0), _Handler)

        # Attach the mutable scenario dict so the handler can reach it.
        self._server.scenario = self.scenario  # type: ignore[attr-defined]

        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(certfile=self._cert_file, keyfile=self._key_file)
        self._server.socket = ctx.wrap_socket(self._server.socket, server_side=True)

        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        if self._server:
            self._server.shutdown()
            self._server.server_close()
        if self._thread:
            self._thread.join(timeout=5)

    # ------------------------------------------------------------------ properties
    @property
    def port(self) -> int:
        assert self._server is not None, "Server not started"
        return self._server.server_address[1]

    @property
    def base_url(self) -> str:
        return f"https://127.0.0.1:{self.port}"

    @property
    def manifest_url(self) -> str:
        return f"{self.base_url}{MANIFEST_PATH}"

    @property
    def setup_url(self) -> str:
        return f"{self.base_url}{SETUP_PATH}"

    # ------------------------------------------------------------------ helpers
    def set_scenario(self, **kwargs: Any) -> None:
        """Reset to defaults then apply the provided overrides."""
        self.scenario.clear()
        self.scenario.update(SCENARIO_DEFAULTS)
        self.scenario.update(kwargs)
