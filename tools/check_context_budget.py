#!/usr/bin/env python3
"""Guard the small, focused default context that keeps agent work affordable.

This checks only the files an agent must read at task start. It deliberately
excludes code, topic facts, generated maps, Drive narrative, and release docs:
those are loaded on demand when the task actually needs them.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
START_FILES = {
    "AGENTS.md": 5000,
    ".agent/STATE.md": 5000,
    ".agent/rules/execution.md": 5000,
    ".agent/INDEX.md": 2600,
}
DEFAULT_LIMIT = 15000


def main() -> int:
    total = 0
    bad = []
    for rel, limit in START_FILES.items():
        path = ROOT / rel
        if not path.is_file():
            bad.append(f"missing {rel}")
            continue
        size = len(path.read_text(encoding="utf-8"))
        total += size
        if size > limit:
            bad.append(f"{rel} is {size} chars (limit {limit})")
    agents = (ROOT / "AGENTS.md").read_text(encoding="utf-8") if (ROOT / "AGENTS.md").is_file() else ""
    index = (ROOT / ".agent/INDEX.md").read_text(encoding="utf-8") if (ROOT / ".agent/INDEX.md").is_file() else ""
    # Explicitly naming the generated map as opt-in is good; only a positive
    # default-read instruction is a regression.
    if ("read `.agent/CODEMAP.md`" in agents
            and "Do **not** read `.agent/CODEMAP.md`" not in agents):
        bad.append("generated CODEMAP is still mandatory default context")
    if total > DEFAULT_LIMIT:
        bad.append(f"default context is {total} chars (limit {DEFAULT_LIMIT})")
    if bad:
        print("FAILED context budget:")
        for item in bad:
            print(f"  - {item}")
        return 1
    print(f"ok -- focused default context: {total} chars across {len(START_FILES)} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
