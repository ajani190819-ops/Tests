#!/usr/bin/env python3
"""End-to-end tests for Agent Notes' API delivery and receipt protocol."""
from __future__ import annotations

import json
import pathlib
import sys
import tempfile
import threading
import unittest
from http.server import ThreadingHTTPServer
from unittest import mock
from urllib.error import HTTPError
from urllib.request import Request, urlopen

ROOT = pathlib.Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools import agent_notes_protocol as protocol
from tools import notes_server


class AgentNotesApiTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        state_dir = pathlib.Path(self.tmp.name) / ".agent"
        self.inbox = state_dir / "INBOX.md"
        self.state = state_dir / "NOTES_STATE.json"
        self.lock = state_dir / "NOTES_STATE.lock"
        self.path_patch = mock.patch.multiple(
            protocol, INBOX=self.inbox, STATE=self.state, STATE_LOCK=self.lock
        )
        self.path_patch.start()
        protocol.initialise_inbox()

        self.server = ThreadingHTTPServer(("127.0.0.1", 0), notes_server.Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = f"http://127.0.0.1:{self.server.server_port}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=3)
        self.path_patch.stop()
        self.tmp.cleanup()

    def request(self, method: str, path: str, body: bytes | None = None,
                headers: dict[str, str] | None = None) -> tuple[int, bytes]:
        request = Request(self.base + path, data=body, headers=headers or {}, method=method)
        try:
            with urlopen(request, timeout=3) as response:
                return response.status, response.read()
        except HTTPError as response:
            return response.code, response.read()

    def post_note(self, key: str, text: str) -> tuple[int, dict]:
        status, body = self.request(
            "POST", "/note", text.encode("utf-8"),
            {"Content-Type": "text/plain; charset=utf-8", "X-Agent-Note-Key": key},
        )
        return status, json.loads(body)

    def test_delivery_checkpoint_and_read_status_are_distinct(self):
        status, body = self.request("GET", "/health")
        self.assertEqual((status, body), (200, b"ok"))
        status, body = self.request("GET", "/status")
        self.assertEqual(status, 200)
        self.assertFalse(json.loads(body)["waiting"])

        wait_id = protocol.set_waiting()
        status, body = self.request("GET", "/status")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body), {"waiting": True, "wait_id": wait_id})
        protocol.clear_waiting()

        first_status, first = self.post_note("n-e2e-first-001", "First durable note")
        self.assertEqual(first_status, 200)
        self.assertEqual(first["state"], "saved")
        self.assertEqual(self.inbox.read_text(encoding="utf-8").count("First durable note"), 1)
        status, body = self.request("GET", "/receipt/" + first["receipt_id"])
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)["state"], "saved")

        # Only notes that existed at the checkpoint can become Read.
        checkpoint = protocol.begin_inbox_checkpoint()
        second_status, second = self.post_note("n-e2e-second-002", "Arrived after checkpoint")
        self.assertEqual(second_status, 200)
        self.assertEqual(protocol.mark_checkpoint_read(checkpoint), 1)

        status, body = self.request("GET", "/receipt/" + first["receipt_id"])
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)["state"], "read")
        status, body = self.request("GET", "/receipt/" + second["receipt_id"])
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)["state"], "saved")
        self.assertEqual(self.inbox.read_text(encoding="utf-8").count("AgentNotes key="), 2)

    def test_retry_recovers_append_after_receipt_state_write_failure(self):
        key = "n-e2e-crash-003"
        real_save_state = protocol.save_state
        should_fail = True

        def fail_once(state):
            nonlocal should_fail
            if should_fail:
                should_fail = False
                raise OSError("simulated process interruption after inbox append")
            real_save_state(state)

        with mock.patch.object(protocol, "save_state", side_effect=fail_once):
            status, body = self.request(
                "POST", "/note", b"Retry must not duplicate this note",
                {"Content-Type": "text/plain", "X-Agent-Note-Key": key},
            )
        self.assertEqual(status, 500)
        self.assertIn("could not persist", json.loads(body)["error"])

        inbox_after_failure = self.inbox.read_text(encoding="utf-8")
        markers_after_failure = protocol.MARKER_RE.findall(inbox_after_failure)
        self.assertEqual(len([item for item in markers_after_failure if item[0] == key]), 1,
                         "the first attempt must have durably appended before state failure")
        original_receipt = next(receipt for note_key, receipt in markers_after_failure
                                if note_key == key)

        retry_status, retry = self.post_note(key, "Retry must not duplicate this note")
        self.assertEqual(retry_status, 200)
        self.assertEqual(retry["receipt_id"], original_receipt)
        final_inbox = self.inbox.read_text(encoding="utf-8")
        self.assertEqual(final_inbox.count("Retry must not duplicate this note"), 1)
        self.assertEqual(len([item for item in protocol.MARKER_RE.findall(final_inbox)
                              if item[0] == key]), 1)
        status, body = self.request("GET", "/receipt/" + original_receipt)
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)["state"], "saved")

    def test_server_is_api_only_and_refuses_browser_origin_writes(self):
        for path in ("/", "/nonce"):
            status, _body = self.request("GET", path)
            self.assertEqual(status, 404, f"obsolete browser route remains at {path}")

        status, _body = self.request(
            "POST", "/note", b"must not enter inbox",
            {"Content-Type": "text/plain", "X-Agent-Note-Key": "n-browser-origin-004",
             "Origin": "https://attacker.example"},
        )
        self.assertEqual(status, 403)
        self.assertNotIn("must not enter inbox", self.inbox.read_text(encoding="utf-8"))

        status, body = self.request(
            "POST", "/note", b"missing idempotency key", {"Content-Type": "text/plain"}
        )
        self.assertEqual(status, 400)
        self.assertNotIn("missing idempotency key", self.inbox.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
