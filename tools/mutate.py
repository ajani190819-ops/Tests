#!/usr/bin/env python3
"""Re-run every registered break-on-purpose mutation against the test suite.

A test that stays green while the code is broken is a test that lies. These
mutations are the breaks: each one deletes or corrupts exactly one safety
property, and the suite must fail. The registry (tools/mutations.json) grows
every turn - any new mutation written while working gets appended in the same
commit, so the whole history of "what the tests must never miss" stays alive
instead of living in a scratch script that dies with the sandbox.

Usage:
    python3 tools/mutate.py            run every registered mutation
    python3 tools/mutate.py --list     show the registry
    python3 tools/mutate.py --check    only verify every needle still matches
                                       its source (fast drift guard)
    python3 tools/mutate.py marker-03  run one mutation by id

Exit code is non-zero if any mutation SURVIVED, if a needle went STALE (the
source moved and the mutation no longer applies - fix the registry entry), or
if the tree was left modified. Sources are always restored byte-for-byte.
"""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REG = ROOT / "tools" / "mutations.json"


def run_suite() -> int:
    r = subprocess.run([sys.executable, "-m", "unittest", "tests.test_windows_apps"],
                       capture_output=True, text=True, cwd=str(ROOT))
    return r.returncode


def main(argv: list[str]) -> int:
    entries = json.loads(REG.read_text(encoding="utf-8"))
    if "--list" in argv:
        for e in entries:
            print("%-12s %-46s %s" % (e["id"], e["file"], e["label"]))
        return 0
    if "--check" in argv:
        stale = 0
        for e in entries:
            src = (ROOT / e["file"]).read_text(encoding="utf-8")
            if src.count(e["old"]) < 1:
                print("STALE %s  %s" % (e["id"], e["label"]))
                stale += 1
        print("%d entries, %d stale" % (len(entries), stale))
        return 1 if stale else 0

    wanted = [a for a in argv[1:] if not a.startswith("-")]
    orig = {}
    survived, stale = [], []
    for e in entries:
        if wanted and e["id"] not in wanted:
            continue
        p = ROOT / e["file"]
        b = p.read_bytes()
        orig.setdefault(e["file"], b)
        s = b.decode("utf-8")
        if s.count(e["old"]) < 1:
            print("STALE    %-12s needle no longer matches - registry entry needs repair" % e["id"])
            stale.append(e["id"])
            continue
        p.write_text(s.replace(e["old"], e["new"], 1), encoding="utf-8")
        rc = run_suite()
        p.write_bytes(orig[e["file"]])
        if rc != 0:
            print("DETECTED %-12s %s" % (e["id"], e["label"]))
        else:
            print("SURVIVED %-12s %s   <-- the suite missed a real break" % (e["id"], e["label"]))
            survived.append(e["id"])
    for rel, b in orig.items():
        if (ROOT / rel).read_bytes() != b:
            (ROOT / rel).write_bytes(b)
            print("RESTORED %s" % rel)
    if run_suite() != 0:
        print("FINAL    suite does not pass on the restored tree - investigate")
        return 1
    print("FINAL    %d run, %d survived, %d stale - tree restored, suite green"
          % (len(entries) - len(stale), len(survived), len(stale)))
    return 1 if (survived or stale) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
