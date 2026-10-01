#!/usr/bin/env python3
"""Copy the shared non-planar engine from the standalone tool into the plugin.

    python3 tools/sync_engine.py          # sync, report what changed
    python3 tools/sync_engine.py --check  # fail if out of sync, change nothing

WHY THIS EXISTS
  One engine, two front ends (decision C1). The engine source lives in two
  places because both front ends must be single self-contained files:

    plugins/unlayered-infill/unlayered_infill_post.py   plain source, between
                                                        BEGIN/END markers
    plugins/unlayered-infill/unlayered_infill_orca.py   the same text as an
                                                        escaped string literal
                                                        (_NONPLANAR_CORE_SRC),
                                                        exec'd at import

  The standalone file is the one you EDIT -- it is ordinary readable Python.
  Then run this script to push it into the plugin. `tests/test_post_script.py`
  fails if the two ever disagree, so a forgotten sync cannot ship.
"""
import ast
import json
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
POST = REPO / "plugins" / "unlayered-infill" / "unlayered_infill_post.py"
ORCA = REPO / "plugins" / "unlayered-infill" / "unlayered_infill_orca.py"

BEGIN = "# --- BEGIN nonplanar_core"
END = "# --- END nonplanar_core"
LITERAL = re.compile(r'^_NONPLANAR_CORE_SRC = (".*")$', re.MULTILINE)


def read_engine():
    """The engine text from the standalone tool -- the editable copy."""
    src = POST.read_text(encoding="utf-8")
    i, j = src.find(BEGIN), src.find(END)
    if i == -1 or j == -1:
        sys.exit(f"ERROR: {POST.name} is missing its BEGIN/END nonplanar_core markers")
    return src[src.index("\n", i) + 1:j]


def current_literal_text():
    """The engine text currently embedded in the plugin."""
    src = ORCA.read_text(encoding="utf-8")
    m = LITERAL.search(src)
    if not m:
        sys.exit(f"ERROR: could not find _NONPLANAR_CORE_SRC in {ORCA.name}")
    return ast.literal_eval(m.group(1)), src, m


def main():
    check_only = "--check" in sys.argv[1:]
    engine = read_engine()

    # Must stand on its own: it is exec'd into a bare module at plugin import.
    try:
        ast.parse(engine)
    except SyntaxError as e:
        sys.exit(f"ERROR: the engine does not parse on its own: line {e.lineno}: {e.msg}")

    embedded, orca_src, m = current_literal_text()
    if embedded == engine:
        print(f"already in sync ({len(engine)} chars)")
        return 0

    if check_only:
        print("OUT OF SYNC: the plugin's engine differs from the standalone's.")
        print("Run:  python3 tools/sync_engine.py")
        return 1

    # json.dumps produces a double-quoted literal whose escapes are all valid
    # Python too (\" \\ \n \t \r and \uXXXX), so it round-trips exactly.
    literal = json.dumps(engine)
    if ast.literal_eval(literal) != engine:
        sys.exit("ERROR: the re-encoded literal does not round-trip; refusing to write")

    ORCA.write_text(orca_src[:m.start(1)] + literal + orca_src[m.end(1):],
                    encoding="utf-8")

    # Prove the written file still parses and still holds the right text.
    after, _, _ = current_literal_text()
    if after != engine:
        sys.exit("ERROR: wrote the plugin but it did not read back identical")
    ast.parse(ORCA.read_text(encoding="utf-8"))

    print(f"synced {len(engine)} chars: {POST.name} -> {ORCA.name}")
    print(f"  ({len(embedded)} chars before, {len(engine)} after)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
