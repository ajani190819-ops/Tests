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
import json
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent


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
        # Keyboard Lighting is the one deliberate exception to "every suite app
        # starts elevated". G-Helper's Custom action for M4/M5 launches it PER
        # KEYPRESS to write a brightness token into %LOCALAPPDATA%, which needs
        # no privileges at all - but requireAdministrator is enforced by the OS
        # at process creation, before wWinMain runs, so a non-elevated G-Helper
        # got a UAC consent prompt on every single keypress. It ships asInvoker
        # and still gets its administrator token from the /RL HIGHEST logon
        # task, from an elevated ProjectHub, or from its own runas
        # self-elevation. Pinned again, in detail, by
        # test_keyboard_lighting_brightness_taps_never_cost_a_uac_prompt.
        for rel in (
            "project-hub/ProjectHub.exe",
            "bluetooth-guard/BluetoothGuard.exe",
            "keyboard-lighting/KeyboardLighting.exe",
            "dns-pilot/DnsPilot.exe",
            "agent-notes/AgentNotes.exe",
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
            if rel == "keyboard-lighting/KeyboardLighting.exe":
                self.assertIn(b'level="asInvoker"', data,
                              f"{rel} must stay asInvoker, or every M4/M5 brightness "
                              f"keypress that G-Helper routes through it costs a UAC "
                              f"consent prompt")
                self.assertNotIn(b"requireAdministrator", data,
                                 f"{rel} requires administrator again")
            else:
                self.assertIn(b"requireAdministrator", data, f"{rel} must require administrator - all suite apps start elevated")

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
        """The Forget button must do a TRUE unpair (BluetoothRemoveDevice), not a
        profile disable, and the device list must stop showing paired-but-never-
        connected phantoms: HasConnectedBefore now requires a real service-GUID
        subkey rather than merely any subkey."""
        btg = read_c_inlined("bluetooth-guard/src/bluetooth_guard.c")

        # the real unpair API is loaded and wrapped
        self.assertIn('GetProcAddress(m, "BluetoothRemoveDevice")', btg)
        self.assertIn("static void DoForgetDevice(", btg)
        self.assertIn("pBluetoothRemoveDevice(&addr)", btg)
        # ... and it is NOT a profile disable (suite policy: never disable profiles)
        forget = btg[btg.index("static void DoForgetDevice("):]
        forget = forget[:forget.index("\n}\n")]
        self.assertNotIn("BluetoothSetServiceState", forget)
        self.assertNotIn("BLUETOOTH_SERVICE_DISABLE", forget)

        # the Forget button is drawn, hit-tested, and routed to a true unpair
        self.assertIn("HIT_DEV_FORGET_BTN", btg)
        self.assertRegex(btg, r'DrawActionBtn\([^;]*forgetR,\s*L"Forget"')
        self.assertIn("AddHit(forgetR, HIT_DEV_FORGET_BTN, i)", btg)
        self.assertIn("EnqueueWork(3, addr, dmac, dkind, dname)", btg)
        self.assertIn("else if (item.op == 3) DoForgetDevice(", btg)

        # phantom filter: require a service-GUID subkey, not just any subkey
        self.assertIn("static BOOL IsServiceGuidSubkey(", btg)
        self.assertIn("RegEnumKeyExW", btg)
        self.assertNotIn("return subKeys > 0;", btg,
                         "HasConnectedBefore still counts ANY subkey, so paired-but-"
                         "never-connected phantoms leak into the device list")

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
            "Keyboard Lighting",
            "Bluetooth Guard",
            "Spatial HUD",
            "OrcaSlicer Plugins",
        ):
            self.assertIn(expected, hub)
            self.assertIn(expected, hub_c)
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
                self.assertLess(known.index(app_id), 7,
                                f"{where} maps outside the 7 built-in slots")
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
        # Every built-in card the hub renders should be described by the
        # catalog, so its text survives a catalog refresh.
        self.assertEqual(sorted(known), sorted(ids),
                         f"suite-apps.json and the hub's built-in slots disagree.\n"
                         f"  hub knows:      {sorted(known)}\n"
                         f"  catalog lists:  {sorted(ids)}\n"
                         f"A built-in card missing from the catalog is never "
                         f"repopulated after the string pool is reset.")

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

    def test_orca_remove_handler_deletes_the_installed_plugin_files(self):
        """Orca's Remove is new. It must delete exactly what
        WorkerInstallOrcaPlugins writes, across every %APPDATA%\\OrcaSlicer*
        profile, or the button would look like it worked and leave the plugins
        installed."""
        hub_c = read_c_inlined("project-hub/src/project_hub.c")
        m = re.search(r'case ID_ORCA_REMOVE:\s*\{(.*?)\n    \}', hub_c, re.S)
        self.assertIsNotNone(m, "ID_ORCA_REMOVE handler is missing or changed shape")
        body = m.group(1)
        self.assertIn("OrcaSlicer*", body)                 # enumerate every profile
        self.assertIn("wave-overhangs", body)
        self.assertIn("unlayered-infill", body)
        self.assertIn("wave_overhangs_orca.py", body)
        self.assertIn("unlayered_infill_orca.py", body)
        self.assertIn("DeleteFileW", body)
        self.assertIn("RefreshProjectStatuses", body)      # card refreshes after removal

    def test_orca_disable_is_persisted_and_gates_the_installer(self):
        """g_orcaDisabled used to be toggled but never loaded, saved, or checked,
        so Orca's Disable was a no-op. Now that it is a visible button it has to
        survive a restart and actually stop the worker installing."""
        hub_c = read_c_inlined("project-hub/src/project_hub.c")
        self.assertRegex(hub_c, r'GetPrivateProfileStringW\(L"Apps",\s*L"OrcaDisabled"')
        self.assertRegex(hub_c, r'WritePrivateProfileStringW\(L"Apps",\s*L"OrcaDisabled"')
        m = re.search(r'static void WorkerInstallOrcaPlugins\(void\)\s*\{(.*?)\n    wchar_t branch',
                      hub_c, re.S)
        self.assertIsNotNone(m, "WorkerInstallOrcaPlugins prologue changed - update this test")
        self.assertIn("g_orcaDisabled", m.group(1))
        self.assertIn("return", m.group(1))

    def test_update_button_is_hidden_unless_an_update_is_needed(self):
        """btns[0] on every card is the dynamic Update/Install action: Install
        (PRIMARY) when the app is missing, Update (AMBER) when a newer build is
        known, hidden once up to date. Open moves to btns[1] and is hidden while
        the app is missing. HUD/Orca mirror the same rule."""
        hub_c = read_c_inlined("project-hub/src/project_hub.c")
        # every built-in card labels btns[0] "Install" in its not-installed branch
        self.assertEqual(hub_c.count('.btns[0].label = L"Install"'), 7,
                         "all 7 built-in cards should show Install when the app is missing")
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
        # Orca: disabled hides, not-installed reveals, else follows the update flag
        self.assertRegex(hub_c, r'BADGE_IDLE;\s*g_cards\[3\]\.btns\[0\]\.style = BTN_SECONDARY;'
                                r'\s*g_cards\[3\]\.btns\[0\]\.hidden = TRUE;')
        self.assertEqual(hub_c.count("g_cards[3].btns[0].hidden = FALSE"), 1)
        self.assertIn("g_cards[3].btns[0].hidden = !upd;", hub_c)
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
        for card in range(7):
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
        # The restore must cover all 7 built-in slots, not the first 6.
        self.assertRegex(body, r"for \(int i = 0; i < 7; i\+\+\)",
                         "RestoreBuiltInCardText must cover all 7 built-in slots; "
                         "the old title-only loop stopped at 6 and left Audio "
                         "Switcher dangling")

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
        its own numbers - 1% every 30ms, a hundred treads over the same three
        seconds - while a single press still takes the coarse 5% so a tap
        reads instantly. Those are two different controls and the test keeps
        them apart.

        Two invariants are easy to break by accident and impossible to notice
        in code review. If rampMs drops below the main loop's sleep, the extra
        resolution is silently thrown away because the ramp can only fire when
        the loop wakes. And if the tap branch keeps stepping per continuation
        token, every held key piles 5% on top of every 1% the ramp already
        made, which reads as a jump at the start of each hold.
        """
        engine = (ROOT / "keyboard-lighting" / "Aura-Background.ps1").read_text(
            encoding="ascii", errors="strict")

        def num(pat, what):
            m = re.search(pat, engine, re.M)
            self.assertIsNotNone(m, "could not find %s" % what)
            return int(m.group(1))

        step = num(r"^\$step = (\d+)$", "the tap step")
        rstep = num(r"^\$rampStep = (\d+)$", "the glide step")
        rms = num(r"^\$rampMs = (\d+)$", "the glide cadence")
        loop = num(r"^\$loopMs = (\d+)$", "the main loop sleep")
        hold = num(r"^\$brightHoldMs = (\d+)$", "the post-release hold window")

        self.assertLess(rstep, step,
                        "the glide step is not finer than the tap step, so a held key "
                        "is still a staircase")
        self.assertGreaterEqual(rms, loop,
                                "the glide cadence (%d ms) is finer than the main loop "
                                "wakes (%d ms), so the extra steps are silently dropped "
                                "and the sweep runs slower than configured" % (rms, loop))

        # Every engine-driven glide uses the fine ramp; nothing glides on the
        # tap step any more.
        self.assertEqual(engine.count("$script:brightRampDir * $rampStep"), 3,
                         "expected exactly three glides (burst, latch, hold-tap "
                         "continuation) on the fine ramp")
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
        overshoot_pct = (hold // max(rms, loop)) * rstep / 10.0
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

    def test_keyboard_lighting_hold_modes_are_reachable_and_bounded(self):
        """All three M4/M5 hold behaviours must be reachable, and the sweep cap
        must be longer than a sweep.

        The engine has supported tap/latch/burst since hold_mode.txt existed and
        re-reads it every tick, but the only way to set it was the -HoldMode
        command line. That left latch and burst - the two that do NOT depend on
        this panel recognising the M-keys - effectively unreachable, while the
        default "tap" degrades silently: its hold-to-sweep needs a typematic
        repeat or a release frame to exist, and on a machine where the M-keys
        produce neither, holding does nothing and every press is one 5% step
        with no explanation anywhere. Hence the tray submenu, and hence the
        read-only indicator that reports HasReleaseSrc rather than mere
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
        # Two timeouts share that shape. The 300 ms one is conditional on there
        # being no release source; the ceiling is the one that applies anyway.
        timeouts = re.findall(
            r"((?:!HasReleaseSrc\(i\) && )?)now - g_sigAt\[i\] > (\d+)ULL", src)
        ceilings = [int(v) for guard, v in timeouts if not guard]
        self.assertEqual(len(ceilings), 1,
                         "expected exactly one hold-stream ceiling that applies even "
                         "when a release source is known, found %d" % len(ceilings))
        ceiling = ceilings[0]
        self.assertGreater(ceiling, sweep_ms,
                           "the hold-stream ceiling of %d ms is shorter than the %d ms a "
                           "real 0-100%% sweep takes, so it cuts legitimate holds short"
                           % (ceiling, sweep_ms))
        cap = src[src.index("now - g_sigAt[i] > %dULL" % ceiling):]
        cap = cap[:cap.index("\n        }\n")]
        self.assertIn("g_scanArmed[i] = 1;", cap,
                      "the ceiling stops the stream without re-arming the key, so the "
                      "next press is read as a typematic repeat instead of a fresh one")

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

        for call in ("InstallRepoFileEverywhere(MIRROR_GENERIC",
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

    def test_update_replaces_a_stray_running_copy_instead_of_leaving_it_alive(self):
        src = self._hub_src()

        # Audio Switcher has no --stop switch and its single-instance mutex makes
        # a freshly launched suite copy exit at once, so the old tray instance
        # would survive its own update.
        self.assertIn('StopForeignInstance(L"TestsAudioSwitcherWnd", c->appDir);', src,
                      "a tray app running from a stray copy survives its update")
        start = src.index("static BOOL StopForeignInstance(")
        body = src[start:start + 1800]
        self.assertIn("QueryFullProcessImageNameW(hp, 0, img, &n)", body,
                      "the hub no longer checks which folder the running instance "
                      "was launched from")
        self.assertIn("_wcsicmp(dir, primaryDir) != 0", body,
                      "the hub would close the instance it is about to relaunch")
        self.assertIn("TerminateProcess(hp, 0)", body,
                      "a foreign instance that ignores WM_CLOSE keeps its exe locked, "
                      "so the stray copy can never be overwritten")

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
        ui = (ROOT / "agent-notes" / "src" / "parts" / "ui.c").read_text(encoding="utf-8")
        en = (ROOT / "agent-notes" / "src" / "parts" / "entry.c").read_text(encoding="utf-8")

        self.assertIn(".agent/AGENT_LINK.txt", ha,
                      "the app no longer polls the committed session link, so the "
                      "owner is back to pasting addresses by hand")
        self.assertIn(r"ProjectHub\\config.ini", ha,
                      "discovery cannot find the active branch without the hub's config")
        self.assertIn("if (!HealthOk(cand)) return 0;", ha,
                      "a discovered link is adopted without proving it is live, so a "
                      "dead session would replace a working link")
        self.assertIn("LinkDiscoveryTick()", ui,
                      "the flush timer never runs discovery, so a new session is only "
                      "found by restarting the app")
        self.assertIn("LinkDiscoveryTick()", en,
                      "startup never runs discovery, so an app opened after a new "
                      "session began waits a full minute for its first tick")
        self.assertIn("Agent offline", ha,
                      "the offline status still blames the link instead of saying "
                      "the note is queued and safe")

        fo = ha[ha.index("static int FlushOutbox("):]
        fo = fo[:fo.index("\n}\n")]
        self.assertEqual(fo.count("DeleteFileW(path);"), 1,
                         "the outbox delete moved - queued notes must survive failures")
        self.assertLess(fo.index("HttpPostNote(g_link"), fo.index("DeleteFileW(path);"),
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
        self.assertIn("if (!g_probeRef[0])", wr,
                      "with no marker the hub has no fallback ref and would probe "
                      "an empty path")
        self.assertIn("if (!hubFromMarker)", wr,
                      "the branch-tip version probe still runs when the marker "
                      "answered, which is exactly the mid-task nudge")
        self.assertLess(wr.index(".agent/RELEASED"), wr.index("git/trees"),
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
        self.assertEqual(wr.count("REPO_NAME_W, g_probeRef"), 10,
                         "some probes still bypass the released ref")

        h = fold[fold.index("static void EnsureProbeRef("):]
        h = h[:h.index("\n}\n")]
        self.assertLess(h.index("g_probeRef[0]"), h.index("EnsureActiveBranch"),
                        "EnsureProbeRef prefers the branch over the released ref")
        self.assertEqual(wi.count("EnsureActiveBranch("), 0,
                         "an install worker still downloads from the branch tip")
        self.assertGreaterEqual(wi.count("EnsureProbeRef(branch, 64);"), 6,
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
        hubver = _re.search(r'#define HUB_VERSION\s+"([0-9.]+)"', main).group(1)
        self.assertIn("hub=%s" % hubver, mk,
                      "the marker's hub version does not match the hub that ships "
                      "with it, so the hub would announce an update to itself")

    def test_hub_settings_page_writes_the_apps_own_files(self):
        """The hub's Settings page must edit, not duplicate, each app's config.

        The owner chose one settings page in the hub over per-app dialogs.
        The risk with a central page is a second schema: the hub keeping its
        own copy of what each setting means, drifting from the app that owns
        it. So every control writes the exact file the app itself uses -
        hold_mode.txt and theme.json for Keyboard Lighting, config.ini for
        DNS Pilot - and the three mode strings are asserted to be the same
        set the lighting engine validates against, so the page can never
        offer a mode that would silently do nothing.

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
        self.assertIn("for (int i = 0; i < 5; i++) {\n        if (PtInRect(&g_topBtns[i].rc, pt))",
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
        smooth = page[page.index("static BOOL ApplySmooth("):]
        smooth = smooth[:smooth.index("\n}\n")]
        self.assertIn('\\\"Smooth\\\"', smooth,
                      "the Smooth toggle no longer edits theme.json's Smooth key")
        self.assertIn("WriteBinaryFileAtomic", smooth,
                      "theme.json is rewritten in place instead of atomically")
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
                             src.count("RepointDesktopShortcutIfPresent("),
                             "%s touches the Desktop outside the repoint helper" % name)

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
                     "WorkerInstallOrcaPlugins()",
                     "WorkerInstallSpatialHud()",
                     "WorkerInstallDnsPilot(FALSE)",
                     "WorkerInstallNotes(FALSE)"):
            self.assertIn(call, block,
                          f"Update All no longer calls {call}, so that app's suite "
                          f"folder keeps the previous build on a one-click update")
        self.assertRegex(block, r"for \(int i = 6; i < g_cardCount; i\+\+\) "
                                r"WorkerInstallGeneric\(i, FALSE\);",
                         "Update All no longer sweeps the generic slots (Audio "
                         "Switcher, catalog cards)")
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


class AppOpenFocusTests(unittest.TestCase):
    """Every hub card's Open button launches the app's exe with no arguments -
    an explicit, non-silent launch. Clicking Open must bring that app's window
    to the foreground, both when it starts fresh and when it is already running
    hidden in the tray. These pin the show/focus path in each app so a future
    edit cannot silently drop the window behind the hub or leave it hidden."""

    @staticmethod
    def _src(rel):
        return read_c_inlined(rel)

    # (path, explicit-first-launch pattern, already-running-resurface pattern)
    APPS = [
        ("bluetooth-guard/src/bluetooth_guard.c",
         r"if \(!silent\) \{\s*ShowWindow\(g_hwnd, SW_SHOWNORMAL\);\s*"
         r"UpdateWindow\(g_hwnd\);\s*SetForegroundWindow\(g_hwnd\);",
         r"WaitForSingleObject\(g_showEvent, 0\) == WAIT_OBJECT_0\)\s*\{\s*"
         r"ShowWindow\(hwnd, SW_SHOWNORMAL\);\s*SetForegroundWindow\(hwnd\);"),
        ("keyboard-lighting/src/keyboard_lighting.c",
         r"if \(!silent\) \{\s*ShowWindow\(g_hwnd, SW_SHOWNORMAL\);\s*"
         r"UpdateWindow\(g_hwnd\);\s*SetForegroundWindow\(g_hwnd\);",
         r"WaitForSingleObject\(g_showEvent, 0\) == WAIT_OBJECT_0\)\s*\{\s*"
         r"ShowWindow\(hwnd, SW_SHOWNORMAL\);\s*SetForegroundWindow\(hwnd\);"),
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

if __name__ == "__main__":
    unittest.main()
