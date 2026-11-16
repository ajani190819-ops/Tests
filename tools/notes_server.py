#!/usr/bin/env python3
"""API-only bridge from the native Agent Notes app to the session inbox.

The app keeps an outbox until this bridge has flushed a stable-key note to the
ignored ``.agent/INBOX.md`` and returned its durable receipt. The separate local
``agent_notes_state.py`` command publishes waiting state and acknowledges a
read only after a real inbox checkpoint. This server deliberately has no
browser capture page: it is a small session API, not a second notes UI.

Endpoints:
    GET  /health              liveness probe
    GET  /status              generic agent waiting state, no question text
    GET  /receipt/<token>     one saved/read receipt state
    POST /note                append one note and return its receipt token

Native requests carry a stable ``X-Agent-Note-Key`` for safe retry. Requests
with an ``Origin`` header are refused; there is no browser nonce or CORS path.
The local outbox is still responsible for retrying when the client, network,
server, or Arena session is offline.
"""
from __future__ import annotations

import json
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

try:
    from . import agent_notes_protocol as protocol
except ImportError:  # direct script execution from tools/notes_server.py
    import agent_notes_protocol as protocol

PORT = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 8765
RATE_WINDOW_SEC = 60.0
RATE_MAX_PER_WINDOW = 20
_rate_hits: dict[str, deque[float]] = {}
_rate_lock = threading.Lock()


def rate_limit_allow(key: str) -> bool:
    now = time.monotonic()
    with _rate_lock:
        hits = _rate_hits.setdefault(key, deque())
        while hits and now - hits[0] > RATE_WINDOW_SEC:
            hits.popleft()
        if len(hits) >= RATE_MAX_PER_WINDOW:
            return False
        hits.append(now)
        if len(_rate_hits) > 512:
            for stale in [k for k, v in _rate_hits.items() if not v]:
                del _rate_hits[stale]
        return True


class Handler(BaseHTTPRequestHandler):
    server_version = "AgentNotesApi/2.0"

    def _client_ip(self) -> str:
        try:
            return self.client_address[0] or "?"
        except Exception:
            return "?"

    def _ok(self, body: bytes = b"ok", ctype: str = "text/plain; charset=utf-8", code: int = 200):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(body)

    def _json(self, value: object, code: int = 200):
        self._ok(json.dumps(value, separators=(",", ":")).encode("utf-8"),
                 "application/json; charset=utf-8", code)

    def do_OPTIONS(self):
        self.send_response(405)
        self.send_header("Allow", "GET, POST")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self):
        path = urlsplit(self.path).path
        if path == "/health":
            self._ok(b"ok")
        elif path == "/status":
            self._json(protocol.public_status())
        elif path.startswith("/receipt/"):
            status = protocol.receipt_status(path[len("/receipt/"):])
            if status is None:
                self._json({"error": "receipt not found"}, 404)
            else:
                self._json(status)
        else:
            self._json({"error": "not found"}, 404)

    def do_POST(self):
        path = urlsplit(self.path).path
        if path != "/note":
            self._json({"error": "not found"}, 404)
            return
        # The removed capture page needed a browser token. Refusing Origin keeps
        # this native-only write API from becoming a cross-site form target.
        if self.headers.get("Origin"):
            self._json({"error": "browser-origin notes are not accepted"}, 403)
            return
        ip = self._client_ip()
        if not rate_limit_allow(ip):
            self._json({"error": "too many notes - slow down"}, 429)
            return

        media_type = (self.headers.get("Content-Type") or "").split(";", 1)[0].strip().lower()
        if media_type != "text/plain":
            self._json({"error": "Content-Type must be text/plain"}, 415)
            return
        raw_len = (self.headers.get("Content-Length") or "").strip()
        try:
            size = int(raw_len, 10)
        except ValueError:
            self._json({"error": "Content-Length must be a number"}, 400)
            return
        if size <= 0 or size > protocol.MAX_NOTE_BYTES:
            self._json({"error": "note is empty or too large"}, 400)
            return
        try:
            data = self.rfile.read(size)
            if len(data) != size:
                self._json({"error": "incomplete note body"}, 400)
                return
            raw = data.decode("utf-8").strip()
        except UnicodeDecodeError:
            self._json({"error": "note body must be UTF-8"}, 400)
            return
        except OSError:
            self._json({"error": "could not read the request body"}, 400)
            return
        if not raw or not protocol.sanitize_note(raw).strip():
            self._json({"error": "note is empty"}, 400)
            return

        note_key = (self.headers.get("X-Agent-Note-Key") or "").strip()
        if not protocol.KEY_RE.fullmatch(note_key):
            self._json({"error": "invalid or missing note key"}, 400)
            return
        user_agent = protocol._CONTROL_RE.sub(
            "", (self.headers.get("User-Agent") or "unknown client").strip())[:80]
        provenance = f"from {ip} · {user_agent}"
        try:
            receipt_id = protocol.persist_note(note_key, raw, provenance)
        except (OSError, TimeoutError) as exc:
            self._json({"error": f"could not persist the inbox receipt: {exc}"}, 500)
            return
        except ValueError as exc:
            self._json({"error": str(exc)}, 400)
            return
        self._json({"receipt_id": receipt_id, "state": "saved"})

    def log_message(self, *args):
        pass


def main() -> None:
    protocol.initialise_inbox()
    print(f"notes API listening on 0.0.0.0:{PORT}  ->  {protocol.INBOX}")
    print(f"  rate limit: {RATE_MAX_PER_WINDOW} notes / {int(RATE_WINDOW_SEC)}s per client;"
          " browser-origin writes refused; inbox receipts + wait status enabled")
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()


if __name__ == "__main__":
    main()
