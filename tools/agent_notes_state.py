#!/usr/bin/env python3
"""Local-only state transitions for the Agent Notes wait/receipt protocol.

This helper never sends a control request through the public preview. It edits
the receiver's ignored local state under the same lock the receiver uses.

Use only these truthful transitions:
  * ``waiting`` immediately before an Arena reply/tool call that leaves the
    agent waiting for owner input. It publishes no question text.
  * ``checkpoint`` immediately before reading .agent/INBOX.md. It snapshots
    exactly which receipts exist at that boundary.
  * ``mark-read <checkpoint>`` immediately after actually reading that inbox
    checkpoint. It acknowledges only the earlier snapshot, never a note that
    may have arrived while the agent was reading.
  * ``clear-wait`` when the owner has answered or agent input is no longer
    required.
"""
from __future__ import annotations

import argparse
import sys

import notes_server


def main() -> int:
    parser = argparse.ArgumentParser(description="Update local Agent Notes wait/receipt state")
    parser.add_argument("command", choices=("waiting", "checkpoint", "mark-read", "clear-wait", "status"))
    parser.add_argument("checkpoint", nargs="?", help="checkpoint token required by mark-read")
    args = parser.parse_args()

    try:
        if args.command == "waiting":
            wait_id = notes_server.set_waiting()
            print(f"published generic Agent Notes wait state: {wait_id}")
        elif args.command == "checkpoint":
            # Token only: callers can safely capture it in a shell variable.
            print(notes_server.begin_inbox_checkpoint())
        elif args.command == "mark-read":
            if not args.checkpoint:
                parser.error("mark-read requires the checkpoint token created before the inbox read")
            count = notes_server.mark_checkpoint_read(args.checkpoint)
            print(f"acknowledged {count} Agent Notes receipt(s) after inbox checkpoint")
        elif args.command == "clear-wait":
            notes_server.clear_waiting()
            print("cleared Agent Notes wait state")
        else:
            print(notes_server.public_status())
    except (OSError, TimeoutError, ValueError) as exc:
        print(f"Agent Notes state was not updated: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
