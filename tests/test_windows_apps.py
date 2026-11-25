#!/usr/bin/env python3
"""Static verification suite for the Windows desktop applications in this repository:
  - keyboard-lighting/  (ROG Strix G16 RGB tray app, HID engine, zone calibration)
  - bluetooth-guard/    (Win32 Bluetooth auto-connect blocker & quick-connect tray app)
  - project-hub/        (Unified Windows Control Center status + updater app)
  - spatial-hud-template-26.3/ PowerShell/batch updaters

Enforces:
  1. Strict 7-bit ASCII across .cs, .cs.txt, and .bat files (and ASCII-clean or UTF-8 BOM
     across .ps1 files) so stock Windows PowerShell 5.1 (CP-1252) never mojibakes tokens.
  2. Pure C# 5.0 compatibility across all .cs / .cs.txt / embedded C# blocks so Windows
     Framework64\\v4.0.30319\\csc.exe compiles cleanly with zero external SDKs.
  3. Pure Windows PowerShell 5.1 syntax + balanced delimiters across every .ps1 script.
  4. Pure CRLF line endings + quote-redirection safety across every .bat file in the repo,
     with Orca-Plugins.bat kept as the sole root .bat file.
  5. Architectural & regression contracts for keyboard-lighting, bluetooth-guard, and
     project-hub (no UI-thread deadlocks, no stale HSEQB URLs, no personal path PII).
"""
from pathlib import Path
import ast
import json
import math
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parent.parent
# Set only by the mutation worker. Clean pre/post runs verify all needles;
# replacing the active file's own needle is not a behavioral mutation kill.
MUTATION_FILE = None


def read_c_inlined(rel, encoding="utf-8", errors="replace"):
    """Read a C source, inlining any local `#include "x.c"` part-files (the
    unity-build modularization) so callers see the whole translation unit.
    Pass encoding="ascii", errors="strict" to enforce 7-bit ASCII across the
    main file AND every part (the keyboard-lighting ASCII contract)."""
    path = ROOT / rel
    text = path.read_text(encoding=encoding, errors=errors)
    base = path.parent

    def _inline(m):
        p = base / m.group(1)
        return p.read_text(encoding=encoding, errors=errors) if p.exists() else m.group(0)

    return re.sub(r'^[ \t]*#include[ \t]+"([^"]+\.c)"[ \t]*$', _inline, text, flags=re.M)


def strip_cs_comments_and_strings(src: str):
    """Stateful C# lexer: verifies delimiter balance and returns stripped code text."""
    out = []
    stack = []
    i = 0
    n = len(src)
    pairs = {"(": ")", "[": "]", "{": "}"}
    closers = {v: k for k, v in pairs.items()}

    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""

        # Line comment
        if c == "/" and nxt == "/":
            while i < n and src[i] != "\n":
                i += 1
            continue

        # Block comment
        if c == "/" and nxt == "*":
            i += 2
            closed = False
            while i + 1 < n:
                if src[i] == "*" and src[i + 1] == "/":
                    i += 2
                    closed = True
                    break
                if src[i] == "\n":
                    out.append("\n")
                i += 1
            if not closed:
                raise AssertionError("Unterminated /* block comment in C# source")
            continue

        # Verbatim string @"..."
        if c == "@" and nxt == '"':
            out.append('""')
            i += 2
            closed = False
            while i < n:
                if src[i] == '"':
                    if i + 1 < n and src[i + 1] == '"':
                        i += 2
                        continue
                    i += 1
                    closed = True
                    break
                if src[i] == "\n":
                    out.append("\n")
                i += 1
            if not closed:
                raise AssertionError("Unterminated @\"...\" verbatim string in C# source")
            continue

        # Interpolated string $"..." -> illegal in C# 5.0!
        if c == "$" and nxt == '"':
            line_no = src[:i].count("\n") + 1
            raise AssertionError(f"C# 6+ interpolated string ($\"...\") at line {line_no} breaks csc v4.0.30319")

        # Regular string "..."
        if c == '"':
            out.append('""')
            i += 1
            closed = False
            while i < n:
                if src[i] == "\\":
                    i += 2
                    continue
                if src[i] == '"':
                    i += 1
                    closed = True
                    break
                if src[i] == "\n":
                    line_no = src[:i].count("\n") + 1
                    raise AssertionError(f"Unterminated string literal at line {line_no}")
                i += 1
            if not closed:
                raise AssertionError("Unterminated string literal at EOF")
            continue

        # Char literal '...'
        if c == "'":
            out.append("'x'")
            i += 1
            closed = False
            while i < n:
                if src[i] == "\\":
                    i += 2
                    continue
                if src[i] == "'":
                    i += 1
                    closed = True
                    break
                i += 1
            if not closed:
                raise AssertionError("Unterminated char literal")
            continue

        if c in pairs:
            stack.append((c, src[:i].count("\n") + 1))
        elif c in closers:
            if not stack:
                line_no = src[:i].count("\n") + 1
                raise AssertionError(f"Unmatched closer '{c}' at line {line_no}")
            top, open_line = stack.pop()
            if top != closers[c]:
                line_no = src[:i].count("\n") + 1
                raise AssertionError(
                    f"Mismatched delimiter '{top}' (line {open_line}) closed by '{c}' (line {line_no})"
                )

        out.append(c)
        i += 1

    if stack:
        top, open_line = stack[-1]
        raise AssertionError(f"Unclosed delimiter '{top}' opened at line {open_line}")

    return "".join(out)


def lex_powershell(src: str):
    """Stateful PowerShell 5.1 lexer checking delimiter balance and PS 7+ syntax leaks."""
    out = []
    stack = []
    i = 0
    n = len(src)
    pairs = {"(": ")", "[": "]", "{": "}"}
    closers = {v: k for k, v in pairs.items()}

    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""

        # Block comment <# ... #>
        if c == "<" and nxt == "#":
            i += 2
            closed = False
            while i + 1 < n:
                if src[i] == "#" and src[i + 1] == ">":
                    i += 2
                    closed = True
                    break
                if src[i] == "\n":
                    out.append("\n")
                i += 1
            if not closed:
                raise AssertionError("Unterminated <# ... #> block comment")
            continue

        # Line comment # ...
        if c == "#":
            while i < n and src[i] != "\n":
                i += 1
            continue

        # Single-quoted here-string @'\n ... \n'@
        if c == "@" and nxt == "'" and (i + 2 < n and src[i + 2] in ("\r", "\n")):
            i += 2
            end_match = re.search(r"(?m)^'@", src[i:])
            if not end_match:
                raise AssertionError("Unterminated @' ... '@ here-string")
            chunk = src[i : i + end_match.end()]
            out.append("''" + "\n" * chunk.count("\n"))
            i += end_match.end()
            continue

        # Double-quoted here-string @"\n ... \n"@
        if c == "@" and nxt == '"' and (i + 2 < n and src[i + 2] in ("\r", "\n")):
            i += 2
            end_match = re.search(r'(?m)^"@', src[i:])
            if not end_match:
                raise AssertionError('Unterminated @" ... "@ here-string')
            chunk = src[i : i + end_match.end()]
            out.append('""' + "\n" * chunk.count("\n"))
            i += end_match.end()
            continue

        # Single-quoted string '...'
        if c == "'":
            out.append("''")
            i += 1
            closed = False
            while i < n:
                if src[i] == "'":
                    if i + 1 < n and src[i + 1] == "'":
                        i += 2
                        continue
                    i += 1
                    closed = True
                    break
                if src[i] == "\n":
                    out.append("\n")
                i += 1
            if not closed:
                raise AssertionError("Unterminated single-quoted string in PowerShell file")
            continue

        # Double-quoted string "..." (note: handle ` escapes; ignore inner braces safely)
        if c == '"':
            out.append('""')
            i += 1
            closed = False
            while i < n:
                if src[i] == "`":
                    if i + 1 < n and src[i + 1] == "\n":
                        out.append("\n")
                    i += 2
                    continue
                if src[i] == '"':
                    if i + 1 < n and src[i + 1] == '"':
                        i += 2
                        continue
                    i += 1
                    closed = True
                    break
                if src[i] == "\n":
                    out.append("\n")
                i += 1
            if not closed:
                raise AssertionError("Unterminated double-quoted string in PowerShell file")
            continue

        # Backtick escape outside string (line continuation)
        if c == "`":
            out.append(" ")
            i += 1
            if i < n:
                if src[i] == "\n":
                    out.append("\n")
                i += 1
            continue

        if c in pairs:
            stack.append((c, src[:i].count("\n") + 1))
        elif c in closers:
            if not stack:
                line_no = src[:i].count("\n") + 1
                raise AssertionError(f"Unmatched closer '{c}' at line {line_no}")
            top, open_line = stack.pop()
            if top != closers[c]:
                line_no = src[:i].count("\n") + 1
                raise AssertionError(
                    f"Mismatched delimiter '{top}' (line {open_line}) closed by '{c}' (line {line_no})"
                )

        out.append(c)
        i += 1

    if stack:
        top, open_line = stack[-1]
        raise AssertionError(f"Unclosed delimiter '{top}' opened at line {open_line}")

    return "".join(out)


class WindowsDesktopAppsTests(unittest.TestCase):
    CS_FILES = [
        ROOT / "keyboard-lighting" / "ui_controls.cs.txt",
        ROOT / "bluetooth-guard" / "BluetoothCore.cs",
        ROOT / "project-hub" / "HubCore.cs",
    ]

    PS_FILES = sorted(
        p for p in ROOT.rglob("*.ps1") if ".git" not in p.parts
    )

    BAT_FILES = sorted(
        p for p in ROOT.rglob("*.bat") if ".git" not in p.parts
    )

    def test_all_csharp_files_are_pure_ascii_and_csharp5_compatible(self):
        cs6_forbidden = [
            (r"\?\.", "null-conditional '?.'"),
            (r"\bnameof\s*\(", "nameof(...)"),
            (r"\busing\s+var\b", "'using var' declaration"),
            (r"\{\s*get;\s*set;\s*\}\s*=", "auto-property initializer"),
        ]

        # Include embedded C# sources inside Setup.ps1 / Aura-Background.ps1 here-strings
        embedded = []
        for ps in self.PS_FILES:
            raw_text = ps.read_text(encoding="utf-8")
            for idx, m in enumerate(re.finditer(r"(?ms)@'\r?\n(using System;.*?)\r?\n'@", raw_text)):
                embedded.append((f"{ps.relative_to(ROOT)}#embedded[{idx}]", m.group(1)))

        targets = [(str(p.relative_to(ROOT)), p.read_bytes().decode("ascii")) for p in self.CS_FILES] + embedded
        self.assertGreaterEqual(len(targets), 6)

        for label, src in targets:
            with self.subTest(target=label):
                stripped = strip_cs_comments_and_strings(src)
                for pattern, desc in cs6_forbidden:
                    m = re.search(pattern, stripped)
                    if m:
                        line_no = stripped[: m.start()].count("\n") + 1
                        self.fail(f"{label} uses C# 6+ syntax ({desc}) at line {line_no}: {m.group(0)!r}")
                # Check C# 7 call-site 'out Type var' (as opposed to method/interface/extern parameter declarations)
                for m in re.finditer(
                    r"\bout\s+(?:int|uint|long|ulong|bool|byte|short|ushort|IntPtr|Guid|string|var)\s+\w+",
                    stripped,
                ):
                    stmt_start = max(
                        stripped.rfind(";", 0, m.start()),
                        stripped.rfind("{", 0, m.start()),
                        stripped.rfind("}", 0, m.start()),
                    ) + 1
                    stmt_prefix = stripped[stmt_start : m.start()].strip()
                    stmt_prefix = re.sub(r"^(?:\[[^\]]*\]\s*)+", "", stmt_prefix)
                    is_decl = bool(
                        re.search(r"\b(?:extern|public|private|internal|protected|static|void)\b", stmt_prefix)
                        or re.match(r"^[\w.<>\[\]]+\s+\w+\s*\(", stmt_prefix)
                    )
                    if not is_decl:
                        line_no = stripped[: m.start()].count("\n") + 1
                        self.fail(
                            f"{label} uses C# 7+ call-site inline out variable at line {line_no}: {m.group(0)!r}"
                        )

    def test_all_powershell_files_lex_cleanly_and_avoid_ps7_operators(self):
        self.assertGreaterEqual(len(self.PS_FILES), 9)
        for ps in self.PS_FILES:
            rel = str(ps.relative_to(ROOT))
            with self.subTest(ps=rel):
                raw = ps.read_bytes()
                # Either pure 7-bit ASCII or explicit UTF-8 BOM so Windows PowerShell 5.1 reads cleanly
                if not raw.startswith(b"\xef\xbb\xbf"):
                    bad_bytes = [i for i, b in enumerate(raw) if b >= 0x80]
                    if bad_bytes:
                        line_no = raw[: bad_bytes[0]].count(b"\n") + 1
                        self.fail(
                            f"{rel} contains non-ASCII byte 0x{raw[bad_bytes[0]]:02x} at line {line_no} "
                            "without UTF-8 BOM (breaks Windows PowerShell 5.1 CP-1252 parser)"
                        )
                text = raw.decode("utf-8-sig")
                stripped = lex_powershell(text)
                for pat, desc in [
                    (r"\?\?=", "PS7 null-coalescing assignment '??='"),
                    (r"\?\?", "PS7 null-coalescing operator '??'"),
                    (r"\?\.", "PS7 null-conditional operator '?.'"),
                ]:
                    m = re.search(pat, stripped)
                    if m:
                        line_no = stripped[: m.start()].count("\n") + 1
                        self.fail(f"{rel} uses {desc} at line {line_no}")

    def test_all_bat_files_pure_crlf_and_quote_safe(self):
        root_bats = [p.name for p in ROOT.glob("*.bat")]
        self.assertEqual(root_bats, ["Orca-Plugins.bat"], "Orca-Plugins.bat must remain the sole root .bat")

        for bat in self.BAT_FILES:
            rel = str(bat.relative_to(ROOT))
            with self.subTest(bat=rel):
                raw = bat.read_bytes()
                bare_lf = sum(1 for i, b in enumerate(raw) if b == 0x0A and (i == 0 or raw[i - 1] != 0x0D))
                self.assertEqual(bare_lf, 0, f"{rel} has {bare_lf} bare LF line endings (must be pure CRLF)")
                text = raw.decode("ascii")
                for line_no, line in enumerate(text.splitlines(), 1):
                    if ">nul" in line.lower():
                        for q in re.findall(r'"([^"]*)"', line):
                            for ch in ("<", ">", "|"):
                                self.assertNotIn(
                                    ch, q,
                                    f"{rel}:{line_no} contains '{ch}' inside double quotes on a '>nul' line",
                                )

    def test_no_stale_urls_or_personal_paths(self):
        scan_dirs = [
            ROOT / "keyboard-lighting",
            ROOT / "bluetooth-guard",
            ROOT / "project-hub",
            ROOT / "dns-pilot",
            ROOT / "spatial-hud-template-26.3",
        ]
        for base in scan_dirs:
            for p in base.rglob("*"):
                if not p.is_file() or p.suffix.lower() in (".ico", ".png", ".jar", ".exe", ".res"):
                    continue
                text = p.read_text(encoding="utf-8", errors="replace")
                rel = str(p.relative_to(ROOT))
                self.assertNotIn("HSEQB", text, f"Stale repo slug HSEQB found in {rel}")
                self.assertIsNone(
                    re.search(r"(?i)C:\\Users\\(?!Public\b|Default\b|<|\$)", text),
                    f"Hardcoded personal user path found in {rel}",
                )

    def test_project_hub_starts_without_elevation_but_children_keep_their_boundaries(self):
        hub = (ROOT / "project-hub" / "src" / "manifest.xml").read_text(encoding="utf-8")
        self.assertIn('level="asInvoker"', hub,
                      "opening Project Hub still asks for administrator consent")
        self.assertNotIn("requireAdministrator", hub,
                         "Project Hub still has a launch-time administrator requirement")

        for rel in ("bluetooth-guard/src/manifest.xml", "dns-pilot/src/manifest.xml",
                    "agent-notes/src/manifest.xml", "weather-radar/src/manifest.xml"):
            with self.subTest(app=rel):
                self.assertIn('level="requireAdministrator"',
                              (ROOT / rel).read_text(encoding="utf-8"),
                              f"{rel} lost its app-owned elevation boundary")
        kbl = (ROOT / "keyboard-lighting" / "src" / "manifest.xml").read_text(encoding="utf-8")
        self.assertIn('level="asInvoker"', kbl,
                      "Keyboard Lighting must remain unelevated for M4/M5 token launches")

    def test_native_windows_gui_executables_are_valid_x64_gui_pe_binaries(self):
        import struct

        allowed_prefixes = ("api-ms-win-crt-",)
        allowed_dlls = {
            "kernel32.dll",
            "user32.dll",
            "gdi32.dll",
            "shell32.dll",
            "comctl32.dll",
            "advapi32.dll",
            "ole32.dll",
            "winhttp.dll",
            "comdlg32.dll",
            "msvcrt.dll",
            "ws2_32.dll",
            "iphlpapi.dll",
            # Windows CNG, in System32 since Vista: ProjectHub hashes local
            # files to compare them against the remote git blob hashes, so a
            # same-size rebuild can no longer look "up to date".
            "bcrypt.dll",
        }
        # Project Hub and Keyboard Lighting run asInvoker. Hub only coordinates
        # user-profile installs; child apps keep their own elevation manifests,
        # so a prompt occurs only when an explicitly selected action needs it.
        # Keyboard Lighting also has to stay asInvoker because G-Helper's M4/M5
        # Custom action launches it per keypress to write a user-profile token.
        # The full KBL startup/elevation contract remains pinned by
        # test_keyboard_lighting_brightness_taps_never_cost_a_uac_prompt.
        for rel in (
            "project-hub/ProjectHub.exe",
            "bluetooth-guard/BluetoothGuard.exe",
            "keyboard-lighting/KeyboardLighting.exe",
            "dns-pilot/DnsPilot.exe",
            "agent-notes/AgentNotes.exe",
            "weather-radar/WeatherRadar.exe",
        ):
            exe = ROOT / rel
            self.assertTrue(exe.exists(), f"Missing standalone native executable {rel}")
            data = exe.read_bytes()
            self.assertEqual(data[:2], b"MZ", f"{rel} missing MZ DOS header")
            pe_off = struct.unpack_from("<I", data, 0x3C)[0]
            self.assertEqual(data[pe_off : pe_off + 4], b"PE\x00\x00", f"{rel} missing PE signature")
            machine, num_sections = struct.unpack_from("<HH", data, pe_off + 4)
            self.assertEqual(machine, 0x8664, f"{rel} must target x86_64 Windows")
            opt_off = pe_off + 24
            subsystem = struct.unpack_from("<H", data, opt_off + 68)[0]
            self.assertEqual(subsystem, 2, f"{rel} must use IMAGE_SUBSYSTEM_WINDOWS_GUI (no console window)")
            self.assertIn(b"PerMonitorV2", data, f"{rel} must embed PerMonitorV2 DPI manifest")
            self.assertIn(b"Microsoft.Windows.Common-Controls", data, f"{rel} must embed ComCtl32 v6 manifest")
            if rel in ("project-hub/ProjectHub.exe", "keyboard-lighting/KeyboardLighting.exe"):
                self.assertIn(b'level="asInvoker"', data,
                              f"{rel} must run asInvoker rather than require elevation at launch")
                self.assertNotIn(b"requireAdministrator", data,
                                 f"{rel} requires administrator again")
            else:
                self.assertIn(b"requireAdministrator", data,
                              f"{rel} must preserve its own action-level elevation boundary")

            imp_rva, _ = struct.unpack_from("<II", data, opt_off + 112 + 8)
            sec_off = opt_off + struct.unpack_from("<H", data, pe_off + 20)[0]
            sections = []
            for i in range(num_sections):
                s = data[sec_off + i * 40 : sec_off + (i + 1) * 40]
                vsize, va, rsize, rptr = struct.unpack_from("<IIII", s, 8)
                sections.append((va, max(vsize, rsize), rptr))

            def rva_to_offset(rva):
                for va, span, rptr in sections:
                    if va <= rva < va + span:
                        return rptr + (rva - va)
                return None

            off = rva_to_offset(imp_rva)
            while off is not None:
                ilt, _, _, name_rva, _ = struct.unpack_from("<IIIII", data, off)
                if not ilt and not name_rva:
                    break
                noff = rva_to_offset(name_rva)
                dll = data[noff : data.index(b"\x00", noff)].decode("ascii").lower()
                self.assertTrue(
                    dll in allowed_dlls or dll.startswith(allowed_prefixes),
                    f"{rel} imports non-stock DLL {dll!r}",
                )
                off += 20

    def test_keyboard_lighting_reliability_contracts(self):
        tray = (ROOT / "keyboard-lighting" / "Tray.ps1").read_text(encoding="utf-8")
        aura = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(encoding="utf-8")
        ui_cs = (ROOT / "keyboard-lighting" / "ui_controls.cs.txt").read_text(encoding="utf-8")
        self.assertTrue((ROOT / "keyboard-lighting" / "Restart.bat").exists())

        # SetUnhandledExceptionMode MUST execute before creating any Form control
        exc_pos = tray.index("SetUnhandledExceptionMode")
        form_pos = tray.index("New-Object System.Windows.Forms.Form")
        self.assertLess(exc_pos, form_pos, "SetUnhandledExceptionMode must run before Form creation")

        # No PowerShell SystemEvents deadlocks or WMI Win32_Process scans anywhere
        self.assertNotIn("Register-ObjectEvent", tray)
        self.assertNotIn("Register-ObjectEvent", aura)
        self.assertNotIn("Get-CimInstance", tray)
        self.assertNotIn("Win32_Process", tray)
        self.assertNotIn("Win32_Process", aura)
        self.assertIn("public static class PowerWatch", ui_cs)
        self.assertIn("public static class AuraPowerWatch", aura)
        self.assertIn("engine.pid", tray)
        self.assertIn("engine.pid", aura)

    def test_keyboard_lighting_dither_keeps_per_lamp_phase_across_blackout(self):
        """A lamp that blacks out must come back on its own dither phase.

        The four keyboard lamps almost always carry the same colour. SeedDither
        gives each lamp/channel a different golden-ratio starting fraction so
        they round - and therefore step - on different frames; that is what
        stops the whole keyboard pulsing in lockstep (the owner's "backlight
        stutters/flashes"). The blackout path in SetZoneG used to reset the
        carried error to 0.0 for every lamp at once, so after any off/on cycle
        (master slider to 0 and back up, or an effect's black phase ending) all
        four lamps restarted in phase and the shimmer came back.
        """
        aura = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="utf-8")

        self.assertIn("void SeedLamp(int i) {", aura,
                      "the per-lamp dither seed is not split out, so the blackout "
                      "path cannot restore a single lamp's phase")
        self.assertIn("for (int i = 0; i < LampCount; i++) SeedLamp(i);", aura,
                      "SeedDither no longer delegates to SeedLamp, so the two can "
                      "drift apart")

        # Reopen must still reseed after a resume/power cycle.
        reopen = aura[aura.index("public bool Reopen()"):]
        reopen = reopen[:reopen.index("\n  }\n")]
        self.assertIn("SeedDither();", reopen,
                      "a resume no longer reseeds the dither, so the lamps come "
                      "back in phase after every sleep/wake")

        # The blackout branch must reseed, not zero.
        blk = aura[aura.index("if (br <= 0.0005) {"):]
        blk = blk[:blk.index("return;")]
        self.assertIn("SeedLamp(i);", blk,
                      "a blacked-out lamp is not reset to its own seed phase")
        self.assertNotIn("er[i] = 0.0; eg[i] = 0.0; eb[i] = 0.0;", blk,
                         "the blackout path zeroes every lamp's carried error at "
                         "once, re-syncing the keyboard lamps into a lockstep pulse")

    def test_keyboard_lighting_animation_position_uses_an_even_step(self):
        """Animation position must stride evenly; physical decay must not.

        dt is used for two things that want opposite properties. A decay needs
        to know how much wall-clock time really passed, which is what makes it
        frame-rate independent. A position only needs each frame to be the same
        distance from the last. Feeding measured dt into the phase makes the
        gradient advance 16.2ms one frame, 17.1ms the next, 15.8ms after that,
        and when a frame misses its deadline the loop resyncs so the next dt is
        short. The loop's own notes put those misses at roughly 1% of frames
        from ordinary scheduler jitter - about one a second at 60fps, which is
        the residual "smooth but I can still see it if I look" step on four
        large deck zones that have nothing between them to average it out.

        The danger in fixing this is over-applying it: passing the nominal step
        to RenderGroup as well would make fire and sparkle decay frame-rate
        dependent again, which is a worse regression than the one being fixed.
        So both halves are pinned here.
        """
        aura = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")

        # Nominal step derives from Fps, so the adaptive backoff that lowers
        # Fps on a sustained overrun keeps real-time animation speed, and a
        # zero Fps cannot divide by zero.
        self.assertIn(
            "double NominalDt { get { return Fps > 0 ? 1.0 / Fps : 1.0 / 60.0; } }",
            aura, "the nominal step is not derived from Fps, so the adaptive "
                  "backoff would change animation speed as well as frame rate")
        self.assertIn("public bool   FixedStep  = true;", aura,
                      "the even-step behaviour is not on by default")
        self.assertIn("$eng.FixedStep = -not ($env:KBL_FIXED_STEP -eq '0')", aura,
                      "there is no way to restore the old measured-dt behaviour "
                      "without editing the engine")

        # Position takes the even step.
        self.assertIn("gr.Phase += (FixedStep ? NominalDt : dt) * gr.Speed;", aura,
                      "animation position is still driven by measured frame time, "
                      "so scheduler jitter makes the gradient stride unevenly")

        # Physics must keep the measured dt.
        self.assertIn("RenderGroup(gr, gr.Phase, dt);", aura,
                      "RenderGroup no longer receives the measured dt, so the "
                      "fire and sparkle decays become frame-rate dependent again")
        for needle, what in (
            ("Math.Pow(0.28, dt)", "the fire fade"),
            ("Math.Pow(0.02, dt)", "the sparkle cooling"),
            ("gr.XfadeLeft -= dt;", "the palette crossfade countdown"),
        ):
            self.assertIn(needle, aura,
                          "%s no longer uses the measured dt, so it is no longer "
                          "frame-rate independent" % what)

        # The outer phase still tracks real time: it drives Now and the overlay
        # windows, which are wall-clock durations, not animation position.
        self.assertIn("phase += dt * Speed;", aura,
                      "the overlay timing phase was switched to the nominal step, "
                      "so battery/cpu overlays no longer last their real seconds")

    def test_keyboard_lighting_watchdog_keeps_ghelper_contained(self):
        """The engine must re-check G-Helper's containment, not trust Setup once.

        Setup writes skip_aura and pins G-Helper's brightness gate and idle
        timers a single time at install. Nothing looked again, so a G-Helper
        update - or one click in its keyboard panel, which writes immediately
        because lighting has no Apply button - silently undid the containment
        and the two apps went back to fighting over the keyboard.

        Rewriting the file is not enough on its own: G-Helper only reads config
        at startup and saves its in-memory copy back over the file on exit, so
        the repair has to restart it and then write the values a SECOND time,
        after the process is gone, or the exit-save restores the drift.

        Restarting another program is disruptive (G-Helper holds fan curves and
        performance modes, and relaunching from this elevated process brings it
        back elevated), so it is rate limited, it stands down permanently rather
        than fighting forever, and it can be disabled without a rebuild.
        """
        aura = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")
        setup = (ROOT / "keyboard-lighting" / "Setup.ps1").read_text(encoding="utf-8")

        def at(needle, why, text=None):
            text = aura if text is None else text
            self.assertEqual(text.count(needle), 1, why + " (anchor count %d)"
                             % text.count(needle))
            return text.index(needle)

        keys = ("skip_aura", "keyboard_brightness", "keyboard_brightness_ac",
                "max_brightness", "keyboard_timeout", "keyboard_ac_timeout")

        # The watchdog must check exactly the keys Setup writes - no more, or it
        # repairs things Setup never set, and no fewer, or drift goes unnoticed.
        want = aura[aura.index("$script:GhWant = @{"):]
        want = want[:want.index("\n}\n")]
        for k in keys:
            self.assertIn("'" + k + "'", want,
                          "the watchdog does not check %s, so G-Helper can drift "
                          "it back and nobody notices" % k)
            self.assertIn("'" + k + "'", setup,
                          "%s is checked by the watchdog but never written by "
                          "Setup, so the two disagree about what contained means" % k)

        drift_def = at("function Get-GHelperDrift {", "the drift check is missing")
        repair_def = at("function Repair-GHelper {", "the repair is missing")
        called = at("$ghDrift = Get-GHelperDrift", "the watchdog is never called from the loop")
        logdef = at("function Write-Log($msg, $level = 'INFO') {", "Write-Log moved")
        self.assertTrue(logdef < drift_def < repair_def < called,
                        "the watchdog functions must be defined after Write-Log "
                        "(they log) and before the loop calls them")

        # The repair must kill G-Helper, then write the config a second time.
        body = aura[repair_def:]
        body = body[:body.index("\n}\n")]
        self.assertIn("Stop-Process -Force", body,
                      "the repair rewrites the config but never restarts G-Helper, "
                      "so the change is not picked up until the next boot")
        self.assertEqual(body.count("Set-Content -Path $script:GhCfgPath"), 2,
                         "the config must be written twice: once before the "
                         "restart and once after, because G-Helper saves its "
                         "in-memory copy back over the file as it exits")
        self.assertLess(body.index("Stop-Process -Force"),
                        body.rindex("Set-Content -Path $script:GhCfgPath"),
                        "the second config write happens before G-Helper is "
                        "stopped, so its exit-save undoes the repair")

        # Bounded, not a fight.
        self.assertIn("$script:GhNextRestart = [DateTime]::UtcNow.AddMinutes(10)", body,
                      "restarts are not rate limited, so a config that keeps "
                      "drifting restarts G-Helper every minute")
        self.assertIn("if ($script:GhRepairs -ge 5 -and -not $script:GhGaveUp) {", aura,
                      "the watchdog never stands down, so it restarts G-Helper "
                      "forever when something else keeps rewriting the file")
        self.assertIn("-not ($env:KBL_GHELPER_WATCHDOG -eq '0')", aura,
                      "there is no way to disable the watchdog without editing "
                      "the engine")

    def test_keyboard_lighting_stops_a_previous_engine_before_taking_the_device(self):
        """A second engine must stand the first one down before opening the device.

        Two engines each pushing 60 fps at the same keyboard is contention no
        host-side timing can fix, and it matches the reported symptom exactly:
        the four deck zones show both loops and step, while the twelve bar lamps
        average the interleaving out and still look smooth. The owner's log
        confirmed it - an exclusive open refused, "lock SHARED, fallbacks 1",
        while an older engine (old log format, different owner pid) was still
        writing.

        The engine used to WRITE engine.pid and never read it, so it could not
        know. The ordering is what makes the handover safe, and each step is
        asserted here because getting any of them wrong is worse than not
        trying: the handover must precede the open (or the exclusive attempt has
        already failed), the stop.flag it writes must be removed before the main
        loop polls that same file (or this engine stops itself immediately), and
        the warnings must be flushed after Write-Log exists (or they are lost).
        """
        aura = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")

        def at(needle, why):
            self.assertEqual(aura.count(needle), 1, why + " (anchor not unique: %d)"
                             % aura.count(needle))
            return aura.index(needle)

        handover = at("$script:StartupWarns = @()",
                      "the previous-engine handover is missing")
        pid_read = at("$prevPidFile = Join-Path $env:LOCALAPPDATA 'KeyboardLighting\engine.pid'",
                      "the engine never reads engine.pid, so it cannot detect a previous instance")
        flag_set = at("try { Set-Content -Path $prevFlag -Value '1'",
                      "the handover never asks the previous engine to stop")
        flag_clear = at("try { Remove-Item $prevFlag -Force",
                        "the handover never clears the stop.flag it wrote")
        opened = at("if (-not $eng.Open()) {", "the device open moved")
        polled = at('        if (Test-Path $stopFile) {\n            Remove-Item $stopFile',
                    "the main loop no longer polls stop.flag")
        logdef = at("function Write-Log($msg, $level = 'INFO') {",
                    "Write-Log moved")
        flushed = at("foreach ($w in $script:StartupWarns) { Write-Log $w 'WARN' }",
                     "the queued startup warnings are never written to the log")

        self.assertTrue(handover < pid_read < flag_set < flag_clear < opened,
                        "the handover must read engine.pid, ask the previous engine "
                        "to stop and clear the flag BEFORE the device is opened, or "
                        "the exclusive open has already been refused by then")
        self.assertTrue(flag_clear < polled,
                        "stop.flag is still present when the main loop first polls "
                        "it, so this engine stops itself immediately after starting")
        self.assertTrue(opened < logdef < flushed,
                        "the startup warnings are flushed before Write-Log exists, "
                        "so the most important line in the log is never written")

        # One-shot diagnostics must never tear down the owner's running engine.
        self.assertIn("if (-not ($Restore -or ($ZoneTest -ne '') -or $ChaseTest)) {",
                      aura, "the handover also runs for one-shot diagnostics, so a "
                            "zone test or a firmware restore would kill the engine "
                            "the owner is actually using")

        # And when the lock is still refused, name what is holding it - by
        # process NAME only. Enumerating command lines needs a process-table WMI
        # query, which can hang for seconds after a sleep/wake, so the existing
        # reliability contract bans that string in this file outright.
        self.assertIn("'LightingService'", aura,
                      "a refused exclusive open does not name the vendor lighting "
                      "services, so the owner has to guess what to close")
        self.assertIn("'ArmouryCrate'", aura,
                      "Armoury Crate is the most common contender and is not named")
        self.assertNotIn("Win32_Process", aura,
                         "the rival scan enumerates command lines via WMI, which "
                         "can hang for seconds after a sleep/wake - match on "
                         "process names instead")

    def test_keyboard_lighting_log_reports_lock_dither_and_device_range(self):
        """The engine must log the three readings that separate the flicker causes.

        Write cost, late frames and failed writes can only ever distinguish
        "too slow" from "refused". They cannot distinguish either of those from
        "another program is writing frames over ours", nor from "the device
        physically has too few colour levels to show a smooth gradient". Both
        of those produce a stepping keyboard deck with a perfectly smooth light
        bar, so both are reported alongside the timings: the lock state, the
        device's own colour range, and whether dither ended up enabled.

        A shared handle is escalated to WARN rather than logged at INFO with
        the ordinary timings, and the README says what to do about it.

        The placeholder/argument counts are checked too: PowerShell's -f
        operator throws at runtime when a format string has more placeholders
        than arguments, which would take the whole status line down.
        """
        aura = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")

        def check_format(label, pattern):
            m = re.search(pattern, aura, re.S)
            self.assertIsNotNone(m, "could not find the %s format string" % label)
            fmt, args = m.group(1), m.group(2)
            placeholders = set(int(n) for n in re.findall(r"\{(\d+)\}", fmt))
            nargs = len([a for a in args.split(",") if a.strip()])
            self.assertEqual(nargs, (max(placeholders) + 1) if placeholders else 0,
                             "%s has %d placeholders (highest {%s}) but %d arguments, "
                             "so PowerShell's -f operator will throw at runtime"
                             % (label, len(placeholders),
                                max(placeholders) if placeholders else "-", nargs))
            self.assertEqual(placeholders, set(range(max(placeholders) + 1)),
                             "%s skips a placeholder index" % label)

        check_format(
            "periodic timing line",
            r"\$msg = \('(.*?)' -f `(.*?)\)\n\s*if \(-not \$script:TimingDone\)")
        check_format(
            "startup colour-range line",
            r"""Say \("  Device colour range (.*?)" -f `\n(.*?)\) 'Gray'""")

        for needle, why in (
            ("lock {7}", "the lock state is not in the periodic line"),
            ("fallbacks {8}", "downgrades to a shared open are not counted in the log"),
            ("dither {9}", "the dither decision is not logged"),
            ("range R{10}/G{11}/B{12}", "the device's own colour range is not logged"),
        ):
            self.assertIn(needle, aura, why)

        self.assertIn(
            "if ($eng.ExclusiveLock) { Write-Log $msg } else { Write-Log $msg 'WARN' }",
            aura, "a shared handle is logged at INFO among the ordinary timings, "
                  "where the one reading that means another program is writing "
                  "over ours is easy to scroll past")

        readme = (ROOT / "keyboard-lighting" / "README.md").read_text(encoding="ascii")
        self.assertIn("`lock SHARED`", readme,
                      "the README does not explain the lock reading, so the owner "
                      "cannot act on it")
        self.assertIn("`range` below 255", readme,
                      "the README does not explain what a coarse device colour "
                      "range means for smoothness")

    def test_keyboard_lighting_paces_the_batched_reports_of_one_frame(self):
        """One frame's batched reports must be paced apart, and the cost measured.

        Each frame goes out as two HID feature reports (lamps 0-7, then 8-15,
        the second carrying the apply flag). HID control transfers are
        scheduled per 1 ms USB frame, so two sent back to back can land in the
        same frame and the EC can drop or coalesce one. The deck then misses
        that frame while the bar keeps up, which reads as stepping rather than
        as a slowdown - and it never shows in WriteFails, because the transfer
        the host was told about succeeded. keyRGB ships the same knob for the
        same reason.

        Two invariants matter. The gap goes BETWEEN batches only, never after
        the last one, or every frame pays for a gap nothing waits on. And it
        must land inside the measured WriteUs window, or the adaptive backoff
        compares a budget against a cost that no longer includes the pacing.
        """
        aura = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")

        self.assertIn("public int ReportPaceUs = 1000;", aura,
                      "the inter-report gap is not a tunable with a non-zero "
                      "default, so the two reports of a frame can share a USB frame")
        self.assertIn("void Pace(int us) {", aura,
                      "there is no precise short wait; Thread.Sleep alone is too "
                      "coarse at these lengths even with the 1ms timer period on")

        pace_body = aura[aura.index("void Pace(int us) {"):]
        pace_body = pace_body[:pace_body.index("\n  }\n")]
        self.assertIn("Thread.SpinWait(40);", pace_body,
                      "Pace does not spin, so the gap it produces is not accurate "
                      "enough to move the two reports into separate USB frames")

        self.assertIn(
            "if (bi + 1 < nbatch && ReportPaceUs > 0) Pace(ReportPaceUs);", aura,
            "the reports of one frame are sent back to back, or the gap is also "
            "taken after the last batch, taxing every frame for nothing")

        # The gap must be inside the window that measures the frame's cost.
        self.assertEqual(aura.count("long t0 = Stopwatch.GetTimestamp();"), 1)
        t0 = aura.index("long t0 = Stopwatch.GetTimestamp();")
        gap = aura.index("Pace(ReportPaceUs);")
        measured = aura.index("WriteUs = us;")
        self.assertTrue(t0 < gap < measured,
                        "pacing happens outside the measured WriteUs window, so "
                        "the adaptive backoff sees a frame cost that excludes it "
                        "and can overdrive the device")

        # Tunable from the environment, so it can be retuned on real hardware
        # without a rebuild.
        self.assertIn("$eng.ReportPaceUs = $paceUs", aura,
                      "the pacing tunable is never handed to the engine")
        self.assertIn("KBL_REPORT_PACE_US", aura,
                      "there is no way to retune or disable pacing without "
                      "editing the engine")

    def test_keyboard_lighting_owns_the_hid_endpoint_exclusively(self):
        """The engine must try to own the HID endpoint before sharing it.

        A second writer interleaving its own frames is the one flicker source
        no host-side timing can fix, and vendor lighting software contests the
        keyboard deck specifically while often leaving the chassis bar alone.
        That is the owner's exact symptom: the four deck zones step while the
        twelve bar lamps stay smooth. dwShareMode 0 makes every other
        process's open of the interface fail with ERROR_SHARING_VIOLATION, so
        holding the handle is the cure and failing to get it is the diagnosis.

        Exclusive must be tried first with a shared fallback, so a machine
        where something else already owns the device still gets lighting
        rather than none. Both the first open and every reopen after a resume
        must go through the one helper, or they drift apart and a sleep/wake
        silently downgrades the lock.
        """
        aura = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")

        # One open path, used by both Open() and Reopen().
        self.assertIn("bool OpenHandle() {", aura,
                      "the exclusive-open logic is not factored into one helper, "
                      "so Open() and Reopen() can drift and a resume can silently "
                      "downgrade the lock to shared")
        self.assertEqual(aura.count("if (!OpenHandle()) {"), 2,
                         "Open() and Reopen() must both go through OpenHandle()")

        helper = aura[aura.index("bool OpenHandle() {"):]
        helper = helper[:helper.index("\n  }\n")]

        # Exclusive attempt (dwShareMode 0) exists and comes first.
        self.assertIn(
            "CreateFileW(DevicePath, 0xC0000000, 0, IntPtr.Zero, 3, 0, IntPtr.Zero)",
            aura, "the engine never asks for an exclusive handle (dwShareMode 0), "
                  "so any other program can write frames over ours")
        self.assertLess(helper.index("0xC0000000, 0,"),
                        helper.index("0xC0000000, 3,"),
                        "the shared open is tried before the exclusive one, so the "
                        "exclusive attempt can never succeed")

        # Shared fallback preserved, and the outcome is observable.
        self.assertIn("CreateFileW(DevicePath, 0xC0000000, 3,", helper,
                      "the shared fallback is gone, so a machine where something "
                      "else already owns the device loses the lighting entirely")
        self.assertIn("ExclusiveLock = true;", helper)
        self.assertIn("LockFallbacks++;", helper,
                      "a downgrade to a shared open is not counted, so the log "
                      "cannot tell device contention from a timing problem")
        self.assertEqual(aura.count("CreateFileW(DevicePath"), 3,
                         "a device open exists outside OpenHandle()")

        # Opt-out without a rebuild, and the result is reported to the owner.
        self.assertIn(
            "$eng.WantExclusiveLock = -not ($env:KBL_EXCLUSIVE_LOCK -eq '0')", aura,
            "there is no way to opt out of the exclusive lock without editing the "
            "engine")
        self.assertIn("if ($eng.ExclusiveLock) {", aura,
                      "the engine never reports whether it actually got the "
                      "exclusive lock, so contention stays invisible")

    def test_keyboard_lighting_retries_for_exclusive_ownership_of_a_shared_handle(self):
        """A shared handle must be retried, not accepted for the whole session.

        Whether we own the device was decided exactly once, at Open(). The
        owner's log then ran fourteen minutes at "lock SHARED, fallbacks 1,
        reopens 0": writes costing 3-6 ms against a measured 2.2 ms
        uncontended, one write taking 34 ms, and roughly 5% of frames late
        where the render loop's own notes put ordinary scheduler jitter at
        about 1%. Those are the fingerprints of a second writer on the same
        LEDs, and nothing host-side can fix it while that writer is there -
        not report pacing, not an even animation step.

        Trading the shared handle in must close ours first, because dwShareMode
        0 refuses to open while ANY handle to the device exists, including our
        own. That leaves a window holding nothing at all, so the two covers for
        it are asserted rather than assumed: OpenHandle's shared fallback, and
        a second attempt before the device is declared lost. Without them a
        rival taking the device in that gap costs the owner the lighting
        entirely instead of only the lock.

        The retry must also back off. A rival that owns the device permanently
        can never be displaced, and hammering it would mean a close/reopen and
        a full repaint on every attempt for as long as the engine runs.
        """
        aura = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")

        body = aura[aura.index("bool TryExclusiveUpgrade() {"):]
        body = body[:body.index("\n  }\n")]

        # Our own handle goes before the reopen, or the reopen cannot succeed.
        self.assertLess(body.index("CloseHandle(h)"), body.index("OpenHandle()"),
                        "the upgrade reopens without releasing our shared handle first, "
                        "so dwShareMode 0 can never succeed - our handle is itself an "
                        "open handle to the device")

        # Never end the attempt with no handle at all.
        self.assertEqual(body.count("OpenHandle()"), 2,
                         "there is no second attempt after the window where we hold no "
                         "handle, so a rival grabbing the device in that gap costs the "
                         "lighting entirely rather than only the exclusive lock")
        self.assertLess(body.index("Thread.Sleep(60)"), body.index("DeviceLost = true"),
                        "the device is declared lost on the first refusal, so the loop "
                        "backs off for 400ms instead of retrying straight away")

        # Coming back shared is a lost lock, not a lost device.
        self.assertIn("if (!ExclusiveLock) return false;", body,
                      "a reacquire that came back shared is reported as success, so the "
                      "caller stops retrying and two writers stay on the keyboard")

        # Same post-open sequence as Reopen(): firmware handback, then a cache
        # that no longer describes the hardware is invalidated.
        self.assertLess(body.index("HidD_SetFeature(h, CtrlOff"), body.index("pr[i] = -1"),
                        "the cached previous colours are cleared before the firmware is "
                        "told to hand control over again")
        self.assertIn("SeedDither();", body,
                      "the per-lamp dither phase is not reseeded after the handle "
                      "changes, so the four deck zones keep the phases of a session "
                      "that is no longer running")

        # The caller gates it, counts it, and backs off.
        call = aura[aura.index("if (!ExclusiveLock && WantExclusiveLock && !DeviceLost"):]
        call = call[:call.index("\n        }\n")]
        self.assertIn("LockRetries++;", call,
                      "attempts are not counted, so the log cannot distinguish 'asked "
                      "once at startup and gave up' from 'asking every five minutes and "
                      "always losing'")
        self.assertIn("if (!TryExclusiveUpgrade()) {", call,
                      "the retry is not driven from the render loop, so it runs on "
                      "another thread against a handle the loop is writing through")
        self.assertIn("lockTryGap * 2.0", call,
                      "the retry gap never grows, so a device owned permanently by "
                      "something else is hammered with a close/reopen and a full "
                      "repaint forever")
        self.assertIn("300.0", call,
                      "the backoff has no ceiling, so a rival that quits late in a "
                      "multi-day session is not locked out for hours afterwards")

        # Both readings reach the periodic line, which is the log the owner
        # actually pastes - the startup output has usually scrolled away.
        self.assertIn("lock retries {13}", aura,
                      "the retry count is not in the periodic status line")
        self.assertIn("function Get-LightingRivals {", aura,
                      "a shared handle is reported without naming what holds it")
        self.assertLess(aura.index("function Get-LightingRivals {"),
                        aura.index("(Get-LightingRivals)"),
                        "Get-LightingRivals is called before it is defined, so the "
                        "periodic status line throws and takes the whole timing "
                        "report down with it")

        fn = aura[aura.index("function Get-LightingRivals {"):]
        fn = fn[:fn.index("\n}\n")]
        self.assertIn("Get-Process -Name $n", fn)
        self.assertNotIn("Win32_Process", fn,
                         "a process-table WMI query in a helper that runs every minute "
                         "can hang for seconds after a sleep/wake or during a Windows "
                         "Update")

        # The startup list and the periodic one must not drift apart, or the log
        # names a different set of suspects than the startup message did.
        lists = re.findall(r"foreach \(\$n in ((?:'[^']+',?\s*)+)\) \{", aura)
        self.assertEqual(len(lists), 2,
                         "expected exactly two vendor-rival name lists (startup and "
                         "periodic), found %d" % len(lists))
        names = [tuple(re.findall(r"'([^']+)'", x)) for x in lists]
        self.assertEqual(names[0], names[1],
                         "the startup and periodic vendor-rival lists have drifted, so "
                         "the two messages can name different suspects")
        self.assertIn("GHelper", names[0],
                      "G-Helper is not in the vendor-rival list, and it is the one that "
                      "writes to this keyboard the moment its panel is clicked")

    def test_bluetooth_guard_contracts(self):
        core = (ROOT / "bluetooth-guard" / "BluetoothCore.cs").read_text(encoding="utf-8")
        ps = (ROOT / "bluetooth-guard" / "BluetoothGuard.ps1").read_text(encoding="utf-8")
        setup = (ROOT / "bluetooth-guard" / "Setup.ps1").read_text(encoding="utf-8")
        native_c = read_c_inlined("bluetooth-guard/src/bluetooth_guard.c")
        self.assertIn("IOCTL_BTH_DISCONNECT_DEVICE = 0x0041000C", core)
        self.assertIn("BluetoothSetServiceState", core)
        self.assertIn("BluetoothEnumerateInstalledServices", core)
        self.assertNotIn("BLUETOOTH_SERVICE_DISABLE", native_c)
        self.assertIn("EnsureProfilesEnabled", native_c)
        self.assertIn("KSPROPERTY_ONESHOT_RECONNECT", native_c)
        self.assertIn("KSPROPERTY_ONESHOT_DISCONNECT", native_c)
        self.assertIn("CheckForegroundManualIntent", native_c)
        self.assertIn("GetLastInputInfo", native_c)
        self.assertIn("HIGH_PRIORITY_CLASS", native_c)
        self.assertIn("RegisterDeviceNotificationW", native_c)
        self.assertIn("Tests_BluetoothGuard_FastBoot", native_c)
        self.assertIn("StartupDelayInMSec", native_c)
        self.assertTrue((ROOT / "tools" / "build_windows.py").exists())
        self.assertIn("CreateRunspace", ps)
        self.assertIn("SessionAllowed", ps)
        self.assertNotIn("$T::", ps, "Never use $Var::Member in PowerShell UI code (breaks Command Mode)")
        self.assertIn("window.ready", ps)
        self.assertIn("crash.log", ps)
        self.assertIn("window.ready", setup)
        self.assertTrue((ROOT / "bluetooth-guard" / "Setup.bat").exists())

    def test_bluetooth_guard_forget_unpairs_and_filters_phantom_devices(self):
        """Forget must unpair and leave a durable tombstone that defeats stale scans."""
        btg = read_c_inlined("bluetooth-guard/src/bluetooth_guard.c")

        self.assertIn('GetProcAddress(m, "BluetoothRemoveDevice")', btg)
        self.assertIn("static void DoForgetDevice(", btg)
        self.assertIn("pBluetoothRemoveDevice(&addr)", btg)
        forget_api = btg[btg.index("static void DoForgetDevice("):]
        forget_api = forget_api[:forget_api.index("\n}\n")]
        self.assertNotIn("BluetoothSetServiceState", forget_api)
        self.assertNotIn("BLUETOOTH_SERVICE_DISABLE", forget_api)

        # Forget is an explicit persisted F tombstone, not a temporary hide.
        handler = btg[btg.index("} else if (kind == HIT_DEV_FORGET_BTN)"):]
        handler = handler[:handler.index("\n}\n")]
        self.assertIn("d->visible = FALSE", handler)
        self.assertIn("d->forgotten = TRUE", handler)
        self.assertIn("SaveRules();", handler)
        self.assertIn("EnqueueWork(3, addr, dmac, dkind, dname)", handler)
        self.assertIn("else if (item.op == 3) DoForgetDevice(", btg)

        # Every native action surface is gated by the same durable membership.
        tray = btg[btg.index("static void ShowTrayMenu("):]
        tray = tray[:tray.index("static LRESULT CALLBACK WndProc")]
        self.assertIn("if (!g_devices[i].visible || g_devices[i].forgotten) continue;", tray)
        command = btg[btg.index("case WM_COMMAND:"):]
        command = command[:command.index("case WM_WORKER_DONE:")]
        self.assertIn("g_devices[di].visible && !g_devices[di].forgotten", command)
        self.assertIn("if (!DeviceIsListed(d)) continue;", btg)
        self.assertIn("g_devices[i].visible && !g_devices[i].forgotten", btg[btg.index("static void DoConnectNow("):])

    def test_bluetooth_guard_persists_only_intentional_device_membership(self):
        btg = read_c_inlined("bluetooth-guard/src/bluetooth_guard.c")
        rules = btg[btg.index("static void SaveRules("):]
        save = rules[:rules.index("static void LoadRules(")]
        load = rules[rules.index("static void LoadRules("):rules.index("static void EnqueueWork(")]
        poll = btg[btg.index("static void PollPairedDevices("):]
        poll = poll[:poll.index("/* Disconnect all guarded")]

        self.assertIn('L"format=2\\n"', save)
        self.assertIn('L"device=%s|%d|%llu|%s|%s|%s\\n"', save)
        self.assertIn('d->visible ? L"K" : (d->denied ? L"D" : L"F")', save)
        self.assertIn("if (!d->visible && !d->forgotten && !d->denied) continue;", save)
        self.assertIn('if (wcsncmp(line, L"device=", 7) != 0) continue;', load)
        self.assertIn("d->visible = known;", load)
        self.assertIn("d->forgotten = forgotten;", load)

        # Paired, disconnected candidates are reviewable, not automatically
        # trusted; unpaired disconnected history is still ignored.
        candidate_gate = "if (!info.fAuthenticated && !info.fRemembered && !info.fConnected) goto next_bluetooth_device;"
        self.assertIn(candidate_gate, poll)
        self.assertLess(poll.index(candidate_gate), poll.index("idx = g_devCount++;"))
        self.assertIn("ApproveDevice(d, FALSE, now);", poll)
        self.assertIn("if (d->transient && !d->paired &&", poll)
        self.assertIn("if (newlyConnected && !manualWindow && g_hwnd)", poll)
        self.assertIn("CheckForegroundManualIntent();", poll)
        self.assertIn("BOOL manualIntent = newlyConnected && manualWindow;", poll)
        self.assertIn("g_devices[idx].allowAuto = FALSE;", poll)
        self.assertIn("if (manualIntent && !d->allowAuto)", poll)
        self.assertIn("DisconnectIfStillBlocked(rejected[i].address", poll)
        self.assertIn("if (trustChanged) SaveRules();", poll)
        self.assertNotIn("HasConnectedBefore", poll)

        connect = btg[btg.index("static void DoConnectNow("):]
        connect = connect[:connect.index("\n}")]
        self.assertIn("g_devices[i].visible && !g_devices[i].forgotten", connect)

    def test_bluetooth_paired_review_runs_actual_poll_policy_and_click_handlers(self):
        """Execute real native discovery/approval code with only Win32 I/O mocked."""
        import shutil
        import tempfile

        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            self.skipTest("a C compiler is required for the Bluetooth review regression")
        root = (ROOT / "bluetooth-guard/src/bluetooth_guard.c").read_text(encoding="utf-8")
        rules = (ROOT / "bluetooth-guard/src/parts/autostart_rules.c").read_text(encoding="utf-8")
        actions = (ROOT / "bluetooth-guard/src/parts/bt_actions.c").read_text(encoding="utf-8")
        ui = (ROOT / "bluetooth-guard/src/parts/window_entry.c").read_text(encoding="utf-8")
        policy = (ROOT / "bluetooth-guard/src/parts/device_policy.c").read_text(encoding="utf-8")

        def declaration(first, last):
            start = root.index(first)
            return root[start:root.index(last, start) + len(last)]

        declarations = "\n".join((
            declaration("enum HitKind {", "\n};"),
            declaration("typedef struct {\n    RECT rect;", "} HitBox;"),
            declaration("typedef struct {\n    ULONGLONG address;", "} BtAudioResult;"),
            declaration("typedef struct {\n    int       op;", "} WorkItem;"),
        ))
        polling = rules[rules.index("static void RecordRejectedLink("):
                        rules.index("/* Disconnect all guarded")]
        connect_and_drop = actions[actions.index("static void DoConnectNow("):
                                   actions.index("/* True unpair:")]
        click = ui[ui.index("static void HandleHitClick("):ui.index("static void BluetoothTraySync(")]
        harness = r'''/* At a paused Bluetooth API, another thread probes the exact
   model lock shared by Guard's WM_TIMER/heartbeat and paint paths. No sleep,
   timing assumption, real Bluetooth action or Windows foreground claim. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
typedef int BOOL;
typedef uint32_t DWORD;
typedef int32_t HRESULT;
#define S_OK 0
#define FAILED(hr) ((hr) < 0)
typedef uint64_t ULONGLONG;
typedef int64_t LONGLONG;
typedef void *HANDLE;
typedef HANDLE HBLUETOOTH_RADIO_FIND;
typedef HANDLE HBLUETOOTH_DEVICE_FIND;
typedef struct { int left, top, right, bottom; } RECT;
#define TRUE 1
#define FALSE 0
#define MAX_DEVICES 64
#define MAX_HITBOXES 200
#define KSPROPERTY_ONESHOT_RECONNECT 0
#define ZeroMemory(p, n) memset(p, 0, n)
#define _wcsicmp wcscmp
''' + declarations + r'''
static DeviceEntry g_devices[MAX_DEVICES];
static int g_devCount, g_lock, g_hitCount, saveCalls, dropCalls, connectCalls, profileCalls, queuedCount;
static BOOL g_guardOn = TRUE, g_radioOn, g_autostart, g_quitting, fakeIntent, g_suspendAutoConnect;
static HANDLE g_hwnd, g_qSem;
static volatile LONGLONG g_manualGraceUntil;
static HitBox g_hits[MAX_HITBOXES];
static WorkItem queued[64];
static ULONGLONG tick = 10000, lastDrop;
static pthread_mutex_t deviceLock = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local int lockDepth;
static BOOL probeUi, uiLockAvailable;
static int uiProbes, uiSnapshotCount;
static void (*betweenRecords)(void);
static void EnterCriticalSection(int *lock) {
    assert(lock == &g_lock && pthread_mutex_lock(&deviceLock) == 0);
    lockDepth++;
}
static void LeaveCriticalSection(int *lock) {
    assert(lock == &g_lock && lockDepth == 1);
    lockDepth--;
    assert(pthread_mutex_unlock(&deviceLock) == 0);
}
static void *TryUiSnapshot(void *unused) {
    (void)unused;
    int result = pthread_mutex_trylock(&deviceLock);
    assert(result == 0 || result == EBUSY);
    uiLockAvailable = result == 0;
    if (uiLockAvailable) {
        uiSnapshotCount = g_devCount;
        assert(pthread_mutex_unlock(&deviceLock) == 0);
    }
    return NULL;
}
static void ProbeUiWhileDriverWaits(void) {
    if (!probeUi) return;
    pthread_t ui;
    assert(pthread_create(&ui, NULL, TryUiSnapshot, NULL) == 0);
    assert(pthread_join(ui, NULL) == 0);
    if (!uiLockAvailable) {
        fputs("Bluetooth enumeration held Guard's UI lock; paint/heartbeat/Open cannot proceed.\n", stderr);
        exit(1);
    }
    assert(uiSnapshotCount >= 0 && uiSnapshotCount <= MAX_DEVICES);
    uiProbes++;
}
static ULONGLONG GetTickCount64(void) { return tick; }
static LONGLONG InterlockedCompareExchange64(volatile LONGLONG *p, LONGLONG n, LONGLONG old) {
    LONGLONG value = *p; if (value == old) *p = n; return value;
}
static LONGLONG InterlockedExchange64(volatile LONGLONG *p, LONGLONG n) {
    LONGLONG value = *p; *p = n; return value;
}
static void CloseHandle(HANDLE h) { (void)h; }
static void SaveRules(void) { saveCalls++; }
static void SetAutostart(BOOL on) { g_autostart = on; }
static void ReleaseSemaphore(HANDLE sem, int n, void *previous) { (void)sem; (void)n; (void)previous; }
static void InvalidateRect(HANDLE hwnd, void *rect, BOOL erase) { (void)hwnd; (void)rect; (void)erase; }
static void DestroyWindow(HANDLE hwnd) { (void)hwnd; }
static void EnqueueWork(int op, ULONGLONG address, const wchar_t *mac,
                        const wchar_t *kind, const wchar_t *name) {
    WorkItem *item = &queued[queuedCount++];
    item->op = op; item->address = address;
    (void)mac; (void)kind; (void)name;
}
static void CheckForegroundManualIntent(void) {
    if (fakeIntent) g_manualGraceUntil = (LONGLONG)(tick + 5000);
}
static void FormatMac(ULONGLONG address, wchar_t *mac, int n) {
    swprintf(mac, (size_t)n, L"%012llX", (unsigned long long)address);
}
static void ClassifyDevice(DWORD cod, const wchar_t *name, wchar_t *kind, int n) {
    (void)cod;
    const wchar_t *classified = wcscmp(name, L"Phone") == 0 ? L"Phone" :
                                (wcscmp(name, L"Earbuds") == 0 ? L"Audio" : L"Device");
    wcsncpy(kind, classified, (size_t)n);
}
static void EnsureProfilesEnabled(ULONGLONG address, const wchar_t *kind) { (void)address; (void)kind; profileCalls++; }
static BtAudioResult fakeAudioResult = {S_OK, S_OK, TRUE, TRUE};
static BtAudioResult TriggerBtAudioKsProperty(ULONGLONG address, const wchar_t *name, int property) {
    (void)address; (void)name; (void)property; connectCalls++;
    return fakeAudioResult;
}
static void DoDisconnectOnly(ULONGLONG address, const wchar_t *kind, const wchar_t *name) {
    (void)kind; (void)name; dropCalls++; lastDrop = address;
}
typedef struct { DWORD dwSize; } BLUETOOTH_FIND_RADIO_PARAMS;
typedef struct {
    DWORD dwSize;
    BOOL fReturnAuthenticated, fReturnRemembered, fReturnConnected, fReturnUnknown, fIssueInquiry;
} BLUETOOTH_DEVICE_SEARCH_PARAMS;
typedef struct {
    DWORD dwSize;
    struct { ULONGLONG ullLong; } Address;
    DWORD ulClassofDevice;
    BOOL fAuthenticated, fRemembered, fConnected;
    wchar_t szName[248];
} BLUETOOTH_DEVICE_INFO;
static BLUETOOTH_DEVICE_INFO observed[64];
static int observedCount, observedIndex;
static HBLUETOOTH_RADIO_FIND FakeFirstRadio(const BLUETOOTH_FIND_RADIO_PARAMS *p, HANDLE *radio) {
    (void)p; ProbeUiWhileDriverWaits(); *radio = (HANDLE)1; return (HANDLE)1;
}
static BOOL FakeRadioClose(HBLUETOOTH_RADIO_FIND h) {
    (void)h; ProbeUiWhileDriverWaits(); return TRUE;
}
static BLUETOOTH_DEVICE_SEARCH_PARAMS lastSearch;
static BOOL FakeNextDevice(HBLUETOOTH_DEVICE_FIND h, BLUETOOTH_DEVICE_INFO *info) {
    (void)h; ProbeUiWhileDriverWaits();
    if (betweenRecords && observedIndex == 0) {
        assert(lockDepth == 0);
        void (*action)(void) = betweenRecords;
        betweenRecords = NULL;
        action();
    }
    while (++observedIndex < observedCount) {
        BLUETOOTH_DEVICE_INFO *d = &observed[observedIndex];
        if ((lastSearch.fReturnAuthenticated && d->fAuthenticated) ||
            (lastSearch.fReturnRemembered && d->fRemembered) ||
            (lastSearch.fReturnConnected && d->fConnected)) {
            *info = *d; return TRUE;
        }
    }
    return FALSE;
}
static HBLUETOOTH_DEVICE_FIND FakeFirstDevice(const BLUETOOTH_DEVICE_SEARCH_PARAMS *p,
                                             BLUETOOTH_DEVICE_INFO *info) {
    ProbeUiWhileDriverWaits(); lastSearch = *p; observedIndex = -1;
    if (p->fReturnUnknown || p->fIssueInquiry) return NULL;
    return FakeNextDevice((HANDLE)1, info) ? (HANDLE)1 : NULL;
}
static BOOL FakeDeviceClose(HBLUETOOTH_DEVICE_FIND h) {
    (void)h; ProbeUiWhileDriverWaits(); return TRUE;
}
static HBLUETOOTH_RADIO_FIND (*pBtFindFirstRadio)(const BLUETOOTH_FIND_RADIO_PARAMS *, HANDLE *) = FakeFirstRadio;
static BOOL (*pBtFindRadioClose)(HBLUETOOTH_RADIO_FIND) = FakeRadioClose;
static HBLUETOOTH_DEVICE_FIND (*pBtFindFirstDevice)(const BLUETOOTH_DEVICE_SEARCH_PARAMS *, BLUETOOTH_DEVICE_INFO *) = FakeFirstDevice;
static BOOL (*pBtFindNextDevice)(HBLUETOOTH_DEVICE_FIND, BLUETOOTH_DEVICE_INFO *) = FakeNextDevice;
static BOOL (*pBtFindDeviceClose)(HBLUETOOTH_DEVICE_FIND) = FakeDeviceClose;
''' + policy + connect_and_drop + polling + click + r'''
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "C line %d: %s\n", __LINE__, #c); return 1; } } while (0)
static void Reset(void) {
    memset(g_devices, 0, sizeof(g_devices)); memset(g_hits, 0, sizeof(g_hits));
    memset(observed, 0, sizeof(observed)); memset(queued, 0, sizeof(queued));
    g_devCount = saveCalls = dropCalls = connectCalls = profileCalls = queuedCount = observedCount = 0;
    tick = 10000; g_radioOn = TRUE; g_suspendAutoConnect = FALSE;
    fakeAudioResult = (BtAudioResult){S_OK, S_OK, TRUE, TRUE};
    g_hitCount = 1; g_guardOn = TRUE; g_hwnd = NULL; fakeIntent = FALSE; g_manualGraceUntil = 0;
    assert(lockDepth == 0); probeUi = FALSE; uiProbes = uiSnapshotCount = 0;
    betweenRecords = NULL;
}
static void Observe(int i, ULONGLONG address, BOOL paired, BOOL connected) {
    memset(&observed[i], 0, sizeof(observed[i]));
    observed[i].Address.ullLong = address;
    observed[i].fAuthenticated = paired; observed[i].fConnected = connected;
    wcscpy(observed[i].szName, L"Earbuds");
    if (observedCount <= i) observedCount = i + 1;
}
static void Click(int kind, ULONGLONG address) {
    g_hits[0].kind = kind; g_hits[0].address = address; HandleHitClick(0);
}
static void DenySecondDuringScan(void) { Click(HIT_DEV_DENY_BTN, 2); }
static void PauseSecondDuringScan(void) { Click(HIT_DEV_CONNECT_BTN, 2); }
int main(void) {
    /* Slow first/next/close driver calls may never own the UI model lock. */
    Reset(); probeUi = TRUE; Observe(0, 99, TRUE, FALSE); PollPairedDevices();
    CHECK(uiProbes >= 6 && lockDepth == 0 && g_devCount == 1);
    observedCount = 0; PollPairedDevices(); /* empty discovery also releases it */
    CHECK(lockDepth == 0 && g_devCount == 1);

    /* Disconnected paired earbuds must appear without becoming trusted. */
    Reset(); Observe(0, 1, TRUE, FALSE); PollPairedDevices();
    CHECK(g_devCount == 1 && DeviceIsListed(&g_devices[0]));
    CHECK(DeviceNeedsReview(&g_devices[0]) && !g_devices[0].visible && !g_devices[0].allowAuto);
    CHECK(saveCalls == 0 && dropCalls == 0);
    /* Missing the Windows UI intent window, including the pre-window boot
       scan, must not drop a pair before the owner has made a decision. */
    Observe(0, 1, TRUE, TRUE); PollPairedDevices();
    CHECK(!g_devices[0].visible && g_devices[0].connected && dropCalls == 0);
    observedCount = 0; PollPairedDevices();
    CHECK(g_devCount == 1 && DeviceNeedsReview(&g_devices[0]) && !g_devices[0].connected);

    /* Honor the real search flags: saved-only earbuds must reach the actual
       review code, without conflating remembered with authenticated/trusted. */
    Reset(); Observe(0, 1, FALSE, FALSE); observed[0].fRemembered = TRUE;
    PollPairedDevices();
    CHECK(lastSearch.fReturnRemembered && !lastSearch.fReturnUnknown && !lastSearch.fIssueInquiry);
    CHECK(g_devCount == 1 && DeviceNeedsReview(&g_devices[0]) && !g_devices[0].paired);
    CHECK(!g_devices[0].visible && saveCalls == 0 && dropCalls == 0);
    observedCount = 0; PollPairedDevices(); CHECK(g_devCount == 1 && DeviceIsListed(&g_devices[0]));
    Observe(0, 1, FALSE, TRUE); observed[0].fRemembered = TRUE; g_guardOn = FALSE;
    PollPairedDevices(); CHECK(dropCalls == 1 && !g_devices[0].visible);
    /* An unauthenticated, disconnected cache entry not returned by a requested
       category must not get manufactured by a too-permissive enumeration mock. */
    Reset(); Observe(0, 9, FALSE, FALSE); PollPairedDevices(); CHECK(g_devCount == 0);

    /* A row stays bound to its address even if a scan reorders the array. */
    Reset(); Observe(0, 1, TRUE, FALSE); Observe(1, 2, TRUE, FALSE); PollPairedDevices();
    DeviceEntry tmp = g_devices[0]; g_devices[0] = g_devices[1]; g_devices[1] = tmp;
    Click(HIT_DEV_CONNECT_BTN, 1);
    int a = FindDeviceIndexByAddress(1), b = FindDeviceIndexByAddress(2);
    CHECK(g_devices[a].visible && g_devices[a].sessionAllowed && !g_devices[a].allowAuto);
    CHECK(!g_devices[b].visible && saveCalls == 1);
    CHECK(queuedCount == 1 && queued[0].op == 2 && queued[0].address == 1);
    DoConnectNow(1, L"Audio", L"Earbuds", FALSE); CHECK(connectCalls == 1);
    Click(HIT_DEV_AUTO_TOGGLE, 2);
    CHECK(g_devices[b].visible && g_devices[b].allowAuto && !g_devices[b].sessionAllowed);
    CHECK(queuedCount == 2 && queued[1].op == 2 && queued[1].address == 2 && saveCalls == 2);
    Observe(0, 1, TRUE, TRUE); Observe(1, 2, TRUE, TRUE); PollPairedDevices();
    CHECK(dropCalls == 0);
    Observe(0, 1, TRUE, FALSE); PollPairedDevices();
    CHECK(!g_devices[a].sessionAllowed);
    Observe(0, 1, TRUE, TRUE); PollPairedDevices();
    CHECK(dropCalls == 1 && lastDrop == 1 && g_devices[b].allowAuto);

    /* Deny does not unpair. A later Accept cancels any stale policy drop and
       keeps an already-live session rather than toggling it off. */
    Reset(); Observe(0, 1, TRUE, TRUE); PollPairedDevices();
    Click(HIT_DEV_DENY_BTN, 1);
    CHECK(g_devices[0].denied && !g_devices[0].forgotten && !g_devices[0].visible);
    CHECK(DeviceIsListed(&g_devices[0]) && !DeviceNeedsReview(&g_devices[0]));
    CHECK(saveCalls == 1 && queuedCount == 1 && queued[0].op == 4);
    DisconnectIfStillBlocked(1, L"Audio", L"Earbuds"); CHECK(dropCalls == 1);
    Click(HIT_DEV_CONNECT_BTN, 1);
    CHECK(g_devices[0].visible && !g_devices[0].denied && g_devices[0].sessionAllowed);
    CHECK(saveCalls == 2 && queuedCount == 1 && g_devices[0].connected);
    DisconnectIfStillBlocked(1, L"Audio", L"Earbuds"); CHECK(dropCalls == 1);
    DenyDevice(&g_devices[0]); g_devices[0].connected = FALSE;
    g_manualGraceUntil = (LONGLONG)(tick + 5000); PollPairedDevices();
    CHECK(g_devices[0].denied && !g_devices[0].visible && dropCalls == 2);
    Click(HIT_DEV_AUTO_TOGGLE, 1);
    CHECK(!g_devices[0].denied && g_devices[0].visible && g_devices[0].allowAuto);
    CHECK(!DeviceShouldBlock(&g_devices[0], TRUE));
    Click(HIT_DEV_FORGET_BTN, 1);
    CHECK(g_devices[0].forgotten && !DeviceIsListed(&g_devices[0]));
    CHECK(queued[queuedCount - 1].op == 3);
    DoConnectNow(1, L"Audio", L"Earbuds", FALSE); CHECK(connectCalls == 0);
    g_manualGraceUntil = 0; PollPairedDevices();
    CHECK(g_devices[0].forgotten && !g_devices[0].visible && dropCalls == 3);

    /* Unpaired strangers stay hidden and blocked, even with Guard paused. */
    Reset(); g_guardOn = FALSE; Observe(0, 9, FALSE, TRUE); PollPairedDevices();
    CHECK(g_devCount == 1 && !DeviceIsListed(&g_devices[0]) && dropCalls == 1);
    observedCount = 0; PollPairedDevices(); CHECK(g_devCount == 0);
    /* Preserve an intentional Windows connection without making it Auto On. */
    Reset(); Observe(0, 1, TRUE, FALSE); PollPairedDevices();
    g_hwnd = (HANDLE)1; fakeIntent = TRUE;
    Observe(0, 1, TRUE, TRUE); PollPairedDevices();
    CHECK(g_devices[0].visible && g_devices[0].sessionAllowed && !g_devices[0].allowAuto);
    CHECK(saveCalls == 1 && dropCalls == 0);
    /* Auto reconnect is a worker action, not a Guard enforcement decision.
       Start with Guard OFF, fail twice, then observe real audio readiness. */
    Reset(); g_guardOn = FALSE; Observe(0, 1, TRUE, FALSE); PollPairedDevices();
    ApproveDevice(&g_devices[0], TRUE, tick);
    fakeAudioResult = (BtAudioResult){S_OK, -5, TRUE, FALSE};
    int repaired = profileCalls; RunAutoConnect(); CHECK(profileCalls == repaired + 1);
    CHECK(connectCalls == 1 && g_devices[0].connectState == BT_CONNECT_FAILED);
    CHECK(!g_devices[0].connected && g_devices[0].nextConnectTick == tick + 5000);
    for (int i = 0; i < 100; i++) { tick += 40; RunAutoConnect(); }
    CHECK(connectCalls == 1); /* never hammer on the 80/250ms scan cadence */
    tick = 15000; RunAutoConnect(); CHECK(connectCalls == 2 && g_devices[0].nextConnectTick == 25000);
    tick = 25000; fakeAudioResult.requested = TRUE; fakeAudioResult.commandHr = S_OK;
    /* An ACL link with no audio must not suppress the next audio request. */
    Observe(0, 1, TRUE, TRUE); PollPairedDevices(); RunAutoConnect();
    CHECK(connectCalls == 3 && g_devices[0].connectState == BT_CONNECT_REQUESTED);
    CHECK(!g_devices[0].audioConnected); /* a successful request is not success */
    g_devices[0].audioStatusValid = g_devices[0].audioEndpointFound = g_devices[0].audioConnected = TRUE;
    PollPairedDevices(); RunAutoConnect(); CHECK(connectCalls == 3 && g_devices[0].connectAttempts == 0);
    CHECK(DeviceAudioReady(&g_devices[0]) && !DeviceAutoConnectDue(&g_devices[0], tick, TRUE));
    /* A later out-of-range loss resumes attempts, with a bounded ceiling. */
    g_devices[0].audioConnected = FALSE; Observe(0, 1, TRUE, FALSE); PollPairedDevices();
    RunAutoConnect(); CHECK(connectCalls == 4 && g_devices[0].connectAttempts == 1);
    for (int i = 0; i < 12; i++) { tick = g_devices[0].nextConnectTick; RunAutoConnect(); }
    CHECK(g_devices[0].connectAttempts == 5 && g_devices[0].nextConnectTick == tick + 60000);
    CHECK(ConnectRetryDelayMs(0) == 5000 && ConnectRetryDelayMs(99) == 60000);
    /* Guard ON must behave identically for approved Auto ON earbuds. */
    g_guardOn = TRUE; tick = g_devices[0].nextConnectTick;
    int before = connectCalls; RunAutoConnect(); CHECK(connectCalls == before + 1);
    /* The radio, manual Disconnect, off toggle and current decisions all gate
       stale work. No choice is inferred from discovery or a successful API. */
    tick = g_devices[0].nextConnectTick; before = connectCalls;
    g_radioOn = FALSE; RunAutoConnect(); CHECK(connectCalls == before);
    g_radioOn = TRUE; g_suspendAutoConnect = TRUE;
    RunAutoConnect(); DoConnectNow(1, L"Audio", L"Earbuds", FALSE); CHECK(connectCalls == before);
    g_suspendAutoConnect = FALSE; g_devices[0].connected = TRUE;
    Click(HIT_DEV_CONNECT_BTN, 1);
    CHECK(g_devices[0].autoPaused && g_devices[0].allowAuto && g_devices[0].connected);
    DisconnectIfStillRequested(1, L"Audio", L"Earbuds"); CHECK(dropCalls == 1);
    g_devices[0].connected = FALSE;
    tick += 3600000; RunAutoConnect(); DoConnectNow(1, L"Audio", L"Earbuds", FALSE);
    CHECK(connectCalls == before && g_devices[0].autoPaused);
    Click(HIT_DEV_CONNECT_BTN, 1);
    CHECK(!g_devices[0].autoPaused && g_devices[0].sessionAllowed);
    DisconnectIfStillRequested(1, L"Audio", L"Earbuds"); CHECK(dropCalls == 1);
    DoConnectNow(1, L"Audio", L"Earbuds", FALSE); CHECK(connectCalls == before + 1);
    Click(HIT_DEV_AUTO_TOGGLE, 1); tick += 60000;
    RunAutoConnect(); DoConnectNow(1, L"Audio", L"Earbuds", FALSE);
    CHECK(connectCalls == before + 1 && !g_devices[0].allowAuto);
    ApproveDevice(&g_devices[0], TRUE, tick); DenyDevice(&g_devices[0]);
    RunAutoConnect(); DoConnectNow(1, L"Audio", L"Earbuds", FALSE); CHECK(connectCalls == before + 1);
    /* Even inconsistent old data cannot bypass Deny or Forget in queued work. */
    g_devices[0].visible = TRUE; g_devices[0].allowAuto = TRUE;
    CHECK(!DeviceAutoConnectDue(&g_devices[0], tick, TRUE));
    RunAutoConnect(); DoConnectNow(1, L"Audio", L"Earbuds", FALSE); CHECK(connectCalls == before + 1);
    g_devices[0].denied = FALSE; g_devices[0].forgotten = TRUE;
    CHECK(!DeviceAutoConnectDue(&g_devices[0], tick, TRUE));
    RunAutoConnect(); DoConnectNow(1, L"Audio", L"Earbuds", FALSE); CHECK(connectCalls == before + 1);
    /* Accept/current-session permission does not authorize automatic work. */
    Reset(); Observe(0, 1, TRUE, FALSE); PollPairedDevices(); ApproveDevice(&g_devices[0], FALSE, tick);
    RunAutoConnect(); DoConnectNow(1, L"Audio", L"Earbuds", TRUE); CHECK(connectCalls == 0);
    /* A saved-only scan with no name/class must not erase a known audio
       kind, make an earbuds row anonymous, or disable automatic requests. */
    ApproveDevice(&g_devices[0], TRUE, tick); observed[0].szName[0] = L'\0';
    PollPairedDevices(); CHECK(wcscmp(g_devices[0].kind, L"Audio") == 0);
    CHECK(wcscmp(g_devices[0].name, L"Earbuds") == 0 && DeviceAutoConnectDue(&g_devices[0], tick, TRUE));
    /* Report no addressed endpoint separately from COM/driver failure. */
    ApproveDevice(&g_devices[0], TRUE, tick);
    fakeAudioResult = (BtAudioResult){S_OK, -1168, FALSE, FALSE}; RunAutoConnect();
    CHECK(g_devices[0].connectState == BT_CONNECT_NO_ENDPOINT && !g_devices[0].connected);
    tick = g_devices[0].nextConnectTick; fakeAudioResult.enumerationHr = -5;
    RunAutoConnect(); CHECK(g_devices[0].connectState == BT_CONNECT_FAILED);

    /* Non-audio phone/HID connections remain Windows-managed. */
    Reset(); Observe(0, 2, TRUE, FALSE); wcscpy(observed[0].szName, L"Phone"); PollPairedDevices();
    ApproveDevice(&g_devices[0], TRUE, tick); RunAutoConnect(); CHECK(connectCalls == 0);
    Observe(0, 2, TRUE, TRUE); wcscpy(observed[0].szName, L"Phone"); PollPairedDevices();
    CHECK(g_devices[0].connected && !DeviceShouldBlock(&g_devices[0], FALSE));
    /* Active addressed audio is still reflected/enforced when a classic
       record goes missing. Its disappearance is also a real disconnect. */
    Reset(); Observe(0, 1, TRUE, FALSE); PollPairedDevices(); ApproveDevice(&g_devices[0], FALSE, tick);
    g_devices[0].audioStatusValid = g_devices[0].audioEndpointFound = g_devices[0].audioConnected = TRUE;
    PollPairedDevices(); CHECK(g_devices[0].connected && !g_devices[0].linkConnected);
    observedCount = 0; PollPairedDevices(); CHECK(g_devices[0].connected && DeviceAudioReady(&g_devices[0]));
    g_devices[0].sessionAllowed = FALSE; PollPairedDevices(); CHECK(dropCalls == 1);
    g_devices[0].audioConnected = FALSE; PollPairedDevices(); CHECK(!g_devices[0].connected);

    /* A UI choice made while the driver is returning the next record must
       survive that observation. Never merge a stale approval/pause snapshot. */
    Reset(); Observe(0, 1, TRUE, FALSE); Observe(1, 2, TRUE, TRUE); PollPairedDevices();
    ApproveDevice(&g_devices[0], TRUE, tick);
    betweenRecords = DenySecondDuringScan; PollPairedDevices();
    CHECK(betweenRecords == NULL && g_devices[1].denied && !g_devices[1].visible);
    CHECK(g_devices[0].visible && g_devices[0].allowAuto && dropCalls == 1 && lastDrop == 2);
    Reset(); Observe(0, 1, TRUE, TRUE); Observe(1, 2, TRUE, TRUE); PollPairedDevices();
    ApproveDevice(&g_devices[0], TRUE, tick); ApproveDevice(&g_devices[1], TRUE, tick);
    betweenRecords = PauseSecondDuringScan; PollPairedDevices();
    CHECK(betweenRecords == NULL && g_devices[1].autoPaused && g_devices[1].allowAuto);
    CHECK(!g_devices[1].sessionAllowed && !g_devices[0].autoPaused && lockDepth == 0);

    /* Capacity exhaustion must not be interpreted as an owner denial. */
    Reset(); g_devCount = MAX_DEVICES;
    for (int i = 0; i < MAX_DEVICES; i++) {
        g_devices[i].address = (ULONGLONG)(i + 100); g_devices[i].visible = TRUE;
    }
    Observe(0, 1, TRUE, TRUE); PollPairedDevices(); CHECK(dropCalls == 0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="bluetooth-review-") as td:
            src, exe = Path(td) / "review.c", Path(td) / "review"
            src.write_text(harness, encoding="utf-8")
            built = subprocess.run(
                [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pthread", str(src), "-o", str(exe)],
                capture_output=True, text=True, timeout=30)
            self.assertEqual(built.returncode, 0, built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
            self.assertEqual(ran.returncode, 0, ran.stderr)

    def test_bluetooth_review_controls_and_denials_are_persistent_and_address_bound(self):
        btg = read_c_inlined("bluetooth-guard/src/bluetooth_guard.c")
        rules = (ROOT / "bluetooth-guard/src/parts/autostart_rules.c").read_text(encoding="utf-8")
        ui = (ROOT / "bluetooth-guard/src/parts/window_entry.c").read_text(encoding="utf-8")
        controls = (ROOT / "bluetooth-guard/src/parts/ui_controls.c").read_text(encoding="utf-8")
        self.assertIn('L"Needs approval"', btg)
        self.assertIn('L"Accept"', ui)
        self.assertIn('L"Deny"', ui)
        self.assertIn('L"Auto-Connect"', ui)
        self.assertIn("if (!DeviceIsListed(d)) continue;", ui)
        self.assertIn("g_hits[hitIdx].address", ui)
        self.assertNotIn("g_hits[hitIdx].index", ui)
        self.assertIn("FindDeviceIndexByAddress(target)", ui)
        self.assertIn("g_trayDeviceAddresses[shown] = g_devices[i].address;", ui)
        self.assertIn("if (g_trayMenuTracking) return;", ui)
        self.assertIn("if (r.top < top) r.top = top;", controls)
        self.assertIn("if (r.bottom > bottom) r.bottom = bottom;", controls)
        self.assertIn('d->denied ? L"D" : L"F"', rules)
        self.assertIn('BOOL denied = (wcscmp(p4, L"D") == 0);', rules)
        self.assertIn("d->denied = denied;", rules)
        self.assertIn("if (!d->visible && !d->forgotten && !d->denied) continue;", rules)
        self.assertIn("else if (item.op == 4) DisconnectIfStillBlocked(", rules)
        deny = ui[ui.index("} else if (kind == HIT_DEV_DENY_BTN)"):
                  ui.index("} else if (kind == HIT_DEV_FORGET_BTN)")]
        self.assertIn("DenyDevice(d);", deny)
        self.assertIn("SaveRules();", deny)
        self.assertIn("EnqueueWork(4, addr, dmac, dkind, dname);", deny)
        self.assertNotIn("EnqueueWork(3", deny, "Deny must not silently unpair a device")
        self.assertNotIn("BLUETOOTH_SERVICE_DISABLE", btg)

    def test_bluetooth_guard_powershell_uses_the_same_trusted_inventory(self):
        ps = (ROOT / "bluetooth-guard" / "BluetoothGuard.ps1").read_text(encoding="utf-8")
        core = (ROOT / "bluetooth-guard" / "BluetoothCore.cs").read_text(encoding="utf-8")

        self.assertIn("sp.fReturnRemembered = false", core)
        self.assertIn("IsWindowsBluetoothUiForeground()", core)
        self.assertIn("GetLastInputInfo", core)
        self.assertIn("DropUntrustedConnection", core)
        self.assertIn("BluetoothRemoveDevice(ref address)", core)
        self.assertIn("DeviceTrustFormat  = 2", ps)
        self.assertIn("$o.DeviceTrustFormat -eq 2 -and $o.Devices", ps)
        self.assertIn("function Update-WindowsUiIntent", ps)
        self.assertIn("$script:WindowsUiIntentUntil = [DateTime]::UtcNow.AddSeconds(5)", ps)
        self.assertIn("$manualUi = Update-WindowsUiIntent", ps)
        self.assertIn("function Update-TrustFromSnapshots", ps)
        self.assertIn("$script:SnapshotBaseline -and $newLink -and $manualUi", ps)
        self.assertIn("function Get-TrustedDeviceSnapshots", ps)
        trusted_builder = ps[ps.index("function Get-TrustedDeviceSnapshots"):ps.index("function Update-WindowsUiIntent")]
        self.assertIn("Address = [uint64]$entry.Address", trusted_builder)
        self.assertIn("Connected = $false", trusted_builder)
        self.assertIn("Render-Devices $trustedSnaps", ps)
        self.assertGreaterEqual(ps.count("Queue-BtWork 'drop'"), 2)
        self.assertIn("AllowAutoConnect = $false", ps)
        self.assertIn("$currentEntry.Forgotten = $true", ps)
        self.assertIn("Queue-BtWork 'forget'", ps)

        tray = ps[ps.index("function Rebuild-TrayMenu"):ps.index("Rebuild-TrayMenu @()")]
        self.assertIn("Get-DeviceEntry", tray)
        self.assertIn("$entry.Forgotten", tray)

    def test_project_hub_command_ids_do_not_collide(self):
        """Guard the 'swallowed button' bug class: the branch-download menu
        reserves a numeric command-ID range, and a button ID that lands
        inside that range gets eaten as a bogus branch selection."""
        src = read_c_inlined("project-hub/src/project_hub.c")
        ids = {}
        for m in re.finditer(r"^#define\s+(ID_[A-Z0-9_]+)\s+(\d+)\s*$", src, re.M):
            name, val = m.group(1), int(m.group(2))
            self.assertNotIn(val, ids,
                             f"command ID {val} claimed by both {ids.get(val)} and {name}")
            ids[val] = name
        self.assertGreaterEqual(len(ids), 20, "expected the full hub command-ID map")
        self.assertIn("ID_MENU_BRANCH_BASE", ids.values())
        self.assertIn("ID_MENU_BRANCH_CUSTOM", ids.values())
        lo = min(v for v, n in ids.items() if n.startswith("ID_MENU_BRANCH"))
        hi = max(v for v, n in ids.items() if n.startswith("ID_MENU_BRANCH"))
        for val, name in ids.items():
            if name.startswith("ID_MENU_BRANCH"):
                continue
            self.assertFalse(lo <= val <= hi,
                             f"{name}={val} falls inside the branch-menu range {lo}-{hi}")

    def test_project_hub_covers_suite_projects(self):
        hub = (ROOT / "project-hub" / "ProjectHub.ps1").read_text(encoding="utf-8")
        hub_c = read_c_inlined("project-hub/src/project_hub.c")
        setup = (ROOT / "project-hub" / "Setup.ps1").read_text(encoding="utf-8")
        for expected in (
            "Prism Lighting",
            "Bluetooth Guard",
            "Spatial HUD",
            "OrcaSlicer Plugins",
        ):
            self.assertIn(expected, hub)
            self.assertIn({"OrcaSlicer Plugins": "Undahang",
                           "Keyboard Lighting": "Prism Lighting"}.get(expected, expected), hub_c)
        self.assertIn("Unlayered Infill", hub_c)
        self.assertIn("DNS Pilot", hub_c)
        self.assertIn("CreateRunspace", hub)
        self.assertNotIn("$T::", hub, "Never use $Var::Member in PowerShell UI code (breaks Command Mode)")
        self.assertIn("window.ready", hub)
        self.assertIn("crash.log", hub)
        self.assertIn("window.ready", setup)
        self.assertTrue((ROOT / "project-hub" / "Setup.bat").exists())
        self.assertTrue((ROOT / "docs" / "UI_STYLE_GUIDE.md").exists())
        self.assertIn("Update Hub", hub_c)
        self.assertIn("WorkerSelfUpdateHub", hub_c)
        self.assertIn("SmoothRoundRect", hub_c)
        self.assertIn("RES_ICON_EMBLEM", hub_c)
        btg_c = read_c_inlined("bluetooth-guard/src/bluetooth_guard.c")
        self.assertIn("SmoothRoundRect", btg_c)
        self.assertIn("DrawPillToggle", btg_c)
        self.assertIn("RES_ICON_EMBLEM", btg_c)
        ui_cs = (ROOT / "keyboard-lighting" / "ui_controls.cs.txt").read_text(encoding="ascii")
        self.assertIn("DrawSuiteEmblem", ui_cs)
        self.assertIn("Radius = 14", ui_cs)
        kbl_c = read_c_inlined("keyboard-lighting/src/keyboard_lighting.c", encoding="ascii", errors="strict")
        self.assertIn("SmoothRoundRect", kbl_c)
        self.assertIn("DrawPillSwitch", kbl_c)
        self.assertIn("RES_ICON_EMBLEM", kbl_c)

        for rel_ico in (
            "docs/brand/emblem.ico",
            "project-hub/hub.ico",
            "bluetooth-guard/btguard.ico",
            "keyboard-lighting/app.ico",
            "dns-pilot/dnspilot.ico",
            "docs/brand/spatial-hud.ico",
            "docs/brand/orca-plugins.ico",
            "agent-notes/agent-notes.ico",
            "audio-switcher/audio-switcher.ico",
        ):
            raw = (ROOT / rel_ico).read_bytes()
            self.assertEqual(raw[:4], b"\x00\x00\x01\x00", f"{rel_ico} missing valid Windows ICO header")
            count = int.from_bytes(raw[4:6], "little")
            self.assertGreaterEqual(count, 5, f"{rel_ico} should include multi-DPI icon frames")

    def test_undahang_icon_has_distinct_multisize_rgba_assets(self):
        import hashlib
        import struct
        import zlib
        apps = json.loads((ROOT / "suite-apps.json").read_text(encoding="utf-8"))["apps"]
        by_id = {app["id"]: app for app in apps}
        icon_path = "docs/brand/undahang-wave-v1.ico"
        self.assertEqual(by_id["orca"]["icon"], icon_path)
        self.assertEqual(by_id["unlayered-infill"]["icon"], "docs/brand/orca-plugins.ico")
        infill = (ROOT / by_id["unlayered-infill"]["icon"]).read_bytes()
        self.assertEqual(hashlib.sha256(infill).hexdigest(), "8133bced08430f323f081cf69468e2c06f64c2e4822ea043291f704544c2f1de",
                         "Unlayered Infill's approved icon must remain unchanged")
        raw = (ROOT / icon_path).read_bytes()
        self.assertNotEqual(raw, infill)
        self.assertEqual(struct.unpack_from("<HHH", raw), (0, 1, 6))
        expected = (256, 64, 48, 32, 24, 16)
        for frame, size in enumerate(expected):
            w, h, colors, reserved, planes, depth, length, offset = struct.unpack_from(
                "<BBBBHHII", raw, 6 + frame * 16)
            self.assertEqual((w or 256, h or 256, colors, reserved, planes, depth),
                             (size, size, 0, 0, 1, 32))
            self.assertGreaterEqual(offset, 6 + 16 * 6)
            self.assertLessEqual(offset + length, len(raw))
            png = raw[offset:offset + length]
            self.assertEqual(png[:8], b"\x89PNG\r\n\x1a\n")
            self.assertEqual(struct.unpack_from(">IIBBBBB", png, 16), (size, size, 8, 6, 0, 0, 0))
            compressed = bytearray()
            pos = 8
            while pos < len(png):
                count = struct.unpack_from(">I", png, pos)[0]
                kind = png[pos + 4:pos + 8]
                payload = png[pos + 8:pos + 8 + count]
                crc = struct.unpack_from(">I", png, pos + 8 + count)[0]
                self.assertEqual(crc, zlib.crc32(kind + payload) & 0xffffffff)
                if kind == b"IDAT":
                    compressed.extend(payload)
                pos += count + 12
            self.assertEqual(len(zlib.decompress(compressed)), size * (size * 4 + 1))
        master = (ROOT / "docs/brand/icon-undahang-wave-v1.png").read_bytes()
        self.assertEqual(master[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(struct.unpack_from(">IIBBBBB", master, 16), (1024, 1024, 8, 6, 0, 0, 0))
        resources = (ROOT / "project-hub/src/hub.rc").read_text(encoding="utf-8")
        self.assertIn('11 ICON "../../' + icon_path + '"', resources)
        self.assertIn('6 ICON "../../docs/brand/orca-plugins.ico"', resources)
        generator = (ROOT / "tools/generate_suite_icons.py").read_text(encoding="utf-8")
        self.assertIn("def build_icon_undahang(", generator)
        self.assertIn('"icon-undahang-wave-v1.png"', generator)
        self.assertIn('("Undahang",', generator)
        self.assertIn('("Unlayered Infill",', generator)
        tree = ast.parse(generator)
        definitions = next(node for node in tree.body if isinstance(node, ast.FunctionDef)
                           and node.name == "suite_icon_definitions")
        entries = next(node.value.elts for node in definitions.body if isinstance(node, ast.Return))
        approved = next(entry for entry in entries if ast.literal_eval(entry.elts[0]) == "Undahang")
        self.assertIsNone(ast.literal_eval(approved.elts[3]), "regeneration must not overwrite the approved master")
        hub = (ROOT / "project-hub/src/project_hub.c").read_text(encoding="utf-8")
        self.assertRegex(hub, r"#define RES_ICON_UNDAHANG +11\b")
        start = hub.index("static void RebuildFontsAndIcons(")
        rebuild = hub[start:hub.index("\n}\n", start)]
        self.assertIn("for (int i = 0; i < 9; i++) {", rebuild,
                      "the ninth resource must load and replace its owned handle on every DPI rebuild")

    def test_hub_build_cache_tracks_the_new_external_icon_resource(self):
        import importlib.util
        import tempfile
        spec = importlib.util.spec_from_file_location("icon_cache_build", ROOT / "tools/build_windows.py")
        builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(builder)
        self.assertIn(ROOT / "docs/brand/undahang-wave-v1.ico", builder.TARGET_SHARED_SOURCES["hub"])
        with tempfile.TemporaryDirectory() as name:
            folder = Path(name)
            src = folder / "src"
            src.mkdir()
            (src / "hub.rc").write_text('11 ICON "../approved.ico"', encoding="ascii")
            icon = folder / "approved.ico"
            icon.write_bytes(b"old icon")
            builder.ROOT = folder
            builder.TARGET_SHARED_SOURCES = {"hub": (icon,)}
            before = builder.target_key("hub", src, [], "test-compiler")
            icon.write_bytes(b"new icon")
            self.assertNotEqual(before, builder.target_key("hub", src, [], "test-compiler"))

    def test_brand_exports_keep_one_canonical_copy_per_asset(self):
        """The icon generator used to write six legacy aliases that were exact
        copies of the canonical masters. They added roughly twenty-six MiB to every
        clone without serving an executable or the spin preview."""
        gen = (ROOT / "tools" / "generate_suite_icons.py").read_text(encoding="utf-8")
        brand = ROOT / "docs" / "brand"
        self.assertIn("REDUNDANT_BRAND_EXPORTS", gen)
        self.assertIn("for stale in REDUNDANT_BRAND_EXPORTS", gen)
        self.assertIn("RemoveRedundantBrandExports()", gen)
        for rel in (
            "icon-project-hub.png", "tri-spoke-spin-1024.webp",
            "tri-spoke-spin-1024.gif", "spin.html",
        ):
            self.assertTrue((brand / rel).is_file(), f"canonical brand export missing: {rel}")
        for rel in (
            "project-hub-icon-2048.png", "project-hub-icon-1024.png",
            "tri-spoke-emblem-2048.png", "tri-spoke-emblem-1024.png",
            "tri-spoke-spin.webp", "tri-spoke-spin.gif",
        ):
            self.assertFalse((brand / rel).exists(), f"redundant brand alias returned: {rel}")

    def test_suite_apps_catalog_is_complete_and_matches_the_hub(self):
        """suite-apps.json is the hub's data-driven extension point: one entry
        adds a card with no hub rebuild. It had no test coverage at all, which
        is how an entry shipped without an `id` -- and AddCatalogCard returns
        early on a missing id, so that card silently stopped being refreshed
        from the catalog and kept pointers into a string pool the next rebuild
        had already reset."""
        catalog = json.loads((ROOT / "suite-apps.json").read_text(encoding="utf-8"))
        apps = catalog["apps"]
        self.assertTrue(apps, "suite-apps.json lists no apps")

        hub_c = read_c_inlined("project-hub/src/project_hub.c")

        # The ids the hub recognises as built-in cards, read out of BuiltInSlot
        # so this test follows the source instead of duplicating a list.
        m = re.search(r'static const char \*known\[\d+\]\s*=\s*\{([^}]*)\}', hub_c)
        self.assertIsNotNone(m, "BuiltInSlot's known[] table changed shape - update this test")
        known = re.findall(r'"([^"]+)"', m.group(1))
        self.assertTrue(known, "could not read the hub's built-in id list")

        ids = []
        for i, app in enumerate(apps):
            where = f"suite-apps.json apps[{i}]"

            # `id` first: everything else is unreachable without it, because
            # AddCatalogCard does `if (!JsonGet(obj, "id", ...)) return;`.
            self.assertIn("id", app,
                          f"{where} ({app.get('title', '<no title>')}) has no `id` - "
                          f"AddCatalogCard returns before reading ANY other field, so "
                          f"this card is never refreshed from the catalog")
            app_id = app["id"]
            ids.append(app_id)
            where = f"suite-apps.json '{app_id}'"

            for field in ("title", "blurb", "dir", "icon", "shortcut", "buttons"):
                self.assertIn(field, app, f"{where} is missing `{field}`")
                self.assertTrue(str(app.get(field, "")).strip(),
                                f"{where} has an empty `{field}`")

            # A built-in id must be one the hub actually recognises, or the
            # entry is treated as a new dynamic card and duplicates the card.
            if app_id in known:
                self.assertLess(known.index(app_id), 8,
                                f"{where} maps outside the 8 built-in slots")
            # An exe-backed app must point at a file that exists in the repo:
            # the hub downloads this exact path from raw.githubusercontent.
            if app.get("exe"):
                self.assertTrue((ROOT / app["exe"]).exists(),
                                f"{where} exe `{app['exe']}` does not exist in the repo")
            self.assertTrue((ROOT / app["icon"]).exists(),
                            f"{where} icon `{app['icon']}` does not exist in the repo")

            # Every verb must be one BuiltInBtnId can map to a WM_COMMAND id,
            # otherwise FillBuiltInDef drops the button silently.
            slots = app["buttons"].split(",")
            self.assertLessEqual(len(slots), 4,
                                 f"{where} declares {len(slots)} buttons; ActionBtn holds 4")
            for slot in slots:
                parts = slot.split(":")
                self.assertGreaterEqual(len(parts), 2,
                                        f"{where} button `{slot}` is not verb:Label[:width]")
                verb = parts[0].strip()
                self.assertTrue(verb, f"{where} button `{slot}` has an empty verb")
                if app_id in known:
                    # BuiltInBtnId's map entries look like: { "open", ID_BTG_OPEN },
                    self.assertRegex(hub_c, rf'\{{\s*"{re.escape(verb)}",',
                                     f"{where} verb `{verb}` is not in BuiltInBtnId's map, "
                                     f"so FillBuiltInDef drops the button silently")
                if len(parts) > 2:
                    self.assertTrue(parts[2].isdigit(),
                                    f"{where} button width `{parts[2]}` is not a number")

        self.assertEqual(len(ids), len(set(ids)),
                         f"duplicate ids in suite-apps.json: {ids}")
        # Every built-in card the hub renders must be described by the
        # catalog, so its text survives a catalog refresh. The catalog may
        # ALSO list dynamic apps the hub has no built-in slot for - that is
        # the whole point of AddCatalogCard - so the old equality assertion
        # became wrong the day the eighth app shipped: what must hold is
        # built-ins ⊆ catalog, and every non-built-in entry carries an exe,
        # because AddCatalogCard returns early without one and the card
        # would then render with no install path at all.
        for kid in known:
            self.assertIn(kid, ids,
                          f"built-in hub card '{kid}' is missing from suite-apps.json, "
                          f"so its text dies the next time the string pool resets")
        for app_id, app in zip(ids, apps):
            if app_id not in known:
                self.assertTrue(str(app.get("exe", "")).strip(),
                                f"suite-apps.json '{app_id}' is not a built-in slot and has "
                                f"no exe, so AddCatalogCard drops it silently")

    def test_weather_radar_hub_card_survives_missing_or_incomplete_catalog(self):
        """Weather Radar is a core suite app but its card used to exist only as
        a remote catalog extra. Any successful partial catalog rebuild could
        reset the Hub to seven built-ins and silently drop that entry."""
        catalog = json.loads((ROOT / "suite-apps.json").read_text(encoding="utf-8"))
        weather = next(app for app in catalog["apps"] if app["id"] == "weatherradar")
        parser = (ROOT / "project-hub/src/parts/catalog_icons_json.c").read_text(encoding="utf-8")
        fallback_start = parser.index("static const char kWeatherRadarFallbackJson[]")
        fallback_end = parser.index("static BOOL HasWeatherRadarCatalogCard", fallback_start)
        fallback = parser[fallback_start:fallback_end]
        c_strings = re.findall(r'"(?:\\.|[^"\\])*"', fallback)
        fallback_json = "".join(ast.literal_eval(part) for part in c_strings)
        fallback_app = json.loads(fallback_json)
        for field in ("id", "title", "blurb", "exe", "dir", "icon", "shortcut", "buttons"):
            self.assertEqual(fallback_app[field], weather[field],
                             f"Weather Radar fallback diverged from suite-apps.json `{field}`")

        ensure_start = parser.index("static void EnsureWeatherRadarCatalogCard(")
        ensure = parser[ensure_start:parser.index("\n}\n", ensure_start)]
        self.assertIn("if (!HasWeatherRadarCatalogCard())", ensure)
        self.assertIn("AddCatalogCard(kWeatherRadarFallbackJson, loadIcon);", ensure)
        self.assertIn("if (icon[0] && loadIcon)", parser,
                      "the startup fallback can block WM_CREATE on an icon download")
        self.assertIn("c->resIconId = isWeatherRadar ? RES_ICON_WEATHER : 0;", parser,
                      "Weather Radar lost its embedded icon identity outside the catalog")
        self.assertIn("g_cardCount >= MAX_APPS - 1", parser,
                      "catalog extras can fill every slot before the required Weather fallback")
        renderer = (ROOT / "project-hub/src/parts/ui_renderer.c").read_text(encoding="utf-8")
        self.assertIn("return slot >= 0 && slot < 7 ? g_cardIcons[slot] : NULL;", renderer)
        self.assertIn("if (card->resIconId == RES_ICON_WEATHER) return g_cardIcons[7];",
                      renderer,
                      "dynamic catalog ordering can hide the embedded Weather Radar icon")

        init_src = (ROOT / "project-hub/src/parts/card_defs_verbs.c").read_text(encoding="utf-8")
        init_start = init_src.index("static void InitCardDefinitions(void)")
        init = init_src[init_start:init_src.index("\n}\n", init_start)]
        self.assertIn("EnsureWeatherRadarCatalogCard(FALSE);", init,
                      "Weather Radar is absent until the first remote check succeeds")

        rebuild_src = (ROOT / "project-hub/src/parts/builtin_card_text.c").read_text(encoding="utf-8")
        rebuild_start = rebuild_src.index("static void RebuildExtraCards(")
        rebuild = rebuild_src[rebuild_start:rebuild_src.index("\n}\n", rebuild_start)]
        self.assertIn("AddCatalogCard(obj, TRUE);", rebuild)
        fallback_call = rebuild.index("EnsureWeatherRadarCatalogCard(TRUE);")
        self.assertGreater(fallback_call, rebuild.index("p = q + 1;"),
                           "the fallback is not restored after parsing the fetched catalog")

        hub = (ROOT / "project-hub/src/project_hub.c").read_text(encoding="utf-8")
        hub_rc = (ROOT / "project-hub/src/hub.rc").read_text(encoding="utf-8")
        self.assertIn("RES_ICON_WEATHER", hub)
        self.assertIn("RES_ICON_WEATHER", hub[hub.index("const int ids[9]"):])
        self.assertIn('7 ICON "../../weather-radar/weather-radar.ico"', hub_rc)

    def _parse_builtin_btnid(self, hub_c):
        """Reimplement BuiltInBtnId in Python straight from the C source.

        BuiltInBtnId only scans map[off[slot] .. off[slot+1]), so a verb that is
        listed for an app in suite-apps.json but whose map entry sits OUTSIDE
        that app's off[] window returns 0 -- and FillBuiltInDef drops the button
        with no error anywhere. The catalog test only checks the verb appears
        somewhere in the map, which cannot catch that. This returns the same
        (known[], defines, btn_id) the runtime would use."""
        defines = dict(re.findall(r'#define\s+(ID_\w+)\s+(\d+)', hub_c))
        km = re.search(r'static const char \*known\[\d+\]\s*=\s*\{([^}]*)\}', hub_c)
        self.assertIsNotNone(km, "BuiltInSlot's known[] table changed shape - update this test")
        known = re.findall(r'"([^"]+)"', km.group(1))
        mm = re.search(r'struct \{ const char \*verb; int id; \} map\[\d+\]\s*=\s*\{(.*?)\};',
                       hub_c, re.S)
        self.assertIsNotNone(mm, "BuiltInBtnId's map[] changed shape - update this test")
        pairs = re.findall(r'\{\s*"(\w+)"\s*,\s*(ID_\w+)\s*\}', mm.group(1))
        om = re.search(r'off\[\d+\]\s*=\s*\{([^}]*)\}', hub_c)
        self.assertIsNotNone(om, "BuiltInBtnId's off[] changed shape - update this test")
        off = [int(x) for x in re.findall(r'\d+', om.group(1))]
        self.assertEqual(len(pairs), off[-2],
                         "map[] entry count and off[] disagree - a verb window is wrong")

        def btn_id(slot, verb):
            if slot < 0 or slot > 7:
                return 0
            for i in range(off[slot], off[slot + 1]):
                if pairs[i][0] == verb:
                    return defines[pairs[i][1]]
            return 0

        return known, defines, btn_id

    def test_weather_radar_shows_the_honest_probability_not_the_blended_one(self):
        """The owner's ask was pointed: predict the weather for a place they
        pick, show a radar, and make the chance of rain the ACTUAL chance -
        not the coverage-blended PoP every other app headlines.

        PoP = confidence x areal coverage. "50% chance of rain" from a
        blended product can mean "I am certain half the region gets wet",
        which is not an answer to "will it rain on me?". The only honest
        point probability available without a forecaster is the ensemble:
        Open-Meteo serves each GFS member as its own precipitation series, so
        the share of members reaching the WMO trace threshold (0.1 mm) at
        these exact coordinates IS the probability, with no areal term.

        Two traps the assertions below exist for:
          * members stop reporting past their own horizon and the API writes
            null. Counting silence as "dry" silently deflates every number,
            so the denominator must be the members that REPORTED the hour.
          * the blended figure must stay visible but visibly second: it is
            drawn as a hollow tick, never as the headline number.
        """
        main = (ROOT / "weather-radar" / "src" / "weather_radar.c").read_text(encoding="utf-8")
        fc = (ROOT / "weather-radar" / "src" / "parts" / "forecast.c").read_text(encoding="utf-8")
        rd = (ROOT / "weather-radar" / "src" / "parts" / "radar.c").read_text(encoding="utf-8")
        pl = (ROOT / "weather-radar" / "src" / "parts" / "places.c").read_text(encoding="utf-8")
        ui = (ROOT / "weather-radar" / "src" / "parts" / "ui.c").read_text(encoding="utf-8")
        wk = (ROOT / "weather-radar" / "src" / "parts" / "workers.c").read_text(encoding="utf-8")

        def code(src):
            # Strip comments WITHOUT eating URLs: "https://" carries a "//"
            # that a naive line-comment regex reads as the start of one.
            src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
            return re.sub(r"(?<!:)//[^\n]*", "", src)

        mc, fcc, rdc, uic = code(main), code(fc), code(rd), code(ui)

        # --- the honest number: ensemble fraction at the exact point ---
        self.assertIn("models=gfs_seamless", fcc,
                      "the ensemble request no longer names a model, so the API "
                      "answers with nothing to count")
        self.assertIn('strncmp(c->key, "precipitation_member", 20)', fcc,
                      "members are no longer discovered by key prefix, so a model "
                      "with a different member count silently loses members")
        self.assertRegex(mc, r"#define RAIN_MM\s+0\.1",
                         "the rain threshold moved off the WMO trace value; 0.1 mm is "
                         "where a gauge starts reporting anything at all")
        self.assertIn("if (v >= RAIN_MM) wet++;", fcc,
                      "a member is counted wet on any nonzero value, so a trace "
                      "drizzle of 0.01 mm inflates the headline odds")

        # --- null is absent, not dry: the denominator trap ---
        ens = fcc[fcc.index("static void FetchEnsemble("):]
        ens = ens[:ens.index("\n}\n")]
        self.assertIn("if (isnan(v)) continue;", ens,
                      "a member that reported nothing for the hour is counted as dry, "
                      "which deflates every longer-range probability toward zero")
        self.assertIn("e->reported[h] = reported;", ens,
                      "the per-hour denominator is not recorded, so the UI cannot say "
                      "how many members the fraction is out of")
        win = fcc[fcc.index("static double EnsWindowProb("):]
        win = win[:win.index("\n}\n")]
        self.assertIn("if (!any) continue;", win,
                      "a member with no reported hour in the window votes 'dry' in the "
                      "window probability")
        self.assertIn("return reporting > 0 ? (double)wet / (double)reporting : NAN;", win,
                      "the window probability is not a member fraction")

        # --- the blended figure stays visible, but as the tick, not the headline ---
        self.assertIn("PoPWindowMax", fcc, "the coverage-blended comparison vanished")
        head = uic[uic.index("static void PaintHonestCard("):]
        head = head[:head.index("\n}\n")]
        self.assertLess(head.index("EnsWindowProb(&g_ens, start, 6)"),
                        head.index("PoPWindowMax(&g_fc, 0, 6)"),
                        "the headline number is the blended PoP and the honest one is "
                        "the footnote - exactly the inversion the owner rejected")
        self.assertIn('wcscpy(pct, L"--")', head,
                      "with no ensemble answer the card invents a number instead of "
                      "saying it has none")
        self.assertIn("coverage-blended", head,
                      "the card never says what the other number is, so the contrast "
                      "is a claim instead of a label")

        # --- radar: catalog, tile URL, cache clocks, source zoom cap ---
        self.assertIn("https://api.rainviewer.com/public/weather-maps.json", rdc,
                      "the frame catalog endpoint moved")
        self.assertIn("#define MAX_FRAMES     64", mc,
                      "the radar timeline still caps its available observed/forecast frames "
                      "at 32, truncating longer nowcast timelines")
        self.assertIn("#define MAX_PAST_FRAMES 24", mc,
                      "the timeline still discards all but twelve recent observed frames")
        self.assertIn("int nowcastTotal = nowcast ? JLen(nowcast) : 0;", rdc)
        self.assertIn("int forecastKeep = nowcastTotal < MAX_FRAMES ? nowcastTotal : MAX_FRAMES;", rdc)
        self.assertIn("int pastKeep = MAX_FRAMES - forecastKeep;", rdc)
        self.assertIn("if (pastKeep > MAX_PAST_FRAMES) pastKeep = MAX_PAST_FRAMES;", rdc)
        self.assertIn("int start = total > pastKeep ? total - pastKeep : 0;", rdc)
        self.assertIn("g_frameN < pastKeep", rdc)
        self.assertIn("for (int i = 0; i < total && g_frameN < MAX_FRAMES; i++)", rdc,
                      "nowcast frames are not retained ahead of older observations")
        self.assertIn("while (j >= 0 && g_frames[j].time > current.time)", rdc,
                      "provider timestamps are not sorted into chronological playback order")
        self.assertIn("g_frameSel = firstForecast >= 0 ? firstForecast", rdc,
                      "the preview still opens on past observations when a future nowcast exists")
        self.assertIn("g_frames[i].time <= g_frames[latestObserved].time", rdc,
                      "a nowcast timestamp at or before the newest observation is presented as future")
        self.assertIn("g_frames[i].time < g_frames[firstForecast].time", rdc,
                      "the initial preview skips the nearest expected time")
        self.assertIn('L"Forecast %d/%d"', uic,
                      "the map does not identify the selected future step in its timeline")
        self.assertIn('L"Forecast · %d steps"', uic,
                      "the map hides how many forecast steps are available while an observation is selected")
        self.assertIn('L"No forecast frames"', uic,
                      "the map silently implies future data when the provider publishes none")
        self.assertIn("img.left + S(520)", uic,
                      "the zoom note overlaps the new forecast-step badge")
        self.assertIn('L"%s%hs/256/%d/%d/%d/2/1_1.png"', rdc,
                      "the RainViewer tile URL is not host+path+256+z/x/y+scheme2+"
                      "smooth1_snow1, so every tile 404s")
        self.assertIn("https://tile.openstreetmap.org/%d/%d/%d.png", rdc,
                      "the basemap URL changed; radar tiles are transparent and need "
                      "something under them")
        self.assertIn('FetchTileGrid(L"rv", z, tx, ty', rdc,
                      "radar frames stopped using their own cache family")
        self.assertIn("2 * 24 * 3600", rdc, "radar frames cache for the wrong window")
        self.assertIn("30 * 24 * 3600", rdc,
                      "basemap tiles no longer cache a month, so every launch "
                      "re-downloads the same nine map tiles")
        self.assertIn("if (dst[i] & 0xFF000000u)", rdc,
                      "the decoder no longer detects visible tile pixels")

        # RainViewer's validated source ceiling is level 7. Apply the cap
        # before fetching: an opaque unsupported tile cannot be detected by
        # the transparent-pixel test. Higher display levels are local crops.
        self.assertIn("#define RADAR_TILE_ZOOM_MIN     7", main)
        self.assertIn("#define RADAR_TILE_ZOOM_MAX     7", main,
                      "a request can reach level 8 and paint the provider's unsupported-zoom marker")
        self.assertIn("if (displayZoom > RADAR_TILE_ZOOM_MAX) return RADAR_TILE_ZOOM_MAX;", main)
        self.assertIn("#define RADAR_DISPLAY_ZOOM_MAX  10", main)
        self.assertIn("RadarTileZoomForDisplay(displayZoom)", rdc)
        self.assertIn("g_zoom = (g_zoom >= RADAR_DISPLAY_ZOOM_MAX) ? RADAR_TILE_ZOOM_MIN : g_zoom + 1;", ui)
        self.assertIn("if (oldTileZoom != newTileZoom) RequestTiles();", ui)
        self.assertIn("int localSteps = displayZoom - sourceZoom;", ui)
        self.assertIn("focusPx /= 2;", ui)
        self.assertIn("detail zoom — sourced from host level 7", ui)
        self.assertIn("openstreetmap contributors", ui.lower(),
                      "OSM tiles ship without the credit their licence requires")
        self.assertIn("rainviewer", ui.lower(),
                      "the radar pane does not credit RainViewer for the frames")

        # --- WIC through COM, never through the import table ---
        self.assertIn("CoCreateInstance(&CLSID_WICImagingFactory", rdc,
                      "WIC is no longer reached via CoCreateInstance")
        bw = (ROOT / "tools" / "build_windows.py").read_text(encoding="utf-8")
        self.assertNotIn("windowscodecs", bw,
                         "windowscodecs is linked statically, which puts a non-contract "
                         "DLL into the exe's import table")

        # --- the UI thread never fetches ---
        self.assertNotIn("HttpGetBuf(", uic,
                         "a paint or click handler fetches synchronously, so a slow "
                         "tile host freezes the window")
        self.assertIn("CreateThread(NULL, 0, WorkerMain", wk,
                      "the worker thread is gone; every fetch is now on the UI thread")
        self.assertIn("PostMessageW(g_hwnd, WM_APP_DATA", fc + rdc + pl,
                      "results are not handed to the UI thread by message, so a paint "
                      "can read a half-filled struct")

        # --- places persist in the app's own config.ini ---
        self.assertIn("CSIDL_LOCAL_APPDATA", pl,
                      "places no longer live under %LOCALAPPDATA%")
        self.assertIn('config.ini", g_cfgDir', pl,
                      "the place list does not live in the app's own config file")
        keep = pl[pl.index("static void RememberPlace("):]
        keep = keep[:keep.index("\n}\n")]
        self.assertIn("SavePlaces();", keep,
                      "remembering a place does not persist it, so the next launch "
                      "forgets the owner's town")
        self.assertIn("CreateMutexW(NULL, FALSE, MUTEX_NAME)",
                      (ROOT / "weather-radar" / "src" / "parts" / "entry.c").read_text(encoding="utf-8"),
                      "two copies could fight over the same config and cache")

        # --- theme-aware like the rest of the suite ---
        ut = (ROOT / "weather-radar" / "src" / "parts" / "util.c").read_text(encoding="utf-8")
        utc = code(ut)
        ip = utc[utc.index("static void InitPalette(void) {"):]
        ip = ip[:ip.index("\n}\n")]
        self.assertIn("if (AppsUseLightTheme())", ip,
                      "the window ignores the owner's Windows theme again")

    def test_weather_radar_shows_selected_place_rain_eta(self):
        """The rain card should turn its hourly, exact-point forecast into a
        location-local onset estimate, while keeping weak or fallback signals
        clearly distinguished from a majority ensemble forecast."""
        main = (ROOT / "weather-radar" / "src" / "weather_radar.c").read_text(encoding="utf-8")
        ui = (ROOT / "weather-radar" / "src" / "parts" / "ui.c").read_text(encoding="utf-8")
        readme = (ROOT / "weather-radar" / "README.md").read_text(encoding="utf-8")
        eta = ui[ui.index("static void RainEtaText("):ui.index("static void PaintHonestCard(")]
        card = ui[ui.index("static void PaintHonestCard("):ui.index("static void PaintHourlyCard(")]

        self.assertRegex(main, r"#define RAIN_ETA_LIKELY_PROB\s+0\.50")
        self.assertIn("ForecastIsRainingNow(f)", eta)
        self.assertIn("int h = start + k;", eta)
        self.assertIn("e->reported[h] <= 0 || isnan(prob)", eta,
                      "members that stopped reporting are being treated as dry for ETA")
        self.assertIn("if (prob >= RAIN_ETA_LIKELY_PROB && likelyK < 0)", eta,
                      "the ETA no longer uses the first majority-likelihood hour")
        self.assertIn("f->hLabel[k]", eta,
                      "the ETA is not displayed in the selected location's local time")
        self.assertIn("f->hP[i] >= RAIN_ALERT_TRACE_IN", eta,
                      "the single-model hourly fallback no longer uses the measurable-rain threshold")
        self.assertIn("possible peak around %s local", eta)
        self.assertIn("no measurable rain signal in the next %d forecast hours", eta)
        self.assertIn("RainEtaText(&g_fc, &g_ens, eta, 192);", card)
        self.assertIn("Point rain odds + ETA from the ensemble", ui)
        self.assertIn("Times are local to the selected place and are hourly, not minute-precise.", readme)

    def test_weather_radar_alert_uses_exact_ensemble_and_latches_one_rain_episode(self):
        """Rain alerts are only useful when they answer the point question and
        never recur while the same shower is underway. The selected-place
        worker ticket also prevents a late forecast for somewhere else from
        announcing rain at the current place."""
        main = (ROOT / "weather-radar" / "src" / "weather_radar.c").read_text(encoding="utf-8")
        alerts = (ROOT / "weather-radar" / "src" / "parts" / "alerts.c").read_text(encoding="utf-8")
        ui = (ROOT / "weather-radar" / "src" / "parts" / "ui.c").read_text(encoding="utf-8")
        workers = (ROOT / "weather-radar" / "src" / "parts" / "workers.c").read_text(encoding="utf-8")
        forecast = (ROOT / "weather-radar" / "src" / "parts" / "forecast.c").read_text(encoding="utf-8")
        entry = (ROOT / "weather-radar" / "src" / "parts" / "entry.c").read_text(encoding="utf-8")

        self.assertRegex(main, r"#define RAIN_ALERT_PROB\s+0\.66")
        self.assertRegex(main, r"#define RAIN_ALERT_HOURS\s+3")
        self.assertIn("RAIN_ALERT_COOLDOWN_SEC", main)
        self.assertIn("EnsWindowProb(&g_ens, 0, RAIN_ALERT_HOURS)", alerts,
                      "the alert stopped using exact-location ensemble odds")
        self.assertNotIn("PoPWindowMax", alerts,
                         "regional coverage-blended PoP must never trigger the alert")
        self.assertIn("ForecastIsRainingNow", alerts)
        self.assertLess(alerts.index("if (raining) {"), alerts.index("ShowRainAlert(p, odds);"),
                        "an ongoing shower can reappear as a new about-to-rain alert")
        self.assertIn("if (samePlace && g_rainAlertEpisode) return;", alerts,
                      "a qualifying episode no longer suppresses duplicate alerts")
        self.assertIn("LoadRainAlertState", alerts)
        self.assertIn("SaveRainAlertState", alerts)
        self.assertIn("Shell_NotifyIconW(NIM_MODIFY", alerts,
                      "the rain watch no longer delivers through the Windows notification area")
        self.assertIn("WeatherTrayInit();", entry)
        self.assertIn("ShowWindow(hwnd, SW_HIDE);", ui,
                      "closing Weather Radar stops the watcher instead of keeping it in the tray")
        self.assertIn("ID_TRAY_EXIT", ui,
                      "a hidden continuous watcher has no explicit exit path")

        self.assertIn("g_placeTicket", main)
        self.assertIn("FetchForecast(&p, ticket)", workers)
        self.assertIn("FetchEnsemble(&p, ticket)", workers)
        self.assertIn("ticket == g_placeTicket", ui,
                      "a late reply for a previous place can overwrite the selected forecast")
        self.assertIn("(LPARAM)(uintptr_t)ticket", forecast,
                      "the worker no longer labels a forecast result with its place generation")

    def test_weather_search_field_scales_and_submits_without_stale_results(self):
        """Weather Radar's one native child control used its creation-time 96 DPI
        rectangle while the painted UI used the final monitor DPI and fit. It
        overlapped the header on scaled screens, and Return did nothing. Search
        answers now stage through the worker so an older response cannot replace
        the query the owner just typed."""
        main = (ROOT / "weather-radar" / "src" / "weather_radar.c").read_text(encoding="utf-8")
        ui = (ROOT / "weather-radar" / "src" / "parts" / "ui.c").read_text(encoding="utf-8")
        places = (ROOT / "weather-radar" / "src" / "parts" / "places.c").read_text(encoding="utf-8")
        workers = (ROOT / "weather-radar" / "src" / "parts" / "workers.c").read_text(encoding="utf-8")

        self.assertIn("static void LayoutSearchEdit(void)", ui)
        self.assertIn("SetWindowPos(g_hSearch", ui)
        self.assertIn("LayoutSearchEdit();\n        InvalidateRect(hwnd, NULL, TRUE);", ui,
                      "the search control is not relaid out after a DPI change")
        self.assertIn("g_inputBrush = CreateSolidBrush(COL_INPUT);",
                      (ROOT / "weather-radar" / "src" / "parts" / "entry.c").read_text(encoding="utf-8"))
        self.assertIn("SetWindowSubclass(g_hSearch, SearchEditSubclass", ui)
        self.assertIn("PostMessageW(GetParent(edit), WM_APP_SEARCH, 0, 0);", ui,
                      "Return in the search box is not routed to the real submit path")
        self.assertIn("case WM_APP_SEARCH:\n        SubmitSearch();", ui)
        self.assertIn("GetWindowTextW(g_hSearch, g_query, 96);", ui,
                      "Search uses a stale EN_CHANGE copy instead of the current edit text")
        self.assertIn("g_searchStage", main)
        self.assertIn("GeocodeSearch(query, ticket);", workers)
        self.assertIn("ticket == g_searchTicket", places)
        self.assertIn("g_searchStageTicket == g_searchTicket", ui)

    def test_weather_radar_has_a_five_day_balanced_planner(self):
        """The approved planner covers five local dates, gives rain,
        temperature, cloud and wind equal selectable daily cells, and uses the
        selected cell for hourly values, a trend and a plain-language reading.
        Exact-point ensemble odds and regional PoP must remain visibly distinct;
        humidity stays supporting context in the current-conditions card."""
        main = (ROOT / "weather-radar" / "src" / "weather_radar.c").read_text(encoding="utf-8")
        forecast = (ROOT / "weather-radar" / "src" / "parts" / "forecast.c").read_text(encoding="utf-8")
        ui = (ROOT / "weather-radar" / "src" / "parts" / "ui.c").read_text(encoding="utf-8")

        self.assertRegex(main, r"#define FC_DAYS\s+5")
        self.assertRegex(main, r"#define FC_HOURS\s+\(FC_DAYS \* 24 \+ 2\)")
        self.assertIn("ForecastDay days[FC_DAYS];", main)
        self.assertIn("double  hCloud[FC_HOURS], hWindMph[FC_HOURS];", main)
        self.assertIn("int     hCode[FC_HOURS], hDay[FC_HOURS];", main)
        self.assertEqual(forecast.count("&forecast_days=5&timezone=auto"), 2,
                         "the daily and exact-point ensemble feeds must cover the same five-day horizon")
        self.assertIn('L"cloud_cover,wind_speed_10m"', forecast,
                      "hourly cloud cover and wind are missing from the planner feed")
        self.assertIn("static void DayLabelFromIso(", forecast)
        self.assertIn("static int DailyIndexForHour(", forecast)
        for needle in (
            'day->maxF = JNum(JAt(JGet(daily, "temperature_2m_max"), d));',
            'day->regionalPoP = JNum(JAt(JGet(daily, "precipitation_probability_max"), d));',
            "f->hCloud[k] = JNum(JAt(cloud, i));",
            "f->hWindMph[k] = JNum(JAt(wind, i));",
            "f->hDay[k] = (ti && ti->str)",
            "f->days[d].cloudMean = cloudSum / (double)cloudN;",
        ):
            self.assertIn(needle, forecast, "five-day planner feed lost `%s`" % needle)

        self.assertIn("L\"Rain\", L\"Temperature\", L\"Cloud cover\", L\"Wind\"", ui)
        self.assertIn("static int PlannerCellRect(", ui)
        self.assertIn("static int HitPlannerCell(", ui)
        self.assertIn("g_planCategory = planCategory;", ui,
                      "clicking a daily category no longer updates its selected detail")
        complete_window = forecast[forecast.index("static double EnsCompleteWindowProb("):forecast.index("static double PoPWindowMax(")]
        self.assertIn("EnsCompleteWindowProb(e, start, day->hourCount)", ui,
                      "daily exact-point rain odds do not count members with any measurable rain")
        self.assertIn("if (isnan(v)) { complete = 0; break; }", complete_window,
                      "a member that stops reporting mid-day is counted as dry for unreported hours")
        self.assertIn("if (!complete) continue;", complete_window,
                      "the daily ensemble denominator includes incomplete member-days")
        self.assertIn('_snwprintf(pointText, 23, L"P %d%%"', ui,
                      "the daily point-ensemble rain odds lost their explicit label")
        self.assertIn('_snwprintf(regionalText, 23, L"R %d%%"', ui,
                      "the daily regional PoP lost its separate label")
        self.assertIn("DrawTextAt(dc, pointText, &pointRect, g_fontSmall, COL_RAIN", ui)
        self.assertIn("DrawTextAt(dc, regionalText, &regionalRect, g_fontSmall, COL_WARN", ui)
        self.assertIn("P = point ensemble (blue)  ·  R = regional PoP (gold)", ui)
        self.assertIn("Humidity %d%%", ui,
                      "humidity was promoted over the four approved daily categories")
        paint = ui[ui.index("static void PaintAll(HWND hwnd)"):ui.index("static void StatusSet", ui.index("static void PaintAll(HWND hwnd)"))]
        self.assertLess(paint.index("PaintHonestCard(mem, &honestCard);"),
                        paint.index("PaintPlannerCard(mem, &plannerCard);"),
                        "the exact-point rain card is no longer kept ahead of the planner")

    def test_weather_radar_layout_keeps_text_controls_and_tiles_separate(self):
        """The owner's screenshot caught three visual defects static data tests
        cannot see: hourly labels sharing the legend strip, opaque radar paint
        covering its controls, 256px tiles written straight into a 768px grid
        so the map appeared as broken horizontal bands, and the promised
        suite mark existing as a dead/nonexistent Weather Radar header."""
        ui = (ROOT / "weather-radar" / "src" / "parts" / "ui.c").read_text(encoding="utf-8")
        radar = (ROOT / "weather-radar" / "src" / "parts" / "radar.c").read_text(encoding="utf-8")
        workers = (ROOT / "weather-radar" / "src" / "parts" / "workers.c").read_text(encoding="utf-8")
        forecast = (ROOT / "weather-radar" / "src" / "parts" / "forecast.c").read_text(encoding="utf-8")
        self.assertIn("wind_gusts_10m,cloud_cover", forecast,
                      "the selected balanced current-conditions view stopped requesting gust and cloud context")
        self.assertIn('f->windGustMph = JNum(JGet(cur, "wind_gusts_10m"));', forecast)
        self.assertIn('f->cloudCover = JNum(JGet(cur, "cloud_cover"));', forecast)
        self.assertIn("#define WIN_LH 960", ui,
                      "the decision view regressed to the short canvas that made radar unreadable")
        self.assertIn("#define RADAR_FOCUS_PX 512", ui,
                      "the legible local-radar focus crop disappeared")
        self.assertIn("static int SavedHeadingY(void)", ui)
        self.assertIn("if (wp != SIZE_MINIMIZED) LayoutSearchEdit();", ui,
                      "the native search field is not corrected after the window receives its final size")
        planner = ui[ui.index("static void PaintPlannerCard("):ui.index("static void PaintRadarPane", ui.index("static void PaintPlannerCard("))]
        detail = ui[ui.index("static void PaintHourlyCard("):ui.index("static void PaintPlannerCard(")]
        self.assertIn("Five-day outlook · select a value to explore its hourly trend", planner,
                      "the day-first planner lost its selection affordance")
        self.assertIn("P = point ensemble (blue)  ·  R = regional PoP (gold)", planner,
                      "the point and regional rain metrics are not independently labelled")
        self.assertIn("for (int day = 0; day < FC_DAYS; day++)", planner,
                      "the planner no longer presents one row for each local date")
        self.assertIn("static int PlannerRowRect(int day, RECT *r)", ui)
        self.assertIn("int top = table.top + S(20 + day * 29);", ui,
                      "planner rows are not organized by date")
        self.assertIn("PlannerCellValue(mem, day, category, &cell, selected);", planner,
                      "daily category values are no longer shown in the day rows")
        self.assertNotIn("DrawRoundedBox(mem, &cell", planner,
                         "the dense per-value box matrix has returned")
        self.assertIn("PaintHourlyCard(mem, &detail, g_planDay, g_planCategory);", planner,
                      "the selected day/category no longer drives the hourly detail")
        self.assertNotIn("PaintPlannerSamples", ui,
                         "the old cramped five-sample strip has returned")
        self.assertIn("RECT plot = { card->left + S(46), card->top + S(29),", detail,
                      "the selected hourly plot is too short to read")
        self.assertIn("card->bottom - S(66)", detail,
                      "the hourly chart no longer reserves room for axes and a reading")
        self.assertIn("PlannerAxisText(category, ticks[i], axis, 24);", detail,
                      "the chart has no readable value scale")
        self.assertIn("g_fc.hLabel[hour]", detail,
                      "the chart has no local-time labels")
        self.assertIn("PaintPlannerSeries(mem, day, PLAN_RAIN, 0", detail,
                      "the rain trend no longer plots exact-point ensemble odds")
        self.assertIn("PaintPlannerSeries(mem, day, PLAN_RAIN, 1", detail,
                      "the rain trend no longer separately plots regional coverage")
        self.assertIn("PlannerReading(day, category, reading, 224);", detail,
                      "the selected category lost its plain-English reading")
        self.assertNotIn("RECT leg", detail,
                         "the hourly detail introduced overlapping text lanes")
        paint = ui[ui.index("static void PaintAll(HWND hwnd)"):ui.index("static void StatusSet", ui.index("static void PaintAll(HWND hwnd)"))]
        self.assertLess(paint.index("PaintRadarPane(mem, &radarCard);"),
                        paint.index("DrawBtn(mem, BTN_PREV"),
                        "the radar card paints over its own playback controls")
        pane = ui[ui.index("static void PaintRadarPane("):ui.index("static void PaintAll", ui.index("static void PaintRadarPane("))]
        self.assertIn("RADAR_FOCUS_PX", pane,
                      "the enlarged radar pane is no longer drawn from the readable local focus")
        self.assertIn("static int RadarFocusOrigin(double pointPx, int focusPx)", ui,
                      "the selected place no longer determines the local crop origin")
        self.assertIn("int origin = (int)llround(pointPx - (double)focusPx / 2.0);", ui,
                      "the selected coordinate is not centered in the zoom crop")
        self.assertIn("int maxOrigin = GRID_PX - focusPx;", ui,
                      "the local crop can extend beyond the fetched three-tile mosaic")
        self.assertIn("PlacePixelInGrid(focusZoom, pl.lat, pl.lon, focusTx, focusTy,", pane,
                      "the crop anchor no longer uses the exact selected place pixel")
        self.assertIn("srcLeft = RadarFocusOrigin(placePixelX, focusPx);", pane)
        self.assertIn("srcTop = RadarFocusOrigin(placePixelY, focusPx);", pane)
        self.assertIn("placePixelX - srcLeft", pane,
                      "the location marker is no longer aligned to the place-centered crop")
        self.assertIn("RadarViewBuild(&p, fi, ticket);", workers,
                      "tile work does not carry the selected-place generation into view construction")
        self.assertIn("if (placeTicket != g_placeTicket)", radar,
                      "a slow tile response can publish an old place's imagery over the current selection")
        self.assertIn("g_viewTicket = placeTicket;", radar,
                      "published radar imagery does not identify the place generation it represents")
        self.assertIn("viewTicket == placeTicket", pane,
                      "the UI can paint a prior city's image under the current city's marker")
        self.assertIn("int sourceZoom = g_viewKey[0];", pane,
                      "the crop no longer uses the source zoom paired with the published view")
        self.assertIn("int sourceTx = g_viewKey[1];", pane,
                      "the crop no longer uses the source tile origin paired with the published view")
        self.assertIn("if (!viewPainted)", pane,
                      "stale radar imagery is not replaced with a loading state")
        paint_lock = pane.index("EnterCriticalSection(&g_lock);")
        view_snapshot = pane.index("uint32_t *view = g_view;")
        view_blit = pane.index("StretchDIBits(mem, d.left, d.top, side, side")
        paint_unlock = pane.index("LeaveCriticalSection(&g_lock);", paint_lock)
        self.assertLess(paint_lock, view_snapshot)
        self.assertLess(view_snapshot, view_blit)
        self.assertLess(view_blit, paint_unlock,
                        "the worker can recycle the published radar buffer while GDI is reading it")
        self.assertIn("focusPx, focusPx, view, &bi", pane,
                      "the blit no longer uses the view pointer whose snapshot remains locked")
        self.assertIn("light     moderate     heavy     intense", pane,
                      "radar colours no longer have a visible intensity legend")
        self.assertIn("cx - S(14)", pane,
                      "the location marker is too small to find over precipitation")
        emblem = (ROOT / "weather-radar" / "src" / "parts" / "emblem.c").read_text(encoding="utf-8")
        engine = (ROOT / "shared" / "win32" / "suite_emblem.c").read_text(encoding="utf-8")
        entry = (ROOT / "weather-radar" / "src" / "parts" / "entry.c").read_text(encoding="utf-8")
        main = (ROOT / "weather-radar" / "src" / "weather_radar.c").read_text(encoding="utf-8")
        self.assertIn('#include "../../shared/win32/suite_emblem.c"', main,
                      "Weather Radar bypasses the canonical suite-emblem engine")
        self.assertIn("SetTimer(hwnd, TIMER_EMBLEM, 50, NULL);", entry,
                      "the Weather Radar emblem never receives animation ticks")
        self.assertIn("DrawWeatherRadarEmblem", ui,
                      "the Weather Radar header paints a static or missing mark")
        self.assertIn("g_radarSpinAngle += 6.283185307179586 / 600.0;", ui,
                      "the Weather Radar header mark no longer turns")
        self.assertIn("SuiteEmblemResolvePalette(COL_BG, COL_ACCENT, COL_TXT", emblem,
                      "the Weather Radar adapter ignores the active light/dark palette")
        self.assertIn("SuiteEmblemDibPixel(COL_BG)", ui,
                      "the narrow Weather Radar repaint writes a raw COLORREF into its DIB")
        self.assertIn("SuiteEmblemDraw", engine,
                      "the shared engine lost the transparent Tri-Spoke draw path")
        self.assertIn("uint32_t tile[TILE_PX * TILE_PX];", radar)
        self.assertIn("for (int row = 0; row < TILE_PX; row++)", radar)
        self.assertIn("memcpy(dstRow, tile + (size_t)row * TILE_PX", radar,
                      "a 256px tile is copied contiguously into the 768px map and wraps into stripes")

    def test_suite_header_emblem_has_one_dib_safe_engine(self):
        """All native spinning headers must share one renderer.  Four copied SDF
        loops had already drifted into an opaque brown/gold box in three apps;
        this contract leaves adapters only and makes raw COLORREF DIB fills a
        detectable regression rather than a screenshot surprise."""
        engine_path = ROOT / "shared" / "win32" / "suite_emblem.c"
        self.assertTrue(engine_path.exists(), "the reusable native emblem engine is missing")
        engine = engine_path.read_text(encoding="utf-8")
        for needle in (
            "typedef struct SuiteEmblemEngine",
            "SuiteEmblemDibPixel",
            "SuiteEmblemResolvePalette",
            "SuiteEmblemAngleNow",
            "SuiteEmblemRelease",
            "SuiteEmblemDraw",
            "straight-alpha AARRGGBB DIB pixels",
        ):
            self.assertIn(needle, engine, "shared emblem engine lost `%s`" % needle)

        consumers = (
            ("project-hub/src/project_hub.c", "project-hub/src/parts/emblem.c", "DrawWayfinderEmblem"),
            ("agent-notes/src/agent_notes.c", "agent-notes/src/parts/emblem.c", "DrawAgentNotesEmblem"),
            ("audio-switcher/src/audio_switcher.c", "audio-switcher/src/parts/control_theming.c", "DrawAudioSwitcherEmblem"),
            ("weather-radar/src/weather_radar.c", "weather-radar/src/parts/emblem.c", "DrawWeatherRadarEmblem"),
        )
        for main_rel, adapter_rel, draw_name in consumers:
            main = (ROOT / main_rel).read_text(encoding="utf-8")
            adapter = (ROOT / adapter_rel).read_text(encoding="utf-8")
            self.assertIn('#include "../../shared/win32/suite_emblem.c"', main,
                          "%s stopped including the canonical engine" % main_rel)
            self.assertIn("SuiteEmblemDraw", adapter,
                          "%s reimplemented or bypassed the canonical draw path" % adapter_rel)
            self.assertIn(draw_name, adapter,
                          "%s has no app-local placement adapter" % adapter_rel)
            for retired in ("EmbRasterize", "RadarEmbRasterize", "EmbCache", "RadarEmbCache",
                            "EMB_SDF_SCALE", "EMB_SDF_BOOST"):
                self.assertNotIn(retired, adapter,
                                 "%s restored a divergent `%s` renderer" % (adapter_rel, retired))

        for paint_rel in (
            "project-hub/src/parts/ui_renderer.c",
            "agent-notes/src/parts/ui.c",
            "audio-switcher/src/parts/control_theming.c",
            "weather-radar/src/parts/ui.c",
        ):
            paint = (ROOT / paint_rel).read_text(encoding="utf-8")
            self.assertIn("SuiteEmblemDibPixel(COL_BG)", paint,
                          "%s can write a raw COLORREF background into an emblem DIB" % paint_rel)
        self.assertNotIn("bits[i] = COL_BG", (ROOT / "agent-notes/src/parts/ui.c").read_text(encoding="utf-8"))
        self.assertNotIn("spinBits[i] = COL_BG", (ROOT / "weather-radar/src/parts/ui.c").read_text(encoding="utf-8"))

        builder = (ROOT / "tools" / "build_windows.py").read_text(encoding="utf-8")
        self.assertIn("TARGET_SHARED_SOURCES", builder,
                      "shared emblem edits can reuse stale native EXEs")
        for target in ("audioswitcher", "hub", "agentnotes", "weatherradar"):
            self.assertIn('"%s"' % target, builder,
                          "the %s cache key omits the shared engine" % target)

    def test_dynamic_catalog_cards_hide_update_when_their_build_is_current(self):
        """Weather Radar is the first non-built-in hub card. It kept its Update
        button after a successful install because the generic status sweep only
        ever made that button amber; unlike the built-ins it never hid it again."""
        src = (ROOT / "project-hub" / "src" / "parts" / "refresh_statuses.c").read_text(encoding="utf-8")
        block = src[src.index("/* Catalogue extras"):src.index("if (g_hwnd) InvalidateRect", src.index("/* Catalogue extras"))]
        self.assertIn("BOOL showAction = !has || (known && upd);", block)
        self.assertIn("c->btns[action].hidden = !showAction;", block,
                      "a current dynamic app still renders a clickable Update button")
        self.assertIn("c->btns[action].label = !has ? L\"Install\" : L\"Update\";", block)
        self.assertIn("if (open >= 0) c->btns[open].hidden = !has;", block)
        self.assertIn("RemoteDiffers(c->remSha, exePath, c->remSize)", block,
                      "dynamic cards must use exact content identity before claiming an update")

    def test_hud_and_orca_cards_expose_update_folder_remove_disable(self):
        """Both cards were redesigned to match the other app cards: Update (shown
        only when an update is available), Folder, Remove, Disable -- dropping the
        HUD 'Mods Dir' picker and the Orca 'Branch' picker. Orca gained a real
        Remove; before this it only had a Disable that did nothing."""
        catalog = json.loads((ROOT / "suite-apps.json").read_text(encoding="utf-8"))
        apps = {a["id"]: a for a in catalog["apps"]}
        for app_id, update_verb in (("hud", "updatemod"), ("orca", "update")):
            slots = apps[app_id]["buttons"].split(",")
            verbs = [s.split(":")[0].strip() for s in slots]
            labels = [s.split(":")[1].strip() for s in slots]
            self.assertEqual(verbs, [update_verb, "folder", "remove", "disable"],
                             f"{app_id} buttons must be Update, Folder, Remove, Disable in that order")
            self.assertEqual(labels, ["Update", "Folder", "Remove", "Disable"],
                             f"{app_id} button labels changed")
        # the dropped pickers must not creep back in
        self.assertNotIn("modsdir", apps["hud"]["buttons"])
        self.assertNotIn("branch", apps["orca"]["buttons"])

    def test_every_catalog_verb_resolves_to_a_real_id_for_its_own_slot(self):
        """The runtime-correctness check the catalog test misses: prove each
        catalog verb resolves to a WM_COMMAND id *inside its own app's off[]
        window*, not just somewhere in the map. Orca's brand-new Remove is exactly
        the entry that could land in the wrong window and vanish."""
        hub_c = read_c_inlined("project-hub/src/project_hub.c")
        known, defines, btn_id = self._parse_builtin_btnid(hub_c)
        catalog = json.loads((ROOT / "suite-apps.json").read_text(encoding="utf-8"))
        for app in catalog["apps"]:
            app_id = app["id"]
            if app_id not in known:
                continue
            slot = known.index(app_id)
            for s in app["buttons"].split(","):
                verb = s.split(":")[0].strip()
                self.assertNotEqual(
                    btn_id(slot, verb), 0,
                    f"{app_id} verb `{verb}` does not resolve inside slot {slot}'s off[] "
                    f"window - FillBuiltInDef would drop this button silently")
        # and the two redesigned cards resolve to the specific ids expected
        self.assertEqual(btn_id(known.index("orca"), "remove"), defines["ID_ORCA_REMOVE"])
        self.assertEqual(btn_id(known.index("orca"), "disable"), defines["ID_ORCA_DISABLE"])
        self.assertEqual(btn_id(known.index("hud"), "remove"), defines["ID_HUD_REMOVE"])

    def test_orca_remove_handler_deletes_only_the_selected_plugins_files(self):
        """Shared removal enumerates all host profiles but is keyed to one
        package/filename. Runtime mocks below verify sibling/settings retention."""
        hub_c = read_c_inlined("project-hub/src/project_hub.c")
        start = hub_c.index("static void RemoveOrcaPlugin(")
        body = hub_c[start:hub_c.index("static BOOL HandleOrcaPluginCommand", start)]
        self.assertIn("&g_orcaPlugins[plugin]", body)
        self.assertIn("FindOrcaProfiles(&data)", body)
        self.assertIn("spec->file", body)
        self.assertIn("OrcaLegacyFile(spec", body)
        self.assertIn("DeleteFileW", body)
        self.assertNotIn("wave-overhangs", body)
        self.assertNotIn("unlayered-infill", body)
        self.assertNotIn("SHFileOperation", body, "removal must not recursively delete user settings")
        self.assertIn("RemoveOrcaPlugin(plugin); RefreshProjectStatuses();", hub_c)

    def test_orca_disable_is_migrated_persisted_independently_and_gates_installs(self):
        hub_c = read_c_inlined("project-hub/src/project_hub.c")
        self.assertRegex(hub_c, r'GetPrivateProfileStringW\(L"Apps",\s*L"OrcaDisabled"')
        for key in ("WaveOverhangsDisabled", "UnlayeredInfillDisabled"):
            self.assertRegex(hub_c, rf'GetPrivateProfileStringW\(L"Apps",\s*L"{key}", legacy')
            self.assertRegex(hub_c, rf'WritePrivateProfileStringW\(L"Apps",\s*L"{key}"')
        self.assertNotRegex(hub_c, r'WritePrivateProfileStringW\(L"Apps",\s*L"OrcaDisabled"')
        start = hub_c.index("static void WorkerInstallOrcaPlugin(")
        body = hub_c[start:hub_c.index("static void RemoveOrcaPlugin", start)]
        self.assertLess(body.index("if (OrcaPluginDisabled(plugin))"), body.index("DownloadRawRepoFile"))
        self.assertIn("EnsureProbeRef(branch, 64);", body)

    def test_update_button_is_hidden_unless_an_update_is_needed(self):
        """btns[0] on every card is the dynamic Update/Install action: Install
        (PRIMARY) when the app is missing, Update (AMBER) when a newer build is
        known, hidden once up to date. Open moves to btns[1] and is hidden while
        the app is missing. HUD/Orca mirror the same rule."""
        hub_c = read_c_inlined("project-hub/src/project_hub.c")
        # every built-in card labels btns[0] "Install" in its not-installed branch
        self.assertEqual(hub_c.count('.btns[0].label = L"Install"'), 6,
                         "six dedicated status branches plus the shared two-plugin helper")
        # HUD up-to-date branch hides btns[0] (label reset to Update)
        self.assertRegex(hub_c, r'BADGE_OK;\s*g_cards\[2\]\.btns\[0\]\.label = L"Update";'
                                r'\s*g_cards\[2\]\.btns\[0\]\.style = BTN_SECONDARY;'
                                r'\s*g_cards\[2\]\.btns\[0\]\.hidden = TRUE;')
        # HUD disabled branch also hides btns[0]
        self.assertRegex(hub_c, r'BADGE_IDLE;\s*g_cards\[2\]\.btns\[0\]\.style = BTN_SECONDARY;'
                                r'\s*g_cards\[2\]\.btns\[0\]\.hidden = TRUE;')
        # not-installed + newer-build branches reveal it (exactly two)
        self.assertEqual(hub_c.count("g_cards[2].btns[0].hidden = FALSE"), 2,
                         "HUD should reveal Update/Install in exactly the not-installed and newer-build branches")
        # Both Orca cards use the same per-plugin visibility rule, executed
        # against independent current/legacy profile fixtures below.
        self.assertIn('c->btns[0].label = status.files ? L"Update" : L"Install";', hub_c)
        self.assertIn('c->btns[0].hidden = disabled || (status.files && (!known || !status.needsUpdate));', hub_c)
        # launcher cards hide Open (btns[1]) while the app is missing
        self.assertEqual(hub_c.count("/* nothing to open until installed */"), 4,
                         "BTG/DNS/Notes/Audio hide Open when the app is missing")
        self.assertIn("g_cards[1].btns[1].hidden = !hasKbl;", hub_c)

    def test_every_card_puts_the_update_action_first(self):
        """Task: Update/Install is the first button on every card, then Open,
        Folder, and finally the card-specific extra (Stop / Unfreeze). Both the
        catalog (suite-apps.json) and the built-in C initializers must agree, or
        the drawn order and the click handler ids drift apart."""
        import json
        cfg = json.loads((ROOT / "suite-apps.json").read_text(encoding="utf-8"))
        apps = cfg["apps"] if isinstance(cfg, dict) and "apps" in cfg else cfg
        by_id = {a["id"]: a["buttons"] for a in apps}
        expected = {
            "btguard":      "update:Update:74,open:Open:68,folder:Folder:68,stop:Stop:62",
            "kbl":          "update:Update:74,open:Open:68,folder:Folder:68,unfreeze:Unfreeze:84",
            "hud":          "updatemod:Update:74,folder:Folder:68,remove:Remove:74,disable:Disable:78",
            "orca":         "update:Update:74,folder:Folder:68,remove:Remove:74,disable:Disable:78",
            "dnspilot":     "update:Update:74,open:Open:68,folder:Folder:68",
            "agentnotes":   "update:Update:74,open:Open:68,folder:Folder:68",
            "audioswitcher":"update:Update:74,open:Open:68,folder:Folder:68",
        }
        for app_id, want in expected.items():
            self.assertEqual(by_id.get(app_id), want,
                             f"{app_id} button order regressed (Update must come first)")

        hub_c = read_c_inlined("project-hub/src/project_hub.c")
        for card in (0, 1, 2, 4, 5, 6):
            self.assertRegex(
                hub_c,
                r'g_cards\[%d\]\.btns\[0\]\s*=\s*\(ActionBtn\)\s*\{\s*ID_[A-Z_0-9]+,\s*L"Update"' % card,
                f"built-in card {card} does not initialize btns[0] as the Update action")

    def test_hub_restores_every_pool_backed_card_field_before_a_rebuild(self):
        """RebuildExtraCards resets g_strPoolUsed, which invalidates EVERY
        pool-backed pointer on the built-in cards. Titles alone used to be
        restored (and only 6 of 8), so a card absent from the catalog came back
        pointing at recycled pool memory and drew another app's text."""
        hub_c = read_c_inlined("project-hub/src/project_hub.c")

        self.assertIn("RestoreBuiltInCardText", hub_c,
                      "the pool-backed card fields are not restored before the "
                      "string pool is reset")
        rebuild = hub_c[hub_c.index("static void RebuildExtraCards"):]
        rebuild = rebuild[:rebuild.index("\nstatic ")]
        restore_at = rebuild.index("RestoreBuiltInCardText();")
        reset_at = rebuild.index("g_strPoolUsed = 0;")
        self.assertLess(restore_at, reset_at,
                        "RestoreBuiltInCardText() must run BEFORE g_strPoolUsed = 0, "
                        "otherwise the literals are written and then invalidated")

        body = hub_c[hub_c.index("static void RestoreBuiltInCardText"):]
        body = body[:body.index("\nstatic ")]
        for field in ("title", "blurb", "exeRepo", "exeName", "shortcutName",
                      "launchArgs"):
            self.assertIn(f"c->{field}", body,
                          f"RestoreBuiltInCardText does not reset `{field}`, which is "
                          f"a pool-backed pointer left dangling across a rebuild")
        for field in ("label", "verb"):
            self.assertIn(f".{field}", body,
                          f"RestoreBuiltInCardText does not reset button `{field}`, "
                          f"which DrawActionBtn hands straight to DrawTextW")
        self.assertRegex(body, r"for \(int i = 0; i < BUILTIN_CARD_COUNT; i\+\+\)",
                         "RestoreBuiltInCardText must cover every built-in slot")
        self.assertIn("InitOrcaCard(0);", body)
        self.assertIn("InitOrcaCard(1);", body)

    def test_hub_version_constants_are_paired_and_well_formed(self):
        """AGENTS.md hard rule 3.4: HUB_VERSION and HUB_VERSION_W are two
        separate lines that must be bumped TOGETHER whenever a new
        ProjectHub.exe ships. HUB_VERSION drives the numeric VerNewer update
        check and is what a self-update reads out of the remote source;
        HUB_VERSION_W is what the header draws. A pair that disagrees makes the
        hub advertise one version while checking for updates against another.
        Nothing enforced this before."""
        hub_c = read_c_inlined("project-hub/src/project_hub.c")
        narrow = re.findall(r'(?m)^#define HUB_VERSION\s+"([^"]+)"\s*$', hub_c)
        wide = re.findall(r'(?m)^#define HUB_VERSION_W\s+L"([^"]+)"\s*$', hub_c)
        self.assertEqual(len(narrow), 1,
                         f"expected exactly one `#define HUB_VERSION \"x.y.z\"`, got {narrow}")
        self.assertEqual(len(wide), 1,
                         f"expected exactly one `#define HUB_VERSION_W L\"x.y.z\"`, got {wide}")
        self.assertEqual(narrow[0], wide[0],
                         f"HUB_VERSION ({narrow[0]}) and HUB_VERSION_W ({wide[0]}) "
                         f"disagree - bump both together (AGENTS.md hard rule 3.4)")
        self.assertRegex(narrow[0], r'^\d+\.\d+\.\d+$',
                         f"HUB_VERSION {narrow[0]!r} is not x.y.z, so VerNewer's "
                         f"numeric compare cannot order it")

    def test_setup_bat_files_avoid_cmd_parenthesized_if_blocks(self):
        for rel in (
            "project-hub/Setup.bat",
            "bluetooth-guard/Setup.bat",
            "keyboard-lighting/Setup.bat",
            "keyboard-lighting/Restart.bat",
            "keyboard-lighting/Zones.bat",
        ):
            txt = (ROOT / rel).read_text(encoding="ascii")
            for idx, line in enumerate(txt.splitlines(), 1):
                self.assertFalse(
                    line.rstrip().endswith("("),
                    f"{rel}:{idx} uses a multiline cmd '(' block instead of flat goto labels",
                )


    # ------------------------------------------------------------------
    # AudioSwitcher: hand-declared Core Audio COM interfaces
    # ------------------------------------------------------------------

    # The real Windows Core Audio vtable orders, IUnknown (slots 0-2) first.
    # AudioSwitcher builds against mingw headers that do not ship these
    # interfaces, so it declares them by hand -- and COM dispatches by slot
    # POSITION, never by name.
    AUDIO_COM_VTABLES = {
        "IMMDeviceEnumerator": [
            "QueryInterface", "AddRef", "Release",
            "EnumAudioEndpoints", "GetDefaultAudioEndpoint", "GetDevice",
            "RegisterEndpointNotificationCallback",
            "UnregisterEndpointNotificationCallback",
        ],
        "IMMDevice": [
            "QueryInterface", "AddRef", "Release",
            "Activate", "OpenPropertyStore", "GetId", "GetState",
        ],
        "IMMDeviceCollection": [
            "QueryInterface", "AddRef", "Release", "GetCount", "Item",
        ],
        "IAudioEndpointVolume": [
            "QueryInterface", "AddRef", "Release",
            "RegisterControlChangeNotify", "UnregisterControlChangeNotify",
            "GetChannelCount", "SetMasterVolumeLevel",
            "SetMasterVolumeLevelScalar", "GetMasterVolumeLevel",
            "GetMasterVolumeLevelScalar", "SetChannelVolumeLevel",
            "SetChannelVolumeLevelScalar", "GetChannelVolumeLevel",
            "GetChannelVolumeLevelScalar", "SetMute", "GetMute",
        ],
        "IAudioSessionManager2": [
            "QueryInterface", "AddRef", "Release",
            "GetAudioSessionControl", "GetSimpleAudioVolume",
            "GetSessionEnumerator", "RegisterSessionNotification",
            "UnregisterSessionNotification", "RegisterDuckNotification",
            "UnregisterDuckNotification",
        ],
        "IAudioSessionEnumerator": [
            "QueryInterface", "AddRef", "Release", "GetCount", "GetSession",
        ],
        "IAudioSessionControl": [
            "QueryInterface", "AddRef", "Release",
            "GetState", "GetDisplayName", "SetDisplayName", "GetIconPath",
            "SetIconPath", "GetGroupingParam", "SetGroupingParam",
            "RegisterAudioSessionNotification",
            "UnregisterAudioSessionNotification",
        ],
        "IAudioSessionControl2": [
            "QueryInterface", "AddRef", "Release",
            "GetState", "GetDisplayName", "SetDisplayName", "GetIconPath",
            "SetIconPath", "GetGroupingParam", "SetGroupingParam",
            "RegisterAudioSessionNotification",
            "UnregisterAudioSessionNotification",
            "GetSessionIdentifier", "GetSessionInstanceIdentifier",
            "GetProcessId", "IsSystemSoundsSession", "SetDuckingPreference",
        ],
        "ISimpleAudioVolume": [
            "QueryInterface", "AddRef", "Release",
            "SetMasterVolume", "GetMasterVolume", "SetMute", "GetMute",
        ],
        "IPolicyConfig": [
            "QueryInterface", "AddRef", "Release",
            "GetMixFormat", "GetDeviceFormat", "ResetDeviceFormat",
            "SetDeviceFormat", "GetProcessingPeriod", "SetProcessingPeriod",
            "GetShareMode", "SetShareMode", "GetPropertyValue",
            "SetPropertyValue", "SetDefaultEndpoint", "SetEndpointVisibility",
        ],
    }

    @staticmethod
    def _audio_switcher_src() -> str:
        return read_c_inlined("audio-switcher/src/audio_switcher.c")

    @classmethod
    def _declared_vtable(cls, src: str, name: str):
        """Method names in declared order for `typedef struct <name>Vtbl {...}`."""
        m = re.search(r"typedef struct " + name + r"Vtbl \{(.*?)\} " + name + r"Vtbl;",
                      src, re.S)
        if not m:
            return None
        # Matches "(__stdcall *MethodName)(" but not parameters such as
        # "void** ppvObject)" or "const GUID* pguidEventContext)", which always
        # have a space before the name.
        return re.findall(r"\*([A-Za-z_][A-Za-z0-9_]*)\)\s*\(", m.group(1))

    def test_audio_switcher_com_vtables_match_windows_slot_order(self):
        """Four of AudioSwitcher's seven hand-written COM vtables were misaligned,
        which broke the app completely while still compiling and linking:

          IMMDevice            GetId at 3, Activate at 4 -- really Activate at 3,
                               OpenPropertyStore at 4, GetId at 5. So every
                               Activate call invoked OpenPropertyStore (no device
                               ids, no friendly names, no endpoint volume, no
                               session manager) and every GetId call invoked
                               Activate.
          IAudioEndpointVolume SetMasterVolumeLevelScalar at 3 -- really
                               RegisterControlChangeNotify, so a master volume
                               slider drag passed a float where a callback
                               pointer belongs.
          IAudioSessionManager2 GetSessionEnumerator at 3 -- really
                               GetAudioSessionControl, so no app was ever listed.
          IAudioSessionControl2 GetProcessId at 3 -- really GetState, so each
                               "PID" was the session state (0/1/2), OpenProcess
                               failed, and every app row came up blank.

        The C source also carries _Static_assert()s that fail the build. This is
        the readable, build-free half of the same guard.
        """
        src = self._audio_switcher_src()

        for name, real in self.AUDIO_COM_VTABLES.items():
            declared = self._declared_vtable(src, name)
            self.assertIsNotNone(declared, f"{name}Vtbl is no longer declared in audio_switcher.c")
            self.assertLessEqual(
                len(declared), len(real),
                f"{name}Vtbl declares {len(declared)} methods but Windows only has "
                f"{len(real)}; extra entries shift every later slot")
            for slot, method in enumerate(declared):
                self.assertEqual(
                    method, real[slot],
                    f"{name}::{method} is declared at slot {slot}, but Windows puts "
                    f"{real[slot]} there. COM dispatches by position, so calling "
                    f"{method} would really invoke {real[slot]} with the wrong "
                    f"arguments.")

    def test_audio_switcher_static_asserts_agree_with_windows_slot_order(self):
        """The C _Static_assert()s are the build-time half of the vtable guard.
        Cross-check every one of them against the independent table above, so a
        wrong assertion cannot be used to "fix" a build error caused by a right
        one."""
        src = self._audio_switcher_src()
        asserts = re.findall(
            r"_Static_assert\(\s*VTBL_SLOT\(\s*(\w+)\s*,\s*(\w+)\s*\)\s*==\s*(\d+)", src)
        self.assertGreaterEqual(
            len(asserts), 20,
            "the compile-time vtable slot assertions have largely disappeared")

        for vtbl, method, slot in asserts:
            name = vtbl[:-4] if vtbl.endswith("Vtbl") else vtbl
            real = self.AUDIO_COM_VTABLES.get(name)
            self.assertIsNotNone(real, f"_Static_assert names unknown vtable {vtbl}")
            self.assertLess(int(slot), len(real),
                            f"_Static_assert puts {name}::{method} at slot {slot}, "
                            f"beyond the end of the real {len(real)}-slot vtable")
            self.assertEqual(
                real[int(slot)], method,
                f"_Static_assert claims {name} slot {slot} is {method}, but Windows "
                f"puts {real[int(slot)]} there")

            declared = self._declared_vtable(src, name)
            self.assertIsNotNone(declared, f"{name}Vtbl is not declared")
            self.assertLess(int(slot), len(declared),
                            f"_Static_assert references slot {slot} of {name}, which "
                            f"declares only {len(declared)} methods")
            self.assertEqual(
                declared[int(slot)], method,
                f"_Static_assert claims {name} slot {slot} is {method}, but the "
                f"declaration puts {declared[int(slot)]} there")

    def test_audio_switcher_switches_the_default_device_for_real(self):
        """The "Set Default" button used to do this and nothing else:

            g_default_endpoint_idx = idx;
            g_endpoints[i].is_default = (i == idx);
            refresh_ui();

        -- mutate its own variables and repaint. The badge said "Default" while
        Windows kept routing audio to the previous device. There is no documented
        API for this, so real switching has to go through IPolicyConfig.
        """
        src = self._audio_switcher_src()

        m = re.search(r"if \(id >= 3000 && id < 3100\) \{(.*?)\n        \}", src, re.S)
        self.assertIsNotNone(m, "the 3000-range WM_COMMAND handler (Set Default) is gone")
        handler = m.group(1)
        self.assertIn(
            "endpoint_make_default", handler,
            "the Set Default handler never calls endpoint_make_default, so it can "
            "only repaint the button without switching any audio")
        self.assertNotIn(
            "g_default_endpoint_idx = idx;", handler,
            "the handler assigns g_default_endpoint_idx directly instead of "
            "re-reading the default device from Windows after a switch")

        start = src.index("static HRESULT endpoint_make_default")
        fn = src[start:src.index("\n}\n", start)]
        self.assertIn("SetDefaultEndpoint", fn,
                      "endpoint_make_default does not call IPolicyConfig::SetDefaultEndpoint")
        for role in ("eConsole", "eMultimedia", "eCommunications"):
            self.assertIn(
                role, fn,
                f"switching never sets the {role} role, so apps pinned to that role "
                f"stay on the old device and the switch reads as half-working")
        self.assertIn(
            "g_last_switch_hr", fn,
            "endpoint_make_default does not record its HRESULT, so a refused switch "
            "would be indistinguishable from a successful one in the UI")
        self.assertIn(
            "policy_activate", fn,
            "endpoint_make_default does not obtain IPolicyConfig before using it")

    def test_audio_switcher_releases_com_references_between_refreshes(self):
        """endpoints_refresh() and sessions_refresh() overwrite their global
        arrays on every call, and sessions_refresh runs on a two-second timer.
        Without releasing the previous COM references first, that leaks one
        ISimpleAudioVolume per app per poll for as long as the window is open.
        """
        src = self._audio_switcher_src()

        for fn_name, release_of in (
            ("endpoints_refresh", "g_endpoints[i].device"),
            ("sessions_refresh", "g_sessions[i].svol"),
        ):
            start = src.index("static HRESULT " + fn_name)
            body = src[start:src.index("\n}\n", start)]

            count_var = "g_endpoint_count = 0;" if "endpoint" in fn_name else "g_session_count = 0;"
            reset_at = body.find(count_var)
            self.assertNotEqual(
                reset_at, -1,
                f"{fn_name} no longer resets {count_var}, so the guard cannot locate "
                f"where the array is recycled")

            release_at = body.find(release_of)
            self.assertNotEqual(
                release_at, -1,
                f"{fn_name} never releases {release_of}, so every refresh leaks the "
                f"previous COM references (sessions_refresh runs on a two-second "
                f"timer, so that is one leak per app per poll, forever)")

            self.assertLess(
                release_at, reset_at,
                f"{fn_name} resets its count before releasing {release_of}, so the "
                f"previous COM references are leaked on every refresh")

    def test_audio_switcher_rebuilds_controls_only_when_the_set_changes(self):
        """The old refresh_ui() destroyed and recreated all 32 endpoint and 128
        session controls on every call, and WM_TIMER called it every two seconds
        -- so dragging a volume slider had the slider destroyed under the mouse.
        Rebuilding must now be gated on an identity signature, and a slider the
        user is dragging must be left alone.
        """
        src = self._audio_switcher_src()

        start = src.index("static void refresh_ui(void)")
        fn = src[start:src.index("\n}\n", start)]
        self.assertIn("endpoints_signature", fn,
                      "refresh_ui no longer compares an endpoint identity signature")
        self.assertIn("sessions_signature", fn,
                      "refresh_ui no longer compares a session identity signature")
        # The rebuild itself moved into ui_rebuild() (it also budgets the rows,
        # publishes the scrollbar and sizes the window); what matters here is
        # that a changed set still tears the tree down and a unchanged one does
        # not.
        self.assertIn("ui_rebuild", fn,
                      "refresh_ui no longer rebuilds the control tree at all")
        rb = src[src.index("static void ui_rebuild(void) {"):]
        rb = rb[:rb.index("\n}\n")]
        self.assertIn("ui_destroy_all", rb,
                      "ui_rebuild no longer destroys the old controls before "
                      "recreating them, so every rebuild would leak a control tree")
        self.assertIn("ui_update_values", fn,
                      "refresh_ui no longer updates existing controls in place")

        # Identity signatures must not include volume/mute, or every poll would
        # look like a change and rebuild anyway.
        for fn_name in ("endpoints_signature", "sessions_signature"):
            s = src.index("static uint64_t " + fn_name)
            body = src[s:src.index("\n}\n", s)]
            self.assertNotIn("volume", body,
                             f"{fn_name} hashes volume, so every level change would "
                             f"force a full control rebuild")

        update = src[src.index("static void ui_update_values"):]
        update = update[:update.index("\n}\n")]
        self.assertIn("GetCapture()", update,
                      "ui_update_values moves sliders without checking GetCapture(), "
                      "so it would fight the user mid-drag")

    def test_keyboard_lighting_logon_task_starts_elevated_and_unsuppressed(self):
        src = read_c_inlined("keyboard-lighting/src/keyboard_lighting.c")

        # The logon task must run at the highest privilege level. Since the
        # manifest is asInvoker - so that an M4/M5 brightness tap costs no UAC
        # prompt - /RL HIGHEST is now the ONLY thing that makes a boot start
        # arrive elevated, because the scheduler never prompts. A limited task
        # means the app lands in preview mode at every logon.
        task = src[src.index("static void EnsureLogonTask(void)"):]
        task = task[:task.index("\n}\n")]
        self.assertIn("/RL HIGHEST", task,
                      "the logon task is not registered /RL HIGHEST, so boot starts "
                      "arrive unelevated (the scheduler never shows a UAC prompt)")
        self.assertNotIn("-NoElevate", task,
                         "the logon task still passes -NoElevate, which forbids the "
                         "app's own relaunch-elevated self-heal and pins a limited-token "
                         "start in preview mode for the whole session")

        # The in-app Startup toggle must not be able to create the same trap.
        setter = src[src.index("static BOOL SetKblAutostart(BOOL enable)"):]
        setter = setter[:setter.index("\n}\n")]
        self.assertNotIn("-NoElevate", setter,
                         "the Startup toggle passes -NoElevate to the logon task again")
        self.assertNotIn('g_isAdmin ? L"/RL HIGHEST" : L""', setter,
                         "the Startup toggle can still create a limited-privilege task "
                         "when clicked from an unelevated session")

        # Refreshed on every launch, like DnsPilot's EnsureAutostart, so a task
        # left by an older build cannot stay limited forever.
        self.assertEqual(src.find("if (g_isAdmin) EnsureLogonTask();"), -1,
                         "the logon task is only refreshed when already elevated, so a "
                         "stale limited task can never be repaired")

    def test_keyboard_lighting_brightness_taps_never_cost_a_uac_prompt(self):
        """M4/M5 brightness must not launch an administrator-required process.

        G-Helper's Custom action for M4/M5 runs KeyboardLighting.exe with
        -BrightUp or -BrightDown, which writes a one-shot token into
        %LOCALAPPDATA%\KeyboardLighting and exits. That needs no privileges
        whatsoever. But a requireAdministrator manifest is enforced by the OS at
        process CREATION, before wWinMain runs, so a non-elevated G-Helper got a
        UAC consent prompt on every single keypress - the owner's exact
        complaint - and the token write never got to be the cheap operation it
        is. Hence asInvoker.

        The panel still obtains its administrator token two ways: the /RL
        HIGHEST logon task, and the runas self-elevation in wWinMain. Only the
        transient command-line branches run unelevated, and they must stay ahead
        of BOTH the single-instance mutex and the elevation block - behind the
        mutex a tap bounces off the already-running panel, and behind the
        elevation block it relaunches this exe as administrator and prompts
        again. That ordering is the whole contract, so it is what is asserted.
        """
        src = read_c_inlined("keyboard-lighting/src/keyboard_lighting.c")
        manifest = (ROOT / "keyboard-lighting" / "src" / "manifest.xml").read_text(
            encoding="utf-8")

        self.assertIn('level="asInvoker"', manifest,
                      "the manifest requires administrator again, so every M4/M5 "
                      "keypress G-Helper routes through this exe costs a UAC consent "
                      "prompt before the brightness token is even written")
        self.assertNotIn("requireAdministrator", manifest,
                         "the manifest still mentions requireAdministrator")

        self.assertEqual(src.count("int WINAPI wWinMain("), 1,
                         "expected exactly one entry point in the inlined source")
        entry = src[src.index("int WINAPI wWinMain("):]
        mutex = entry.index("CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME)")
        elevate = entry.index('L"runas"')

        for arg in ("--stop", "-HoldMode", "-BrightUp", "-BrightDown"):
            at = entry.index(arg)
            self.assertLess(at, mutex,
                            "%s is handled after the single-instance check, so a "
                            "brightness tap bounces off the running panel instead of "
                            "writing its token" % arg)
            self.assertLess(at, elevate,
                            "%s is handled after the elevation block, so a tap from a "
                            "non-elevated G-Helper relaunches this exe as administrator "
                            "and prompts for consent on every keypress" % arg)

        # asInvoker must not cost the panel its administrator token.
        # Precise slice, not a whole-file substring: /RL HIGHEST also appears in
        # SetKblAutostart, so a loose check would pass even with the logon task
        # itself left limited - which, under asInvoker, is the one thing that
        # decides whether a boot start arrives elevated at all. Comments are
        # stripped from the slice first, because this function discusses /RL
        # HIGHEST in prose twice and a substring test was passing on the prose
        # alone while the schtasks command had lost it.
        task = src[src.index("static void EnsureLogonTask(void)"):]
        task = task[:task.index("\n}\n")]
        task = re.sub(r"/\*.*?\*/", "", task, flags=re.S)
        task = re.sub(r"//[^\n]*", "", task)
        self.assertIn("/RL HIGHEST", task,
                      "the logon task no longer requests the highest privilege level, "
                      "and with an asInvoker manifest that is the only thing which "
                      "makes a boot start arrive elevated")
        self.assertIn('L"runas"', src,
                      "the panel can no longer elevate itself when started by hand")
        self.assertIn("if (!g_isAdmin && !noElevate)", src,
                      "the self-elevation is no longer conditional on actually being "
                      "unelevated, so an elevated start would relaunch itself")

    def test_keyboard_lighting_glide_is_finer_than_a_tap(self):
        """The held-key glide must be a sweep, and a tap must still be a tap.

        The complaint was that sweep-while-held looked like a staircase next
        to the screen-brightness slider, and it did: one 5% step every 150ms is
        twenty visible treads over a three-second sweep. The glide now runs on
        its own numbers - 1% down or 1.5% up every 30ms - while a single press
        still takes the coarse 5% so a tap reads instantly. Those are two
        different controls and the test keeps
        them apart.

        Two invariants are easy to break by accident and impossible to notice
        in code review. If rampMs drops below the main loop's sleep, the extra
        resolution is silently thrown away because the ramp can only fire when
        the loop wakes. And if the tap branch keeps stepping per continuation
        token, every held key piles 5% on top of every fine ramp step, which
        reads as a jump at the start of each hold.
        """
        engine = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")

        def num(pat, what):
            m = re.search(pat, engine, re.M)
            self.assertIsNotNone(m, "could not find %s" % what)
            return int(m.group(1))

        step = num(r"^\$step = (\d+)$", "the tap step")
        rstep = num(r"^\$rampStep = (\d+)$", "the downward glide step")
        upstep = num(r"^\$rampUpStep = (\d+)$", "the upward glide step")
        rms = num(r"^\$rampMs = (\d+)$", "the glide cadence")
        loop = num(r"^\$loopMs = (\d+)$", "the main loop sleep")
        hold = num(r"^\$brightHoldMs = (\d+)$", "the post-release hold window")

        self.assertLess(rstep, step,
                        "the downward glide step is not finer than the tap step")
        self.assertGreater(upstep, rstep,
                           "brightness-up is no longer faster than brightness-down")
        self.assertLess(upstep, step,
                        "the upward glide step is as coarse as the immediate tap")
        self.assertGreaterEqual(rms, loop,
                                "the glide cadence (%d ms) is finer than the main loop "
                                "wakes (%d ms), so the extra steps are silently dropped "
                                "and the sweep runs slower than configured" % (rms, loop))

        # Every engine-driven glide uses the directional helper, not the
        # coarse tap step. There are three consumers: burst, latch, and hold-tap.
        self.assertEqual(engine.count("Step-BrightRamp $script:brightRampDir"), 3,
                         "expected burst, latch, and hold-tap to use the fine ramp helper")
        self.assertEqual(engine.count("$script:brightRampDir * $step"), 0,
                         "a glide still uses the coarse tap step")
        # Three immediate tap responses: latch press, burst press, first token
        # of a held gesture. The Fn keys step separately and are not counted.
        self.assertEqual(engine.count("$tapDir * $step"), 3,
                         "expected exactly three immediate 5% tap responses")

        # Continuation tokens must not step on top of the ramp.
        tap_branch = engine[engine.index("$gapMs = ($nowU - $script:brightRampLast)"): ]
        tap_branch = tap_branch[:tap_branch.index("$script:brightRampLast = $nowU")]
        self.assertEqual(tap_branch.count("Set-Live"), 1,
                         "the tap branch steps on continuation tokens as well as on a "
                         "fresh gesture, so a held key double-steps")

        # Overshoot after the key is let go is bounded by the hold window.
        overshoot_pct = (hold // max(rms, loop)) * max(rstep, upstep) / 10.0
        self.assertLessEqual(overshoot_pct, 15.0,
                             "the glide keeps running %.0f%% after release, which reads "
                             "as the brightness moving on its own" % overshoot_pct)

        # The gate re-assert must be rate limited, or a 33Hz ramp sends a WMI
        # call and a HID feature report per step, competing with frame writes.
        self.assertIn("$script:GateReassertAt", engine,
                      "Set-Live re-asserts the EC gate on every call again")
        self.assertIn(
            "if ($level -gt 0 -and [DateTime]::UtcNow -ge $script:GateReassertAt)",
            engine, "Set-Live no longer gates the re-assert behind the timestamp")
        guard = engine[engine.index(
            "if ($level -gt 0 -and [DateTime]::UtcNow -ge $script:GateReassertAt)"):]
        guard = guard[:guard.index("\n    }\n")]
        self.assertIn("Set-KbdBacklight", guard,
                      "the rate-limited block no longer re-asserts the gate at all")

        # ---- the flyout matches the Windows one the owner runs ----
        osd = (ROOT / "keyboard-lighting" / "src" / "parts" / "brightness_osd.c").read_text(
            encoding="ascii", errors="strict")
        self.assertIn("AppsUseLightTheme", osd,
                      "the flyout hardcodes one theme, so on a light desktop it is the "
                      "one obvious tell that this popup is not Windows")
        # Defined is not called: replacing the call with a constant leaves the
        # function in the file and the assertIn above none the wiser.
        self.assertRegex(osd, r"int light = OsdLightTheme\(\);",
                         "the theme is read but the result is discarded, so the flyout "
                         "stays on one theme whatever Windows is doing")
        self.assertIn("RRF_RT_REG_DWORD", osd,
                      "the theme is read without checking the call succeeded")
        self.assertIn("RGB(249, 249, 249)", osd, "no light-theme body colour")
        self.assertIn("RGB(32, 32, 32)", osd, "no dark-theme body colour")
        self.assertIn("Ellipse(mem, tx - tr2, ty - tr2, tx + tr2, ty + tr2);", osd,
                      "the slider has no thumb, which is the part that reads as a "
                      "Windows slider")
        self.assertLess(osd.index("fillW = barW * pct / 100"),
                        osd.index("Ellipse(mem, tx - tr2"),
                        "no round thumb after the filled portion, which is the part "
                        "that reads as a Windows slider")
        self.assertLess(osd.index("Ellipse(mem, tx - tr2 - 2"),
                        osd.index("Ellipse(mem, tx - tr2,"),
                        "the thumb is drawn without its separating ring, so it "
                        "disappears into the filled part of the track")
        self.assertNotIn('L"%d%%"', osd,
                         "the flyout still prints a percentage, which the Windows "
                         "flyout does not")

    def test_keyboard_lighting_hold_ramp_is_not_quantized_by_the_panel(self):
        """A continuation token refreshes hold state, not the ramp clock.

        The old loop restarted its 30 ms deadline on every token. When tokens
        arrived in step with the engine loop, the deadline was perpetually in
        the future and a held key barely moved. Keep the ramp clock independent,
        pump key tokens on a finer timer, and preserve the 35 ms slider readback.
        """
        hotkeys = (ROOT / "keyboard-lighting/src/parts/brightness_master.c").read_text(encoding="ascii")
        poll = hotkeys[hotkeys.index("static void PollFnBrightness(void) {"):]
        poll = poll[:poll.index("\n}\n")]
        self.assertIn("now - g_lastFnPoll < 35ULL", poll)
        self.assertIn("SyncMasterBrightness();", poll)
        self.assertIn("FlushPendingBrightnessCfg();", poll)
        observe = hotkeys[hotkeys.index("static void SyncMasterBrightness(void) {"):]
        observe = observe[:observe.index("\n}\n")]
        self.assertIn("strtol(txt, &end, 10)", observe)
        self.assertIn("!parseOk || !end || *end != '\\0'", observe)
        self.assertIn("PublishMasterBrightness(before, FALSE)", observe)
        self.assertNotIn("ScheduleLiveApply();", hotkeys)
        self.assertIn("g_brightnessCfgSaveAt = now + 300ULL;", hotkeys)
        self.assertNotIn("WriteLiveBrightness();", poll)

        engine = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")

        def num(pattern, label):
            match = re.search(pattern, engine, re.M)
            self.assertIsNotNone(match, "could not find %s" % label)
            return int(match.group(1))

        down = num(r"^\$rampStep = (\d+)$", "the downward ramp step")
        up = num(r"^\$rampUpStep = (\d+)$", "the upward ramp step")
        cadence = num(r"^\$rampMs = (\d+)$", "the ramp cadence")
        loop = num(r"^\$loopMs = (\d+)$", "the engine loop cadence")
        hold_ms = num(r"^\$brightHoldMs = (\d+)$", "the release grace window")
        self.assertGreater(up, down,
                           "brightness-up is not faster than brightness-down")
        self.assertGreaterEqual(cadence, loop,
                                "the ramp cadence is finer than the engine loop can run")

        ramp_step = engine[engine.index("function Get-BrightRampStep"):]
        ramp_step = ramp_step[:ramp_step.index("\n}\n")]
        self.assertIn("if ($direction -gt 0) { return $rampUpStep }", ramp_step)
        self.assertIn("return $rampStep", ramp_step)

        tap = engine[engine.index("$gapMs = ($nowU - $script:brightRampLast)"):]
        tap = tap[:tap.index("} elseif ($script:brightRampDir -ne 0)")]
        self.assertIn("$continuing = ($tapDir -eq $script:brightRampDir -and", tap,
                      "continuation tokens are not distinguished from fresh gestures")
        self.assertIn("$script:brightPending -and $gapMs -le $brightHoldMs", tap,
                      "a held continuation does not refresh the live hold window")
        self.assertIn("} elseif ($nowU -ge $script:brightRampNext) {", tap,
                      "a due fine step is postponed whenever a continuation token arrives")
        self.assertEqual(tap.count("$script:brightRampNext = $nowU.AddMilliseconds($rampMs)"), 2,
                         "the continuation path resets the ramp deadline instead of "
                         "advancing it only after a due brightness step")
        self.assertIn("Step-BrightRamp $tapDir", tap,
                      "a due continuation does not move the brightness")

        picker = (ROOT / "keyboard-lighting/src/parts/picker_window_entry.c").read_text(
            encoding="ascii", errors="strict")
        self.assertIn("SetTimer(hwnd, 3, 15, NULL)", picker,
                      "hold polling is still quantized by the 45 ms preview timer")
        timer_start = picker.rindex("case WM_TIMER:")
        timer = picker[timer_start:picker.index("case WM_TRAYICON:", timer_start)]
        timer3 = timer[timer.index("} else if (wp == 3)"):]
        self.assertIn("WatchBrightTokens();", timer3)
        self.assertIn("HoldPump();", timer3)
        timer1 = timer[:timer.index("} else if (wp == 2)")]
        self.assertNotIn("HoldPump();", timer1,
                         "the fine token pump still waits on the preview timer")

        src = read_c_inlined("keyboard-lighting/src/keyboard_lighting.c")
        hold = src[src.index("static void HoldPump(void) {"):]
        hold = hold[:hold.index("\nstatic void PressSignal")]
        self.assertIn("g_scanNext[i] = now + 30;", hold,
                      "the token target is no longer aligned with the 30 ms fine ramp")
        destroy = src[src.index("case WM_DESTROY:"):]
        destroy = destroy[:destroy.index("\n    }\n")]
        self.assertIn("FlushPendingBrightnessCfg();", destroy,
                      "quitting during the debounce can discard the final held brightness")

    def test_keyboard_lighting_palette_balance_keeps_orange_and_neutral_white(self):
        """Balance is perceptual, visible, and cannot turn neutrals blue.

        A linear-RGB gain could only dim the brighter orange stop when purple
        needed a boost, washing orange out. The replacement moves OKLab lightness
        toward a shared target, preserves hue, and uses one dither phase for
        equal device-space RGB values.
        """
        engine = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")
        prep = engine[engine.index("void PrepPaletteG(Zone gr, bool xfade)"):]
        prep = prep[:prep.index("// OKLab, from LINEAR sRGB")]
        self.assertIn("if (gr.Equalise) BalancePalette(gr.plR, gr.plG, gr.plB);", prep,
                      "group palettes no longer use Balance Colors")
        plain = engine[engine.index("void PrepPalette() {"):]
        plain = plain[:plain.index("// Cyclic palette sample")]
        self.assertIn("BalancePalette(plR, plG, plB);", plain,
                      "the non-group palette path no longer uses Balance Colors")

        balance_start = engine.index("static void BalancePalette(")
        balance_end = engine.index("// Palette blends use a straight OKLab path", balance_start)
        balance = engine[balance_start:balance_end]
        self.assertIn("const double BalanceStrength = 0.55;", balance,
                      "Balance Colors has no meaningful, documented strength")
        self.assertIn("balancedL = L + (target - L) * BalanceStrength", balance,
                      "Balance Colors does not actually move palette-stop lightness")
        self.assertIn("FitOklabGamut", balance,
                      "lightness correction can leave RGB channels clipped")

        mix_start = engine.index("static void MixOklab(")
        mix_end = engine.index("static void ToOklab(", mix_start)
        mix = engine[mix_start:mix_end]
        self.assertIn("double Am = A1 + (A2 - A1) * w;", mix,
                      "palette stops no longer follow a straight perceptual chord")
        self.assertIn("double Bm = B1 + (B2 - B1) * w;", mix,
                      "palette stops no longer follow a straight perceptual chord")

        gamut_start = engine.index("static bool InOklabGamut(")
        gamut_end = engine.index("// Balance each palette stop", gamut_start)
        gamut = engine[gamut_start:gamut_end]
        for channel in ("r", "g", "b"):
            self.assertIn(channel + " <= 1.0 + 1e-9", gamut,
                          "the gamut check can clip a bright %s channel and bend hue" % channel)
        sample_start = engine.index("void SamplePal(")
        sample_end = engine.index("// Pull every group's live inbox", sample_start)
        sample = engine[sample_start:sample_end]
        self.assertIn("MixOklab(La, Aa, Ba, Lb, Ab, Bb, w", sample,
                      "palette transitions still rotate through the vivid OKLCh magenta band")
        self.assertNotIn("MixOklch(", engine,
                         "a palette or palette-change path still takes the saturated hue arc")
        self.assertIn("MixOklab(La, Aa, Ba, Lb, Ab, Bb, u", engine,
                      "palette replacements do not use the same pink-suppressing path")

        push_start = engine.index("void Push(bool force) {")
        push_end = engine.index("public bool PinGate", push_start)
        push = engine[push_start:push_end]
        self.assertIn("double sharedErr = (er[i] + eg[i] + eb[i]) / 3.0;", push,
                      "neutral colours still use unrelated red/green/blue dither phases")
        self.assertIn("qr = q; qg = q; qb = q;", push,
                      "equal device-space channels are not quantized to the same code")

        # Numeric probe of the shipped lightness/gamut rule for the reported
        # saturated purple/orange pair and for requested neutral white.
        def to_linear(v):
            c = v / 255.0
            return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

        def to_oklab(rgb):
            r, g, b = rgb
            l = (0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b) ** (1.0 / 3.0)
            m = (0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b) ** (1.0 / 3.0)
            s = (0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b) ** (1.0 / 3.0)
            return (0.2104542553*l + 0.7936177850*m - 0.0040720468*s,
                    1.9779984951*l - 2.4285922050*m + 0.4505937099*s,
                    0.0259040371*l + 0.7827717662*m - 0.8086757660*s)

        def from_oklab(L, A, B):
            lp = L + 0.3963377774*A + 0.2158037573*B
            mp = L - 0.1055613458*A - 0.0638541728*B
            sp = L - 0.0894841775*A - 1.2914855480*B
            l, m, s = lp**3, mp**3, sp**3
            return (4.0767416621*l - 3.3077115913*m + 0.2309699292*s,
                    -1.2684380046*l + 2.6097574011*m - 0.3413193965*s,
                    -0.0041960863*l - 0.7034186147*m + 1.7076147010*s)

        def fit(L, A, B):
            rgb = from_oklab(L, A, B)
            def in_gamut(c):
                return min(c) >= -1e-9 and max(c) <= 1.0 + 1e-9
            if not in_gamut(rgb):
                lo, hi = 0.0, 1.0
                for _ in range(18):
                    mid = (lo + hi) * 0.5
                    if in_gamut(from_oklab(L, A * mid, B * mid)):
                        lo = mid
                    else:
                        hi = mid
                rgb = from_oklab(L, A * lo, B * lo)
            return tuple(min(1.0, max(0.0, c)) for c in rgb)

        def balance(colors):
            lab = [to_oklab(c) for c in colors]
            visible = [c[0] for c in lab if c[0] > 0.0005]
            target = math.exp(sum(math.log(L) for L in visible) / len(visible))
            out = []
            for L, A, B in lab:
                out.append((0.0, 0.0, 0.0) if L <= 0.0005
                           else fit(L + (target - L) * 0.55, A, B))
            return out

        def mix_oklab(a, b, weight):
            la, lb = to_oklab(a), to_oklab(b)
            return fit(*(x + (y - x) * weight for x, y in zip(la, lb)))

        purple = tuple(to_linear(v) for v in (128, 0, 255))
        orange = tuple(to_linear(v) for v in (255, 128, 0))
        white = (1.0, 1.0, 1.0)
        adjusted = balance([purple, orange, white])
        orange_out = adjusted[1]
        self.assertGreater(orange_out[0], 0.65,
                           "Balance Colors made the orange stop too dim to read")
        self.assertGreater(orange_out[1], 0.10,
                           "the orange stop lost its warm green component")
        white_out = adjusted[2]
        self.assertLess(max(white_out) - min(white_out), 1e-6,
                        "balancing a white stop introduced a channel tint")

        # The old short OKLCh hue arc preserved nearly all chroma at the
        # purple/orange midpoint, which is the vivid pink/red band reported by
        # the owner. A straight OKLab chord keeps the endpoints untouched and
        # naturally reduces that midpoint's chroma instead of flashing it.
        p_lab, o_lab = to_oklab(adjusted[0]), to_oklab(adjusted[1])
        endpoint_floor = min(math.hypot(*p_lab[1:]), math.hypot(*o_lab[1:]))
        mid_lab = to_oklab(mix_oklab(adjusted[0], adjusted[1], 0.5))
        self.assertLess(math.hypot(*mid_lab[1:]), endpoint_floor * 0.65,
                        "the purple/orange midpoint is still vivid enough to flash pink")

    def test_keyboard_lighting_hold_modes_are_reachable_and_bounded(self):
        """All three M4/M5 hold behaviours must be reachable, and the sweep cap
        must be longer than a sweep.

        The engine has supported tap/latch/burst since hold_mode.txt existed and
        re-reads it every tick, but the only way to set it was the -HoldMode
        command line. That left latch and burst - the two that do NOT depend on
        this panel recognising the M-keys - effectively unreachable, while the
        default "tap" needs a typematic repeat or a release frame from the
        M-keys. If neither exists, the tray and Suite Settings must make that
        limitation visible and keep the press-to-glide fallback available. The
        read-only tray indicator reports HasReleaseSrc rather than mere
        learning, because a release source is the actual gate on the stream.

        The ceiling added to HoldPump is checked against the engine's own
        numbers rather than a hardcoded guess: it exists to stop a stream whose
        release never arrives, and it would be a regression if it cut a
        legitimate sweep short.
        """
        src = read_c_inlined("keyboard-lighting/src/keyboard_lighting.c")
        engine = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")

        # The modes offered must be exactly the modes the engine accepts, or a
        # tray choice silently does nothing.
        offered = re.search(r"static const char \*kHoldModes\[3\] = \{([^}]*)\};", src)
        self.assertIsNotNone(offered, "kHoldModes is gone")
        modes = re.findall(r'"([a-z]+)"', offered.group(1))
        accepted = re.search(
            r"if \(\$m -eq '([a-z]+)' -or \$m -eq '([a-z]+)' -or \$m -eq '([a-z]+)'\)",
            engine)
        self.assertIsNotNone(accepted, "could not find the engine's hold-mode validation")
        self.assertEqual(sorted(modes), sorted(accepted.groups()),
                         "the tray offers %s but the engine accepts %s, so one of the "
                         "choices does nothing" % (sorted(modes), sorted(accepted.groups())))
        self.assertEqual(len(modes), 3, "expected three hold modes")

        # Written to the same file -HoldMode writes, so no restart is needed and
        # the two entry points cannot disagree.
        self.assertIn("WriteAtomicUtf8(g_holdModePath, kHoldModes[mode])", src,
                      "the tray does not write hold_mode.txt through the same path as "
                      "-HoldMode, so the two can disagree about the current mode")

        # The menu really offers all three, plus the indicator.
        menu = src[src.index("static void ShowTrayContextMenu(HWND hwnd) {"):]
        menu = menu[:menu.index("\n}\n")]
        self.assertIn("CreatePopupMenu()", menu[menu.index("HMENU hHold"):],
                      "the hold modes are appended to the top-level menu instead of a "
                      "submenu")
        self.assertIn("MF_POPUP", menu, "the submenu is never attached to the tray menu")
        self.assertIn("kHoldLabels[i]", menu, "the three modes are not all appended")
        self.assertIn("HoldReadyLabel()", menu,
                      "nothing tells the owner whether hold-to-sweep can work, which is "
                      "the one question that decides whether to use tap or latch")
        self.assertIn("MF_GRAYED", menu,
                      "the hold-state line is clickable but does nothing")

        # The indicator must report the real gate, not just whether a code was learned.
        label = src[src.index("static const wchar_t *HoldReadyLabel(void) {"):]
        label = label[:label.index("\n}\n")]
        self.assertIn("HasReleaseSrc(0) && HasReleaseSrc(1)", label,
                      "the indicator reports learning rather than the release source that "
                      "actually gates the stream, so it can say Hold Ready for a key that "
                      "has never been seen to release and will not sweep")

        # All three menu IDs are handled. Word-boundary matched, not a plain
        # substring: a typo'd handler reading IDM_TRAY_HOLD_BURST_DISABLED still
        # CONTAINS IDM_TRAY_HOLD_BURST, so assertIn passed on a mutation that
        # left the menu item dead.
        handler = src[src.index("case WM_COMMAND:"):]
        for want in ("IDM_TRAY_HOLD_TAP", "IDM_TRAY_HOLD_LATCH", "IDM_TRAY_HOLD_BURST"):
            self.assertRegex(handler, r"id == %s\b" % want,
                             "%s is in the menu but nothing compares against it, so "
                             "clicking it does nothing" % want)

        # Menu IDs must not collide with the effect or preset ranges.
        ids = dict(re.findall(r"#define (IDM_\w+)\s+(\d+)", src))
        taken = {}
        for name, val in ids.items():
            self.assertNotIn(val, taken, "%s and %s share menu id %s" % (taken.get(val), name, val))
            taken[val] = name
        eff, pat = int(ids["IDM_EFF_BASE"]), int(ids["IDM_PAT_BASE"])
        for name in ("IDM_TRAY_HOLD_TAP", "IDM_TRAY_HOLD_LATCH",
                     "IDM_TRAY_HOLD_BURST", "IDM_TRAY_HOLD_STATE"):
            v = int(ids[name])
            self.assertFalse(eff <= v < eff + 40 or pat <= v < pat + 12,
                             "%s (%d) collides with the effect/preset command ranges" % (name, v))

        # The ceiling must outlast a full sweep, computed from the engine's own
        # step and cadence rather than assumed.
        rstep = int(re.search(r"^\$rampStep = (\d+)$", engine, re.M).group(1))
        rms = int(re.search(r"^\$rampMs = (\d+)$", engine, re.M).group(1))
        loop = int(re.search(r"^\$loopMs = (\d+)$", engine, re.M).group(1))
        # The loop is the real metronome: a ramp cadence finer than the loop
        # sleep can never fire faster than the loop wakes up.
        cadence = max(rms, loop)
        sweep_ms = (1000 // rstep) * cadence         # LiveLevel runs 0..1000
        # Ceiling is tied to the beginning of a physical gesture, never the
        # latest typematic repeat (which used to keep extending it forever).
        hold = (ROOT / "keyboard-lighting/src/parts/m4_m5_hold.c").read_text(encoding="ascii")
        match = re.search(r"now - g_gestureAt\[i\] >= (\d+)ULL", hold)
        self.assertIsNotNone(match)
        ceiling = int(match.group(1))
        self.assertGreater(ceiling, sweep_ms)
        cap = hold[hold.index("now - g_gestureAt[i] >="):]
        cap = cap[:cap.index("\n        }")]
        self.assertIn("BrightnessRelease(&g_brightness, i);", cap)
        self.assertNotIn("g_scanArmed[i] = 1", cap,
                         "a stuck raw key can re-arm itself without a real release")
        controller = (ROOT / "keyboard-lighting/src/parts/brightness_controller.h").read_text(encoding="ascii")
        self.assertIn("now - s->startedAt >= 10000", controller)


    def test_m4_brightness_hold_learns_on_the_first_token_and_remembers_release(self):
        """The first G-Helper tap is also the learning event. A delayed 600 ms
        poll or a two-release threshold left M4 brightness-down at one 5% step
        until the owner retried the key several times."""
        src = read_c_inlined("keyboard-lighting/src/keyboard_lighting.c")
        hold = src[src.index("static int ParseBrightTokenTick"):]
        picker = (ROOT / "keyboard-lighting/src/parts/picker_window_entry.c").read_text(
            encoding="ascii", errors="strict")

        self.assertIn("TickDistance(r->tick, tokenAt)", hold,
                      "the action token is not correlated with the raw key event, so "
                      "the first M4 hold cannot teach its own scan/HID binding")
        self.assertIn("if (cand && ScanCurrentlyDown((DWORD)cand, candVk))", hold,
                      "a launch-delayed first token still waits for a second keypress "
                      "even when the captured M4 key is physically held")
        self.assertIn("BindScan(dir, bestKey->scan, bestKey->vk)", hold,
                      "the correlated key does not persist both scan and virtual key")
        self.assertIn("r->vk = ri->data.keyboard.VKey;", hold,
                      "raw keyboard input never records the VKey used for release polling")
        self.assertIn("g_scanVk[0] = (DWORD)GetPrivateProfileIntW(L\"Keys\", L\"M4Vk\"", picker)
        self.assertIn("SeedBrightTokenWatch();", picker,
                      "a stale flag is not seeded at startup, so a prior tap can be "
                      "mistaken for the current learning gesture")
        timer_start = picker.rindex("case WM_TIMER:")
        timer_block = picker[timer_start:]
        timer3 = timer_block[timer_block.index("} else if (wp == 3)"):]
        self.assertIn("WatchBrightTokens();", timer3,
                      "token learning is not on the dedicated fine timer")
        self.assertIn("HoldPump();", timer3,
                      "hold repeats are not on the dedicated fine timer")
        self.assertIn("g_hidRelSeen[dir] >= 1", src,
                      "one observed HID release pair still does not enable immediate repeat")
        self.assertIn('PersistKeyValue(dir == 0 ? L"M4HidRelease" : L"M5HidRelease", 1);', src,
                      "a known HID release source is not persisted for the next launch")
        self.assertIn('g_hidRelSeen[0] = (BYTE)GetPrivateProfileIntW(L"Keys", L"M4HidRelease"', picker,
                      "a known HID release source is not restored after launch")
        self.assertIn("GetAsyncKeyState((int)g_scanVk[i])", src,
                      "lost raw key-up messages can leave the repeat stream running")
        self.assertIn("g_scanNext[dir] = now + 30;", hold,
                      "the first continuation can miss the engine's 120 ms hold window "
                      "and be misread as a second tap")

    def test_keyboard_lighting_watches_keys_via_raw_input_not_a_global_hook(self):
        """A WH_KEYBOARD_LL hook runs on the app's UI thread, and that thread
        blocks for seconds in the engine (re)start waits. Windows times a stalled
        low-level hook out and DROPS the keystroke - PrintScreen is the most
        visibly broken, and it fails machine-wide for as long as the app runs.
        Raw input (WM_INPUT) is queued to our own loop instead, so it never
        intercepts system input. It must be registered against a real window:
        InitRawInput used to be called before CreateWindowExW, i.e. with a NULL
        hwnd, so no WM_INPUT ever arrived and the hook was doing all the work."""
        src = read_c_inlined("keyboard-lighting/src/keyboard_lighting.c")

        self.assertNotIn("SetWindowsHookEx", src,
                         "the app still installs a system-wide low-level keyboard "
                         "hook, which stalls on the busy UI thread and makes Windows "
                         "drop PrintScreen (and other keys) everywhere while it runs")
        self.assertNotIn("KbHoldProc", src,
                         "the low-level keyboard hook procedure is still present")

        # Raw input is the replacement, and it must still drive the M4/M5 keys.
        self.assertIn("case WM_INPUT:", src,
                      "raw input is no longer handled, so the M4/M5 brightness keys "
                      "would stop working once the hook is gone")
        self.assertIn("OnScanEvent(ri->data.keyboard.MakeCode", src,
                      "raw keyboard input no longer feeds the M4/M5 scan handler")

        create_at = src.index("g_hwnd = CreateWindowExW(")
        raw_at = src.index("InitRawInput(g_hwnd);")
        self.assertGreater(raw_at, create_at,
                           "InitRawInput runs before the window is created, so it "
                           "registers with a NULL hwnd and no WM_INPUT ever arrives")

    def test_keyboard_lighting_raw_input_keeps_keyboard_and_all_hid_reports(self):
        """A failed vendor-usage registration must not discard keyboard input,
        and every HID report batched into one WM_INPUT must reach the hold learner."""
        src = read_c_inlined("keyboard-lighting/src/keyboard_lighting.c")
        init_start = src.index("static void InitRawInput(HWND hwnd) {")
        raw_start = src.index("static void OnRawInputMsg(HWND hwnd, LPARAM lp) {")
        init = src[init_start:raw_start]
        raw = src[raw_start:src.index("\n}\n", raw_start)]

        self.assertIn("rid[n].usUsagePage = 0x01; rid[n].usUsage = 0x06;", init,
                      "generic keyboard input is no longer the first registered usage")
        self.assertIn("for (j = 0; j < n; j++)", init,
                      "ASUS HID registration can duplicate the keyboard usage and make "
                      "the complete registration fail")
        self.assertIn("if (!RegisterRawInputDevices(rid, n, sizeof(RAWINPUTDEVICE)))", init,
                      "registration failure is ignored, silently leaving M4 hold detection blind")
        self.assertIn("RegisterRawInputDevices(rid, 1, sizeof(RAWINPUTDEVICE));", init,
                      "a rejected vendor collection can still disable generic keyboard input")

        self.assertIn("const RAWHID *hid = &ri->data.hid;", raw)
        self.assertIn("hid->dwCount == 0", raw,
                      "the report loop can divide by zero on malformed raw input")
        self.assertIn("available / (size_t)hid->dwCount", raw,
                      "the raw HID report count is not bounded by the received packet size")
        self.assertIn("for (DWORD i = 0; i < hid->dwCount; i++)", raw,
                      "WM_INPUT HID batches discard every report after the first")
        self.assertIn("hid->bRawData + (size_t)i * (size_t)hid->dwSizeHid", raw,
                      "the HID loop does not advance to the next report in the batch")

    def test_hub_auto_probe_prefers_main_over_a_compiled_session_branch(self):
        src = read_c_inlined("project-hub/src/project_hub.c")

        start = src.index("if (!chosen) {")
        block = src[start:src.index("EnterCriticalSection", start)]
        main_probe = block.find('L"/%s/%s/main/audio-switcher/AudioSwitcher.exe"')
        fall_probe = block.find("FALLBACK_BRANCH_W);")
        self.assertNotEqual(main_probe, -1, "the auto probe no longer considers main")
        self.assertNotEqual(fall_probe, -1, "the auto probe no longer considers the session branch")
        self.assertLess(main_probe, fall_probe,
                        "a compiled-in session branch is probed before main, so every hub "
                        "with no saved choice silently serves that branch's builds even "
                        "after newer ones reached main")

        self.assertEqual(src.find('L"arena/b6d41021-tests"'), -1,
                         "the fallback branch is a stale session name again; hubs built "
                         "from it keep downloading its old executables")
    def test_hub_update_check_compares_content_not_just_size(self):
        src = read_c_inlined("project-hub/src/project_hub.c")

        # A HEAD request yields only Content-Length, and two different builds
        # can share a byte count. Size-only comparison is how a same-size
        # rebuild looked "up to date" on the branch that actually changed it.
        self.assertNotIn("BOOL kblUpd = kblKnown && locKblSz != remKblSz;", src,
                         "the Keyboard Lighting update decision is size-only again, so a "
                         "rebuild that lands at the same byte count is invisible to the Hub")

        self.assertIn("static BOOL RemoteDiffers(", src,
                      "the Hub no longer has a content-identity comparison for updates")
        self.assertIn("/git/trees/", src,
                      "the Hub never fetches remote blob hashes, so RemoteDiffers can "
                      "only ever fall back to sizes")
        self.assertIn('TreeShaFor(treeJson, "keyboard-lighting/KeyboardLighting.exe"', src,
                      "the remote blob hash of KeyboardLighting.exe is never recorded")
        self.assertIn("BCRYPT_SHA1_ALGORITHM", src,
                      "local files are not hashed, so the remote blob hash has nothing "
                      "to compare against")

        uses = src.count("RemoteDiffers(g_rem") + src.count("RemoteDiffers(c->remSha")
        self.assertGreaterEqual(uses, 7,
                         f"only {uses} update decisions compare content; the rest still "
                         f"trust file size alone")
    def test_hub_mod_card_follows_the_selected_branch_build(self):
        src = read_c_inlined("project-hub/src/project_hub.c")

        # CI republishes spatial-hud-build-<branch> on every push; querying only
        # spatial-hud-latest (main's pointer) made a branch's jar invisible even
        # with the Hub pointed at that branch, so mod fixes never showed up.
        self.assertIn('const wchar_t *hudTags[2] = { tagBranch, L"spatial-hud-latest" };', src,
                      "the mod card no longer tries the selected branch's build release "
                      "before falling back to main's pointer, so branch jars stay invisible")
        self.assertIn('L"spatial-hud-build-"', src,
                      "the Hub never builds the per-branch release tag that CI publishes")
        self.assertNotIn('_snwprintf(p, 255, L"/repos/%s/%s/releases/tags/spatial-hud-latest", REPO_OWNER_W, REPO_NAME_W);',
                         src,
                         "the mod card queries only the main pointer again, so a branch "
                         "with mod fixes still reports the mod up to date")


class SuiteFolderInstallTests(unittest.TestCase):
    """An update has to land in the folder the user actually browses.

    The hub used to resolve each app's install folder by looking next to its
    own executable first. A hub launched from Downloads therefore installed
    Audio Switcher into Downloads and never touched
    %LOCALAPPDATA%\\Programs\\TestsSuite\\AudioSwitcher, so the suite folder kept a
    build from days ago while every card reported "Up to Date" - it was
    measuring the stray copy it had just updated.
    """

    @staticmethod
    def _hub_src() -> str:
        return read_c_inlined("project-hub/src/project_hub.c")

    def test_suite_folder_is_the_install_target_for_every_app(self):
        src = self._hub_src()

        self.assertIn("static void SuiteAppPath(", src,
                      "the hub lost the helper that builds the managed suite folder path")
        self.assertIn(r'L"%s\\Programs\\TestsSuite"', src,
                      "the managed suite folder path changed; the mirror logic and the "
                      "cards would disagree about where an app lives")

        # Audio Switcher is the reported case: the suite copy wins over a copy
        # sitting next to the hub, and the losing locations become mirrors.
        start = src.index("/* 2d. Audio Switcher")
        end = src.index("/* 3. OrcaSlicer plugins */")
        block = src[start:end]
        self.assertIn("SuiteAppPath(asSuite, MAX_PATH, L\"AudioSwitcher\");", block,
                      "Audio Switcher no longer derives its suite folder install path")
        self.assertRegex(
            block,
            r"if \(FileExistsW\(cand\)\) wcsncpy\(g_audioSwitcherDir, asSuite, MAX_PATH - 1\);\s*"
            r"else if \(asInHubDir\)",
            "the folder next to the hub wins over the suite folder again, so a hub "
            "launched from Downloads installs Audio Switcher into Downloads",
        )
        self.assertIn("AddMirrorDir(MIRROR_AUDIO, asSuite);", block)
        self.assertIn("if (asInHubDir) AddMirrorDir(MIRROR_AUDIO, g_exeDir);", block,
                      "a stray copy next to the hub is no longer kept in sync")
        self.assertIn("AddMirrorDir(MIRROR_BTG, btgSuite);", src,
                      "Bluetooth Guard is not mirrored into the suite folder")
        self.assertIn("BOOL btgInSuite = FileExistsW(cand);", src,
                      "the Bluetooth Guard suite check no longer looks for the exe, so "
                      "the installer and the badge disagree about which folder is real")
        self.assertIn("if (btgInSuite) wcsncpy(g_btgDir, btgSuite, MAX_PATH - 1);", src,
                      "Bluetooth Guard stopped preferring its suite folder copy")
        self.assertIn("AddMirrorDir(MIRROR_KBL, kblSuite);", src,
                      "Keyboard Lighting is not mirrored into the suite folder")
        self.assertIn("AddMirrorDir(MIRROR_DNS, dnsSuite);", src,
                      "DNS Pilot is not mirrored into the suite folder")
        self.assertIn("AddMirrorDir(MIRROR_NOTES, notesSuite);", src,
                      "Agent Notes is not mirrored into the suite folder")

    def test_installers_write_one_download_to_every_existing_copy(self):
        src = self._hub_src()

        self.assertIn("static BOOL InstallRepoFileEverywhere(", src,
                      "the hub lost the one-download-every-copy installer")
        body_start = src.index("static BOOL InstallRepoFileEverywhere(")
        body = src[body_start:body_start + 1400]
        self.assertIn("WriteBinaryFileAtomic(dest, data, len)", body,
                      "the suite copy is no longer written from the downloaded bytes")
        self.assertIn("MirrorFileEverywhere(slot, dest)", body,
                      "the installer writes only one location again, so the other "
                      "copies of the app keep the previous build")

        self.assertIn("static int MirrorFileEverywhere(", src)
        mirror_body_start = src.index("static int MirrorFileEverywhere(")
        mirror_body = src[mirror_body_start:mirror_body_start + 1600]
        self.assertRegex(mirror_body, r"for \(int i = 0; i < g_mirrorCount\[slot\]; i\+\+\)",
                         "the mirror loop no longer walks every registered copy")
        self.assertIn("CopyFileW(primaryPath, alt, FALSE)", mirror_body,
                      "mirror copies are not overwritten with the fresh build")
        self.assertIn("if (!FileExistsW(alt) && !inSuite) continue;", mirror_body,
                      "the mirror would create copies in folders where the user "
                      "never had the app")
        self.assertRegex(mirror_body,
                         r"BOOL inSuite = \(_wcsicmp\(g_mirrorDirs\[slot\]\[i\], g_suiteDir\) == 0\)",
                         "the suite folder is no longer recognised as the managed home, "
                         "so its copy would not be created on a first install")

        for call in ("InstallRepoFileEverywhere(mirrorSlot",
                     "InstallRepoFileEverywhere(MIRROR_DNS",
                     "InstallRepoFileEverywhere(MIRROR_NOTES"):
            self.assertIn(call, src, f"{call}...) is gone - that installer went back "
                                     "to writing a single folder")
        for call in ("MirrorFileEverywhere(MIRROR_BTG",
                     "MirrorFileEverywhere(MIRROR_KBL"):
            self.assertIn(call, src, f"{call}...) is gone - that installer leaves its "
                                     "other copies behind")
        self.assertGreaterEqual(src.count("MirrorFileEverywhere("), 3,
                                "the mirror helper is barely used; most installers "
                                "still update one folder only")

    def test_cards_point_at_the_folder_the_installer_writes(self):
        src = self._hub_src()

        # All three places that give a built-in card its appDir must agree with
        # the install target, or Open/Folder/Install act on different folders.
        self.assertGreaterEqual(src.count("SuiteAppPath(c->appDir, MAX_PATH"), 3,
                                "a card definition stopped deriving appDir from the "
                                "suite folder")
        self.assertNotIn("wcsncpy(g_cards[6].appDir, g_audioSwitcherDir, MAX_PATH - 1);", src,
                         "the Audio Switcher card points at the detected stray copy "
                         "again, so its badge measures a folder Update may not write")

        # The card must say which folder it manages.
        self.assertIn("static void AppendInstallFolder(", src,
                      "cards no longer show the folder they install into")
        self.assertIn("AppendInstallFolder(g_cards[6].detail, asDir);", src,
                      "the Audio Switcher card hides its install folder")

        # Bluetooth Guard and Keyboard Lighting badges must measure the same
        # folder their installer writes, with the found copy as the fallback.
        self.assertIn("const wchar_t *btgDir = g_cards[0].appDir[0] ? g_cards[0].appDir : g_btgDir;",
                      src, "the Bluetooth Guard badge measures a folder its installer "
                           "no longer writes")
        self.assertIn("const wchar_t *kblDir = g_cards[1].appDir[0] ? g_cards[1].appDir : g_kblDir;",
                      src, "the Keyboard Lighting badge measures a folder its installer "
                           "no longer writes")
        self.assertNotIn('_snwprintf(btgExeP, MAX_PATH, L"%s\\\\BluetoothGuard.exe", g_btgDir);',
                         src, "the Bluetooth Guard update check went back to the detected "
                              "stray copy instead of the install target")
        # The badge must measure the copy the installer writes, with the found
        # copy as the fallback until the first update lands in the suite folder.
        self.assertIn("const wchar_t *asDir = g_cards[6].appDir[0] ? g_cards[6].appDir : g_audioSwitcherDir;",
                      src, "the Audio Switcher badge no longer follows the install target")
        self.assertIn("_snwprintf(asExe, MAX_PATH, L\"%s\\\\AudioSwitcher.exe\", asDir);", src,
                      "the Audio Switcher badge measures a folder Update does not write")
        self.assertGreaterEqual(src.count("AppendInstallFolder("), 4,
                                "only a couple of cards report where they install")

    def test_update_closes_and_proves_every_managed_executable_copy_is_gone(self):
        """An update must close the app first, not discover its file lock after
        writing one of several mirrors.  The gate is process-image based so it
        covers same-folder, stray, hidden-tray, and catalogue apps alike."""
        src = self._hub_src()
        install = (ROOT / "project-hub" / "src" / "parts" / "install_folder.c").read_text(encoding="utf-8")
        identity = (ROOT / "project-hub" / "src" / "parts" / "content_identity.c").read_text(encoding="utf-8")
        workers = (ROOT / "project-hub" / "src" / "parts" / "workers_install.c").read_text(encoding="utf-8")
        cards = (ROOT / "project-hub" / "src" / "parts" / "card_defs_verbs.c").read_text(encoding="utf-8")
        audio = (ROOT / "audio-switcher" / "src" / "parts" / "diag.c").read_text(encoding="utf-8")

        self.assertIn("#include <tlhelp32.h>", src,
                      "the Hub cannot enumerate live process images before an update")
        scan_start = identity.index("static int FindProcessesUsingImage(")
        scan = identity[scan_start:identity.index("static BOOL NoProcessRunsImage(", scan_start)]
        self.assertIn("CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)", scan)
        self.assertIn("QueryFullProcessImageNameW(process, 0, processPath, &pathLen)", scan,
                      "the close gate no longer proves the process came from the managed path")
        self.assertIn("return uncertain ? -1 : out->count;", scan,
                      "an uninspectable same-named process is guessed safe")

        gate_start = install.index("static BOOL CloseManagedExeCopies(")
        gate = install[gate_start:install.index("static void EnsureProbeRef(", gate_start)]
        self.assertIn("g_mirrorCount[slot]", gate,
                      "the close gate ignores registered stray copies")
        self.assertIn("CloseRunningImageForUpdate(paths[i], quitCommand)", gate,
                      "the close gate no longer asks each managed running copy to exit")
        self.assertIn("NoProcessRunsImage(paths[i])", gate,
                      "the close gate no longer verifies every image exited")
        self.assertIn("No files were changed", gate,
                      "a still-running app is not reported as a refused update")

        atomic_start = identity.index("static BOOL WriteBinaryFileAtomic(")
        atomic = identity[atomic_start:atomic_start + 1700]
        self.assertIn("IsExeImagePath(dest) && !NoProcessRunsImage(dest)", atomic,
                      "an app can restart after preflight and still be overwritten")
        mirror_start = install.index("static int MirrorFileEverywhere(")
        mirror = install[mirror_start:mirror_start + 1800]
        self.assertIn("IsExeImagePath(alt) && !NoProcessRunsImage(alt)", mirror,
                      "a reopened stray mirror can be overwritten after the gate")

        for call in (
            "CloseManagedExeCopies(MIRROR_BTG, g_btgDir, L\"BluetoothGuard.exe\"",
            "CloseManagedExeCopies(MIRROR_KBL, g_kblDir, L\"KeyboardLighting.exe\"",
            "CloseManagedExeCopies(MIRROR_DNS, g_dnsDir, L\"DnsPilot.exe\"",
            "CloseManagedExeCopies(MIRROR_NOTES, g_notesDir, L\"AgentNotes.exe\"",
            "CloseManagedExeCopies(mirrorSlot, c->appDir, c->exeName,",
        ):
            self.assertIn(call, workers + cards,
                          "%s is missing, so that app can be written while open" % call)

        self.assertIn("? MIRROR_AUDIO : MIRROR_GENERIC;", cards,
                      "Audio Switcher lost its own registered mirror set in the generic worker")
        self.assertIn("InstallRepoFileEverywhere(mirrorSlot", cards,
                      "the generic worker closes Audio Switcher mirrors but then writes a different set")
        self.assertIn("RequestManagedExeCopiesStop(mirrorSlot, c->appDir, c->exeName);", cards,
                      "a catalogue tray app no longer receives its explicit exit request")
        self.assertLess(cards.index("RequestManagedExeCopiesStop(mirrorSlot"),
                        cards.index("CloseManagedExeCopies(mirrorSlot"),
                        "the graceful stop request runs after the close-and-prove gate")
        self.assertIn('_wcsicmp(c->exeName, L"AudioSwitcher.exe") == 0', cards,
                      "Audio Switcher no longer receives its graceful stop request")
        self.assertIn('_wcsicmp(c->exeName, L"WeatherRadar.exe") == 0', cards,
                      "Weather Radar update falls back to its tray-hiding WM_CLOSE")
        stop_start = install.index("static void RequestManagedExeCopiesStop(")
        stop = install[stop_start:install.index("static BOOL CloseManagedExeCopies(", stop_start)]
        self.assertIn("g_mirrorCount[slot]", stop,
                      "the explicit stop route ignores registered catalogue mirrors")
        self.assertIn('stop.lpParameters = L"--stop";', stop,
                      "the explicit stop route no longer invokes the app's real exit command")
        self.assertIn("WaitForSingleObject(stop.hProcess, 2500)", stop,
                      "the updater races immediately past the explicit stop helper")
        self.assertIn("AUDIO_SWITCHER_QUIT_COMMAND", cards,
                      "Audio Switcher's tray-only WM_CLOSE is no longer given its real Quit command")
        self.assertIn('wcsstr(cmdLine, L"--stop")', audio,
                      "Audio Switcher itself does not support the updater stop contract")
        self.assertIn("IDM_TRAY_QUIT", audio,
                      "Audio Switcher --stop does not follow the app's graceful tray Quit path")
        weather = (ROOT / "weather-radar" / "src" / "parts" / "entry.c").read_text(encoding="utf-8")
        self.assertIn('wcsstr(cmdLine, L"--stop")', weather,
                      "Weather Radar itself does not support the updater stop contract")
        self.assertIn("ID_TRAY_EXIT", weather,
                      "Weather Radar --stop does not follow the app's graceful tray Exit path")
        self.assertNotIn("static BOOL StopForeignInstance(", src,
                         "the old Audio-Switcher-only forced termination path remains instead of the shared safe gate")

    def test_agent_notes_discovers_the_live_session_itself(self):
        """The notes channel used to need a pasted link that went stale.

        The receiver lives in the agent's workspace, whose public address is
        per-session, so every reset silently broke the app until the owner
        pasted a new link. The agent now commits .agent/AGENT_LINK.txt each
        turn and the app polls that one file, adopting a discovered address
        ONLY when its /health probe answers - so a dead session, a stale
        file or a half-written commit can never replace a link that works.

        The outbox invariant is the other half of "nothing is lost": an
        outbox file is deleted only after a confirmed 200, which is why
        queued notes survive every kind of outage. Both halves get asserted
        here, plus the honest offline wording that replaced "re-paste your
        link" - queued-is-not-broken is the whole point.
        """
        ha = (ROOT / "agent-notes" / "src" / "parts" / "http_agent.c").read_text(encoding="utf-8")
        watch = (ROOT / "agent-notes" / "src" / "parts" / "wait_alerts.c").read_text(encoding="utf-8")
        ui = (ROOT / "agent-notes" / "src" / "parts" / "ui.c").read_text(encoding="utf-8")
        en = (ROOT / "agent-notes" / "src" / "parts" / "entry.c").read_text(encoding="utf-8")
        branch_cache = (ROOT / "project-hub" / "src" / "parts" / "version_probe_cache.c").read_text(encoding="utf-8")

        self.assertIn(".agent/AGENT_LINK.txt", ha,
                      "the app no longer polls the committed session link, so the "
                      "owner is back to pasting addresses by hand")
        self.assertIn(r"ProjectHub\\config.ini", ha,
                      "discovery cannot find the active branch without the hub's config")
        self.assertIn("if (!HealthOk(cand)) return 0;", ha,
                      "a discovered link is adopted without proving it is live, so a "
                      "dead session would replace a working link")
        self.assertIn("LINK_DISCOVERY_INTERVAL_MS", ha,
                      "the live-link refresh has no bounded polling interval")
        self.assertIn("if (LinkDiscoveryTick()) result->linkFound = 1;", watch,
                      "the watcher stopped refreshing a saved link, so a prior Arena "
                      "sandbox can strand every queued note")
        self.assertNotIn("if (!g_link[0] && LinkDiscoveryTick())", watch,
                         "the watcher only discovers when its saved link is blank; "
                         "a stale but non-empty previous-session link can never recover")
        auto_refresh = branch_cache[branch_cache.index("static void RefreshAutomaticBranch(void)"):]
        auto_refresh = auto_refresh[:auto_refresh.index("\n}\n")]
        self.assertIn("SaveBranchCfg(newest);", auto_refresh,
                      "automatic Hub branch following is not persisted for Agent Notes, "
                      "so it can keep polling a previously selected dead branch")
        self.assertIn("StartStatusPoll()", ui,
                      "the background wait/receipt poll no longer rediscovers a new session, so "
                      "the owner would have to restart the app")
        self.assertIn("LinkDiscoveryTick()", en,
                      "startup never runs discovery, so an app opened after a new "
                      "session began waits a full minute for its first tick")
        self.assertIn("Agent offline", ha,
                      "the offline status still blames the link instead of saying "
                      "the note is queued and safe")

        fo = ha[ha.index("static int FlushOutbox("):]
        fo = fo[:fo.index("\n}\n")]
        self.assertEqual(fo.count("HistMarkSaved(fd.cFileName, receipt) && DeleteFileW(path)"), 1,
                         "the outbox delete moved - queued notes must survive failures")
        self.assertLess(fo.index("HttpPostNote(g_link"), fo.index("HistMarkSaved(fd.cFileName, receipt)"),
                        "an outbox file is deleted before delivery is confirmed, so a "
                        "crash mid-flush loses the note")

        import re as _re
        lk = (ROOT / ".agent" / "AGENT_LINK.txt").read_text(encoding="utf-8").strip()
        self.assertRegex(lk, r"^https://8765-[a-z0-9]+\.e2b\.app$",
                         "the committed session link is not a usable receiver address")
        ex = (ROOT / ".agent" / "rules" / "execution.md").read_text(encoding="utf-8")
        for needle in ("AGENT_LINK.txt", "INBOX.md", "notes_server.py"):
            self.assertIn(needle, ex,
                          "the turn-start protocol no longer mentions %s, so the "
                          "channel's last mile rots again" % needle)

    def test_hub_startup_keeps_agent_notes_explicitly_user_opened(self):
        """Hub startup checks suite status only. Opening the Agent Notes composer
        remains an explicit card/tray action, never a launch side effect."""
        entry = (ROOT / "project-hub" / "src" / "parts" / "branch_picker_entry.c").read_text(encoding="utf-8")
        start = entry.index("case WM_CREATE:")
        end = entry.index("case WM_DPICHANGED:", start)
        startup = entry[start:end]
        self.assertIn("EnqueueTask(TASK_CHECK_REMOTE);", startup,
                      "Hub startup no longer performs its background release check")
        for forbidden in ("TASK_INSTALL_NOTES", "TASK_OPEN_OR_INSTALL_NOTES",
                          "ID_NOTES_OPEN", "AgentNotes.exe", "WorkerInstallNotes", "ShellExecuteW"):
            self.assertNotIn(forbidden, startup,
                             f"Hub startup can open Agent Notes through {forbidden}")

    def test_hub_agent_notes_update_uses_the_app_owned_elevated_exit_route(self):
        """An asInvoker Hub must not send a UIPI-blocked WM_COMMAND into the
        elevated Agent Notes window. Its explicit --stop companion requests exit;
        the existing all-copy process gate remains authoritative before writes."""
        workers = (ROOT / "project-hub" / "src" / "parts" / "workers_install.c").read_text(encoding="utf-8")
        helper_start = workers.index("static BOOL HubRequestAgentNotesExit(HWND notes)")
        helper_end = workers.index("\n}\n", helper_start)
        helper = workers[helper_start:helper_end]
        self.assertIn("QueryFullProcessImageNameW", helper)
        self.assertIn('stop.lpVerb = L"runas";', helper,
                      "the stop companion no longer reaches the running app's integrity level")
        self.assertIn('stop.lpParameters = L"--stop";', helper,
                      "the helper no longer uses Agent Notes' explicit real-exit route")

        worker_start = workers.index("static void WorkerInstallNotes(BOOL launchAfter)")
        worker_end = workers.index("static void WorkerInstallDnsPilot", worker_start)
        worker = workers[worker_start:worker_end]
        self.assertIn("HubRequestAgentNotesExit(hNotes)", worker)
        self.assertIn("CloseManagedExeCopies(MIRROR_NOTES", worker,
                      "an elevated stop request replaced the close-and-prove gate")
        self.assertNotIn("PostMessageW(hNotes", worker,
                         "the unelevated Hub still sends a cross-integrity WM_COMMAND")

    def test_hub_open_reveals_a_running_agent_notes_window(self):
        """Notes shares the Hub-owned reveal route instead of spawning a
        companion and hoping it can transfer focus to the hidden instance."""
        entry = (ROOT / "project-hub/src/parts/branch_picker_entry.c").read_text(encoding="utf-8")
        activation = (ROOT / "project-hub/src/parts/app_activation.c").read_text(encoding="utf-8")
        action = entry[entry.index("case ID_NOTES_OPEN:"):entry.index("    case ID_NOTES_UPDATE:")]
        self.assertIn('L"AgentNotes.exe", L"AgentNotesWnd"', activation)
        self.assertIn("if (!HubOpenApp(p, NULL, g_notesDir))", action)
        self.assertIn("EnqueueTask(TASK_OPEN_OR_INSTALL_NOTES)", action)
        self.assertNotIn("ShellExecuteW", action, "Notes still bypasses the shared show-or-launch policy")
        self.assertIn("if (!started)", activation)
        self.assertIn("HubReportOpenFailure(imagePath, dir, launchError);", activation)

    def test_unified_hub_tray_consolidates_icons_and_safe_controls(self):
        """The owner selected one Project Hub tray icon, with app windows and
        safe toggles reachable from it. Individual icons remain a fallback only
        when the Hub host is absent; no sign-in/startup behaviour is added."""
        hub = (ROOT / "project-hub" / "src" / "project_hub.c").read_text(encoding="utf-8")
        hub_ui = (ROOT / "project-hub" / "src" / "parts" / "branch_picker_entry.c").read_text(encoding="utf-8")
        tray = (ROOT / "project-hub" / "src" / "parts" / "tray_unified.c").read_text(encoding="utf-8")
        host = (ROOT / "shared" / "win32" / "hub_tray_host.h").read_text(encoding="utf-8")
        notes_watch = (ROOT / "agent-notes" / "src" / "parts" / "wait_alerts.c").read_text(encoding="utf-8")
        weather_alerts = (ROOT / "weather-radar" / "src" / "parts" / "alerts.c").read_text(encoding="utf-8")
        weather_entry = (ROOT / "weather-radar" / "src" / "parts" / "entry.c").read_text(encoding="utf-8")
        kbl = (ROOT / "keyboard-lighting" / "src" / "parts" / "picker_window_entry.c").read_text(encoding="utf-8")
        dns = (ROOT / "dns-pilot" / "src" / "parts" / "ui.c").read_text(encoding="utf-8")
        btg = (ROOT / "bluetooth-guard" / "src" / "parts" / "window_entry.c").read_text(encoding="utf-8")

        self.assertIn("HUB_TRAY_HOST_EVENT", host)
        self.assertIn("OpenEventW(SYNCHRONIZE", host,
                      "apps no longer probe whether the Hub owns the suite tray")
        self.assertIn("HubTrayInit();", hub_ui)
        nc_create_start = hub_ui.index("case WM_NCCREATE:")
        nc_create_end = hub_ui.index("case WM_CREATE:", nc_create_start)
        nc_create = hub_ui[nc_create_start:nc_create_end]
        self.assertIn("g_hwnd = hwnd;", nc_create,
                      "WM_CREATE runs before CreateWindowEx returns, so the tray NID otherwise gets a NULL HWND")
        self.assertIn("return DefWindowProcW(hwnd, msg, wp, lp);", nc_create,
                      "the Hub no longer preserves default WM_NCCREATE handling")
        self.assertLess(nc_create_end, hub_ui.index("HubTrayInit();", nc_create_end),
                        "the notification icon must be registered only after the global HWND is bound")
        self.assertIn("HubTrayRestore();", hub_ui,
                      "Explorer/taskbar restart leaves the Hub icon missing")
        self.assertIn("HubTrayEnsureLive();", hub_ui,
                      "a transient Hub tray registration failure leaves every app on fallback icons")
        retry_start = tray.index("static void HubTrayEnsureLive(void)")
        retry = tray[retry_start:tray.index("static void HubTrayInit(void)", retry_start)]
        self.assertIn("if (!g_hubTrayVisible)", retry,
                      "the Hub no longer retries a failed initial NIM_ADD")
        self.assertIn("HubTrayRestore();", retry,
                      "the Hub retry does not recreate its tray icon")
        self.assertIn("HubTrayPublishHostMarker();", retry,
                      "a successful Hub icon cannot recover a failed host-marker create")
        marker_start = tray.index("static void HubTrayPublishHostMarker(void)")
        marker = tray[marker_start:tray.index("static void HubTrayRestore(void)", marker_start)]
        self.assertIn("CreateEventW(NULL, TRUE, TRUE, HUB_TRAY_HOST_EVENT)", marker,
                      "the retry cannot republish the live Hub host capability")
        self.assertIn("HubTrayRemove();", hub_ui)
        self.assertIn("SuiteTaskbarCreatedMessage", host)
        self.assertIn("WM_HUB_TRAYICON", hub)
        hub_resource = (ROOT / "project-hub/src/hub.rc").read_text(encoding="utf-8")
        self.assertRegex(hub_resource, r'(?m)^1 ICON "\.\./hub\.ico"$')
        self.assertIn("g_hubNid.hIcon = g_appIcon;", tray,
                      "the Hub tray is not using its blue/silver Tri-Spoke application icon")
        self.assertIn("Shell_NotifyIconW(NIM_ADD, &g_hubNid)", tray)
        self.assertIn("CreateEventW(NULL, TRUE, TRUE, HUB_TRAY_HOST_EVENT)", tray,
                      "Hub does not publish its successfully-created tray host")
        self.assertIn("CloseHandle(g_trayHostEvent)", tray,
                      "app fallback icons cannot recover after Hub exits")
        self.assertIn("if (!g_hubQuitting)", hub_ui)
        self.assertIn("ShowWindow(hwnd, SW_HIDE);", hub_ui,
                      "normal Hub close no longer hides to the unified tray")
        self.assertIn("ID_TRAY_HUB_EXIT", tray)
        self.assertIn("MF_OWNERDRAW", tray,
                      "the Hub tray context menu still uses the stock unstyled popup rows")
        self.assertIn("SetMenuInfo(menu, &info)", tray,
                      "the menu and submenus do not use the Soft Slate surface")
        self.assertIn("HubTrayDrawMenuItem", tray)
        self.assertIn("RoundRect(dc, hover.left", tray,
                      "selected menu rows no longer use the rounded hover treatment")
        self.assertIn("COL_BTN_HOVER", tray)
        self.assertIn("COL_TEXT_PRIMARY", tray)
        self.assertIn("WM_MEASUREITEM", hub_ui,
                      "the Hub window does not measure its owner-drawn menu rows")
        self.assertIn("WM_DRAWITEM", hub_ui,
                      "the Hub window does not render its owner-drawn menu rows")

        # Every tray-owning native app must suppress only its duplicate icon
        # while the new Hub host is actually present, then keep its fallback.
        app_sources = {
            "Agent Notes": ROOT / "agent-notes" / "src" / "parts" / "wait_alerts.c",
            "Bluetooth Guard": ROOT / "bluetooth-guard" / "src" / "parts" / "window_entry.c",
            "Keyboard Lighting": ROOT / "keyboard-lighting" / "src" / "parts" / "picker_window_entry.c",
            "DNS Pilot": ROOT / "dns-pilot" / "src" / "parts" / "ui.c",
            "Audio Switcher": ROOT / "audio-switcher" / "src" / "parts" / "ui_reconcile.c",
            "Weather Radar": ROOT / "weather-radar" / "src" / "parts" / "alerts.c",
        }
        for name, path in app_sources.items():
            src = path.read_text(encoding="utf-8")
            self.assertIn("SuiteHubOwnsTray()", src,
                          f"{name} keeps a duplicate icon while Hub owns the tray")
            self.assertIn("NIM_DELETE", src,
                          f"{name} cannot remove its duplicate tray icon")
            self.assertIn("NIM_ADD", src,
                          f"{name} cannot restore its fallback tray icon when Hub exits")

        # Every owner receives TaskbarCreated so Explorer restarts do not strand
        # either the unified icon or the independent fallback path.
        taskbar_receivers = {
            "Agent Notes": ROOT / "agent-notes" / "src" / "parts" / "ui.c",
            "Bluetooth Guard": ROOT / "bluetooth-guard" / "src" / "parts" / "window_entry.c",
            "Keyboard Lighting": ROOT / "keyboard-lighting" / "src" / "parts" / "picker_window_entry.c",
            "DNS Pilot": ROOT / "dns-pilot" / "src" / "parts" / "ui.c",
            "Audio Switcher": ROOT / "audio-switcher" / "src" / "parts" / "ui_reconcile.c",
            "Weather Radar": ROOT / "weather-radar" / "src" / "parts" / "ui.c",
        }
        for name, path in taskbar_receivers.items():
            self.assertIn("SuiteTaskbarCreatedMessage()", path.read_text(encoding="utf-8"),
                          f"{name} cannot restore its fallback after an Explorer restart")

        for needle in ("Agent Notes", "Bluetooth Guard", "Prism Lighting", "DNS Pilot",
                       "Audio Switcher", "Weather Report", "Show window", "Hide window", "Quit"):
            self.assertIn(needle, tray,
                          f"the unified Hub tray no longer exposes {needle}")
        self.assertIn("Toggle Guard Enabled", tray)
        self.assertIn("Toggle Lighting On or Off", tray)
        self.assertIn("Toggle DNS Auto-Pilot", tray)
        self.assertIn("--hub-toggle-guard", tray)
        self.assertIn("--hub-toggle", tray)
        self.assertIn("--hub-toggle-auto", tray)
        self.assertIn("ApplyHubGuardToggle", btg)
        self.assertIn("ApplyHubLightingToggle", kbl)
        self.assertIn("RequestHubAutoToggle", dns)
        # Elevated controls and quit are file-based companion requests, never
        # a cross-integrity WM_COMMAND that can be silently blocked by UIPI.
        self.assertNotIn("SendMessage", tray)
        self.assertIn("--hub-exit", tray)
        for src, name in ((btg, "Bluetooth Guard"), (kbl, "Keyboard Lighting"),
                          (dns, "DNS Pilot")):
            self.assertIn("ApplyHubExit", src,
                          f"{name} does not consume the elevated-safe Hub quit token")
            self.assertIn("if (ApplyHubExit()) return 0;", src,
                          f"{name} never applies the Hub exit token on its live timer")
            self.assertIn("OpenMutexW(SYNCHRONIZE", src,
                          f"{name} can leave a Hub token for a future launch")
        self.assertIn('wcsstr(cmdLine, L"--stop")', weather_entry,
                      "Weather Radar still has no explicit non-hiding quit route")

        # Agent Notes waiting and Weather Radar rain alerts remain native
        # Windows notifications through the one Hub icon, not silently lost.
        self.assertIn('L"HubRelay", L"WaitId"', notes_watch)
        self.assertIn("HubTrayPollAgentWait", tray)
        self.assertIn("Agent is waiting for input", tray)
        self.assertIn("NIIF_INFO | (HubTrayAgentWaitSound() ? 0 : NIIF_NOSOUND)", tray)
        wait_alert_start = tray.index("static BOOL HubTrayShowAgentWaitingAlert(void)")
        wait_alert = tray[wait_alert_start:tray.index("\n}\n", wait_alert_start)]
        self.assertIn("BOOL notified = Shell_NotifyIconW(NIM_MODIFY, &g_hubNid);", wait_alert)
        self.assertIn("return notified;", wait_alert,
                      "the Hub cannot tell whether Windows accepted an agent-wait alert")
        wait_poll_start = tray.index("static void HubTrayPollAgentWait(void)")
        wait_poll = tray[wait_poll_start:tray.index("\n}\n", wait_poll_start)]
        self.assertIn("if (!HubTrayShowAgentWaitingAlert()) return;", wait_poll,
                      "a failed wait alert is marked seen instead of retried")
        self.assertLess(wait_poll.index("if (!HubTrayShowAgentWaitingAlert()) return;"),
                        wait_poll.index("wcsncpy(g_hubSeenAgentWait"),
                        "the Hub persists the wait token before Windows accepts its alert")
        weather_poll_start = tray.index("static void HubTrayPollWeatherAlert(void)")
        weather_poll = tray[weather_poll_start:tray.index("\n}\n", weather_poll_start)]
        self.assertIn("BOOL notified = Shell_NotifyIconW(NIM_MODIFY, &g_hubNid);", weather_poll)
        self.assertIn("if (!notified) return;", weather_poll,
                      "a failed rain alert is marked seen instead of retried")
        self.assertLess(weather_poll.index("if (!notified) return;"),
                        weather_poll.index("wcsncpy(g_hubSeenRainAlert"),
                        "the Hub persists the rain token before Windows accepts its alert")
        show_rain = weather_alerts[weather_alerts.index("static void ShowRainAlert("):]
        show_rain = show_rain[:show_rain.index("\n}\n")]
        self.assertIn("PublishHubRainAlert(p, odds);", show_rain)
        self.assertIn("HubTrayPollWeatherAlert", tray)
        self.assertIn("Exact-location ensemble chance", weather_alerts)
        self.assertNotRegex(hub + tray, r"(?i)(RegSetValue.*\\\\Run|schtasks|CreateService)",
                            "the unified Hub tray silently added startup persistence")

    def test_unified_hub_tray_menu_row_paint_is_isolated(self):
        """Hover redraws must repaint the row surface and leave no GDI state
        behind for the next owner-drawn menu item."""
        tray = (ROOT / "project-hub" / "src" / "parts" / "tray_unified.c").read_text(encoding="utf-8")
        content_start = tray.index("static void HubTrayDrawMenuItemContent(")
        content_end = tray.index("\nstatic BOOL HubTrayDrawMenuItem(", content_start)
        content = tray[content_start:content_end]
        draw_start = content_end + 1
        draw_end = tray.index("\nstatic void HubTrayAgentNotesCfg", draw_start)
        draw = tray[draw_start:draw_end]

        self.assertIn("FillRect(dc, &rc, bg);", content,
                      "a row repaint can leave its old hover fill behind")
        self.assertLess(content.index("FillRect(dc, &rc, bg);"),
                        content.index("BOOL selected ="),
                        "the old selection is not erased before deciding whether this row is hovered")
        self.assertIn("int savedDc = SaveDC(draw->hDC);", draw,
                      "a row does not snapshot the menu drawing context")
        self.assertIn("HubTrayDrawMenuItemContent(draw);", draw)
        self.assertIn("if (savedDc) RestoreDC(draw->hDC, savedDc);", draw,
                      "owner-drawn text/background state leaks into subsequent hover repaints")
        self.assertLess(draw.index("SaveDC(draw->hDC)"),
                        draw.index("HubTrayDrawMenuItemContent(draw);"))
        self.assertLess(draw.index("HubTrayDrawMenuItemContent(draw);"),
                        draw.index("RestoreDC(draw->hDC, savedDc)"))

    def test_unified_tray_menu_tracking_cannot_reenter_shared_paint_state(self):
        """TrackPopupMenu pumps owner messages, so a second tray callback must
        not replace the active menu's global item data or background brush."""
        tray = (ROOT / "project-hub" / "src" / "parts" / "tray_unified.c").read_text(encoding="utf-8")
        show_start = tray.index("static void HubTrayShowMenu(HWND hwnd)")
        show_end = tray.index("\nstatic int HubTrayHandleCommand", show_start)
        show = tray[show_start:show_end]

        guard = show.index("if (g_hubTrayMenuTracking) return;")
        reset_items = show.index("g_hubTrayMenuItemCount = 0;")
        create_brush = show.index("g_hubTrayMenuBrush = CreateSolidBrush(COL_CARD_BG);")
        track = show.index("TrackPopupMenu(menu,")
        destroy = show.index("DestroyMenu(menu);", track)
        delete_brush = show.index("DeleteObject(g_hubTrayMenuBrush);", destroy)
        release_tracking = show.index("g_hubTrayMenuTracking = FALSE;", destroy)

        self.assertIn("static BOOL g_hubTrayMenuTracking = FALSE;", tray)
        self.assertLess(guard, reset_items,
                        "a nested tray callback can clear the active menu's item table")
        self.assertLess(guard, create_brush,
                        "a nested tray callback can replace the active menu's background brush")
        self.assertLess(show.index("g_hubTrayMenuTracking = TRUE;"), track,
                        "the guard is not held throughout TrackPopupMenu's message loop")
        self.assertLess(track, destroy)
        self.assertLess(destroy, delete_brush,
                        "menu handles must stop using the shared brush before it is deleted")
        self.assertLess(delete_brush, release_tracking,
                        "reentry must remain blocked until the menu resources are retired")

    def test_agent_notes_is_readable_in_either_windows_theme(self):
        """Agent Notes used to expose the internal session URL despite automatic
        discovery, making a no-setup outbox look broken and leaving a second,
        irrelevant control in the way.  The refreshed app has one composer,
        automatic delivery status, and theme-correct RichEdit text."""
        an = (ROOT / "agent-notes" / "src" / "agent_notes.c").read_text(encoding="utf-8")
        ui = (ROOT / "agent-notes" / "src" / "parts" / "ui.c").read_text(encoding="utf-8")
        en = (ROOT / "agent-notes" / "src" / "parts" / "entry.c").read_text(encoding="utf-8")

        def code(src):
            """Comments must never be able to satisfy an assertion below."""
            src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
            return re.sub(r"//[^\n]*", "", src)

        an_c, ui_c, en_c = code(an), code(ui), code(en)

        # --- the palette follows the owner's Windows theme, both branches ---
        ip = an_c[an_c.index("static void InitPalette(void) {"):]
        ip = ip[:ip.index("\n}\n")]
        self.assertIn("if (AppsUseLightTheme())", ip,
                      "InitPalette never asks which theme the owner uses, so one of the "
                      "two themes is guaranteed to be unreadable")
        self.assertRegex(an_c, r'Themes\\\\Personalize',
                         "theme detection no longer reads the documented Personalize key")
        self.assertEqual(an_c.count("COL_BG      = RGB("), 2,
                         "the palette lost one of its two schemes")
        self.assertNotIn("RGB(24, 27, 36)", ui_c,
                         "the paint code hardcodes the dark backdrop again instead of "
                         "using the theme palette")
        emblem = (ROOT / "agent-notes" / "src" / "parts" / "emblem.c").read_text(encoding="utf-8")
        engine = (ROOT / "shared" / "win32" / "suite_emblem.c").read_text(encoding="utf-8")
        self.assertIn("SuiteEmblemResolvePalette(COL_BG, COL_ACCENT, COL_TXT", emblem,
                      "the spinning emblem uses a fixed pale colour and vanishes in light mode")
        self.assertIn("if (luma >= 160)", engine,
                      "the shared emblem has no light-surface contrast branch")
        self.assertIn("*nearColor = accent;", engine,
                      "the shared emblem ignores the app's readable accent on a light surface")
        self.assertIn("*farColor = text;", engine,
                      "the shared emblem ignores readable primary text on a light surface")

        # --- RichEdit must explicitly receive both existing and future colour ---
        self.assertIn("EM_SETCHARFORMAT", ui_c)
        self.assertIn("cf.dwMask = CFM_COLOR;", ui_c)
        self.assertIn("SCF_SELECTION", ui_c)
        self.assertIn("SCF_DEFAULT", ui_c)
        self.assertIn("EM_GETSEL", ui_c)
        for m in re.finditer(r"EM_SETBKGNDCOLOR", en_c):
            tail = en_c[m.end():m.end() + 200]
            self.assertIn("RichTextColor", tail,
                          "the note editor background has no matching RichEdit text colour")

        note_hint = ui_c[ui_c.index("static void NoteSetHint(int on)"):]
        note_hint = note_hint[:note_hint.index("\n}\n")]
        self.assertIn("RichTextColor", note_hint)
        self.assertRegex(note_hint, r"COL_MUTED : COL_TXT")

        # --- exactly one owner-facing input: the note composer ---
        combined = an_c + ui_c + en_c
        for retired in ("g_hLink", "g_linkHint", "HINT_LINK", "ID_EDIT_LINK",
                        "BTN_SAVE", "LinkSetHint", "Save Link", "Session link"):
            self.assertNotIn(retired, combined,
                             "%s remains in the app, so the owner is still asked to "
                             "manage an automatic session endpoint" % retired)
        self.assertEqual(en_c.count('L"RichEdit50W"'), 1,
                         "Agent Notes exposes more than the one intended note composer")
        self.assertIn("Automatic delivery", ui_c,
                      "the replacement for the removed URL field does not explain delivery")
        self.assertIn("DeliverySummary()", ui_c,
                      "the delivery card does not report its automatic state")
        self.assertIn("Recent activity", ui_c)
        self.assertIn("SetRect(r, S(20), S(410), S(600), S(454));", ui_c,
                      "Send is no longer the full-width primary action")
        self.assertNotIn("S(20) + S(190), S(380) + S(38)", ui_c,
                         "Send shrank back to the old narrow chip")
        paint = ui_c[ui_c.index("static void PaintAll(HWND hwnd)"):]
        paint = paint[:paint.index("\n}\n")]
        self.assertEqual(paint.count('L"Agent Notes"'), 1,
                         "the title is drawn more than once and overlaps itself")

        # Discovery still happens at startup, and later wait/receipt checks use
        # a worker so an unavailable preview link cannot freeze the composer.
        self.assertIn("StartStatusPoll()", ui_c)
        self.assertIn("LinkDiscoveryTick()", en_c)
        self.assertIn("Agent session found - notes will deliver automatically.", en,
                      "startup discovery still leaves the owner looking for a link field")

    def test_agent_notes_waiting_alerts_and_two_stage_receipts(self):
        """A sent HTTP request is not evidence the agent read a note, and an
        app cannot promise an alert while its device/process/network is away.
        The owner selected a native visual wait alert (sound opt-in) plus a
        truthful local-inbox receipt followed by an explicit checkpoint read."""
        app = (ROOT / "agent-notes" / "src" / "agent_notes.c").read_text(encoding="utf-8")
        config = (ROOT / "agent-notes" / "src" / "parts" / "config_outbox.c").read_text(encoding="utf-8")
        http = (ROOT / "agent-notes" / "src" / "parts" / "http_agent.c").read_text(encoding="utf-8")
        watch = (ROOT / "agent-notes" / "src" / "parts" / "wait_alerts.c").read_text(encoding="utf-8")
        ui = (ROOT / "agent-notes" / "src" / "parts" / "ui.c").read_text(encoding="utf-8")
        entry = (ROOT / "agent-notes" / "src" / "parts" / "entry.c").read_text(encoding="utf-8")
        server = (ROOT / "tools" / "notes_server.py").read_text(encoding="utf-8")
        protocol = (ROOT / "tools" / "agent_notes_protocol.py").read_text(encoding="utf-8")
        state = (ROOT / "tools" / "agent_notes_state.py").read_text(encoding="utf-8")
        hub = (ROOT / "project-hub" / "src" / "project_hub.c").read_text(encoding="utf-8")
        workers = (ROOT / "project-hub" / "src" / "parts" / "workers_install.c").read_text(encoding="utf-8")

        def code(src):
            src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
            return re.sub(r"//[^\n]*", "", src)

        config_c, http_c, watch_c, ui_c, entry_c = map(code, (config, http, watch, ui, entry))

        # Receiver-side acceptance means an fsync'ed inbox append plus an
        # opaque receipt. Retry uses a stable client key, so a crash between
        # the two local files recovers instead of duplicating the note.
        for needle in ("/status", "/receipt/", "X-Agent-Note-Key", "Origin"):
            self.assertIn(needle, server,
                          f"API bridge lost {needle}, weakening delivery or its browser boundary")
        for needle in ("receipt_in_inbox", "persist_note", "state_lock"):
            self.assertIn(needle, protocol,
                          f"durable protocol lost {needle}, weakening its receipt guarantees")
        save_state_impl = protocol[protocol.index("def save_state("):]
        save_state_impl = save_state_impl[:save_state_impl.index("\ndef set_waiting(")]
        append_impl = protocol[protocol.index("def append_inbox("):]
        append_impl = append_impl[:append_impl.index("\ndef persist_note(")]
        for durable_write in (save_state_impl, append_impl):
            self.assertIn("f.flush()\n        os.fsync(f.fileno())", durable_write,
                        "a receipt or its inbox note can be acknowledged before disk flush")
        self.assertIn("<!-- AgentNotes key={note_key} receipt={receipt_id} -->", protocol)
        self.assertIn('"state": "saved"', server)
        self.assertIn('"state": "read"', protocol)
        self.assertNotIn("PAGE =", server,
                         "the API bridge grew a second browser-based notes UI")
        self.assertIn("begin_inbox_checkpoint", state)
        self.assertIn("mark_checkpoint_read", state)
        self.assertIn('choices=("waiting", "checkpoint", "mark-read", "clear-wait", "status")', state)
        self.assertIn("protocol.begin_inbox_checkpoint()", state)
        self.assertIn("protocol.mark_checkpoint_read(args.checkpoint)", state)

        # Do not delete a local outbox item merely because a response was 200.
        # Its durable receipt must first be recorded locally; only then may the
        # retry-safe source file disappear.
        self.assertIn("HttpPostNote(g_link, L\"/note\", noteKey", http_c)
        self.assertIn("HistMarkSaved(fd.cFileName, receipt) && DeleteFileW(path)", http_c)
        self.assertLess(http_c.index("HistMarkSaved(fd.cFileName, receipt)"),
                        http_c.index("DeleteFileW(path)"),
                        "Agent Notes deletes an outbox note before saving its inbox receipt")
        self.assertIn("HistMarkReceiptRead", config_c)
        self.assertIn("Saved", ui_c)
        self.assertIn("Read", ui_c)
        self.assertIn("Inbox persistence is confirmed; read acknowledgement follows separately.", ui)

        # A regular Windows notification-area visual is the alert. Sound is
        # quiet by default through NIIF_NOSOUND, persists only if selected,
        # and no custom mixer/sound file is introduced.
        self.assertIn("Shell_NotifyIconW(NIM_ADD", watch_c)
        self.assertIn("Shell_NotifyIconW(NIM_MODIFY", watch_c)
        self.assertIn("NIIF_INFO | (g_waitSound ? 0 : NIIF_NOSOUND)", watch_c)
        self.assertIn("Play a sound for waiting alerts", watch)
        self.assertIn("WaitSound", config_c)
        self.assertIn("StartStatusPoll", watch_c)
        self.assertIn("CreateThread", watch_c)
        self.assertIn("TIMER_WATCH, 15000", entry_c)
        self.assertIn("            StartStatusPoll();\n", ui_c,
                      "the live watcher timer no longer starts its background session poll")
        self.assertIn("ShowWindow(hwnd, SW_HIDE)", ui_c)
        self.assertIn("Exit (stop waiting alerts)", watch)
        self.assertIn("if (wcscmp(result->waitId, g_seenWait) != 0)", watch_c,
                      "a freshly published agent wait no longer produces a native alert")
        self.assertNotRegex(app + watch + entry, r"(?i)(RegSetValue.*\\\\Run|schtasks|CreateService)",
                            "Agent Notes silently added a sign-in/service startup path")

        # The tray watcher must have an explicit updater exit route. Ordinary
        # WM_CLOSE now hides it, so Hub asks the app-owned elevated --stop
        # companion and still applies the managed-copy verification gate.
        self.assertIn("AGENT_NOTES_EXIT_COMMAND", hub)
        self.assertIn("HubRequestAgentNotesExit(hNotes)", workers)
        self.assertIn('stop.lpVerb = L"runas";', workers)
        self.assertIn('stop.lpParameters = L"--stop";', workers)
        self.assertIn('L\"Agent Notes\", AGENT_NOTES_EXIT_COMMAND', workers)
        self.assertIn('wcsstr(cmdLine, L"--stop")', entry_c)

        rules = (ROOT / ".agent" / "rules" / "execution.md").read_text(encoding="utf-8")
        for needle in ("agent_notes_state.py checkpoint", "agent_notes_state.py mark-read",
                       "agent_notes_state.py waiting", "agent_notes_state.py clear-wait",
                       "notes_server.py"):
            self.assertIn(needle, rules,
                          f"the agent contract omits {needle}, so read/wait state can drift")
        ignored = (ROOT / ".gitignore").read_text(encoding="utf-8")
        self.assertIn(".agent/NOTES_STATE.json", ignored,
                      "private receipt/wait state could leak into the public repository")

    def test_card_detail_line_survives_hidden_buttons(self):
        """The strip under each card title went blank, and it was our bug.

        The detail line's right edge was the leftmost button's left edge - but
        the leftmost button is the dynamic Update button, which HIDES itself
        on an up-to-date card, and a hidden button keeps a zero rect. So the
        detail rect collapsed to right < left and DrawText drew nothing: every
        up-to-date card lost its summary line. The owner saw dead space and
        asked for it back, doing something useful.

        It now bounds itself by the leftmost VISIBLE button, and the line
        carries live facts: how long a running app has been up (from its lock
        file's write time) and which released build the up-to-date verdict
        was made against. Appends must come after the state branch that sets
        the base text, or they land in an empty string and vanish.
        """
        ui = (ROOT / "project-hub" / "src" / "parts" / "ui_renderer.c").read_text(encoding="utf-8")
        rs = (ROOT / "project-hub" / "src" / "parts" / "refresh_statuses.c").read_text(encoding="utf-8")

        self.assertNotIn("int firstBtnLeft = (card->btnCount > 0) ? card->btns[0].rc.left", ui,
                         "the detail rect is bounded by btns[0] again, whose zero "
                         "rect blanks the line whenever Update hides itself")
        self.assertIn("card->btns[b].rc.left > 0", ui,
                      "the detail rect does not skip hidden buttons' zero rects")
        for needle in ("DetailAppendUp", "DetailAppendBuild", "build %.7S"):
            self.assertIn(needle, rs, "the card detail line lost a live fact")
        self.assertIn("DetailAppendUp(0, btgLock)", rs,
                      "Bluetooth Guard no longer reports its uptime")
        self.assertIn("DetailAppendUp(1, kblLock)", rs,
                      "Keyboard Lighting no longer reports its uptime")
        self.assertIn("DetailAppendBuild(1, ", rs,
                      "Keyboard Lighting no longer reports the released build its "
                      "up-to-date verdict was made against")
        self.assertIn("DetailAppendBuild(4, ", rs,
                      "DNS Pilot no longer reports the released build its up-to-date "
                      "verdict was made against")
        self.assertLess(rs.index("Phone audio protection"), rs.index("DetailAppendUp(0, btgLock)"),
                        "the uptime append runs before the base detail text is "
                        "set, so it is written into an empty string and lost")
        self.assertLess(rs.index("ROG Strix G16 RGB lighting"), rs.index("DetailAppendUp(1, kblLock)"),
                        "the KBL uptime append runs before its base detail text")

    def test_tooling_cache_registry_and_report(self):
        """The three testing tools must exist and keep their promises.

        Incremental builds: the skip cache lives outside the repo (workspace
        snapshots stay lean), only fires when the EXE is actually present, and
        --force bypasses it - a cache that cannot be bypassed is a lie waiting
        to happen. The key hashes the whole src dir, so a resource or icon
        swap invalidates it just like a source edit.

        Mutation registry: every break-on-purpose ever written stays alive in
        tools/mutations.json and is re-run by tools/mutate.py. The drift guard
        here is the important half - a mutation whose needle no longer matches
        the source would silently stop testing anything, so the registry is
        asserted applicable on every suite run.

        Suite report: the owner's one double-click that collects what the
        sandbox cannot see. It must stay out of the repo root (root .bat files
        are reserved), stay pure CRLF like every other .bat, and keep reading
        the released marker, the engine log and the process table.
        """
        import json as _json
        bw = (ROOT / "tools" / "build_windows.py").read_text(encoding="utf-8")
        self.assertIn("/home/user/.cache/winbuild", bw,
                      "the build cache moved into the repo, which bloats every "
                      "workspace snapshot")
        self.assertIn("--force", bw, "the cache cannot be bypassed")
        self.assertIn("rglob", bw,
                      "the cache key no longer covers the whole src dir, so a "
                      "resource or icon change would reuse a stale EXE")
        self.assertIn("TARGET_SHARED_SOURCES", bw,
                      "a shared native source can change without invalidating its consumer EXE")
        self.assertIn("target_key(name, rc_dir, libs, zig_ver)", bw,
                      "the build key does not identify which shared sources a target consumes")
        self.assertIn("out_exe.exists()", bw,
                      "the cache would claim success for an EXE that is not there")

        reg = _json.loads((ROOT / "tools" / "mutations.json").read_text(encoding="utf-8"))
        self.assertGreaterEqual(len(reg), 19,
                                "the mutation registry lost entries")
        ids = [e["id"] for e in reg]
        self.assertEqual(len(ids), len(set(ids)), "duplicate mutation ids")
        for e in reg:
            p = ROOT / e["file"]
            self.assertTrue(p.exists(), "mutation %s points at a missing file" % e["id"])
            src = p.read_text(encoding="utf-8")
            if e["file"] != MUTATION_FILE:
                self.assertGreaterEqual(src.count(e["old"]), 1,
                                        "mutation %s no longer applies - its needle "
                                        "drifted and it now tests nothing" % e["id"])
        mu = (ROOT / "tools" / "mutate.py").read_text(encoding="utf-8")
        for needle in ("SURVIVED", "STALE", "--list"):
            self.assertIn(needle, mu, "the mutation runner lost a mode")

        bat = (ROOT / "project-hub" / "SuiteReport.bat").read_bytes()
        self.assertIn(b"\r\n", bat, "the report bat has no CRLF at all")
        self.assertNotIn(b"\n", bat.replace(b"\r\n", b""),
                         "the report bat is not pure CRLF")
        self.assertFalse((ROOT / "SuiteReport.bat").exists(),
                         "a new .bat appeared at the repo root, which is reserved")
        ps = (ROOT / "project-hub" / "SuiteReport.ps1").read_text(encoding="utf-8")
        for needle in ("SuiteReport.txt", ".agent/RELEASED", "engine.log",
                       "Get-Process -Name"):
            self.assertIn(needle, ps,
                          "the suite report no longer collects %s" % needle)

    def test_hub_and_tray_only_consume_released_work(self):
        """The hub must not see work the agent has not finished.

        The owner got update nudges while a task was still in progress: the
        hub probed the branch tip, and every mid-task save looked like a
        release. The fix is a released marker - a two-line file naming the
        commit that holds finished work and the hub version that went with
        it. The marker is the ONLY read taken from the branch tip; every
        other probe, and every download, uses the commit it blesses. No
        marker (offline, a branch that predates the file) falls back to the
        old behaviour rather than brining the hub to a halt.

        The same rule reaches the lighting tray: its background refresh and
        the new -Restart sync both resolve the released commit first. The
        sync exists because Restart.bat used to relaunch whatever EXE was on
        disk, and the EXE only changed when Setup ran - so "restart" quietly
        meant "restart the old build".
        """
        hub = ROOT / "project-hub" / "src"
        wr = (hub / "parts" / "workers_remote.c").read_text(encoding="utf-8")
        fold = (hub / "parts" / "install_folder.c").read_text(encoding="utf-8")
        wi = (hub / "parts" / "workers_install.c").read_text(encoding="utf-8")
        tray = (ROOT / "keyboard-lighting" / "Tray.ps1").read_text(encoding="utf-8")
        main = (hub / "project_hub.c").read_text(encoding="utf-8")

        self.assertIn(".agent/RELEASED", wr,
                      "the hub never reads the released marker, so branch-tip work "
                      "is still visible mid-task")
        self.assertIn('HttpGetCached(L"raw.githubusercontent.com", murl', wr,
                      "the marker URL is built but never fetched, so the hub still "
                      "probes the branch tip for everything")
        self.assertIn("commit=", wr, "the marker's commit line is not parsed")
        self.assertIn("hub=", wr, "the marker's hub version line is not parsed")
        self.assertIn("if (!probeRef[0])", wr,
                      "with no marker the hub has no fallback ref and would probe "
                      "an empty path")
        self.assertIn("if (!hubFromMarker)", wr,
                      "the branch-tip version probe still runs when the marker "
                      "answered, which is exactly the mid-task nudge")
        worker_start = wr.index("static void WorkerCheckRemote(void)")
        worker_body = wr[worker_start:]
        self.assertLess(worker_body.index(".agent/RELEASED"),
                        worker_body.index("FetchRemoteTree(probeRef"),
                        "the trees probe runs before the marker is read, so it "
                        "cannot honour it")
        self.assertEqual(wr.count("REPO_NAME_W, branch"), 2,
                         "more than the marker and its guarded fallback read the "
                         "branch tip")
        first = wr.index("REPO_NAME_W, branch")
        second = wr.index("REPO_NAME_W, branch", first + 1)
        self.assertIn(".agent/RELEASED", wr[first - 120:first + 40],
                      "the first branch-tip read is not the marker fetch")
        self.assertLess(wr.index("if (!hubFromMarker)"), second,
                        "the branch-tip version probe is not inside the marker "
                        "fallback")
        self.assertEqual(wr.count("REPO_NAME_W, probeRef"), 10,
                         "some probes still bypass the captured released ref")

        h = fold[fold.index("static void EnsureProbeRef("):]
        h = h[:h.index("\n}\n")]
        self.assertLess(h.index("g_probeRef[0]"), h.index("EnsureActiveBranch"),
                        "EnsureProbeRef prefers the branch over the released ref")
        self.assertEqual(wi.count("EnsureActiveBranch("), 0,
                         "an install worker still downloads from the branch tip")
        self.assertGreaterEqual((wi + (ROOT / "project-hub/src/parts/orca_plugins.c").read_text(encoding="utf-8")).count("EnsureProbeRef(branch, 64);"), 6,
                         "not every install worker honours the released ref")

        # The tray obeys the same marker, and Restart syncs before starting.
        self.assertIn(".agent/RELEASED", tray,
                      "the tray still pulls scripts from the branch tip mid-task")
        self.assertIn("function Sync-FromBranch", tray,
                      "Restart has no sync step, so it relaunches the on-disk build")
        self.assertLess(tray.index("function Sync-FromBranch"), tray.index("if ($Restart) {"),
                        "Sync-FromBranch is defined after the -Restart block runs, "
                        "so the call would throw")
        rb = tray[tray.index("if ($Restart) {"):]
        rb = rb[:rb.index("\n}\n")]
        self.assertIn("Sync-FromBranch", rb,
                      "the -Restart path never calls the sync")
        self.assertLess(rb.index("Stop-AllInstances"), rb.index("Sync-FromBranch"),
                        "the sync replaces the EXE before the old instance is "
                        "stopped, so the file is still locked")
        sy = tray[tray.index("function Sync-FromBranch"):]
        sy = sy[:sy.index("\n}\n", sy.index("foreach"))]
        self.assertIn("KeyboardLighting.exe", sy,
                      "the restart sync refreshes scripts but not the EXE, which is "
                      "the half that was actually stale")
        self.assertIn("} -ArgumentList (Get-UpdateBase), $Here", tray,
                      "the background refresh still uses the branch tip")

        # The marker in the repo is the release record: keep it honest.
        mk = (ROOT / ".agent" / "RELEASED").read_text(encoding="utf-8")
        import re as _re
        self.assertRegex(mk, r"(?m)^commit=[0-9a-f]{7,40}$",
                         "the released marker has no usable commit line")
        marker_sha = _re.search(r"(?m)^commit=([0-9a-f]{7,40})$", mk).group(1)
        marker_hub = _re.search(r"(?m)^hub=(\d+\.\d+\.\d+)$", mk)
        self.assertIsNotNone(marker_hub, "the released marker has no hub version")
        marker_hubver = marker_hub.group(1)
        show = subprocess.run(
            ["git", "show", "%s:project-hub/src/project_hub.c" % marker_sha],
            cwd=ROOT, capture_output=True, text=True)
        if show.returncode == 0:
            # The release marker names an immutable implementation commit;
            # compare its version, not a newer branch-tip candidate.
            released_source = show.stdout
            hubver = _re.search(r'#define HUB_VERSION\s+"([0-9.]+)"', released_source).group(1)
            self.assertEqual(marker_hubver, hubver,
                             "the marker's hub version does not match the source it blesses")
        else:
            # During the first (implementation) commit, the old marker is
            # intentionally retained until the separate release-marker commit.
            # A shallow checkout may omit the old marker target, so allow the
            # marker version to trail the checked-out candidate, but never lead it.
            candidate = _re.search(r'#define HUB_VERSION\s+"([0-9.]+)"', main).group(1)
            self.assertLessEqual(tuple(map(int, marker_hubver.split("."))),
                                 tuple(map(int, candidate.split("."))),
                                 "an unavailable release marker is newer than the branch source")

    def test_hub_settings_page_writes_the_apps_own_files(self):
        """The hub's Settings page must edit, not duplicate, each app's config.

        The owner chose one settings page in the hub over per-app dialogs.
        The risk with a central page is a second schema: the hub keeping its
        own copy of what each setting means, drifting from the app that owns
        it. Keyboard Lighting keeps preferences in panel.json and applies its
        live engine inbox through theme.json; the Hub must update both. Hold
        mode still belongs in hold_mode.txt, DNS Pilot in config.ini, and the
        three hold strings must match the engine so no choice silently does
        nothing.

        The page is also asserted to stay out of the Desktop entirely: it is
        a settings editor, and shortcut creation has exactly one door left
        in the whole suite (the emblem menu pin).
        """
        hub = ROOT / "project-hub" / "src"
        main = (hub / "project_hub.c").read_text(encoding="utf-8")
        rend = (hub / "parts" / "ui_renderer.c").read_text(encoding="utf-8")
        page = (hub / "parts" / "settings_page.c").read_text(encoding="utf-8")
        fold = (hub / "parts" / "install_folder.c").read_text(encoding="utf-8")
        pick = (hub / "parts" / "branch_picker_entry.c").read_text(encoding="utf-8")
        engine = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")
        tray = (ROOT / "keyboard-lighting" / "Tray.ps1").read_text(encoding="utf-8")

        self.assertIn('#include "parts/settings_page.c"', main,
                      "the settings page is not part of the hub build")
        self.assertIn("#define ID_BTN_SETTINGS", main,
                      "the header has no Settings button id")
        btns = main[main.index("g_topBtns[5]"):]
        self.assertIn('L"Settings"', btns[:btns.index("};")],
                      "the header button table has no Settings button")
        self.assertIn("for (int i = 4; i >= 0; i--)", rend,
                      "the header layout still places four buttons, so the "
                      "fifth is never positioned")
        self.assertIn("for (int i = 0; i < 5; i++) {\n        if (g_topBtns[i].hidden) continue;\n        if (PtInRect(&g_topBtns[i].rc, pt))",
                      rend, "the hit test cannot reach the fifth button")

        # The page offers exactly the modes the engine accepts, no more.
        eng = set(re.findall(r"\$m -eq '([a-z]+)'", engine))
        hub_modes = set(re.findall(r'"([a-z]+)"',
                                   page[page.index("kSetModes[3]"):page.index("];", page.index("kSetModes[3]"))]))
        self.assertEqual(eng, hub_modes,
                         "the settings page offers hold modes the engine would "
                         "reject, or is missing one it accepts")

        hold = page[page.index("static BOOL ApplyHoldMode("):]
        hold = hold[:hold.index("\n}\n")]
        self.assertIn("hold_mode.txt", hold,
                      "the hold mode is written somewhere other than the file the "
                      "engine and the tray submenu use")
        self.assertIn("WriteBinaryFileAtomic", hold,
                      "the hold mode is written non-atomically, so the engine can "
                      "read a half-written mode")
        self.assertIn("int savedMode = HoldModeCurrent();", page,
                      "a failed hold-mode save leaves the radio group lying about its value")
        self.assertIn("i == savedMode ? BST_CHECKED : BST_UNCHECKED", page)
        smooth = page[page.index("static BOOL ApplySmooth("):]
        smooth = smooth[:smooth.index("\n}\n")]
        self.assertIn('ApplyKeyboardSetting(NULL, "Smooth", on)', smooth,
                      "the Steady Colours toggle no longer updates Keyboard Lighting")
        balance_setting = page[page.index("static BOOL ApplyBalance("):]
        balance_setting = balance_setting[:balance_setting.index("\n}\n")]
        self.assertIn('ApplyKeyboardSetting("Kbd", "Equalise", on)', balance_setting,
                      "Balance Colours no longer targets the keyboard's own group")
        kbl_write = page[page.index("static BOOL ApplyKeyboardSetting("):]
        kbl_write = kbl_write[:kbl_write.index("\nstatic void ModNamesW", 1)]
        self.assertIn('L"panel.json"', kbl_write,
                      "Keyboard Lighting preferences are not persisted to panel.json")
        self.assertIn('L"theme.json"', kbl_write,
                      "Keyboard Lighting changes no longer reach the live engine")
        self.assertIn("WriteBinaryFileAtomic(panelPath", kbl_write,
                      "panel.json is not written atomically")
        self.assertIn("WriteBinaryFileAtomic(themePath", kbl_write,
                      "theme.json is not written atomically")
        self.assertIn("restore the saved file", kbl_write,
                      "a failed live update can leave Keyboard Lighting's two settings sources apart")
        self.assertIn("function Sync-ExternalPanelCfg {", tray,
                      "the tray can overwrite a Hub change from its stale in-memory panel settings")
        sync = tray[tray.index("function Sync-ExternalPanelCfg {"):]
        sync = sync[:sync.index("\n}\n")]
        for required in ("Load-Cfg", "Load-UiFromCfg", "Write-Theme"):
            self.assertIn(required, sync,
                          "external panel changes do not refresh the tray UI and live theme")
        watch = tray[tray.index("$watch.Add_Tick({"):]
        watch = watch[:watch.index("$watch.Start()")]
        self.assertIn("$script:tick++\n    Sync-ExternalPanelCfg", watch,
                      "the tray does not notice Hub settings before its next UI write")
        self.assertIn("$script:PanelStamp = Get-PanelStamp", tray,
                      "the tray does not advance the panel timestamp after its own save")
        self.assertIn('L"Balance Colours for the keyboard"', page,
                      "the current keyboard balance preference is missing from Hub Settings")
        self.assertIn('L"Steady Colours (off uses fine dithering)"', page,
                      "the current steady/dither setting is mislabeled in Hub Settings")
        self.assertIn("wc.hbrBackground = NULL;", page,
                      "the Settings window can erase to the system's white window brush")
        self.assertIn("ApplyModernWindowFrame(g_setHwnd);", page,
                      "Settings does not use the Hub's dark native window frame")
        self.assertIn("DrawSettingsCard(dc, dpi, client.right", page,
                      "Settings cards are not inset to the live window width")
        self.assertIn("Brightness hotkeys:", page,
                      "the existing read-only global brightness-hotkey summary was lost")
        self.assertIn("UpdateHotkeysSummary();", page,
                      "the displayed global hotkeys are not refreshed from presets.ini")
        self.assertIn("SetWindowTextW(g_setHotkeys, line);", page,
                      "the summary no longer displays the resolved hotkey names")
        for key in ('L"UpMod"', 'L"UpKey"', 'L"DownMod"', 'L"DownKey"'):
            self.assertIn(key, page,
                          "the hotkey summary reads a different presets.ini key than the panel")
        self.assertIn("HoldSweepReady()", page,
                      "the selected hold mode has no usable M4/M5 readiness check")
        self.assertIn("M4/M5 hold signals are still learning", page,
                      "the page does not explain why sweep-while-held may not be ready")
        self.assertIn("Press-to-glide works without key recognition", page,
                      "the page hides the fallback that does not depend on key learning")
        self.assertIn("SetTimer(g_setHwnd, 1, 1000, NULL)", page,
                      "key-recognition status is not refreshed while Settings is open")
        self.assertIn("static const int kSetIntervals[6] = { 0, 15, 30, 60, 120, 300 }", page,
                      "the page's DNS choices do not match DNS Pilot's interval cycle")
        self.assertIn("Auto (live watch)", page,
                      "DNS Pilot's 0-minute live-watch mode is mislabeled or unavailable")
        self.assertIn("if (selIdx < 0)", page,
                      "an unsupported DNS interval can still appear as an unsupported choice")
        self.assertIn('WritePrivateProfileStringW(L"Pilot", L"IntervalMin", valid, cfg)', page,
                      "an unsupported DNS interval is not normalized before it is displayed")
        self.assertIn("DrawSettingsCard", page,
                      "the Settings page no longer uses the Suite's card layout")
        self.assertIn("COL_CARD_BG", page)
        self.assertIn("COL_TEXT_PRIMARY", page)
        self.assertIn('GetProcAddress(g_settingsThemeModule', page,
                      "the themed checkbox/combo renderer can still ignore the page's "
                      "readable Soft Slate colors")
        self.assertIn('"SetWindowTheme"', page)
        self.assertIn('g_settingsSetWindowTheme(ctl, L"", L"")', page,
                      "native controls are not opted out of system black text on dark cards")
        self.assertIn('lstrcmpiW(cls, L"BUTTON") == 0', page)
        self.assertIn("SetBkMode(dc, TRANSPARENT);", page)
        self.assertIn("WritePrivateProfileStringW", page,
                      "DNS Pilot's config is not written through the same INI API "
                      "the app reads with")
        for key in ("L\"Pilot\"", "L\"Auto\"", "L\"IntervalMin\""):
            self.assertIn(key, page, "the page writes a different INI key than DNS Pilot reads")

        self.assertEqual(page.count("CreateShortcutLink"), 0,
                         "the settings page creates shortcuts, which is not its job")
        self.assertEqual(page.count("CSIDL_DESKTOP"), 0,
                         "the settings page touches the Desktop at all")

        # One-time cleanup of the shortcuts that already accumulated.
        cl = fold[fold.index("static void RemoveLegacyDesktopShortcuts(void) {"):]
        cl = cl[:cl.index("\n}\n")]
        self.assertEqual(cl.count('.lnk"'), 5,
                         "the legacy cleanup no longer covers exactly the five names "
                         "the suite used to put on the Desktop")
        self.assertIn('L"Project Hub.lnk"', cl,
                      "the hub's own old Desktop shortcut is left behind")
        self.assertIn("desktop_cleanup_done", cl,
                      "the cleanup has no once-only flag, so it would delete a "
                      "shortcut the owner pins later")
        self.assertIn("GetFileAttributesW(flag)", cl,
                      "the once-only flag is never checked, so the cleanup runs on "
                      "every launch and would delete a shortcut pinned later")
        self.assertLess(cl.index("GetFileAttributesW(flag)"), cl.index("DeleteFileW"),
                        "the once-only flag is checked after the deletion, so it "
                        "guards nothing")
        self.assertIn("RemoveLegacyDesktopShortcuts();", pick,
                      "nothing calls the legacy Desktop cleanup, so the old icons "
                      "stay until they are deleted by hand")
        self.assertLess(pick.index("InitAllPaths();"), pick.index("RemoveLegacyDesktopShortcuts();"),
                        "the cleanup runs before the folders it needs are resolved")

    def test_hub_keyboard_settings_json_updates_are_scoped_and_preserve_other_groups(self):
        """Run the Hub's actual JSON patcher against current and legacy files."""
        import shutil
        import tempfile

        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            self.skipTest("a native C compiler is required for the portable JSON helper test")
        page = (ROOT / "project-hub" / "src" / "parts" / "settings_page.c").read_text(
            encoding="utf-8")
        first = page[page.index("static BOOL JsonSpace("):]
        first = first[:first.index("\nstatic int KeyboardSettingCurrent", 1)]
        second = page[page.index("static BOOL AddJsonEntry("):]
        second = second[:second.index("\nstatic int SmoothCurrent", 1)]
        harness = r'''#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define BOOL int
#define DWORD uint32_t
#define TRUE 1
#define FALSE 0
#define _snprintf snprintf
''' + first + second + r'''
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "failed at C line %d\n", __LINE__); return __LINE__; } } while (0)
int main(void) {
    const char *live = "{\"Brightness\":1,\"Smooth\":false,\"Kbd\":{\"Equalise\":true,\"Colors\":\"#2814FF\"},\"Bar\":{\"Equalise\":false}}";
    char *out = NULL, *open = NULL, *close = NULL;
    DWORD n = 0; int value = -1;
    CHECK(BuildJsonBoolUpdate(live, (DWORD)strlen(live), NULL, "Smooth", 1, &out, &n));
    CHECK(JsonReadSetting(out, n, NULL, "Smooth", &value) && value == 1);
    free(out); out = NULL;
    CHECK(BuildJsonBoolUpdate(live, (DWORD)strlen(live), "Kbd", "Equalise", 0, &out, &n));
    CHECK(JsonReadSetting(out, n, "Kbd", "Equalise", &value) && value == 0);
    CHECK(JsonObjectSpan(out, out + n, "Bar", &open, &close));
    CHECK(JsonReadBool(open, close, "Equalise", &value) && value == 0);
    free(out); out = NULL;
    const char *legacy = "{\"Effect\":\"gradient\",\"Equalise\":false}";
    CHECK(BuildJsonBoolUpdate(legacy, (DWORD)strlen(legacy), "Kbd", "Equalise", 1, &out, &n));
    CHECK(JsonReadSetting(out, n, "Kbd", "Equalise", &value) && value == 1);
    CHECK(!JsonObjectSpan(out, out + n, "Kbd", &open, &close));
    free(out); out = NULL;
    const char *barOnly = "{\"Bar\":{\"Equalise\":false}}";
    CHECK(BuildJsonBoolUpdate(barOnly, (DWORD)strlen(barOnly), "Kbd", "Equalise", 1, &out, &n));
    CHECK(JsonObjectSpan(out, out + n, "Kbd", &open, &close));
    CHECK(JsonReadBool(open, close, "Equalise", &value) && value == 1);
    CHECK(JsonObjectSpan(out, out + n, "Bar", &open, &close));
    CHECK(JsonReadBool(open, close, "Equalise", &value) && value == 0);
    free(out); out = NULL;
    CHECK(BuildJsonBoolUpdate(NULL, 0, "Kbd", "Equalise", 0, &out, &n));
    CHECK(JsonReadSetting(out, n, "Kbd", "Equalise", &value) && value == 0);
    free(out); out = NULL;
    const char bom[] = "\xEF\xBB\xBF{\"Smooth\":true}";
    CHECK(BuildJsonBoolUpdate(bom, (DWORD)(sizeof(bom) - 1), NULL, "Smooth", 0, &out, &n));
    CHECK(JsonReadSetting(out, n, NULL, "Smooth", &value) && value == 0);
    free(out);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as td:
            src = Path(td) / "settings_json_test.c"
            exe = Path(td) / "settings_json_test"
            src.write_text(harness, encoding="utf-8")
            built = subprocess.run(
                [compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", str(src), "-o", str(exe)],
                capture_output=True, text=True)
            self.assertEqual(built.returncode, 0,
                             "the portable settings JSON helper did not compile: " + built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0,
                             "Keyboard Lighting settings JSON patch failed at C line " + ran.stderr)

    def test_hub_repoints_desktop_shortcuts_but_never_creates_them(self):
        """Every install and update used to drop a Desktop shortcut per app.

        That is how they accumulated: the hub's install workers and each app's
        Setup re-created the .lnk on every run, so an owner who deleted one
        got it back at the next update. The rule now is that the Desktop is
        not ours to fill in. A shortcut the owner pinned is still repointed
        after an update so it never dangles at a replaced copy, but creation
        happens only through the explicit pin item in the emblem menu.

        The guard lives in one helper so there is exactly one place the rule
        can be checked, and the install paths are asserted to contain no
        direct shortcut creation at all - a worker that grows a new
        CreateShortcutLink call fails this even if nobody notices the icon.
        """
        parts = ROOT / "project-hub" / "src" / "parts"
        folder = (parts / "install_folder.c").read_text(encoding="utf-8")
        workers = (parts / "workers_install.c").read_text(encoding="utf-8")
        cards = (parts / "card_defs_verbs.c").read_text(encoding="utf-8")
        picker = (parts / "branch_picker_entry.c").read_text(encoding="utf-8")

        h = folder.index("static BOOL RepointDesktopShortcutIfPresent(")
        hbody = folder[h:folder.index("\n}\n", h) + 3]
        self.assertIn("GetFileAttributesW(lnkPath) == INVALID_FILE_ATTRIBUTES", hbody,
                      "the repoint helper no longer checks that the shortcut exists, "
                      "so it creates one whenever it is called")
        self.assertLess(hbody.index("GetFileAttributesW"), hbody.index("CreateShortcutLink("),
                        "the existence check runs after the creation, so it guards nothing")

        for name, src in (("workers_install.c", workers), ("card_defs_verbs.c", cards)):
            self.assertEqual(src.count("CreateShortcutLink("), 0,
                             "%s creates a Desktop shortcut directly, so the next "
                             "update drops a new icon on the Desktop" % name)
            self.assertEqual(src.count("CSIDL_DESKTOPDIRECTORY"),
                             src.count("RepointDesktopShortcutIfPresent(") +
                             src.count("RepointAppDesktopShortcutsIfPresent("),
                             "%s touches the Desktop outside the repoint helpers" % name)

        pin = picker[picker.index("case ID_MENU_SHORTCUT:"):]
        pin = pin[:pin.index("case ID_MENU_SUITE_DIR:")]
        self.assertIn("CreateShortcutLink(", pin,
                      "the explicit pin item no longer creates a shortcut, so there is "
                      "no way left to get one on purpose")
        tos = picker[picker.index("case ID_MENU_HUB_TOSUITE:"):]
        tos = tos[:tos.index("SetFooterStatus", tos.index("repointed"))]
        self.assertEqual(tos.count("CreateShortcutLink("), 0,
                         "the consolidate menu item creates a Desktop shortcut again")

    def test_hub_self_update_keeps_a_suite_copy_and_repoints_the_shortcut(self):
        src = self._hub_src()

        self.assertIn(r'_snwprintf(g_suiteHubExe, MAX_PATH, L"%s\\ProjectHub.exe", g_suiteDir);',
                      src, "the hub lost its canonical suite-folder copy path")
        start = src.index("static void WorkerSelfUpdateHub(void)")
        body = src[start:start + 4200]
        self.assertIn("DownloadRepoBytes(branch, L\"project-hub/ProjectHub.exe\", &newLen)", body,
                      "the self-update no longer holds the downloaded bytes, so it "
                      "cannot write a second copy")
        self.assertIn("WriteBinaryFileAtomic(g_suiteHubExe, newBytes, newLen)", body,
                      "a hub launched from Downloads updates only itself and leaves "
                      "the suite folder holding the previous build")
        self.assertIn("RepointDesktopShortcutIfPresent(lnk, g_suiteHubExe, g_suiteDir", body,
                      "after a self-update a pinned Desktop shortcut still points at "
                      "the unmanaged copy")
        self.assertIn("free(newBytes);", body, "the downloaded hub bytes leak")
        self.assertIn('L"%s.pending.exe", g_exePath', body,
                      "the self-update no longer stages a separate handoff executable")
        self.assertNotIn("MoveFileExW(g_exePath,", body,
                         "the worker swaps the interactive Hub while it is still running")

        handoff = (ROOT / "project-hub" / "src" / "parts" / "branch_picker_entry.c").read_text(encoding="utf-8")
        apply_start = handoff.index("static BOOL ApplyStagedHubUpdate(")
        apply = handoff[apply_start:handoff.index("int WINAPI wWinMain", apply_start)]
        self.assertIn("WaitForSingleObject(oldProcess, 30000)", apply,
                      "the staged updater does not wait for the old Hub to close")
        self.assertIn("if (!NoProcessRunsImage(target)) return FALSE;", apply,
                      "the staged updater swaps a Hub image without proving it is no longer running")
        self.assertIn("MoveFileExW(target, old, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)", apply)
        self.assertIn("MoveFileExW(pending, target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)", apply)
        self.assertIn('L"--apply-self-update=%lu"', handoff,
                      "the old Hub relaunches immediately instead of using the safe staged handoff")

        # One click to consolidate without waiting for the next self-update.
        self.assertIn("#define ID_MENU_HUB_TOSUITE", src)
        self.assertIn('L"Install Hub into Suite Folder"', src,
                      "the utility menu lost the one-click hub consolidation entry")
        self.assertIn("case ID_MENU_HUB_TOSUITE:", src,
                      "the suite-folder hub menu entry is not handled")

    def test_update_all_refreshes_every_builtin_app_suite_folder(self):
        src = self._hub_src()
        start = src.index("case TASK_UPDATE_ALL:")
        block = src[start:src.index("break;", start)]

        # Slots 0-5 have dedicated workers (each with its own MIRROR_ set) and
        # slots 6+ go through the generic worker. Agent Notes (slot 5) was the
        # one dedicated worker missing here, so "Update All" left its suite copy
        # stale while every other app updated - the owner's "the suite folders
        # don't actually update" for at least one app.
        for call in ("WorkerInstallBluetoothGuard(FALSE)",
                     "WorkerInstallKeyboardLighting()",
                     "WorkerInstallOrcaPlugin(0)",
                     "WorkerInstallOrcaPlugin(1)",
                     "WorkerInstallSpatialHud()",
                     "WorkerInstallDnsPilot(FALSE)",
                     "WorkerInstallNotes(FALSE)"):
            self.assertIn(call, block,
                          f"Update All no longer calls {call}, so that app's suite "
                          f"folder keeps the previous build on a one-click update")
        self.assertIn("WorkerInstallGeneric(6, FALSE);", block)
        self.assertRegex(block, r"for \(int i = BUILTIN_CARD_COUNT; i < g_cardCount; i\+\+\)\s+"
                                r"WorkerInstallGeneric\(i, FALSE\);",
                         "Update All must sweep EXE extras without installing a plugin as an EXE")
        self.assertIn("WorkerCheckRemote();", block,
                      "Update All does not re-check the remote afterwards, so the "
                      "badges would still read stale after a successful update")

    def test_hub_version_advanced_for_the_suite_folder_install_fix(self):
        src = self._hub_src()
        m = re.search(r'#define HUB_VERSION\s+"(\d+)\.(\d+)\.(\d+)"', src)
        self.assertIsNotNone(m, "HUB_VERSION is missing or malformed")
        version = tuple(int(g) for g in m.groups())
        self.assertGreaterEqual(version, (2, 8, 39),
                                "the hub version was not bumped for the Update All "
                                "coverage fix, so an installed hub never offers it")


class ProjectHubUpdateVisibilityAndBranchFollowingTests(unittest.TestCase):
    """The hub should surface actions only when they are useful, and should
    follow the owner's newest test work without trampling an explicit choice."""

    @staticmethod
    def _hub_src() -> str:
        return read_c_inlined("project-hub/src/project_hub.c")

    def test_hub_update_action_is_hidden_until_a_newer_hub_is_verified(self):
        src = self._hub_src()
        top_start = src.index("static ActionBtn        g_topBtns[5]")
        top = src[top_start:src.index("};", top_start)]
        self.assertRegex(top,
                         r'ID_BTN_SELF_UPDATE, L"Update Hub", BTN_AMBER,\s+112,\s+\{0,0,0,0\}, NULL, TRUE',
                         "Update Hub starts visible before any remote version is known")
        self.assertIn('{ ID_BTN_UPDATE_ALL,  L"Update All", BTN_PRIMARY,   108, {0,0,0,0}, NULL, FALSE }', top,
                      "hiding the Hub updater must not hide Update All")

        status_start = src.index("static void RefreshProjectStatuses(void)")
        status = src[status_start:src.index("/* 1. Bluetooth Guard */", status_start)]
        self.assertIn("g_hubUpdAvail = (remHub[0] != '\\0' && VerNewer(remHub, HUB_VERSION));", status)
        self.assertIn("g_topBtns[1].hidden = !g_hubUpdAvail;", status,
                      "a current Hub still leaves a dead Update Hub button visible")

        layout_start = src.index("static void ComputeLayout(HWND hwnd)")
        layout = src[layout_start:src.index("static int HitTestButtons", layout_start)]
        self.assertIn("if (g_topBtns[i].hidden) {\n            SetRect(&g_topBtns[i].rc, 0, 0, 0, 0);\n            continue;\n        }", layout,
                      "a hidden top button still consumes header space or keeps a stale hit box")
        hit_start = src.index("static int HitTestButtons")
        hit = src[hit_start:src.index("/* Blit the spinning emblem", hit_start)]
        self.assertIn("if (g_topBtns[i].hidden) continue;", hit,
                      "a hidden Update Hub rectangle is still clickable")
        paint_start = src.index("static void PaintHubWindow")
        paint = src[paint_start:src.index("static void ShowUtilityMenu", paint_start)]
        self.assertIn("if (g_topBtns[i].hidden) continue;\n        DrawActionBtn", paint,
                      "the header renderer ignores top-button visibility")
        renderer = (ROOT / "project-hub/src/parts/ui_renderer.c").read_text(encoding="utf-8")
        utility_start = renderer.index("static void ShowUtilityMenu")
        utility = renderer[utility_start:]
        self.assertIn("if (g_hubUpdAvail)\n        AppendMenuW(hMenu, MF_STRING, ID_BTN_SELF_UPDATE", utility,
                      "the utility menu still exposes self-update while the Hub is current")

    def test_remote_identity_fallback_is_known_and_uses_one_probe_ref(self):
        identity = (ROOT / "project-hub/src/parts/content_identity.c").read_text(encoding="utf-8")
        statuses = (ROOT / "project-hub/src/parts/refresh_statuses.c").read_text(encoding="utf-8")
        worker = (ROOT / "project-hub/src/parts/workers_remote.c").read_text(encoding="utf-8")

        known = identity[identity.index("static BOOL RemoteIdentityKnown"):identity.index("static BOOL DirExistsW")]
        self.assertIn("(remSha && remSha[0]) || remSize > 0", known,
                      "a known remote size is discarded when the tree API has no blob SHA")
        self.assertIn("return (remSize > 0 && loc > 0 && loc != remSize);", known,
                      "a local hash failure still suppresses a differing known size")
        self.assertIn("RemoteIdentityKnown(g_remBtgSha, remBtgSz)", statuses)
        self.assertIn("RemoteIdentityKnown(g_remDnsSha, remDnsSz)", statuses)
        self.assertIn("RemoteIdentityKnown(g_remNotesSha, remNotesSz)", statuses)
        self.assertIn("RemoteIdentityKnown(g_remAudioSwitcherSha, remAsSz)", statuses)
        self.assertIn("BOOL kblKnown = kblExeKnown || kblPs1Known;", statuses,
                      "Keyboard Lighting is hidden as unknown even when either build file resolves")
        self.assertIn("RemoteIdentityKnown(c->remSha, c->remSize)", statuses)

        sizes_start = worker.index("/* Remote build sizes:")
        sizes_end = worker.index("/* generic catalog extras", sizes_start)
        sizes = worker[sizes_start:sizes_end]
        self.assertNotIn("g_probeRef", sizes,
                         "one request still uses the mutable global ref, mixing branch "
                         "and released-commit identities in a single check")
        self.assertIn("FetchRemoteTree(probeRef, releasedRef", worker)
        self.assertIn("probeStillCurrent = (_wcsicmp(g_activeBranch, branch) == 0 &&", worker,
                      "a branch change during a remote request can publish stale card identities")
        self.assertIn("if (!probeStillCurrent) ClearRemoteProbeResults();", worker)
        self.assertIn("TreeResponseComplete(data)", worker,
                      "a truncated trees response can be accepted as a complete identity set")
        self.assertIn("if (!treeJson) {\n        EnterCriticalSection(&g_cs);\n        ClearRemoteBuildHashes();", worker,
                      "failed tree requests leave stale SHAs in place and mask the size fallback")
        self.assertIn("App update check incomplete", worker,
                      "a probe with missing identities still reports an apparently complete check")

    def test_latest_test_branch_setting_is_defaulted_and_manual_choice_pauses_it(self):
        cache = (ROOT / "project-hub/src/parts/version_probe_cache.c").read_text(encoding="utf-8")
        entry = (ROOT / "project-hub/src/parts/branch_picker_entry.c").read_text(encoding="utf-8")
        remote = (ROOT / "project-hub/src/parts/workers_remote.c").read_text(encoding="utf-8")
        settings = (ROOT / "project-hub/src/parts/settings_page.c").read_text(encoding="utf-8")

        self.assertIn('GetPrivateProfileIntW(L"Update", L"FollowLatestTestBranch", 1, ini)', cache,
                      "automatic following must be on by default")
        self.assertIn('WritePrivateProfileStringW(L"Update", L"FollowLatestTestBranch", on ? L"1" : L"0", ini)', cache,
                      "the follow setting is not persisted")
        self.assertIn("/repos/ajani190819-ops/Tests/branches?per_page=100", cache,
                      "automatic following does not inspect available branches")
        self.assertIn('L"/repos/%s/%s/commits/%S"', cache,
                      "the selected branch is not ranked by its newest commit")
        branch_json = (ROOT / "project-hub/src/parts/branch_json.h").read_text(encoding="utf-8")
        self.assertIn("HubBranchJsonExtractArenaTips", cache,
                      "automatic following still depends on exact compact-JSON token spelling")
        self.assertIn("HubBranchCheckInterval(g_autoBranchRetryDelay)", cache,
                      "failed discovery still uses the six-hour success cache interval")
        self.assertIn("g_autoBranchRetryDelay = HubBranchRetryAfterFailure(g_autoBranchRetryDelay);", cache,
                      "failed API scans do not schedule a bounded retry")
        self.assertIn("g_autoBranchRetryDelay = 0; /* successful result returns to the six-hour cache */", cache,
                      "a successful scan does not clear the failure backoff")
        self.assertIn("while (*p == ' ' || *p == '\\t' || *p == '\\r' || *p == '\\n') p++;", branch_json,
                      "the branch-list parser does not accept ordinary JSON whitespace")
        self.assertIn("g_probeRef[0] = L'\\0';", cache,
                      "switching automatic branches can reuse the previous branch's released commit")

        self.assertIn("SaveBranchCfg(branch);\n    SaveFollowLatestTestBranch(FALSE);", entry,
                      "choosing a branch manually must pause automatic following")
        worker_start = remote.index("static void WorkerCheckRemote(void)")
        worker = remote[worker_start:remote.index("/* Resolve this check to one snapshot", worker_start)]
        self.assertLess(worker.index("RefreshAutomaticBranch();"), worker.index("EnsureActiveBranch(branch, 64);"),
                        "the remote probe resolves a branch before automatic following runs")

        self.assertIn('L"Automatically follow the newest active test branch"', settings,
                      "Settings has no discoverable automatic-branch control")
        self.assertIn("SetFollowLatestTestBranch(on);", settings,
                      "the Settings checkbox does not apply the branch-follow preference")
        self.assertIn("EnqueueTask(TASK_CHECK_REMOTE);", settings,
                      "enabling automatic following does not check immediately")

    def test_branch_json_reader_and_retry_policy_with_realistic_api_payload(self):
        """Exercise the exact portable C parser used by the Hub, not a Python
        reimplementation: GitHub JSON may contain whitespace and nested values,
        and an unsuccessful scan must back off then retry before six hours."""
        import shutil
        import subprocess
        import tempfile

        header = ROOT / "project-hub/src/parts/branch_json.h"
        self.assertTrue(header.exists(), "the tested portable branch parser is missing")
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            self.skipTest("a native C compiler is required for the portable parser test")

        payload = [
            {"name": "main", "commit": {"sha": "a" * 40}},
            {"name": "arena/old-tests", "commit": {
                "sha": "b" * 40,
                "message": {"name": "nested-name-is-not-a-branch", "sha": "c" * 40},
            }},
            {"name": "arena/not-a-test", "commit": {"sha": "d" * 40}},
            {"name": "arena/new-tests", "commit": {
                "sha": "e" * 40,
                "message": "quoted fake key: \\\"name\\\": \\\"arena/wrong-tests\\\"",
            }},
        ]
        payload_literal = json.dumps(json.dumps(payload, indent=2))
        harness = f'''#include <string.h>
#include "project-hub/src/parts/branch_json.h"
int main(void) {{
    const char payload[] = {payload_literal};
    HubBranchTip tips[8];
    int count = HubBranchJsonExtractArenaTips(payload, tips, 8);
    if (count != 2) return 1;
    if (strcmp(tips[0].name, "arena/old-tests") != 0 || tips[0].sha[0] != 'b') return 2;
    if (strcmp(tips[1].name, "arena/new-tests") != 0 || tips[1].sha[0] != 'e') return 3;
    if (HubBranchJsonExtractArenaTips("[ {{ bad }} ]", tips, 8) != -1) return 4;
    if (HubBranchCheckInterval(0) != 21600000ULL) return 5;
    if (HubBranchCheckInterval(300000ULL) != 300000ULL) return 6;
    if (HubBranchRetryAfterFailure(0) != 300000ULL) return 7;
    if (HubBranchRetryAfterFailure(300000ULL) != 600000ULL) return 8;
    if (HubBranchRetryAfterFailure(600000ULL) != 1200000ULL) return 9;
    if (HubBranchRetryAfterFailure(1200000ULL) != 2400000ULL) return 10;
    if (HubBranchRetryAfterFailure(2400000ULL) != 3600000ULL) return 11;
    if (HubBranchRetryAfterFailure(3600000ULL) != 3600000ULL) return 12;
    return 0;
}}
'''
        with tempfile.TemporaryDirectory(prefix="hub-branch-json-") as td:
            source = Path(td) / "branch_json_test.c"
            exe = Path(td) / "branch_json_test"
            source.write_text(harness, encoding="utf-8")
            compiled = subprocess.run(
                [compiler, "-std=c99", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT), str(source), "-o", str(exe)],
                capture_output=True, text=True,
            )
            self.assertEqual(compiled.returncode, 0,
                             "portable branch parser did not compile:\\n" + compiled.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0,
                             "branch JSON/backoff behavior failed with code " + str(ran.returncode))


class AudioSwitcherReachabilityTests(unittest.TestCase):
    """Every row of the panel has to be reachable, and the panel has to fit.

    The owner's report: "the bottom part of it is cut off and I can't see what
    apps are actually available, and it'll say that no apps are available when
    there are apps that are broadcasting audio". Both halves came out of the
    layout: OUTPUT DEVICES took a fixed budget and reserved only three rows for
    APP VOLUME, so with several output devices the app rows ran out of room.
    ss_max hit 0, zero session controls were created, and WM_PAINT drew "No apps
    are playing audio right now" over a list that sessions_refresh() had already
    enumerated. Nothing scrolled, and the window could be taller than the
    screen, so whatever survived the budget sat under the taskbar.
    """

    @staticmethod
    def _src() -> str:
        return read_c_inlined("audio-switcher/src/audio_switcher.c")

    @staticmethod
    def _fn(src: str, signature: str) -> str:
        start = src.index(signature)
        return src[start:src.index("\n}\n", start)]

    def test_app_section_keeps_a_guaranteed_minimum_of_rows(self):
        src = self._src()
        budget = self._fn(src, "static void ui_layout_budget(int client_h) {")

        self.assertIn("#define UI_MIN_APP_ROWS 5", src,
                      "the guaranteed minimum for the APP VOLUME section is gone")
        self.assertIn("int ss_min = g_session_count < UI_MIN_APP_ROWS", budget,
                      "the app section no longer reserves a minimum number of rows, "
                      "so a long device list can squeeze it back to nothing")
        self.assertIn("g_ss_max = ss_room > ss_min ? ss_room : ss_min;", budget,
                      "the reserved app rows are not applied to the row count")
        self.assertNotIn("int reserve = hdr_h + gap + 3 * ss_row;", src,
                         "the old three-row reservation is back: it is what let the "
                         "app list collapse to zero visible rows")

    def test_rows_the_budget_cannot_show_are_scrolled_to_not_dropped(self):
        src = self._src()

        self.assertIn("WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL", src,
                      "the window has no vertical scrollbar, so rows past the "
                      "budget are unreachable again")
        budget = self._fn(src, "static void ui_layout_budget(int client_h) {")
        # Travel is counted in ROWS (the re-aim model), and the column is the
        # endpoint list then the session list: endpoints scroll off first, then
        # sessions. Counting it in pixels - or offsetting sessions by g_ep_max -
        # left the first g_ep_max sessions unreachable and double-applied the
        # offset to the endpoint band.
        self.assertIn("g_ep_scroll_max = g_endpoint_count > g_ep_max ? g_endpoint_count - g_ep_max : 0;",
                      budget, "the endpoint scroll travel is not derived from the "
                              "rows that do not fit")
        self.assertIn("g_ss_scroll_max = g_session_count  > g_ss_max ? g_session_count  - g_ss_max : 0;",
                      budget, "the session scroll travel is not derived from the "
                              "rows that do not fit")
        self.assertIn("g_scroll_range  = g_ep_scroll_max + g_ss_scroll_max;",
                      budget, "the scroll range no longer covers both sections")
        self.assertNotIn("g_scroll_range = total > visible_rows", budget,
                         "the scroll range is measured in pixels again, which the "
                         "row-based WM_VSCROLL handler cannot drive")

        viewport = self._fn(src, "static void ui_apply_viewport(void) {")
        self.assertIn("SetScrollInfo(g_hwnd, SB_VERT, &si, TRUE)", viewport,
                      "the scrollbar is never published, so it cannot be dragged")
        self.assertIn("ShowScrollBar(g_hwnd, SB_VERT, g_scroll_range > 0", viewport,
                      "the scrollbar stays visible with nothing to scroll")
        self.assertIn("c->session_idx = (logical < g_session_count) ? logical : -1;",
                      viewport, "scrolling no longer re-aims the session rows")
        self.assertIn("c->endpoint_idx = (logical < g_endpoint_count) ? logical : -1;",
                      viewport, "scrolling no longer re-aims the endpoint rows")
        self.assertIn("int ep_start = g_scroll_offset < g_ep_scroll_max ? g_scroll_offset : g_ep_scroll_max;",
                      viewport, "the endpoint band does not clamp its start to the "
                                "rows that can actually scroll off")
        self.assertIn("int ss_start = g_scroll_offset - ep_start;", viewport,
                      "the session band does not pick up the offset left over after "
                      "the endpoints are exhausted")
        self.assertIn("int logical = ep_start + i;", viewport,
                      "endpoint slots are not re-aimed from the scrolled start row")
        self.assertIn("int logical = ss_start + i;", viewport,
                      "session slots are not re-aimed from the scrolled start row")
        # The band tops must NOT slide with the offset, or the re-aimed content
        # would be offset twice (once by ep_start, once by the moving band).
        geo = self._fn(src, "static void ui_row_geometry(int logical_ep_row")
        self.assertIn("int top = S(PANEL_Y) + g_row_hdr;", geo,
                      "the row band still slides with g_scroll_offset, double-"
                      "applying the scroll on top of the re-aimed rows")

        for case in ("case WM_VSCROLL:", "case WM_MOUSEWHEEL:"):
            self.assertIn(case, src, f"{case} is not handled, so the panel cannot scroll")
        wheel = self._fn(src, "static LRESULT CALLBACK TrackbarSkinProc")
        self.assertIn("SendMessageW(panel, WM_MOUSEWHEEL, wp, lp)", wheel,
                      "the wheel stops working as soon as a volume slider has focus, "
                      "because WM_MOUSEWHEEL goes to the focused child")

        vscroll = src[src.index("case WM_VSCROLL: {"):]
        vscroll = vscroll[:vscroll.index("case WM_MOUSEWHEEL:")]
        self.assertIn("GetCapture() == g_endpoint_ctrls[i].hwnd_vol", vscroll,
                      "scrolling can now move a slider out from under a thumb the "
                      "user is holding")
        rebuild = self._fn(src, "static void ui_rebuild(void) {")
        self.assertIn("ui_apply_viewport();", rebuild,
                      "a rebuild does not re-publish the viewport")

    def test_window_is_clamped_to_the_screen_it_is_on(self):
        src = self._src()
        rebuild = self._fn(src, "static void ui_rebuild(void) {")

        self.assertIn("MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST)", rebuild,
                      "the window is sized without asking which screen it is on")
        self.assertIn("GetMonitorInfoW(mon, &mi)", rebuild,
                      "the work area is never read, so the taskbar is not accounted for")
        self.assertIn("mi.rcWork.bottom", rebuild,
                      "the window height is not clamped to the work area, so the "
                      "bottom of the panel can sit under the taskbar")
        self.assertIn("if (y + want_h > mi.rcWork.bottom) y = mi.rcWork.bottom - want_h;",
                      rebuild, "a window that does not fit is not moved back onto the screen")
        self.assertNotIn("SetWindowPos(g_hwnd, NULL, 0, 0, want_w, content + frame_h,",
                         src, "the old fixed-position resize is back")

    def test_panel_geometry_scales_with_dpi(self):
        src = self._src()

        # PANEL_X / PANEL_Y were used raw while every neighbouring size went
        # through S(), so at 150% the panel sat too high and the rows overlapped
        # the header band.
        self.assertNotIn("g_y_outputs_header = PANEL_Y;", src,
                         "the outputs header is positioned without DPI scaling")
        self.assertNotIn("int budget = S(UI_MAX_CLIENT) - footer - (PANEL_Y + hdr_h);", src,
                         "the layout budget mixes a scaled height with an unscaled origin")
        self.assertNotIn("            PANEL_X, PANEL_Y - S(38), S(104), S(28),", src,
                         "the Refresh button is positioned without DPI scaling")
        for needle in ("S(PANEL_X)", "S(PANEL_Y)", "S(PANEL_W)"):
            self.assertIn(needle, src, f"{needle} is gone - panel geometry went raw again")
        self.assertIn("case WM_DPICHANGED:", src,
                      "moving the window to a monitor with a different scale leaves "
                      "the layout built for the old DPI")

    def test_rows_resolve_through_the_visible_slot_not_a_stale_control_id(self):
        src = self._src()

        # Scrolling re-aims each slot at a different logical row, so a control
        # id baked at creation time is the wrong key - and once the list got
        # long enough to scroll, ids walked out of their handler range entirely
        # (4000 + 200 is not < 4200), silently dropping the volume change.
        hscroll = src[src.index("case WM_HSCROLL: {"):]
        hscroll = hscroll[:hscroll.index("case WM_TIMER:")]
        self.assertIn("if (c->hwnd_vol != slider) continue;", hscroll,
                      "slider notifications are not resolved through the slot that "
                      "owns the slider")
        self.assertNotIn("int id = (int)GetWindowLongPtrW(slider, GWLP_ID);", hscroll,
                         "slider notifications still key off the control id, which "
                         "goes stale as soon as the panel scrolls")

        rebuild = self._fn(src, "static void ui_rebuild(void) {")
        for needle in ("(HMENU)(INT_PTR)(4000 + i)", "(HMENU)(INT_PTR)(5000 + i)",
                       "(HMENU)(INT_PTR)(1000 + i)", "(HMENU)(INT_PTR)(2000 + i)",
                       "(HMENU)(INT_PTR)(3000 + i)"):
            self.assertIn(needle, rebuild,
                          f"{needle} is gone - control ids must identify the slot so "
                          f"they stay inside their handler's id range")

        cmd = src[src.index("    case WM_COMMAND: {"):]
        cmd = cmd[:cmd.index("case WM_HSCROLL:")]
        self.assertIn("int idx = g_endpoint_ctrls[slot].endpoint_idx;", cmd,
                      "the endpoint buttons still trust their control id as a row index")
        self.assertIn("int idx = g_session_ctrls[slot].session_idx;", cmd,
                      "the app mute buttons still trust their control id as a row index")
        self.assertIn("if (idx < 0 || idx >= g_endpoint_count) return 0;", cmd,
                      "an empty endpoint slot can still be clicked through")

    def test_spare_slots_do_not_paint_empty_cards(self):
        src = self._src()
        paint = src[src.index("    case WM_PAINT: {"):]
        paint = paint[:paint.index("EndPaint(hwnd, &ps);")]
        self.assertIn("if (idx < 0 || idx >= g_endpoint_count) continue;   /* spare slot */",
                      paint, "scrolled-past endpoint rows paint empty card frames")
        self.assertIn("if (idx < 0 || idx >= g_session_count) continue;    /* spare slot */",
                      paint, "scrolled-past app rows paint empty card frames")
        self.assertIn("ui_brush(idx == g_default_endpoint_idx ? COL_ACCENT : COL_BORDER)",
                      paint, "the current-output accent border keys off the slot "
                             "instead of the row it is showing")


class AudioSwitcherSessionSweepTests(unittest.TestCase):
    """An app that is playing must be listed, even when it is not playing on the
    output Windows calls default.

    sessions_refresh() asked only the default render endpoint for its session
    enumerator. An app pinned to another output (headphones left as the default
    in Windows' own mixer, a game on a second device) has its session there and
    nowhere else, so the panel said "no apps are playing audio" while audio was
    plainly playing - the second half of the owner's report.
    """

    @staticmethod
    def _src() -> str:
        return read_c_inlined("audio-switcher/src/audio_switcher.c")

    @staticmethod
    def _fn(src: str, signature: str) -> str:
        start = src.index(signature)
        return src[start:src.index("\n}\n", start)]

    def test_every_active_output_is_enumerated_not_just_the_default(self):
        """An app renders to whichever endpoint it opened its stream on, and
        Windows does not move that stream when the default later changes. A game
        on a non-default output has its session there and nowhere else, so listing
        only the default shows 'no apps are playing audio' while a game is plainly
        making sound. Enumerate every active render endpoint, like the Windows
        mixer and EarTrumpet do."""
        src = self._src()
        refresh = self._fn(src, "static HRESULT sessions_refresh(void) {")

        self.assertIn("static int sessions_collect_from_device(IMMDevice *dev", src,
                      "session collection is not per-device, so only the default "
                      "output can ever be listed")
        self.assertRegex(
            refresh,
            r"for \(int i = 0; i < g_endpoint_count && g_session_count < 128; i\+\+\)",
            "the refresh no longer walks every enumerated render endpoint")
        self.assertIn("sessions_collect_from_device(g_endpoints[i].device, !is_def", refresh,
                      "endpoints other than the default are not enumerated, so apps "
                      "rendering to another output never appear")
        self.assertIn("BOOL is_def = (i == g_default_endpoint_idx) || g_endpoints[i].is_default;",
                      refresh,
                      "the default endpoint is not identified, so its own apps would be "
                      "labelled as being on another output")
        self.assertNotIn("if (g_session_count == 0) {", refresh,
                         "the sweep is still gated on the default being empty, so a single "
                         "app on the default hides every game on the other outputs")
        self.assertIn("if (g_endpoint_count == 0 && g_enum)", refresh,
                      "no fallback when endpoint enumeration itself failed")

    def test_second_launch_shows_the_existing_window(self):
        """The Hub's Open button (and a desktop shortcut) re-launch the exe; the
        single-instance mutex stops a second window, so the running instance must
        be told to show itself. It normally sits hidden in the tray and is usually
        elevated, so the signal is a named event (UIPI would drop a cross-process
        PostMessage from a non-elevated launcher), marshalled onto the UI thread."""
        src = self._src()

        # An explicit launch shows; only --silent (the logon/tray start) hides.
        self.assertRegex(
            src,
            r"if \(g_silent\) \{\s*ShowWindow\(hwnd, SW_HIDE\);\s*\} else \{\s*"
            r"ShowWindow\(hwnd, SW_SHOWNORMAL\);",
            "an explicit (non-silent) launch does not show the window, so clicking Open "
            "in the Hub appears to do nothing")

        # First instance creates the event; a watcher thread waits and marshals it.
        self.assertIn("g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT_NAME);", src,
                      "the first instance never creates the show event, so a second "
                      "launch's SetEvent has nothing to signal")
        self.assertIn("static DWORD WINAPI ShowWatcherThread(", src,
                      "nothing waits for the show event")
        watcher = src[src.index("static DWORD WINAPI ShowWatcherThread("):
                      src.index("int WINAPI wWinMain(")]
        self.assertIn("WaitForSingleObject(g_showEvent", watcher,
                      "the watcher does not wait on the show event")
        self.assertIn("PostMessageW(g_hwnd, WM_ASW_SHOW", watcher,
                      "the watcher does not marshal the show onto the UI thread")
        self.assertIn("g_showThread = CreateThread(NULL, 0, ShowWatcherThread", src,
                      "the watcher thread is never started")

        # The UI thread shows and focuses the window on WM_ASW_SHOW.
        self.assertIn("#define WM_ASW_SHOW", src, "no show message is defined")
        show_case = src[src.index("case WM_ASW_SHOW:"):]
        show_case = show_case[:show_case.index("case WM_CLOSE:")]
        self.assertIn("if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);", show_case,
                      "a minimized window is not restored")
        self.assertIn("ShowWindow(hwnd, SW_SHOW);", show_case, "the window is not shown")
        self.assertIn("SetForegroundWindow(hwnd);", show_case,
                      "the window is shown but not brought to the foreground")

        # Second instance signals it: event first, window message as fallback.
        self.assertIn("OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME)", src,
                      "a second launch does not signal the running instance")
        self.assertIn("FindWindowW(L\"TestsAudioSwitcherWnd\", NULL)", src,
                      "no FindWindow fallback for an instance that predates the event")

    def test_swept_rows_say_which_output_they_are_on(self):
        src = self._src()
        # _fn's "\n}\n" cut lands on the closing brace of the for-loop inside
        # this function, so slice to the next top-level function instead.
        collect = src[src.index("static int sessions_collect_from_device("):
                      src.index("static HRESULT sessions_refresh(void) {")]
        self.assertIn("on_other_device", collect,
                      "a session found on another output is indistinguishable from "
                      "one on the current output")
        self.assertIn(chr(92) + 'u2014 on %s', collect,
                         "the row label no longer names the device the app is on")
        self.assertIn("h = ui_mix(h, (uint64_t)(g_sessions[i].on_other_device ? 1 : 0));",
                      src, "the UI identity signature ignores the device marker, so a "
                           "row that moves between outputs would not be relabelled")

    def test_every_session_manager_stays_alive_while_its_sliders_do(self):
        src = self._src()
        # _fn's "\n}\n" cut lands on the closing brace of the for-loop inside
        # this function, so slice to the next top-level function instead.
        collect = src[src.index("static int sessions_collect_from_device("):
                      src.index("static HRESULT sessions_refresh(void) {")]
        self.assertIn("static IAudioSessionManager2 *g_session_mgrs[32];", src,
                      "the manager array is smaller than the endpoint array, so a system "
                      "with many outputs would not retain every device's manager")
        self.assertIn("if (g_session_mgr_count < 32) g_session_mgrs[g_session_mgr_count++] = mgr;",
                      collect,
                      "a swept device's session manager is released while the "
                      "ISimpleAudioVolume pointers it handed out are still on screen")

        refresh = self._fn(src, "static HRESULT sessions_refresh(void) {")
        release_at = refresh.find("for (int i = 0; i < g_session_mgr_count; i++)")
        reset_at = refresh.find("g_session_count = 0;")
        self.assertNotEqual(release_at, -1, "the managers are never released")
        self.assertNotEqual(reset_at, -1, "the session list is never reset")
        self.assertLess(release_at, reset_at,
                        "sessions_refresh resets the list before releasing the "
                        "previous managers, leaking one per device per poll")

        cleanup = self._fn(src, "static void sessions_cleanup(void) {")
        self.assertIn("g_session_mgr_count", cleanup,
                      "shutdown still releases only the default device's manager")
        self.assertIn("g_session_mgr = NULL;", cleanup)


class LightingInputTests(unittest.TestCase):
    """Production receiver/hold state with synthetic Win32 packets only.
    No Windows or physical-keyboard acceptance is inferred from these tests."""

    @classmethod
    def setUpClass(cls):
        import shutil
        import tempfile
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            raise unittest.SkipTest("portable C compiler is unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="lighting-input-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.exe = Path(cls.temp.name) / "lighting-input"
        built = subprocess.run(
            [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
             str(ROOT / "tests/native/lighting_input_harness.c"), "-o", str(cls.exe)],
            capture_output=True, text=True)
        if built.returncode:
            raise AssertionError("lighting input harness did not compile:\n" + built.stderr)

    def _run(self, scenario):
        result = subprocess.run([str(self.exe), scenario], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Lighting input " + scenario + " passed", result.stdout)

    def test_keyboard_and_deduplicated_vendor_usages_register_at_the_capacity_boundary(self):
        self._run("enumeration")

    def test_failed_or_growing_device_lists_never_become_iteration_counts(self):
        self._run("enumeration-errors")

    def test_keyboard_fallback_and_both_registration_errors_are_observable(self):
        self._run("registration")

    def test_both_directions_keep_first_token_learning_pacing_and_release(self):
        self._run("hold")

    def test_batched_hid_press_release_and_repeat_reports_reach_the_learner(self):
        self._run("hid-batch")

    def test_snapshot_is_bounded_rate_limited_and_has_only_allowlisted_scalars(self):
        self._run("snapshot")

    def test_modes_physical_release_ceiling_and_failed_repeat_write_stay_safe(self):
        self._run("modes-ceiling")

    def test_latch_and_burst_do_not_receive_hold_repeat_tokens_in_either_direction(self):
        self._run("non-hold-modes")

    def test_genuine_custom_taps_duplicates_stale_tokens_and_restart_seeding(self):
        self._run("custom")

    def test_a_delayed_command_after_release_is_only_a_tap_not_a_new_hold(self):
        self._run("delayed-release")

    def test_native_latch_stops_and_burst_is_exactly_twenty_percent_in_both_directions(self):
        self._run("controller-modes")

    def test_rails_external_fn_inputs_and_half_percent_readback_do_not_echo(self):
        self._run("bounds-external")

    def test_learning_never_copies_keys_and_one_physical_gesture_is_consumed_once(self):
        self._run("distinct-duplicate")

    def test_snapshot_runs_after_registration_and_off_the_fine_input_timer(self):
        entry = (ROOT / "keyboard-lighting/src/parts/picker_window_entry.c").read_text(encoding="ascii")
        self.assertIn("InitRawInput(g_hwnd);\n    WriteHoldDiagnostics(TRUE);", entry)
        timer = entry[entry.index("case WM_TIMER:"):entry.index("case WM_TRAYICON:")]
        self.assertIn("WriteHoldDiagnostics(FALSE);", timer[timer.index("} else if (wp == 2)"):])
        fine = timer[timer.index("} else if (wp == 3)"):]
        self.assertNotIn("WriteHoldDiagnostics", fine)
        self.assertIn("WatchBrightTokens();", fine)
        self.assertIn("HoldPump();", fine)


class AppOpenFocusTests(unittest.TestCase):
    """The Hub reveals an existing instance and launches only an absent app.
    Exercise its actual activation code and routing, and retain app-side show
    contracts for direct/shortcut launches. Native Windows review is separate."""

    @staticmethod
    def _src(rel):
        return read_c_inlined(rel)

    # (path, explicit-first-launch pattern, already-running-resurface pattern)
    APPS = [
        ("bluetooth-guard/src/bluetooth_guard.c",
         r"if \(!silent\) \{\s*RevealPanel\(g_hwnd\);\s*UpdateWindow\(g_hwnd\);",
         r"WaitForSingleObject\(g_showEvent, 0\) == WAIT_OBJECT_0\)\s*\{\s*"
         r"RevealPanel\(hwnd\);"),
        ("keyboard-lighting/src/keyboard_lighting.c",
         r"if \(!silent\) \{\s*RevealLightingPanel\(g_hwnd\);\s*UpdateWindow\(g_hwnd\);",
         r"WaitForSingleObject\(g_showEvent, 0\) == WAIT_OBJECT_0\)\s*\{\s*"
         r"RevealLightingPanel\(hwnd\);"),
        ("dns-pilot/src/dns_pilot.c",
         r"if \(!silent\) \{\s*ShowWindow\(g_hwnd, SW_SHOW\);\s*"
         r"SetForegroundWindow\(g_hwnd\);\s*\}",
         r"FindWindowW\(L\"KBLSuiteDnsPilotWindow\", NULL\)[\s\S]{0,200}?"
         r"SetForegroundWindow\(other\);"),
        ("agent-notes/src/agent_notes.c",
         r"ShowWindow\(g_hwnd, SW_SHOW\);\s*UpdateWindow\(g_hwnd\);\s*"
         r"SetForegroundWindow\(g_hwnd\);",
         r"MsgWaitForMultipleObjects\(1, &g_showEvent[\s\S]{0,340}?"
         r"SetForegroundWindow\(g_hwnd\);"),
        ("audio-switcher/src/audio_switcher.c",
         r"if \(g_silent\) \{\s*ShowWindow\(hwnd, SW_HIDE\);\s*\} else \{\s*"
         r"ShowWindow\(hwnd, SW_SHOWNORMAL\);\s*SetForegroundWindow\(hwnd\);",
         r"case WM_ASW_SHOW:[\s\S]{0,420}?SetForegroundWindow\(hwnd\);"),
    ]

    def test_hub_open_executes_real_activation_policy_with_mocked_win32_io(self):
        """Exercise hidden/minimized/modal/offscreen, startup, double-click,
        image lookup, focus cancellation, timeout, failure, and shell reentry.
        This runs production C; Win32 delivery/monitor I/O remains mocked."""
        import shutil
        import tempfile
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            self.skipTest("portable C compiler is unavailable")
        with tempfile.TemporaryDirectory(prefix="hub-open-") as td:
            exe = Path(td) / "hub-open"
            creates = []
            for app, entry, function in (("bluetooth-guard", "window_entry.c", "GuardWindowCreate"),
                                         ("keyboard-lighting", "picker_window_entry.c", "LightingWindowCreate")):
                source = (ROOT / app / "src/parts" / entry).read_text()
                start = source.index("    case WM_CREATE:") + len("    case WM_CREATE:")
                body = source[start:source.index("    case WM_ERASEBKGND:", start)]
                creates.append("static int " + function + "(HWND hwnd) {" + body + "}\n")
            (Path(td) / "hub_open_create_production.h").write_text("\n".join(creates))
            build = subprocess.run(
                [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", td,
                 str(ROOT / "tests/native/hub_open_harness.c"), "-o", str(exe)],
                capture_output=True, text=True, timeout=30)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn("Hub Open activation scenarios passed", run.stdout)
            self.assertIn("Hub launch error detail scenarios passed", run.stdout)

    def test_every_hub_open_surface_uses_the_same_show_or_launch_route(self):
        entry = (ROOT / "project-hub/src/parts/branch_picker_entry.c").read_text(encoding="utf-8")
        cards = (ROOT / "project-hub/src/parts/card_defs_verbs.c").read_text(encoding="utf-8")
        tray = (ROOT / "project-hub/src/parts/tray_unified.c").read_text(encoding="utf-8")
        for start, end in (("ID_BTG_OPEN", "ID_BTG_UPDATE"),
                           ("ID_DNS_OPEN", "ID_DNS_UPDATE"),
                           ("ID_NOTES_OPEN", "ID_NOTES_UPDATE"),
                           ("ID_KBL_OPEN", "ID_KBL_RESTART"),
                           ("ID_AUDIOSWITCHER_OPEN", "ID_AUDIOSWITCHER_UPDATE")):
            with self.subTest(action=start):
                action = entry[entry.index("case " + start + ":"):entry.index("case " + end + ":")]
                self.assertIn("if (!HubOpenApp(", action)
                self.assertNotIn("ShellExecuteW", action,
                                 "Open bypasses existing-window and startup duplicate protection")
        dynamic = cards[cards.index('} else if (wcscmp(verb, L"open") == 0)'):cards.index("static void InitCardDefinitions")]
        self.assertIn("if (!HubOpenApp(p, c->launchArgs, c->appDir))", dynamic)
        self.assertNotIn("ShellExecuteW", dynamic)
        for app in ("NOTES", "BTG", "KBL", "DNS"):
            self.assertIn("HandleActionCommand(g_hwnd, ID_" + app + "_OPEN);", tray)
        self.assertIn("HandleActionCommand(g_hwnd, ID_AUDIOSWITCHER_OPEN);", tray)
        weather = tray[tray.index("static void HubTrayOpenWeather("):tray.index("static void HubTrayOpenApp(")]
        self.assertIn('HandleVerb(idx, L"open");', weather)
        self.assertIn("if (!HubOpenApp(weatherExe, NULL, weatherDir))", weather)
        self.assertNotIn("ShellExecuteW", weather)

    def test_hub_native_window_and_mutex_identities_match_each_current_app(self):
        activation = (ROOT / "project-hub/src/parts/app_activation.c").read_text(encoding="utf-8")
        entries = re.findall(r'\{ L"([^"]+\.exe)", L"([^"]+)", L"([^"]+)" \}', activation)
        apps = {
            "BluetoothGuard.exe": ("bluetooth-guard/src/bluetooth_guard.c", "SINGLE_MUTEX_NAME"),
            "KeyboardLighting.exe": ("keyboard-lighting/src/keyboard_lighting.c", "SINGLE_MUTEX_NAME"),
            "DnsPilot.exe": ("dns-pilot/src/dns_pilot.c", "SINGLE_MUTEX_NAME"),
            "AgentNotes.exe": ("agent-notes/src/agent_notes.c", "MUTEX_NAME"),
            "AudioSwitcher.exe": ("audio-switcher/src/audio_switcher.c", "SINGLE_MUTEX_NAME"),
            "WeatherRadar.exe": ("weather-radar/src/weather_radar.c", "MUTEX_NAME"),
        }
        self.assertEqual({exe for exe, _, _ in entries}, set(apps))
        for exe, klass, mutex in entries:
            with self.subTest(app=exe):
                source, macro = apps[exe]
                native = self._src(source)
                self.assertIn('L"' + klass + '"', native)
                actual = re.search(r'^#define ' + macro + r'\s+L"([^"]+)"', native, re.M)
                self.assertIsNotNone(actual)
                self.assertEqual(mutex, actual.group(1), "stale mutex can spawn an unnecessary second process at startup")

    def test_hub_activation_retries_only_on_the_ui_timer_and_preserves_boundaries(self):
        root = (ROOT / "project-hub/src/project_hub.c").read_text(encoding="utf-8")
        entry = (ROOT / "project-hub/src/parts/branch_picker_entry.c").read_text(encoding="utf-8")
        activation = (ROOT / "project-hub/src/parts/app_activation.c").read_text(encoding="utf-8")
        self.assertLess(root.index('#include "parts/app_activation.c"'),
                        root.index('#include "parts/card_defs_verbs.c"'))
        self.assertIn("if (wp == HUB_OPEN_TIMER) { HubPollOpen(); return 0; }", entry)
        destroy = entry[entry.index("case WM_DESTROY:"):entry.index("/* The staged Hub EXE")]
        self.assertIn("HubCancelOpen();", destroy)
        self.assertNotIn("Sleep(", activation)
        self.assertNotIn("WaitForSingleObject(", activation)
        self.assertNotIn("AttachThreadInput(", activation)
        self.assertNotIn('L"runas"', activation)
        self.assertNotIn("ASFW_ANY", activation)
        self.assertIn('launch.lpVerb = L"open";', activation)
        self.assertIn("if (launch.hProcess) CloseHandle(launch.hProcess);", activation)

    def test_lighting_open_is_wired_to_its_own_show_only_ui_without_engine_actions(self):
        main = (ROOT / "keyboard-lighting/src/keyboard_lighting.c").read_text(encoding="ascii")
        entry = (ROOT / "keyboard-lighting/src/parts/picker_window_entry.c").read_text(encoding="ascii")
        helper = (ROOT / "keyboard-lighting/src/parts/window_activation.c").read_text(encoding="ascii")
        self.assertIn('#include "../../shared/win32/window_reveal.h"', main)
        self.assertLess(main.index('#include "parts/window_activation.c"'),
                        main.index('#include "parts/picker_window_entry.c"'))
        proc = entry[entry.index("static LRESULT CALLBACK KblWndProc("):]
        self.assertIn("if (HandleHubLightingReveal(hwnd, msg, wp, lp)) return 0;", proc)
        self.assertLess(proc.index("HandleHubLightingReveal"), proc.index("switch (msg)"))
        create = proc[proc.index("case WM_CREATE:"):proc.index("case WM_ERASEBKGND:")]
        self.assertRegex(create, r"(?m)^[ \t]*EnableHubLightingReveal\(hwnd\);[ \t]*$")
        for begin, end in (("} else if (id == IDM_TRAY_OPEN)", "} else if (id == IDM_TRAY_TOGGLE)"),
                           ("if (FileExistsW(g_showPath))", "if (g_showEvent && WaitForSingleObject"),
                           ("if (g_showEvent && WaitForSingleObject", "if (!g_wantOff && !IsEngineAlive()"),
                           ("case WM_TRAYICON:", "case WM_CLOSE:")):
            route = proc[proc.index(begin):proc.index(end, proc.index(begin))]
            self.assertIn("RevealLightingPanel(hwnd);", route)
            self.assertNotIn("SW_SHOWNORMAL", route)
            self.assertNotIn("StartEngine", route)
        duplicate = entry[entry.index("if (GetLastError() == ERROR_ALREADY_EXISTS)"):
                          entry.index("/* Request elevation once")]
        self.assertIn('FindWindowW(L"TestsKeyboardLightingSuiteWnd", NULL)', duplicate)
        self.assertIn("if (pid) AllowSetForegroundWindow(pid);", duplicate)
        self.assertLess(duplicate.index("AllowSetForegroundWindow"), duplicate.index("WriteAtomicUtf8(g_showPath"))
        self.assertLess(duplicate.index("AllowSetForegroundWindow"), duplicate.index("SetEvent(ev)"))
        self.assertEqual(helper.count("ChangeWindowMessageFilterEx("), 1)
        self.assertIn("if (message) ChangeWindowMessageFilterEx(hwnd, message, MSGFLT_ALLOW, NULL);", helper)
        for forbidden in ("ChangeWindowMessageFilter(", "StartEngine", "StopEngine", "WriteAtomicUtf8",
                          "CreateFile", "ShellExecute", "CreateProcess", "SendMessage(", "AttachThreadInput",
                          "HWND_TOPMOST", "ASFW_ANY"):
            self.assertNotIn(forbidden, helper)
        # The explicit restart remains a different action, not an Open fallback.
        actions = (ROOT / "project-hub/src/parts/branch_picker_entry.c").read_text()
        opening = actions[actions.index("case ID_KBL_OPEN:"):actions.index("case ID_KBL_RESTART:")]
        self.assertNotIn("RestartKeyboardLightingCleanly", opening)
        self.assertNotIn("StopProcess", opening)

    def test_explicit_open_shows_and_foregrounds_the_window(self):
        for rel, first, _second in self.APPS:
            with self.subTest(app=rel):
                self.assertRegex(
                    self._src(rel), first,
                    rel + ": an explicit Open (the hub launches with no --silent) "
                          "does not show AND foreground the window, so it can open "
                          "behind the hub or stay hidden in the tray")

    def test_a_running_instance_is_resurfaced_and_focused(self):
        for rel, _first, second in self.APPS:
            with self.subTest(app=rel):
                self.assertRegex(
                    self._src(rel), second,
                    rel + ": when the app is already running in the tray, a second "
                           "Open does not bring the existing window forward")

class HubLongevityTests(unittest.TestCase):
    """Production icon/tray ownership with a finite mocked Windows handle budget.
    Native Explorer recovery and a real prolonged-use soak remain separate."""

    @classmethod
    def setUpClass(cls):
        import shutil
        import tempfile
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            raise unittest.SkipTest("portable C compiler is unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="hub-longevity-")
        cls.addClassCleanup(cls.temp.cleanup)
        folder = Path(cls.temp.name)
        icons = (ROOT / "project-hub/src/parts/catalog_icons_json.c").read_text(encoding="utf-8")
        tray = (ROOT / "project-hub/src/parts/tray_unified.c").read_text(encoding="utf-8")
        icon_code = icons[icons.index("static HICON LoadIconFilePx("):icons.index("static wchar_t g_strPool[")]
        tray_start = tray.index("static void HubTrayPublishHostMarker(void)")
        tray_end = tray.index("\n}\n", tray.index("static void HubTrayRemove(void)")) + 3
        (folder / "hub_longevity_production.h").write_text(icon_code + tray[tray_start:tray_end], encoding="utf-8")
        cls.exe = folder / "hub-longevity"
        built = subprocess.run(
            [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(folder),
             str(ROOT / "tests/native/hub_longevity_harness.c"), "-o", str(cls.exe)],
            capture_output=True, text=True, timeout=30)
        if built.returncode:
            raise AssertionError(built.stderr)

    def _run(self, scenario, expected):
        run = subprocess.run([str(self.exe), scenario], capture_output=True, text=True, timeout=15)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertIn(expected, run.stdout)

    def test_catalog_icon_refresh_stays_bounded_and_preserves_last_good_icon(self):
        self._run("icons", "Hub catalog icon lifetime scenarios passed")

    def test_tray_recovers_silent_shell_loss_and_never_revives_after_quit(self):
        self._run("tray", "Hub tray lifetime scenarios passed")

    def test_catalog_icon_swaps_and_drawing_share_the_ownership_lock(self):
        icons = (ROOT / "project-hub/src/parts/catalog_icons_json.c").read_text(encoding="utf-8")
        start = icons.index("static void ReplaceCardIcon(")
        replace = icons[start:icons.index("\n}\n", start)]
        self.assertLess(replace.index("EnterCriticalSection(&g_cs);"), replace.index("c->hIcon = replacement;"))
        self.assertLess(replace.index("c->hIcon = replacement;"), replace.index("DestroyIcon(previous)"))
        self.assertLess(replace.index("DestroyIcon(previous)"), replace.index("LeaveCriticalSection(&g_cs);"))
        renderer = (ROOT / "project-hub/src/parts/ui_renderer.c").read_text(encoding="utf-8")
        icon = renderer.index("HICON ico = HubCardIcon(card, c);")
        enter = renderer.rfind("EnterCriticalSection(&g_cs);", 0, icon)
        leave = renderer.index("LeaveCriticalSection(&g_cs);", icon)
        self.assertGreater(enter, renderer.index("/* Catalog icon first"))
        self.assertLess(icon, renderer.index("DrawIconEx(hdc, ix, iy, ico"))
        self.assertLess(renderer.index("DrawIconEx(hdc, ix, iy, ico"), leave)

    def test_catalog_rebuild_releases_extra_icons_through_the_same_owned_route(self):
        source = (ROOT / "project-hub/src/parts/builtin_card_text.c").read_text(encoding="utf-8")
        rebuild = source[source.index("static void RebuildExtraCards("):]
        self.assertIn("ReplaceCardIcon(&g_cards[i], NULL);", rebuild)
        self.assertLess(rebuild.index("ReplaceCardIcon(&g_cards[i], NULL);"),
                        rebuild.index("ZeroMemory(&g_cards[i], sizeof(g_cards[i]));"))
        self.assertNotIn("DestroyIcon(", rebuild,
                         "catalog cleanup bypasses the paint/ownership lock")


class HubGroupedEntriesTests(unittest.TestCase):
    """Production C with mocked profiles/files and DPI/window geometry.
    Native Windows fitting, Explorer and real Orca remain acceptance work."""

    @classmethod
    def setUpClass(cls):
        import shutil
        import tempfile
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            raise unittest.SkipTest("portable C compiler is unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="hub-groups-")
        cls.addClassCleanup(cls.temp.cleanup)
        folder = Path(cls.temp.name)
        root = (ROOT / "project-hub/src/project_hub.c").read_text(encoding="utf-8")
        parts = ROOT / "project-hub/src/parts"
        defines = "\n".join(re.findall(
            r"(?m)^#define (?:ID_\w+|RES_ICON_\w+|MAX_APPS|BUILTIN_CARD_COUNT) +\d+.*$", root))
        embedded_ids = re.search(r"const int ids\[\d+\] = (\{.*?\});", root, re.S).group(1)
        defines += "\nstatic const int testCardIconIds[] = " + embedded_ids + ";"
        category = re.search(r"enum \{ HUB_GROUP_GENERAL.*?\};", root).group(0)
        types = root[root.index("enum {\n    BADGE_IDLE"):root.index("static HINSTANCE")]
        top_at = root.index("static ActionBtn        g_topBtns[5]")
        top_buttons = root[top_at:root.index("};", top_at) + 2].replace("g_topBtns[5]", "testTopButtons[5]")
        (folder / "hub_groups_types.h").write_text(
            defines + "\n" + category + "\n" + types + "\n" + top_buttons, encoding="utf-8")
        cfg = (parts / "version_probe_cache.c").read_text(encoding="utf-8")

        def function(text, start):
            at = text.index(start)
            return text[at:text.index("\n}\n", at) + 3]

        config = cfg[cfg.index("/* Hub-only opt-outs"):cfg.index("static void EnsureActiveBranch")]
        catalog = (parts / "catalog_icons_json.c").read_text(encoding="utf-8")
        catalog = catalog[catalog.index("static wchar_t g_strPool["):]
        orca = (parts / "orca_plugins.c").read_text(encoding="utf-8")
        fallback = (parts / "builtin_card_text.c").read_text(encoding="utf-8")
        cards = (parts / "card_defs_verbs.c").read_text(encoding="utf-8")
        cards = cards[cards.index("static void InitCardDefinitions(void)"):]
        layout = (parts / "ui_renderer.c").read_text(encoding="utf-8")
        layout = layout[layout.index("/* Physical card slots"):layout.index("/* Blit the spinning emblem")]
        production = (config + function(cfg, "static BOOL ExtractQuotedAfter(") +
                      function(cfg, "static BOOL VerNewer(") + catalog + orca + fallback + cards + layout)
        (folder / "hub_groups_production.h").write_text(production, encoding="utf-8")
        cls.exe = folder / "hub-groups"
        built = subprocess.run(
            [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(folder),
             str(ROOT / "tests/native/hub_groups_harness.c"), "-o", str(cls.exe)],
            capture_output=True, text=True, timeout=30)
        if built.returncode:
            raise AssertionError(built.stderr)

    def _run(self, scenario, expected):
        run = subprocess.run([str(self.exe), scenario], capture_output=True, text=True, timeout=15)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertIn(expected, run.stdout)

    def test_undahang_and_infill_keep_distinct_online_and_offline_icons(self):
        self._run("icons", "Hub independent plugin icon and stale catalogue scenarios passed")

    def test_plugin_preferences_migrate_and_actions_are_independent(self):
        self._run("config", "Hub independent configuration and routing passed")

    def test_each_plugin_installs_and_removes_only_its_files_preserving_settings(self):
        self._run("files", "Hub independent plugin filesystem scenarios passed")

    def test_each_plugins_status_uses_all_profiles_and_both_install_paths(self):
        self._run("status", "Hub independent plugin status scenarios passed")

    def test_partial_old_and_duplicate_catalogues_preserve_every_known_card(self):
        self._run("catalog", "Hub catalogue fallback and duplicate scenarios passed")

    def test_grouped_layout_scrolling_clipping_and_dpi_route_to_the_drawn_card(self):
        self._run("layout", "Hub grouped DPI geometry and clipped routing passed")

    def test_header_aligns_visible_buttons_with_the_title_when_they_fit(self):
        self._run("header", "Hub aligned header and visible-control breakpoints passed")

    def test_catalogue_categories_and_legacy_id_remain_stable(self):
        apps = json.loads((ROOT / "suite-apps.json").read_text(encoding="utf-8"))["apps"]
        expected = {"hud": "Minecraft mods", "orca": "Orca plugins", "unlayered-infill": "Orca plugins"}
        self.assertEqual(len(apps), 9)
        for app in apps:
            self.assertEqual(app["category"], expected.get(app["id"], "General apps"))
        by_id = {a["id"]: a for a in apps}
        self.assertEqual(by_id["orca"]["title"], "Undahang")
        self.assertEqual(by_id["unlayered-infill"]["title"], "Unlayered Infill")
        self.assertEqual(by_id["orca"]["dir"], "OrcaPlugins")
        for app_id in ("orca", "unlayered-infill"):
            self.assertFalse(by_id[app_id]["exe"])
        # Public branding and plugin/preset APIs are deliberately separate.
        self.assertEqual(by_id["kbl"]["title"], "Prism Lighting")
        self.assertEqual(by_id["weatherradar"]["title"], "Weather Report")
        manifest = json.loads((ROOT / "plugins.json").read_text(encoding="utf-8"))["plugins"]
        self.assertEqual([a["id"] for a in manifest], ["wave-overhangs", "unlayered-infill"])
        self.assertEqual(manifest[0]["name"], "Wave Overhangs")

    def test_public_names_survive_offline_old_partial_duplicate_catalogues(self):
        self._run("names", "Hub public brands and stable catalogue identities passed")

    def test_renderer_and_pointer_events_share_grouped_geometry(self):
        parts = ROOT / "project-hub/src/parts"
        renderer = (parts / "ui_renderer.c").read_text(encoding="utf-8")
        router = (parts / "branch_picker_entry.c").read_text(encoding="utf-8")
        self.assertIn("RECT title = g_groupRects[group];", renderer)
        self.assertIn("DrawTextW(hdc, g_groupTitles[group]", renderer)
        self.assertIn("IntersectClipRect(hdc, 0, headerH + 1, w, footerTop - 1)", renderer)
        self.assertIn("if (card->resIconId == RES_ICON_ORCA) return g_cardIcons[3];", renderer)
        self.assertIn("HICON ico = HubCardIcon(card, c);", renderer)
        for event in ("WM_MOUSEMOVE", "WM_LBUTTONUP"):
            at = router.index("case " + event + ":")
            body = router[at:router.index("return 0;", at)]
            self.assertLess(body.index("ComputeLayout(hwnd)"), body.index("HitTestButtons("))
            self.assertIn("HubHeaderHeight(cli.right, dpi)", body)
        self.assertIn("if (HandleOrcaPluginCommand(hwnd, id)) return;", router)
        dispatch = (parts / "enqueue_restart_prompts.c").read_text(encoding="utf-8")
        self.assertIn("case TASK_INSTALL_ORCA_WAVE:   WorkerInstallOrcaPlugin(0);", dispatch)
        self.assertIn("case TASK_INSTALL_ORCA_INFILL: WorkerInstallOrcaPlugin(1);", dispatch)
        self.assertNotIn("for (int i = 6; i < g_cardCount; i++) WorkerInstallGeneric", dispatch)


class BluetoothAudioEndpointTests(unittest.TestCase):
    """Real COM traversal with deterministic I/O; not native driver acceptance."""

    @classmethod
    def setUpClass(cls):
        import shutil
        import tempfile
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            raise unittest.SkipTest("portable C compiler is unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="bluetooth-audio-")
        cls.addClassCleanup(cls.temp.cleanup)
        folder = Path(cls.temp.name)
        root = (ROOT / "bluetooth-guard/src/bluetooth_guard.c").read_text(encoding="utf-8")
        start = root.index("typedef struct {\n    ULONGLONG address;")
        end = root.index("} BtAudioResult;", start) + len("} BtAudioResult;")
        constants = "\n".join(re.findall(
            r"(?m)^static const (?:GUID|CLSID|IID) +k(?:KsPropSetBtAudio|ClsidMMDevEnum|IidIMMDevEnum|IidIDeviceTopo|IidIKsControl) +.*$", root))
        (folder / "bluetooth_audio_types.h").write_text(root[start:end] + "\n" + constants, encoding="utf-8")
        history = (ROOT / "bluetooth-guard/src/parts/device_history.c").read_text(encoding="utf-8")
        production = history[history.index("static void FormatCompactHex("):]
        production += (ROOT / "bluetooth-guard/src/parts/audio_endpoints.c").read_text(encoding="utf-8")
        (folder / "bluetooth_audio_production.h").write_text(production, encoding="utf-8")
        cls.exe = folder / "bluetooth-audio"
        built = subprocess.run(
            [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(folder),
             str(ROOT / "tests/native/bluetooth_audio_harness.c"), "-o", str(cls.exe)],
            capture_output=True, text=True, timeout=30)
        if built.returncode:
            raise AssertionError(built.stderr)

    def run_case(self, case):
        ran = subprocess.run([str(self.exe), case], capture_output=True, text=True, timeout=10)
        self.assertEqual(ran.returncode, 0, ran.stderr)

    def test_address_routing_and_adapter_ikscontrol_activation(self):
        self.run_case("routing")

    def test_active_unplugged_disabled_and_request_are_distinct(self):
        self.run_case("state")

    def test_com_failures_release_every_object_and_device_id(self):
        self.run_case("failures")

    def test_status_refresh_is_throttled_address_bound_and_never_trusts(self):
        self.run_case("refresh")

    def test_worker_status_and_manual_disconnect_wiring(self):
        parts = ROOT / "bluetooth-guard/src/parts"
        rules = (parts / "autostart_rules.c").read_text(encoding="utf-8")
        ui = (parts / "window_entry.c").read_text(encoding="utf-8")
        worker = rules[rules.index("static DWORD WINAPI WorkerThread("):]
        self.assertIn("RefreshAudioConnections();\n        PollPairedDevices();\n        RunAutoConnect();", worker)
        self.assertIn("DoConnectNow(item.address, item.kind, item.name, FALSE)", worker)
        self.assertIn("DisconnectIfStillRequested(item.address, item.kind, item.name)", worker)
        self.assertEqual(ui.count("SetManualConnectionChoice(d,"), 3, "card and tray must share pause/resume semantics")
        presentation = (parts / "device_presentation.c").read_text(encoding="utf-8")
        self.assertIn('L"Audio connected"', presentation)
        self.assertIn('L"Bluetooth link only; audio not connected"', presentation)
        self.assertIn('L"No address-matched audio endpoint"', presentation)
        self.assertIn('L"Auto-Connect paused"', presentation)
        self.assertIn("DescribeDevice(d, radioOn, GetTickCount64(), &visual)", ui)
        self.assertNotIn('L"Connected audio session"', presentation)
        saved = rules[rules.index("static void SaveRules("):rules.index("static void LoadRules(")]
        self.assertNotIn("autoPaused", saved, "manual Disconnect pause is current-run state, not a new persisted policy")


class BluetoothPanelTests(unittest.TestCase):
    """Production presentation and own-UI reveal wiring; native focus/DPI is separate."""

    @classmethod
    def setUpClass(cls):
        import shutil
        import tempfile
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            raise unittest.SkipTest("portable C compiler is unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="bluetooth-panel-")
        cls.addClassCleanup(cls.temp.cleanup)
        folder = Path(cls.temp.name)
        root = (ROOT / "bluetooth-guard/src/bluetooth_guard.c").read_text(encoding="utf-8")
        parts = ROOT / "bluetooth-guard/src/parts"
        policy = (parts / "device_policy.c").read_text(encoding="utf-8")
        start = root.index("typedef struct {\n    ULONGLONG address;")
        end = root.index("};", root.index("enum BtConnectState", start)) + 2
        production = root[start:end] + "\n"
        production += "\n".join(re.findall(r"(?m)^#define COL_.*$", root)) + "\n"
        for name in ("DeviceNeedsReview", "DeviceIsAudio", "DeviceAudioReady"):
            start = policy.index("static BOOL " + name + "(")
            production += policy[start:policy.index("\n}\n", start) + 3]
        production += (parts / "device_presentation.c").read_text(encoding="utf-8")
        (folder / "bluetooth_presentation_production.h").write_text(production, encoding="utf-8")
        cls.exe = folder / "bluetooth-panel"
        built = subprocess.run(
            [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(folder),
             str(ROOT / "tests/native/bluetooth_presentation_harness.c"), "-o", str(cls.exe)],
            capture_output=True, text=True, timeout=30)
        if built.returncode:
            raise AssertionError(built.stderr)

    def run_case(self, case):
        ran = subprocess.run([str(self.exe), case], capture_output=True, text=True, timeout=10)
        self.assertEqual(ran.returncode, 0, ran.stderr)

    def test_connected_green_and_paired_blue_do_not_depend_on_auto_preference(self):
        self.run_case("connection")

    def test_review_denied_and_saved_only_records_cannot_masquerade_as_paired(self):
        self.run_case("approval")

    def test_request_failure_retry_and_manual_pause_keep_readable_details(self):
        self.run_case("details")

    def test_all_presentation_combinations_are_read_only_and_truthful(self):
        self.run_case("matrix")

    def test_every_guard_open_route_uses_the_owning_ui_and_only_show_is_allowed(self):
        parts = ROOT / "bluetooth-guard/src/parts"
        entry = (parts / "window_entry.c").read_text(encoding="utf-8")
        helper = (parts / "window_activation.c").read_text(encoding="utf-8")
        self.assertIn("if (HandleHubPanelReveal(hwnd, msg, wp, lp)) return 0;", entry)
        create = entry[entry.index("case WM_CREATE:"):entry.index("case WM_ERASEBKGND:")]
        self.assertRegex(create, r"(?m)^[ \t]*EnableHubPanelReveal\(hwnd\);[ \t]*$")
        for begin, end in (("if (id == IDM_TRAY_OPEN)", "} else if (id == IDM_TRAY_PAUSE)"),
                           ("if (g_showEvent && WaitForSingleObject", "WriteHeartbeat();"),
                           ("case WM_TRAYICON:", "case WM_CLOSE:")):
            route = entry[entry.index(begin):entry.index(end, entry.index(begin))]
            self.assertIn("RevealPanel(hwnd);", route)
            self.assertNotIn("SW_SHOWNORMAL", route)
        self.assertIn("RevealPanel(g_hwnd);", entry)
        duplicate = entry[entry.index("if (GetLastError() == ERROR_ALREADY_EXISTS)"):entry.index("CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)")]
        self.assertIn("if (pid) AllowSetForegroundWindow(pid);", duplicate)
        self.assertLess(duplicate.index("AllowSetForegroundWindow"), duplicate.index("SetEvent(ev)"))
        self.assertEqual(helper.count("ChangeWindowMessageFilterEx("), 1)
        self.assertIn("ChangeWindowMessageFilterEx(hwnd, message, MSGFLT_ALLOW, NULL)", helper)
        self.assertIn("if (message) ChangeWindowMessageFilterEx", helper)
        self.assertNotIn("ChangeWindowMessageFilter(", helper)
        for forbidden in ("ShellExecute", "CreateProcess", "EnqueueWork", "DoConnect", "CreateFile", "SendMessage(", "AttachThreadInput", "HWND_TOPMOST"):
            self.assertNotIn(forbidden, helper)
        guard_manifest = (ROOT / "bluetooth-guard/src/manifest.xml").read_text(encoding="utf-8")
        hub_manifest = (ROOT / "project-hub/src/manifest.xml").read_text(encoding="utf-8")
        self.assertIn('level="requireAdministrator" uiAccess="false"', guard_manifest)
        self.assertIn('level="asInvoker" uiAccess="false"', hub_manifest)

    def test_cards_give_connection_details_and_auto_choice_separate_rows(self):
        entry = (ROOT / "bluetooth-guard/src/parts/window_entry.c").read_text(encoding="utf-8")
        self.assertIn("int cardH = S(108, dpi);", entry)
        self.assertIn("DescribeDevice(d, radioOn, GetTickCount64(), &visual);", entry)
        self.assertIn("visual.status, visual.statusFg, visual.statusBg", entry)
        self.assertIn("visual.statusFg, visual.statusFg, listTop, listBottom)", entry)
        self.assertIn("DrawTextW(mem, visual.detail,", entry)
        self.assertIn("DrawTextW(mem, visual.preference,", entry)
        self.assertIn("SetTextColor(mem, visual.preferenceFg);", entry)
        self.assertIn("cr.top + S(38, dpi), cr.right - S(16, dpi), cr.top + S(60, dpi)", entry)
        self.assertIn("btnTopY, forgetR.left - S(14, dpi), btnBottomY", entry)

    def test_three_binary_cache_keys_follow_the_shared_reveal_protocol(self):
        import importlib.util
        import tempfile
        spec = importlib.util.spec_from_file_location("reveal_cache_build", ROOT / "tools/build_windows.py")
        builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(builder)
        shared = ROOT / "shared/win32/window_reveal.h"
        for app in ("hub", "btguard", "kbl"):
            self.assertIn(shared, builder.TARGET_SHARED_SOURCES[app])
        with tempfile.TemporaryDirectory() as name:
            folder = Path(name)
            src = folder / "src"
            src.mkdir()
            (src / "app.c").write_text("/* unchanged */", encoding="ascii")
            header = folder / "window_reveal.h"
            header.write_text("/* protocol one */", encoding="ascii")
            builder.ROOT = folder
            builder.TARGET_SHARED_SOURCES = {app: (header,) for app in ("hub", "btguard", "kbl")}
            before = {app: builder.target_key(app, src, [], "test") for app in ("hub", "btguard", "kbl", "agentnotes")}
            header.write_text("/* protocol two */", encoding="ascii")
            for app in ("hub", "btguard", "kbl"):
                self.assertNotEqual(before[app], builder.target_key(app, src, [], "test"))
            self.assertEqual(before["agentnotes"], builder.target_key("agentnotes", src, [], "test"))


class RadarWindowsLocationTests(unittest.TestCase):
    """Actual C lifecycle/storage/selection logic with Windows provider I/O mocked.
    Windows consent, native rendering and actual-position accuracy stay unverified."""

    @staticmethod
    def _source(name):
        return (ROOT / "weather-radar/src/parts" / name).read_text(encoding="utf-8")

    @staticmethod
    def _function(source, signature):
        start = source.index(signature)
        return source[start:source.index("\n}\n", start) + 3]

    def test_one_shot_location_executes_production_lifecycle_and_storage(self):
        import shutil
        import tempfile
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            self.skipTest("portable C compiler is unavailable")
        main = (ROOT / "weather-radar/src/weather_radar.c").read_text(encoding="utf-8")
        model = re.search(r"typedef struct \{.*?\n\} Place;", main, re.S).group(0)
        places = self._source("places.c")
        storage = places[places.index("static void LoadPlaces("):places.index("/* Geocoding:")]
        ui = self._source("ui.c")
        funcs = "\n".join(self._function(ui, sig) for sig in (
            "static void OnPickSearchRow(", "static void OnPickSavedRow("))
        apply_start = ui.index("        if (kind == JOB_LOCATION) {")
        apply_end = ui.index("        EnterCriticalSection(&g_lock);", apply_start)
        apply = "static int DeliverLocationMessage(DWORD ticket) { int kind = JOB_LOCATION;\n" + ui[apply_start:apply_end] + "return 0; }\n"
        buttons = main[main.index("enum { BTN_SEARCH"):main.index("};", main.index("enum { BTN_SEARCH")) + 2]
        layout = buttons + "\nstatic int g_dpi = 96, g_fit = 100;\n" + self._function(main, "static int S(int px)")
        layout += "\n".join(re.search(r"^#define " + name + r" .*$", ui, re.M).group(0)
                            for name in ("WIN_LW", "WIN_LH", "PLACE_LIST_TOP")) + "\n"
        layout += "\n".join(self._function(ui, signature) for signature in (
            "static void BtnRect(", "static int SearchRowRect(", "static int SavedHeadingY(",
            "static int SavedRowRect(", "static int HitBtn("))
        header = model + "\n#define MAX_PLACES 8\n#define MAX_SEARCH 8\n" + """
static Place g_places[MAX_PLACES], g_search[MAX_SEARCH];
static int g_placeN, g_placeSel;
""" + layout + "\n" + storage + '\n#include "' + (ROOT / "weather-radar/src/parts/location.c").as_posix() + '"\n' + """
static void RequestPlaceData(const Place *p) {
    assert(p == &g_places[g_placeSel]); weatherRequests++; g_placeTicket++;
}
static void StatusSet(const wchar_t *text, int kind) { (void)kind; wcsncpy(statusText, text, 319); }
""" + funcs + "\n" + apply
        with tempfile.TemporaryDirectory(prefix="radar-location-") as td:
            (Path(td) / "radar_location_under_test.h").write_text(header, encoding="utf-8")
            exe = Path(td) / "radar-location"
            build = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", td,
                                    str(ROOT / "tests/native/radar_location_harness.c"), "-lm", "-o", str(exe)],
                                   capture_output=True, text=True, timeout=30)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn("Radar one-shot location scenarios passed (mocked Windows provider)", run.stdout)

    def test_location_permission_is_explicit_foreground_and_nonblocking(self):
        ui, entry = self._source("ui.c"), self._source("entry.c")
        core, provider = self._source("location.c"), self._source("location_winrt.c")
        self.assertEqual(ui.count("RequestWindowsLocation();"), 1)
        self.assertIn("case BTN_LOCATION:\n                RequestWindowsLocation();", ui)
        self.assertNotIn("RequestWindowsLocation(", entry)
        self.assertNotIn("LocationProvider", entry)
        self.assertIn("if (wp == TIMER_LOCATION) { PollWindowsLocation(); return 0; }", ui)
        request = self._function(core, "static void RequestWindowsLocation(")
        self.assertLess(request.index("GetForegroundWindow() != g_hwnd"), request.index("LocationProviderBeginAccess();"))
        self.assertIn("if (g_locationPending) { CancelWindowsLocation(); return; }", request)
        self.assertIn("state != LOCATION_PROVIDER_READY", core)
        self.assertIn("LocationProviderBeginFix();", core)
        self.assertIn('L"Windows.Devices.Geolocation.Geolocator"', provider)
        self.assertIn("factory->lpVtbl->RequestAccessAsync(factory, &g_locationAccess)", provider)
        self.assertIn("return allowed == 1 ? LOCATION_PROVIDER_READY", provider)
        self.assertIn("GetGeopositionAsyncWithAgeAndTimeout(", provider)
        self.assertIn("maxAge = {60LL * 10000000LL}, timeout = {30LL * 10000000LL}", provider)
        self.assertIn("if (status == Started) return LOCATION_PROVIDER_PENDING;", provider)
        self.assertIn("if (status != Completed)", provider)
        for forbidden in ("Sleep(", "WaitForSingleObject(", "HttpGetBuf(", "add_PositionChanged(",
                          "add_StatusChanged(", "AllowFallbackToConsentlessPositions(", "ILocation_", "CreateProcess"):
            self.assertNotIn(forbidden, core + provider)
        self.assertIn('LoadLibraryExW(L"combase.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32)', provider)
        self.assertIn("coordinate->lpVtbl->get_Accuracy(coordinate, &sample->accuracyM)", provider)
        self.assertIn("coordinate->lpVtbl->get_Timestamp(coordinate, &stamp)", provider)
        self.assertIn("point->lpVtbl->get_Position(point, &basic)", provider)
        self.assertNotIn("->get_Latitude(", provider)  # use modern Point.Position, not deprecated getters
        self.assertIn("if (cancel) (void)IAsyncInfo_Cancel(g_locationInfo);", provider)
        self.assertIn("(void)IAsyncInfo_Close(g_locationInfo);", provider)
        self.assertIn("LocationRoUninitialize();", provider)

    def test_location_selection_cancellation_and_single_coordinate_route(self):
        ui, workers = self._source("ui.c"), self._source("workers.c")
        for signature in ("static void OnPickSearchRow(", "static void OnPickSavedRow("):
            self.assertIn("CancelWindowsLocation();", self._function(ui, signature))
        close = ui[ui.index("case WM_SHOWWINDOW:"):ui.index("case WM_PAINT:")]
        self.assertGreaterEqual(close.count("CancelWindowsLocation();"), 3)  # hide, close, minimize
        destroy = ui[ui.index("case WM_DESTROY:"):]
        self.assertIn("CancelWindowsLocation();", destroy)
        self.assertIn("LocationProviderShutdown();", destroy)
        drop = ui[ui.index("case BTN_DROP:", ui.index("case WM_LBUTTONUP:")):ui.index("default: break;", ui.index("case BTN_DROP:", ui.index("case WM_LBUTTONUP:")))]
        self.assertLess(drop.index("CancelWindowsLocation();"), drop.index("DropSelectedPlace();"))
        self.assertIn("else ForgetPlaceData();", drop)
        clear = self._function(workers, "static void ForgetPlaceData(")
        for statement in ("g_reqHas = 0;", "if (++g_placeTicket == 0) g_placeTicket = 1;", "g_viewReady = 0;"):
            self.assertIn(statement, clear)
        apply_start = ui.index("if (kind == JOB_LOCATION)")
        apply = ui[apply_start:ui.index("EnterCriticalSection(&g_lock);", apply_start)]
        self.assertLess(apply.index("TakeWindowsLocation(ticket, &result)"), apply.index("RememberPlace(&result.place);"))
        self.assertIn("if (result.have)", apply)
        self.assertIn("RequestPlaceData(&g_places[g_placeSel]);", apply)
        self.assertIn("g_reqPlace = *p;", workers)
        self.assertIn("p->lat, p->lon", self._source("forecast.c"))
        self.assertIn("pl.lat, pl.lon", ui)  # marker and crop use the same selected point

    def test_location_source_accuracy_timestamp_and_settings_remain_visible(self):
        ui, core = self._source("ui.c"), self._source("location.c")
        paint = self._function(ui, "static void PaintLocationDetail(")
        for text in ("Source: Windows saved point", "Source: city/postcode search", "%.5f, %.5f",
                     "Approx. +/-", "Reported +/-", "Fix:", "g_locationNotice"):
            self.assertIn(text, paint)
        self.assertIn("PaintLocationDetail(mem);", ui)
        self.assertIn('L"Use my location"', ui)
        self.assertIn('L"Cancel location"', ui)
        self.assertIn('L"ms-settings:privacy-location"', ui)
        self.assertIn("sample->accuracyM >= 1000.0", core)
        self.assertIn("Approximate Windows fix", core)
        self.assertIn("LocationFixProblem(result->place.lat", core)
        self.assertIn("g_locationStageTicket != ticket", core)
        self.assertIn("if (!WindowsLocationCurrent(ticket)) return;", core)
        # All eight matches plus eight saved rows fit below the new controls.
        top = int(re.search(r"#define PLACE_LIST_TOP (\d+)", ui).group(1))
        self.assertGreater(top, 326 + 24)
        self.assertLess(top + 8 * 30 + 12 + 22 + 7 * 30 + 28, 960 - 56)

    def test_geolocation_abi_prefixes_match_documented_sdk_slots(self):
        header = self._source("location_abi.h")
        expected = {
            "LocStatics": ["RequestAccessAsync"],
            "LocGeolocator": ["get_DesiredAccuracy", "put_DesiredAccuracy", "get_MovementThreshold",
                              "put_MovementThreshold", "get_ReportInterval", "put_ReportInterval",
                              "get_LocationStatus", "GetGeopositionAsync", "GetGeopositionAsyncWithAgeAndTimeout"],
            "LocAccessOp": ["put_Completed", "get_Completed", "GetResults"],
            "LocPositionOp": ["put_Completed", "get_Completed", "GetResults"],
            "LocPosition": ["get_Coordinate"],
            "LocCoordinate": ["get_Latitude", "get_Longitude", "get_Altitude", "get_Accuracy",
                              "get_AltitudeAccuracy", "get_Heading", "get_Speed", "get_Timestamp"],
            "LocCoordinatePoint": ["get_Point"], "LocPoint": ["get_Position"],
        }
        for interface, methods in expected.items():
            declaration = header.split("struct " + interface + "Vtbl {", 1)[1].split("\n};", 1)[0]
            self.assertIn("LOC_INSPECTABLE(" + interface + ");", declaration)
            self.assertEqual(re.findall(r"\*([a-zA-Z_]+)\)", declaration), methods)
        self.assertIn("0x9a8e7571,0x2df5,0x4591,{0x9f,0x87,0xeb,0x5f,0xd8,0x94,0xe9,0xb7}", header)
        self.assertIn("0xa9c3bf62,0x4524,0x4989,{0x8a,0xa9,0xde,0x01,0x9d,0x2e,0x55,0x1f}", header)
        self.assertIn("0xfeea0525,0xd22c,0x4d46,{0xb5,0x27,0x0b,0x96,0x06,0x6f,0xc7,0xdb}", header)
        self.assertIn("0x00000036,0,0,{0xc0,0,0,0,0,0,0,0x46}", header)
        self.assertIn("typedef struct { INT64 Duration; } LocTimeSpan;", header)
        self.assertIn("typedef struct { INT64 UniversalTime; } LocDateTime;", header)
        self.assertIn("typedef struct { double Latitude, Longitude, Altitude; } LocBasicPosition;", header)


class SafetyMutationPropertyTests(unittest.TestCase):
    """Properties previously hidden by the mutation registry's own drift check.
    Static call-order contracts and production-C helpers are distinct from
    native Windows/network acceptance; no app behavior is changed here."""

    def _native(self, source):
        import shutil
        import tempfile
        compiler = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
        if compiler is None:
            raise FileNotFoundError("Portable C compiler is required for this property test")
        with tempfile.TemporaryDirectory(prefix="safety-property-") as name:
            folder = Path(name)
            (folder / "property.c").write_text(source, encoding="utf-8")
            built = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                                    str(folder / "property.c"), "-o", str(folder / "property")],
                                   capture_output=True, text=True, timeout=15)
            self.assertEqual(built.returncode, 0, built.stderr)
            ran = subprocess.run([str(folder / "property")], capture_output=True, text=True, timeout=15)
            self.assertEqual(ran.returncode, 0, ran.stderr)

    def test_notes_palette_is_initialized_before_native_window_resources(self):
        entry = (ROOT / "agent-notes/src/parts/entry.c").read_text()
        self.assertIn("    InitPalette();", entry)
        self.assertLess(entry.index("InitPalette();"), entry.index("CreateMutexW("))
        self.assertLess(entry.index("InitPalette();"), entry.index("CreateWindowExW("))

    def test_geocoding_ticket_helper_rejects_old_answers_under_its_lock(self):
        places = (ROOT / "weather-radar/src/parts/places.c").read_text()
        start = places.index("static int SearchTicketIsCurrent(DWORD ticket)")
        helper = places[start:places.index("\n}", start) + 2]
        self._native("""#include <assert.h>
#include <stdint.h>
typedef uint32_t DWORD;
static int g_lock, locked;
static DWORD g_searchTicket;
static void EnterCriticalSection(int *p) { assert(p == &g_lock && !locked); locked = 1; }
static void LeaveCriticalSection(int *p) { assert(p == &g_lock && locked); locked = 0; }
""" + helper + """
int main(void) {
    const DWORD tickets[] = {0, 1, 100, UINT32_MAX};
    for (unsigned i = 0; i < sizeof(tickets)/sizeof(tickets[0]); i++) {
        g_searchTicket = tickets[i];
        assert(SearchTicketIsCurrent(tickets[i]) && !locked);
        assert(!SearchTicketIsCurrent(tickets[i] + 1) && !locked);
    }
    return 0;
}
""")

    def test_canonical_emblem_conversion_exports_the_required_bgr_adapter(self):
        emblem = (ROOT / "shared/win32/suite_emblem.c").read_text()
        start = emblem.index("static uint32_t SuiteEmblemDibRgb(")
        helpers = emblem[start:emblem.index("/* One shared timebase", start)]
        self._native("""#include <assert.h>
#include <stdint.h>
typedef uint32_t COLORREF;
#define GetRValue(c) ((c) & 255)
#define GetGValue(c) (((c) >> 8) & 255)
#define GetBValue(c) (((c) >> 16) & 255)
""" + helpers + """
int main(void) {
    assert(SuiteEmblemDibPixel(0x00412111) == 0x00112141);
    assert(SuiteEmblemDibPixel(0) == 0);
    assert(SuiteEmblemDibPixel(0x00ffffff) == 0x00ffffff);
    return 0;
}
""")

    def test_old_place_forecast_cannot_publish_status_or_consider_rain_alerts(self):
        ui = (ROOT / "weather-radar/src/parts/ui.c").read_text()
        start = ui.index("if (kind == JOB_FORECAST)")
        forecast = ui[start:ui.index("else if (kind == JOB_ENSEMBLE)", start)]
        self.assertIn("int current = (ticket == g_placeTicket);", forecast)
        self.assertIn("if (!current) return 0;", forecast)
        guard = forecast.index("if (!current) return 0;")
        self.assertLess(guard, forecast.index("StatusSet("))
        self.assertLess(guard, forecast.index("RainAlertConsider("))

    def test_hub_settings_persist_panel_before_live_theme_and_keep_rollback(self):
        page = (ROOT / "project-hub/src/parts/settings_page.c").read_text()
        start = page.index("static BOOL ApplyKeyboardSetting(")
        apply = page[start:page.index("static BOOL ApplySmooth(", start)]
        panel = "if (!WriteBinaryFileAtomic(panelPath, (const BYTE *)panelOut, panelOutLen))"
        theme = "if (!WriteBinaryFileAtomic(themePath, (const BYTE *)themeOut, themeOutLen))"
        self.assertIn(panel, apply, "Hub must persist the saved panel preference")
        self.assertIn(theme, apply, "Hub must persist the corresponding live preference")
        self.assertLess(apply.index(panel), apply.index(theme))
        rollback = apply[apply.index(theme):apply.index("ok = TRUE;")]
        self.assertIn("WriteBinaryFileAtomic(panelPath, (const BYTE *)panelTxt, panelLen)", rollback)
        self.assertIn("else DeleteFileW(panelPath);", rollback)
        self.assertIn("goto done;", rollback)

    def test_waiting_notes_relay_the_same_wait_id_before_local_duplicate_suppression(self):
        notes = (ROOT / "agent-notes/src/parts/wait_alerts.c").read_text()
        start = notes.index("static void HandleStatusPoll(")
        waiting = notes[start:notes.index("ShowAgentWaitingAlert();", start)]
        self.assertRegex(waiting, r"(?s)if \(result->statusOk\).*if \(result->waiting\).*"
                                 r"PublishHubWaitRelay\(result->waitId\);")
        self.assertLess(waiting.index("PublishHubWaitRelay(result->waitId);"),
                        waiting.index("wcscmp(result->waitId, g_seenWait)"))


class GuardSecurityReportTests(unittest.TestCase):
    """Portable source/regex contracts; Windows query fixtures run separately."""

    @staticmethod
    def _source():
        return (ROOT / "tools/guard_security_report.ps1").read_text(encoding="ascii")

    def _function(self, name):
        source = self._source()
        start = source.index("function " + name + " {")
        end = source.find("\nfunction ", start + 1)
        return source[start:end if end >= 0 else len(source)]

    def test_collector_has_closed_readonly_command_and_type_allowlists(self):
        code = lex_powershell(self._source())
        commands = set(re.findall(r"\b[A-Za-z][A-Za-z0-9]*-[A-Za-z][A-Za-z0-9-]*\b", code))
        allowed = {
            "Format-GuardValue", "Add-GuardField", "Format-GuardQueryError",
            "Get-GuardResourceMatches", "Convert-GuardEventData", "Assert-GuardLocalPath",
            "Get-GuardFileEvidence", "New-GuardSecurityReport", "Write-GuardReportFile",
            "Invoke-GuardSecurityReport", "Get-CimInstance", "Get-MpComputerStatus",
            "Get-MpThreatDetection", "Get-MpThreat", "Get-WinEvent", "Join-Path",
            "Sort-Object", "Select-Object", "Where-Object", "ForEach-Object", "Write-Host",
        }
        self.assertFalse(commands - allowed, "New collector commands require safety review")
        types = set(re.findall(r"\[([A-Za-z][\w.]*(?:\[[\w.]+\])?)\]", code))
        self.assertFalse(types - {
            "string", "int", "DateTime", "regex", "ordered", "pscustomobject", "BitConverter",
            "Collections.Generic.List[string]", "Xml.XmlDocument", "IO.Path", "IO.DriveInfo",
            "IO.DriveType", "IO.Directory", "IO.File", "IO.FileAttributes", "IO.FileMode",
            "IO.FileAccess", "IO.FileShare", "IO.StreamWriter", "Text.UTF8Encoding",
            "Security.Cryptography.SHA256", "Diagnostics.FileVersionInfo", "Environment", "PlatformID",
        })
        self.assertNotIn("&", code, "No command invocation, launch or dot-sourced dependency")
        self.assertNotRegex(code, r"(?i)\b(?:Import-Module|using\s+(?:module|assembly))\b")
        self.assertIn("$MyInvocation.InvocationName -ne '.'", self._source())
        self.assertNotIn("ExecutionPolicy", code)

    def test_only_explicit_new_local_text_report_can_be_written(self):
        writer = self._function("Write-GuardReportFile")
        reader = self._function("Get-GuardFileEvidence")
        local = self._function("Assert-GuardLocalPath")
        self.assertEqual(re.findall(r"\[IO\.FileMode\]::(\w+)", writer), ["CreateNew"])
        self.assertEqual(re.findall(r"\[IO\.FileAccess\]::(\w+)", writer), ["Write"])
        self.assertEqual(re.findall(r"\[IO\.FileMode\]::(\w+)", reader), ["Open"])
        self.assertEqual(re.findall(r"\[IO\.FileAccess\]::(\w+)", reader), ["Read"])
        self.assertEqual(self._source().count("[IO.File]::Open("), 2)
        for function in (writer, reader):
            self.assertLess(function.index("Assert-GuardLocalPath $Path"), function.index("[IO.File]::Open("))
            self.assertIn("$stream.Dispose()", function)
        self.assertIn("GetExtension($full) -ine '.txt'", writer)
        self.assertIn("[IO.DriveType]::Fixed", local)
        self.assertIn("[IO.DriveType]::Removable", local)
        self.assertIn("$Path.Substring(2).Contains(':')", local)
        self.assertIn("[IO.FileAttributes]::ReparsePoint", local)
        self.assertIn("$parent = $parent.Parent", local)
        self.assertIn("[Text.UTF8Encoding]::new($true)", writer)
        self.assertIn("GuardSecurityReport*.txt", (ROOT / ".gitignore").read_text())

    def test_actual_resource_regex_rejects_suffixes_and_similar_executables(self):
        helper = self._function("Get-GuardResourceMatches")
        literal = re.search(r"\$text -notmatch '((?:[^']|'')*)'", helper).group(1).replace("''", "'")
        pattern = re.compile(literal)
        for resource in (
            "BluetoothGuard.exe", r"file:_C:\Suite\BluetoothGuard.exe",
            r"file:_C:\Users\Name With Spaces\BluetoothGuard.exe;", r'"C:\Suite\BLUETOOTHGUARD.EXE"',
            r"containerfile:_C:\bundle.zip;file:_C:\Suite\BluetoothGuard.exe",
            "C:/Suite/BluetoothGuard.exe", r"C:\Unicode-\u03a9\BluetoothGuard.exe",
        ):
            with self.subTest(resource=resource):
                self.assertIsNotNone(pattern.search(resource))
        for resource in (
            "BluetoothGuardXexe", "NotBluetoothGuard.exe", r"C:\Suite\BluetoothGuard.exe.bak",
            r"C:\Suite\BluetoothGuard.exe-old", r"C:\Suite\BluetoothGuard.exe_substitute",
            r"C:\Suite\BluetoothGuard.exe\child", "BluetoothGuard.exe:stream", "ProjectHub.exe", "",
        ):
            with self.subTest(resource=resource):
                self.assertIsNone(pattern.search(resource))
        self.assertIn("[regex]::Escape($InstalledPath.Replace", helper)
        self.assertIn("filename-only candidate; NOT installed-path proof", helper)
        self.assertIn("installed-path text; NOT file-hash proof", helper)

    def test_history_correlates_affected_resources_not_process_mentions(self):
        report = self._function("New-GuardSecurityReport")
        self.assertIn("Get-GuardResourceMatches $detection.Resources $guardPath", report)
        self.assertIn("Get-GuardResourceMatches @($data['Path'], $data['Resources']) $guardPath", report)
        self.assertNotIn("$detection.ProcessName", report)
        self.assertNotIn("$event.Message", report)
        self.assertIn("if ($matches.Count -gt 0)", report)
        self.assertIn("if ($matches.Count -eq 0) { continue }", report)
        xml = self._function("Convert-GuardEventData")
        self.assertIn("$document.XmlResolver = $null", xml)
        self.assertIn("<!DOCTYPE", xml)
        self.assertIn("$node.GetAttribute('Name')", xml)
        self.assertIn("$node.InnerText", xml)
        self.assertIn("$invalidEvents++", report)
        self.assertNotIn("$data['Detection User']", report)

    def test_local_queries_fail_explicitly_and_history_limits_are_visible(self):
        report = self._function("New-GuardSecurityReport")
        for command in ("Get-CimInstance", "Get-MpComputerStatus", "Get-MpThreatDetection", "Get-MpThreat"):
            invocation = next(line for line in report.splitlines() if command + " " in line)
            self.assertIn("-ErrorAction Stop", invocation)
        self.assertIn("-OperationTimeoutSec 8", report)
        self.assertNotRegex(report, r"-(?:ComputerName|CimSession|Credential)\b")
        self.assertIn("Id = @(1116, 1117, 1118, 1119); StartTime = $since", report)
        self.assertIn("-MaxEvents $EventLimit -ErrorAction Stop", report)
        self.assertIn("-First 50", report)
        self.assertIn("$events.Count -ge $EventLimit", report)
        self.assertIn("$rows.Count -gt 50", report)
        self.assertIn("NoMatchingEventsFound*", report)
        self.assertIn("UNAVAILABLE:", self._function("Format-GuardQueryError"))
        self.assertIn("NOT a clean verdict", report)
        self.assertIn("history is NOT exhaustive", report)
        self.assertIn("Third-party histories are NOT collected", report)
        self.assertNotIn("file is clean", report.lower())
        self.assertNotIn("no malware detected", report.lower())

    def test_full_hashes_are_bound_to_frozen_git_artifacts_not_tip_or_short_prefixes(self):
        report = self._function("New-GuardSecurityReport")
        commit = re.search(r"\$baselineCommit = '([0-9a-f]{40})'", report).group(1)
        # Independently audited known-answer provenance. The collector deliberately
        # stays frozen when future EXEs change; a depth-one/offline checkout has
        # neither the old Git objects nor permission to fetch replacement bytes.
        self.assertEqual(commit, "543be95ad860d6e11ffadf30248d413e0dcc839c")
        frozen_hashes = {
            "guardHash": "4ad58a32f01a94713c37061cb79e2bf596e7c61ce8f6255b33caccb96938a2a6",
            "hubHash": "289de84e01b63375bc7e622ea11cc82a647f29acadb650c957d6a693d1f0a8e0",
        }
        for variable, expected in frozen_hashes.items():
            actual = re.search(r"\$" + variable + r" = '([0-9a-f]{64})'", report).group(1)
            self.assertEqual(actual, expected)
        reader = self._function("Get-GuardFileEvidence")
        self.assertIn("$sha.ComputeHash($stream)", reader)
        self.assertIn("$hash -eq $ExpectedHash", reader)
        self.assertNotIn("Substring", reader)
        self.assertIn("MATCHES pinned bytes; NOT a malware verdict", reader)
        self.assertIn("NOT COMPARED.", reader)
        self.assertIn("Signature trust/catalog/revocation validation is NOT attempted", report)
        self.assertNotIn("Get-AuthenticodeSignature", self._source())

    def test_field_privacy_and_query_errors_do_not_export_unfiltered_details(self):
        formatter = self._function("Format-GuardValue")
        field = self._function("Add-GuardField")
        errors = self._function("Format-GuardQueryError")
        report = self._function("New-GuardSecurityReport")
        self.assertIn("Format-GuardValue $Value", field)
        for prefix in ("$env:LOCALAPPDATA", "$env:USERPROFILE", "%LOCALAPPDATA%", "%USERPROFILE%"):
            self.assertIn(prefix, formatter)
        self.assertIn(".Replace(\"`r\", '\\r').Replace(\"`n\", '\\n')", formatter)
        self.assertNotIn(".Message", errors)
        self.assertNotIn(".ToString()", errors)
        self.assertNotRegex(report, r"\$(?:detection|status|product)\.(?:UserName|PSComputerName|ComputerID)")
        self.assertIn("REVIEW BEFORE SHARING", report)
        self.assertIn("Registered antivirus products (NOT proof which product blocked this launch)", report)
        self.assertIn("not an assertion of current active provider", report)

    def test_windows_fixture_suite_is_separate_from_real_provider_acceptance(self):
        fixture = (ROOT / "tests/native/guard_security_report_fixtures.ps1").read_text(encoding="ascii")
        lex_powershell(fixture)
        for mocked in ("Get-CimInstance", "Get-MpComputerStatus", "Get-MpThreatDetection", "Get-MpThreat", "Get-WinEvent"):
            self.assertIn("function " + mocked + " {", fixture)
        for case in ("Denied", "Empty", "NoEvents", "Cap", "PRIVATE_ACCOUNT", "UNRELATED_THREAT", "<!DOCTYPE"):
            self.assertIn(case, fixture)
        self.assertIn("CreateDirectory($scratch)", fixture)
        self.assertIn("[IO.Directory]::Delete($scratch, $true)", fixture)
        self.assertIn("fixture.bin", fixture)
        self.assertNotIn("Start-Process", fixture)
        self.assertNotIn("ExecutionPolicy", fixture)
        self.assertIn("Not a native provider, malware, launch or focus verdict", fixture)


class PublicNameRolloutTests(unittest.TestCase):
    """Public labels are not storage, launch, preset or G-code identities."""

    def test_catalogue_brands_keep_storage_launch_and_update_identities(self):
        apps = {a["id"]: a for a in json.loads((ROOT / "suite-apps.json").read_text())["apps"]}
        for app_id, title, exe, directory in (
            ("kbl", "Prism Lighting", "keyboard-lighting/KeyboardLighting.exe", "KeyboardLighting"),
            ("weatherradar", "Weather Report", "weather-radar/WeatherRadar.exe", "WeatherRadar"),
        ):
            app = apps[app_id]
            self.assertEqual((app["title"], app["shortcut"]), (title, title))
            self.assertEqual((app["exe"], app["dir"], app["state"], app["worker"]),
                             (exe, directory, app_id, app_id))
        self.assertEqual(apps["btguard"]["title"], "Bluetooth Guard")
        self.assertEqual(apps["unlayered-infill"]["title"], "Unlayered Infill")

    def test_weather_brand_is_consistent_without_changing_native_identities(self):
        source = (ROOT / "weather-radar/src/weather_radar.c").read_text()
        self.assertRegex(source, r'#define APP_TITLE_W\s+L"Weather Report"')
        self.assertRegex(source, r'#define APP_DIRNAME_W\s+L"WeatherRadar"')
        for identity in (r'Local\\Tests_WeatherRadar_SingleInstance_v1',
                         r'Local\\Tests_WeatherRadar_Show_v1', 'TestsSuite-WeatherRadar/1.0'):
            self.assertIn(identity, source)
        entry = (ROOT / "weather-radar/src/parts/entry.c").read_text()
        self.assertIn('L"WeatherRadarMainV1"', entry)
        alerts = (ROOT / "weather-radar/src/parts/alerts.c").read_text()
        for label in ('L"Weather Report - exact-place rain watch"', 'L"Open Weather Report"',
                      'L"Weather Report keeps watching"', 'Open Weather Report for details.',
                      'Weather Report will not repeat this while this rain episode continues.'):
            self.assertIn(label, alerts)
        location = (ROOT / "weather-radar/src/parts/location.c").read_text()
        self.assertIn('L"Bring Weather Report to the front, then use My location."', location)
        self.assertNotIn('Weather Radar', alerts + location)

    def test_prism_hub_labels_keep_native_and_legacy_launch_routes(self):
        parts = ROOT / "project-hub/src/parts"
        for rel in ('settings_page.c', 'tray_unified.c', 'enqueue_restart_prompts.c'):
            source = (parts / rel).read_text()
            self.assertIn('Prism Lighting', source)
            self.assertNotIn('Keyboard Lighting', source)
        worker = (parts / "workers_install.c").read_text()
        self.assertIn('desk, L"Prism Lighting", L"Keyboard Lighting",', worker)
        self.assertIn('L"KeyboardLighting.exe"', worker)
        self.assertIn('L"TestsKeyboardLightingSuiteWnd"', worker)
        folder = (parts / "install_folder.c").read_text()
        self.assertIn('{ L"Prism Lighting", L"Keyboard Lighting" }', folder)
        self.assertIn('kblLinkNames[i]', folder)
        self.assertIn('AddMirrorDir(MIRROR_KBL, found);', folder)
        generic = (parts / "card_defs_verbs.c").read_text()
        self.assertIn('_wcsicmp(c->id, L"weatherradar") == 0', generic)
        self.assertIn('? L"Weather Radar" : NULL;', generic)
        self.assertIn('RepointAppDesktopShortcutsIfPresent(desk, name, legacy, destExe,', generic)
        # The fallback PowerShell Hub is not part of the gated lighting app.
        legacy = (ROOT / "project-hub/ProjectHub.ps1").read_text(encoding="utf-8")
        self.assertIn("New-ProjectCard 'Prism Lighting'", legacy)

    def test_orca_brand_does_not_rename_preset_or_output_identities(self):
        source = (ROOT / "plugins/wave-overhangs/wave_overhangs_orca.py").read_text()
        manifest = json.loads((ROOT / "plugins.json").read_text())["plugins"][0]
        self.assertEqual((manifest["id"], manifest["name"], manifest["orca_dir"], manifest["file"]),
                         ("wave-overhangs", "Wave Overhangs", "WaveOverhangs", "wave_overhangs_orca.py"))
        self.assertEqual(manifest["path"], "plugins/wave-overhangs/wave_overhangs_orca.py")
        expected = ["Wave Overhangs", "Wave Overhangs - Settings guide & check"]
        self.assertEqual(manifest["capabilities"], expected)
        self.assertIn('# name = "Wave Overhangs"', source)
        self.assertIn('# description = "Undahang (Orca identity: Wave Overhangs).', source)
        tree = ast.parse(source)
        names = []
        for node in tree.body:
            if isinstance(node, ast.ClassDef) and node.name in ("WaveOverhangsSlicing", "WaveOverhangsCheck"):
                method = next(f for f in node.body if isinstance(f, ast.FunctionDef) and f.name == "get_name")
                names.append(ast.literal_eval(method.body[0].value))
        self.assertEqual(names, expected, "Orca stores these strings in presets and global config")
        self.assertIn('Undahang v{PLUGIN_VERSION} -- setup check', source)
        self.assertIn('Orca identity: Wave Overhangs (kept for saved presets).', source)
        self.assertIn('Wave Overhangs. Host selection/reference handling can', source)
        self.assertIn('vary; this setup check never rewires it for you.', source)
        self.assertIn('; wave-overhangs v', source)
        self.assertIn('Undahang v{PLUGIN_VERSION} loaded', source)

    def test_existing_shortcut_aliases_repoint_without_creating_or_renaming(self):
        import shutil
        import tempfile
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            raise unittest.SkipTest("portable C compiler is unavailable")
        text = (ROOT / "project-hub/src/parts/install_folder.c").read_text()
        functions = []
        for name in ('RepointDesktopShortcutIfPresent', 'RepointAppDesktopShortcutsIfPresent'):
            start = text.index('static BOOL ' + name + '(')
            functions.append(text[start:text.index('\n}\n', start) + 3])
        with tempfile.TemporaryDirectory(prefix="name-shortcuts-") as tmp:
            folder = Path(tmp)
            (folder / "name_shortcut_production.h").write_text('\n'.join(functions))
            exe = folder / "name-shortcuts"
            run = subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                                  '-I', str(folder), str(ROOT / 'tests/native/name_shortcut_harness.c'),
                                  '-o', str(exe)], capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn('existing-only, deduplicated, bounded, failure-honest', run.stdout)

    def test_future_version_bump_updates_brand_docs_but_not_orca_identity(self):
        import shutil
        import sys
        import tempfile
        files = ['tools/bump_version.py', 'Orca-Plugins.bat', 'plugins.json', 'README.md',
                 'docs/ROADMAP.md', 'plugins/unlayered-infill/README.md',
                 'plugins/wave-overhangs/README.md', 'plugins/wave-overhangs/CHANGELOG.md',
                 'plugins/wave-overhangs/wave_overhangs_orca.py']
        with tempfile.TemporaryDirectory(prefix="brand-version-") as tmp:
            folder = Path(tmp)
            for rel in files:
                target = folder / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / rel, target)
            changelog = folder / 'plugins/wave-overhangs/CHANGELOG.md'
            changelog.write_text('## 9.9.9 — 2026-11-16\n\n* Future version probe.\n\n' + changelog.read_text())
            run = subprocess.run([sys.executable, str(folder / 'tools/bump_version.py'),
                                  'wave-overhangs', '9.9.9'], capture_output=True, text=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            readme = (folder / 'README.md').read_text()
            self.assertIn('[Undahang](plugins/wave-overhangs/) | 9.9.9 |', readme)
            self.assertIn('`Undahang v9.9.9 -- setup check`', readme)
            self.assertIn('`Wave Overhangs v9.9.9`', readme)
            self.assertIn('**9.9.9** for Undahang.', readme)
            self.assertIn('| Undahang | 9.9.9.', (folder / 'docs/ROADMAP.md').read_text())
            self.assertIn('Version column reads **9.9.9**.',
                          (folder / 'plugins/wave-overhangs/README.md').read_text())
            raw = (folder / 'Orca-Plugins.bat').read_bytes()
            self.assertIn(b'wave-overhangs^|Wave Overhangs^|9.9.9^|WaveOverhangs^|', raw)
            self.assertNotIn(b'\n', raw.replace(b'\r\n', b''))
            manifest = json.loads((folder / 'plugins.json').read_text())["plugins"]
            self.assertEqual((manifest[0]['name'], manifest[0]['version']), ('Wave Overhangs', '9.9.9'))
            self.assertEqual(manifest[1], json.loads((ROOT / 'plugins.json').read_text())['plugins'][1],
                             'Bumping Undahang must not change the other plugin at any current version')
            run = subprocess.run([sys.executable, str(folder / 'tools/bump_version.py'), '--check-docs'],
                                 capture_output=True, text=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)



class LightingStateTests(unittest.TestCase):
    """Execute actual native load/save, hotkey, readback and live-apply code.
    Deterministic mock disk and clock; not installed Windows/hardware acceptance.
    """

    @classmethod
    def setUpClass(cls):
        import shutil
        import tempfile
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            raise unittest.SkipTest("portable C compiler is unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="lighting-state-")
        cls.addClassCleanup(cls.temp.cleanup)
        folder = Path(cls.temp.name)
        root = (ROOT / "keyboard-lighting/src/keyboard_lighting.c").read_text(encoding="ascii")
        hotkeys = (ROOT / "keyboard-lighting/src/parts/brightness_hotkeys.c").read_text(encoding="ascii")
        bootstrap = (ROOT / "keyboard-lighting/src/parts/engine_bootstrap.c").read_text(encoding="ascii")

        def function(source, signature):
            start = source.index(signature)
            return source[start:source.index("\n}\n", start) + 3]

        subject = "\n".join(function(root, signature) for signature in (
            "static COLORREF ParseHexColor(", "static void FormatHexColor(",
            "static int FindEffectIndex(", "static void InitDefaultCfg("))
        subject += "\n" + hotkeys[:hotkeys.index("/* One-line plain-English")]
        subject += "\n" + function(hotkeys, "static void FormatThemeBlock(")
        subject += "\n" + function(hotkeys, "static void WriteThemeJson(")
        subject += "\n" + function(hotkeys, "static void SyncLinkedZone(")
        drag = (ROOT / "keyboard-lighting/src/parts/mouse_drag_menus.c").read_text(encoding="ascii")
        subject += "\n" + function(drag, "static void ApplySliderDrag(")
        subject += "\n" + function(bootstrap, "static void ScheduleLiveApply(void) {")
        subject += "\n" + function(bootstrap, "static void FlushLiveApply(void) {")
        (folder / "lighting_state_subject.h").write_text(subject)
        cls.exe = folder / "lighting-state"
        result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                                 "-Wno-unused-function", "-Wno-unused-variable", "-I", str(folder),
                                 "-I", str(ROOT / "keyboard-lighting/src/parts"),
                                 str(ROOT / "tests/native/lighting_state_harness.c"), "-lm", "-o", str(cls.exe)],
                                capture_output=True, text=True)
        if result.returncode:
            raise AssertionError("lighting state harness did not compile:\n" + result.stderr)

    def _run(self, scenario):
        result = subprocess.run([str(self.exe), scenario], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Lighting state " + scenario + " passed", result.stdout)

    def test_all_eight_balance_steady_zone_combinations_survive_save_close_and_reopen(self):
        self._run("restore")

    def test_json_scope_order_and_legacy_flat_preferences_are_preserved(self):
        self._run("scope-legacy")

    def test_external_hub_or_tray_changes_survive_a_later_unrelated_native_save(self):
        self._run("external")

    def test_unknown_fields_and_custom_palette_slot_positions_survive(self):
        self._run("unknown-custom")

    def test_registered_hotkeys_reach_live_master_before_readback_and_debounced_apply(self):
        self._run("hotkey")

    def test_a_malformed_external_file_is_not_overwritten_with_defaults(self):
        self._run("malformed")

    def test_exact_master_survives_restart_and_slider_and_zone_settings_do_not_fight(self):
        self._run("exact-slider")

    def test_restart_uses_saved_master_not_a_stale_live_file_from_an_earlier_run(self):
        self._run("stale-startup")

    def test_native_executable_contains_the_exact_matched_engine_not_an_old_branch_bootstrap(self):
        engine = (ROOT / "keyboard-lighting/Aura-Background.ps1").read_bytes()
        self.assertIn(engine, (ROOT / "keyboard-lighting/KeyboardLighting.exe").read_bytes())
        rc = (ROOT / "keyboard-lighting/src/kbl.rc").read_text(encoding="ascii")
        self.assertIn('3 RCDATA "../Aura-Background.ps1"', rc)
        lifecycle = (ROOT / "keyboard-lighting/src/parts/engine_bootstrap.c").read_text(encoding="ascii")
        self.assertIn("const wchar_t *script = PrepareBundledEngine();", lifecycle)
        self.assertIn("WriteAtomicUtf8(cached, text)", lifecycle)
        self.assertNotIn("FALLBACK_BRANCH_W", lifecycle)
        self.assertIn('ROOT / "keyboard-lighting" / "Aura-Background.ps1"', (ROOT / "tools/build_windows.py").read_text())

    def test_failed_save_keeps_local_deltas_for_a_successful_retry(self):
        self._run("failed-save")

    def test_framework_atomic_replace_uses_a_null_string_not_an_empty_backup_filename(self):
        # The first stock PowerShell 5.1 execution failed in actual File.Replace:
        # plain $null is coerced to an illegal empty backup filename on Framework.
        for relative, destination in (("Tray.ps1", "$path"), ("Aura-Background.ps1", "$liveFile")):
            with self.subTest(adapter=relative):
                source = (ROOT / "keyboard-lighting" / relative).read_text(encoding="ascii")
                call = "[IO.File]::Replace($tmp, " + destination + ", "
                self.assertIn(call + "[System.Management.Automation.Language.NullString]::Value)", source)
                self.assertNotIn(call + "$null)", source)

    def test_engine_protocol_and_windows_runtime_regression_are_wired(self):
        engine = (ROOT / "keyboard-lighting/Aura-Background.ps1").read_text(encoding="ascii")
        bootstrap = (ROOT / "keyboard-lighting/src/parts/engine_bootstrap.c").read_text(encoding="ascii")
        tray = (ROOT / "keyboard-lighting/Tray.ps1").read_text(encoding="ascii")
        master = (ROOT / "keyboard-lighting/src/parts/brightness_hotkeys.c").read_text(encoding="ascii")
        picker = (ROOT / "keyboard-lighting/src/parts/picker_window_entry.c").read_text(encoding="ascii")
        self.assertIn("-NativeBrightness", bootstrap)
        self.assertIn("[switch]$NativeBrightness", engine)
        legacy = engine[engine.index("if (-not $NativeBrightness) {"):engine.index("# The app that owns us has gone.")]
        self.assertIn("$brightUpFile", legacy)
        self.assertIn("$brightDownFile", legacy)
        apply = engine[engine.index("function Apply-LiveTheme"):engine.index("# --- wake from sleep")]
        self.assertIn("if (-not $NativeBrightness -and $null -ne $j.Brightness)", apply)
        self.assertIn("Set-Live $bv", apply)
        self.assertIn("-Smooth %S", bootstrap)
        self.assertIn('g_cfg.smooth ? "on" : "off"', bootstrap)
        self.assertIn("$z.SoftBlend = ($steady -ne 'off')", engine)
        self.assertIn("$engine.DitherMode = if ($steady -eq 'on')", engine)
        self.assertIn("Merge-PanelObject $script:Cfg $script:PanelBase $disk", tray)
        self.assertIn("Write-PanelAtomic $CfgFile $merged", tray)
        save = tray[tray.index("function Save-Cfg {"):tray.index("function Get-EffectToken {")]
        self.assertIn("if ($externalAdopted", save)
        self.assertIn("Load-UiFromCfg", save)
        ui = tray[tray.index("function Load-UiFromCfg {"):tray.index("function Sync-ExternalPanelCfg {")]
        for required in ("$trkMaster.Value =", "$chkSmooth.SetQuiet", "$chkEq.SetQuiet", "$chkOverlay.SetQuiet"):
            self.assertIn(required, ui)
        self.assertIn("$src = Get-Item -LiteralPath $EngineSource", engine)
        self.assertIn('"%llu:%lu"', picker)
        self.assertIn(" -Smooth ", tray)
        self.assertLess(engine.index("Set-StartupPreferences $eng"), engine.index("$eng.Start()"))
        self.assertIn("SyncMasterBrightness();\n    SetMasterBrightnessLevel(g_brightness.level + dir * 100)", master)
        self.assertNotIn("WriteLiveBrightness();", bootstrap[bootstrap.index("static void FlushLiveApply"):bootstrap.index("/* Light Bar physical")])
        self.assertIn("SyncMasterBrightness();\n    BrightnessStop(&g_brightness);\n    FlushPendingBrightnessCfg();", bootstrap)
        self.assertIn("SyncExternalPanelCfg();", picker)
        self.assertIn("./tests/lighting_engine_state.ps1", (ROOT / ".github/workflows/ci.yml").read_text())
        runtime = (ROOT / "tests/lighting_engine_state.ps1").read_text(encoding="ascii")
        for name in ("Apply-LiveTheme", "Sync-Live", "Set-StartupPreferences", "Load-Cfg", "Save-Cfg"):
            self.assertIn("'" + name + "'", runtime)
        # Linux source checks are distinct from the stock PowerShell 5.1
        # function-execution regression in the Windows CI job.



if __name__ == "__main__":
    unittest.main()
