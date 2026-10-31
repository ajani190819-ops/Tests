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


if __name__ == "__main__":
    unittest.main()
