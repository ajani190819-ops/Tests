#!/usr/bin/env python3
"""Agent notes receiver - the direct channel between the owner's AgentNotes
Windows app (or any browser) and the agent workspace.

Listens on 0.0.0.0 (the Arena preview proxy exposes it), appends every
incoming note to .agent/INBOX.md with a timestamp, and serves a tiny
quick-capture page at / for when the app is not at hand.

The agent starts this at every turn start and reads .agent/INBOX.md at
notes checkpoints instead of the old Google Doc middleman.

Endpoints:
    GET  /           quick-capture web page (browser fallback)
    GET  /health     liveness probe (the Windows app uses this to find
                     which saved session link is live)
    POST /note       append one note (plain text or JSON {"text": ...})
"""
import json
import sys
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INBOX = ROOT / ".agent" / "INBOX.md"
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765

PAGE = """<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Agent Notes</title>
<style>
 body { background:#12141c; color:#e8eaf2; font:15px/1.5 "Segoe UI",sans-serif;
        max-width:640px; margin:40px auto; padding:0 16px; }
 h1 { font-size:20px; } .sub { color:#8b93a8; font-size:13px; margin-bottom:18px; }
 textarea { width:100%; height:140px; box-sizing:border-box; border-radius:10px;
            border:1px solid #2a2f40; background:#191c27; color:#e8eaf2;
            padding:12px; font:inherit; resize:vertical; }
 button { margin-top:10px; background:#3b82f6; color:#fff; border:0;
          border-radius:10px; padding:11px 22px; font:600 14px "Segoe UI",sans-serif;
          cursor:pointer; }
 #st { margin-top:12px; color:#8b93a8; font-size:13px; }
</style></head><body>
<h1>Agent Notes</h1>
<div class="sub">Type anything for the agent - it lands in the workspace instantly.</div>
<textarea id="t" placeholder="Bug, idea, correction, screenshot description..."></textarea>
<button onclick="send()">Send to agent</button>
<div id="st"></div>
<script>
async function send() {
  const t = document.getElementById('t');
  const st = document.getElementById('st');
  if (!t.value.trim()) { st.textContent = 'Nothing to send.'; return; }
  st.textContent = 'Sending...';
  try {
    const r = await fetch('/note', { method:'POST', body: t.value });
    if (r.ok) { t.value = ''; st.textContent = 'Sent - the agent will see this at the next checkpoint.'; }
    else st.textContent = 'Send failed (' + r.status + ').';
  } catch (e) { st.textContent = 'Send failed: ' + e; }
}
document.getElementById('t').addEventListener('keydown', e => {
  if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) send();
});
</script></body></html>
"""


class Handler(BaseHTTPRequestHandler):
    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")

    def _ok(self, body=b"ok", ctype="text/plain; charset=utf-8"):
        self.send_response(200)
        self._cors()
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_GET(self):
        if self.path.startswith("/health"):
            self._ok(b"ok")
        else:
            self._ok(PAGE.encode("utf-8"), "text/html; charset=utf-8")

    def do_POST(self):
        if not self.path.startswith("/note"):
            self.send_error(404)
            return
        n = int(self.headers.get("Content-Length") or 0)
        if n <= 0 or n > 64000:
            self.send_error(400)
            return
        raw = self.rfile.read(n).decode("utf-8", "replace").strip()
        ctype = (self.headers.get("Content-Type") or "").lower()
        if "json" in ctype:
            try:
                obj = json.loads(raw)
                raw = (obj.get("text") or "").strip()
            except Exception:
                raw = ""
        if not raw:
            self.send_error(400)
            return
        stamp = datetime.now().strftime("%Y-%m-%d %H:%M")
        with INBOX.open("a", encoding="utf-8") as f:
            f.write(f"\n### {stamp}\n{raw}\n")
        self._ok(b"saved")

    def log_message(self, *args):
        pass


def main():
    INBOX.parent.mkdir(parents=True, exist_ok=True)
    if not INBOX.exists():
        INBOX.write_text(
            "# Agent notes inbox\n\n"
            "(Written by tools/notes_server.py - the AgentNotes Windows app and the\n"
            "browser quick-capture page POST here. The agent reads this file at\n"
            "every notes checkpoint.)\n",
            encoding="utf-8",
        )
    print(f"notes server listening on 0.0.0.0:{PORT}  ->  {INBOX}")
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()


if __name__ == "__main__":
    main()
