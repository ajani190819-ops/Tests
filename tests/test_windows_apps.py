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

    def test_bluetooth_guard_contracts(self):
        core = (ROOT / "bluetooth-guard" / "BluetoothCore.cs").read_text(encoding="utf-8")
        ps = (ROOT / "bluetooth-guard" / "BluetoothGuard.ps1").read_text(encoding="utf-8")
        setup = (ROOT / "bluetooth-guard" / "Setup.ps1").read_text(encoding="utf-8")
        native_c = (ROOT / "bluetooth-guard" / "src" / "bluetooth_guard.c").read_text(encoding="utf-8")
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

    def test_project_hub_command_ids_do_not_collide(self):
        """Guard the 'swallowed button' bug class: the branch-download menu
        reserves a numeric command-ID range, and a button ID that lands
        inside that range gets eaten as a bogus branch selection."""
        src = (ROOT / "project-hub" / "src" / "project_hub.c").read_text(encoding="utf-8")
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
        hub_c = (ROOT / "project-hub" / "src" / "project_hub.c").read_text(encoding="utf-8")
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
        btg_c = (ROOT / "bluetooth-guard" / "src" / "bluetooth_guard.c").read_text(encoding="utf-8")
        self.assertIn("SmoothRoundRect", btg_c)
        self.assertIn("DrawPillToggle", btg_c)
        self.assertIn("RES_ICON_EMBLEM", btg_c)
        ui_cs = (ROOT / "keyboard-lighting" / "ui_controls.cs.txt").read_text(encoding="ascii")
        self.assertIn("DrawSuiteEmblem", ui_cs)
        self.assertIn("Radius = 14", ui_cs)
        kbl_c = (ROOT / "keyboard-lighting" / "src" / "keyboard_lighting.c").read_text(encoding="ascii")
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
            "clipboard-memory/clipboard-memory.ico",
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

        hub_c = (ROOT / "project-hub" / "src" / "project_hub.c").read_text(encoding="utf-8")

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
        # Every built-in card the hub renders should be described by the
        # catalog, so its text survives a catalog refresh.
        self.assertEqual(sorted(known), sorted(ids),
                         f"suite-apps.json and the hub's built-in slots disagree.\n"
                         f"  hub knows:      {sorted(known)}\n"
                         f"  catalog lists:  {sorted(ids)}\n"
                         f"A built-in card missing from the catalog is never "
                         f"repopulated after the string pool is reset.")

    def test_hub_restores_every_pool_backed_card_field_before_a_rebuild(self):
        """RebuildExtraCards resets g_strPoolUsed, which invalidates EVERY
        pool-backed pointer on the built-in cards. Titles alone used to be
        restored (and only 6 of 8), so a card absent from the catalog came back
        pointing at recycled pool memory and drew another app's text."""
        hub_c = (ROOT / "project-hub" / "src" / "project_hub.c").read_text(encoding="utf-8")

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
        # The restore must cover all 8 built-in slots, not the first 6.
        self.assertRegex(body, r"for \(int i = 0; i < 8; i\+\+\)",
                         "RestoreBuiltInCardText must cover all 8 built-in slots; "
                         "the old title-only loop stopped at 6 and left Audio "
                         "Switcher and Clipboard Memory dangling")

    def test_hub_version_constants_are_paired_and_well_formed(self):
        """AGENTS.md hard rule 3.4: HUB_VERSION and HUB_VERSION_W are two
        separate lines that must be bumped TOGETHER whenever a new
        ProjectHub.exe ships. HUB_VERSION drives the numeric VerNewer update
        check and is what a self-update reads out of the remote source;
        HUB_VERSION_W is what the header draws. A pair that disagrees makes the
        hub advertise one version while checking for updates against another.
        Nothing enforced this before."""
        hub_c = (ROOT / "project-hub" / "src" / "project_hub.c").read_text(encoding="utf-8")
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
        return (ROOT / "audio-switcher" / "src" / "audio_switcher.c").read_text(
            encoding="utf-8")

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
        src = (ROOT / "keyboard-lighting" / "src" / "keyboard_lighting.c").read_text(encoding="utf-8")

        # The logon task must run at the highest privilege level, or the
        # scheduler starts a requireAdministrator exe with a limited token and
        # no UAC prompt: the app lands in preview mode at every boot.
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

    def test_hub_auto_probe_prefers_main_over_a_compiled_session_branch(self):
        src = (ROOT / "project-hub" / "src" / "project_hub.c").read_text(encoding="utf-8")

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
        src = (ROOT / "project-hub" / "src" / "project_hub.c").read_text(encoding="utf-8")

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
        src = (ROOT / "project-hub" / "src" / "project_hub.c").read_text(encoding="utf-8")

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
        return (ROOT / "project-hub" / "src" / "project_hub.c").read_text(
            encoding="utf-8", errors="replace"
        )

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
        end = src.index("/* 3a. Clipboard Memory")
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
        self.assertIn("AddMirrorDir(MIRROR_CLIP, cmSuite);", src,
                      "Clipboard Memory is not mirrored into the suite folder")
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
        self.assertNotIn("wcsncpy(g_cards[7].appDir, g_clipboardMemoryDir, MAX_PATH - 1);", src,
                         "the Clipboard Memory card points at the detected stray copy again")

        # The card must say which folder it manages.
        self.assertIn("static void AppendInstallFolder(", src,
                      "cards no longer show the folder they install into")
        self.assertIn("AppendInstallFolder(g_cards[6].detail, asDir);", src,
                      "the Audio Switcher card hides its install folder")
        self.assertIn("AppendInstallFolder(g_cards[7].detail, cmDir);", src,
                      "the Clipboard Memory card hides its install folder")

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
        self.assertGreaterEqual(src.count("AppendInstallFolder("), 5,
                                "only a couple of cards report where they install")

    def test_update_replaces_a_stray_running_copy_instead_of_leaving_it_alive(self):
        src = self._hub_src()

        # Audio Switcher has no --stop switch and its single-instance mutex makes
        # a freshly launched suite copy exit at once, so the old tray instance
        # would survive its own update.
        self.assertIn('StopForeignInstance(L"TestsAudioSwitcherWnd", c->appDir);', src,
                      "a tray app running from a stray copy survives its update")
        self.assertIn('StopForeignInstance(L"TestsClipboardMemoryWnd", c->appDir);', src)
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
        self.assertIn("CreateShortcutLink(lnk, g_suiteHubExe, g_suiteDir", body,
                      "after a self-update the Desktop shortcut still points at the "
                      "unmanaged copy")
        self.assertIn("free(newBytes);", body, "the downloaded hub bytes leak")

        # One click to consolidate without waiting for the next self-update.
        self.assertIn("#define ID_MENU_HUB_TOSUITE", src)
        self.assertIn('L"Install Hub into Suite Folder"', src,
                      "the utility menu lost the one-click hub consolidation entry")
        self.assertIn("case ID_MENU_HUB_TOSUITE:", src,
                      "the suite-folder hub menu entry is not handled")

    def test_hub_version_advanced_for_the_suite_folder_install_fix(self):
        src = self._hub_src()
        m = re.search(r'#define HUB_VERSION\s+"(\d+)\.(\d+)\.(\d+)"', src)
        self.assertIsNotNone(m, "HUB_VERSION is missing or malformed")
        version = tuple(int(g) for g in m.groups())
        self.assertGreaterEqual(version, (2, 8, 38),
                                "the hub version was not bumped, so an installed hub "
                                "never offers this update")


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
        return (ROOT / "audio-switcher" / "src" / "audio_switcher.c").read_text(
            encoding="utf-8", errors="replace"
        )

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
        return (ROOT / "audio-switcher" / "src" / "audio_switcher.c").read_text(
            encoding="utf-8", errors="replace"
        )

    @staticmethod
    def _fn(src: str, signature: str) -> str:
        start = src.index(signature)
        return src[start:src.index("\n}\n", start)]

    def test_the_other_active_outputs_are_swept_when_the_default_shows_nothing(self):
        src = self._src()
        refresh = self._fn(src, "static HRESULT sessions_refresh(void) {")

        self.assertIn("static int sessions_collect_from_device(IMMDevice *dev", src,
                      "session collection is not per-device, so only the default "
                      "output can ever be listed")
        self.assertIn("if (g_session_count == 0) {", refresh,
                      "the fallback sweep is gone, or now runs unconditionally and "
                      "duplicates every app that is already listed")
        self.assertRegex(
            refresh,
            r"for \(int i = 0; i < g_endpoint_count && g_session_count < 128; i\+\+\)",
            "the sweep no longer walks the enumerated render endpoints")
        self.assertIn("if (i == g_default_endpoint_idx) continue;", refresh,
                      "the sweep re-lists the default device, so every app on it "
                      "would appear twice")
        self.assertIn("sessions_collect_from_device(g_endpoints[i].device, TRUE", refresh,
                      "swept rows are not labelled with the device they play on, so "
                      "an inaudible app looks like a broken slider")
        self.assertIn("g_session_sweep_used = TRUE;", refresh,
                      "nothing records that the sweep produced the list")

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
        self.assertIn("if (g_session_mgr_count < 8) g_session_mgrs[g_session_mgr_count++] = mgr;",
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


if __name__ == "__main__":
    unittest.main()
