# AGENTS.md — read this before doing anything in this folder

This is the rulebook for any AI assistant (or human contributor) working on
the **Keyboard Lighting** app. It is written in the same spirit as the
repository root `AGENTS.md`, which still governs repo-wide things (git
discipline, the plugin projects, `MEMORY.md`). For work inside
`keyboard-lighting/`, **this file outranks both your own defaults and the
root file wherever they disagree**.

`README.md` in this folder is the app's user manual and the source of truth
for user-visible behaviour. Read it before changing anything the owner can
see, and update it in the same change when that behaviour changes.

---

## 1. Who you are working for

The owner is **a beginner** at C#, the Windows HID layer, and laptop
firmware. That is a design constraint, not an apology:

* **Explain plainly.** Short sentences. Define every piece of jargon the
  first time it appears (there is a glossary at the bottom of this file).
* **Never hide a failure.** "I could not test this because there is no
  Windows and no keyboard in this sandbox" is worth more than a green
  checkmark. Say it every time it is true.
* **The app is Windows-only.** This sandbox is Linux. You can read the code
  and run two Python checkers, and that is all. **No change here is ever
  tested on real hardware until the owner runs it** — label it so.

## 2. How to work — the standards

1. **Plan before you build.** Post what you will do, in what order, what you
   will NOT do, and where the risks are. The owner should never watch a
   silent build.
2. **Ask instead of guessing.** Batch clarifying questions with concrete
   options and a recommended default. One round of good questions is
   efficient, not annoying.
3. **Small, reviewable steps.** One logical change at a time.
4. **Verify what can be verified, then say exactly what that was.** From
   this sandbox: `python3 tools/paramcheck.py` (pure Python, always runs)
   and `python3 tools/cslint.py` (needs `pip install tree-sitter
   tree-sitter-c-sharp`). Separate "verified" from "not tested" in your
   report. That distinction is the whole rule.
5. **Report in three parts:** what I did / what it means for you / what is
   next.
6. **Leave the docs as good as you found them.** README (the manual), this
   file, and the comments in `Aura-Background.ps1` carry hard-won history.
   Do not delete history comments because they look stale — most of them
   exist so a mistake is not repeated.

## 3. What this app is

Custom animated keyboard lighting for a **ROG Strix G16 (G615JPR)** without
Armoury Crate. One visible application (window + tray icon) and one separate
engine process that drives the keyboard's HID LampArray interface directly.
It coexists with G-Helper, which is told to leave the keyboard colours
alone. **Everything runs on the owner's machine; this folder is the source.**

## 4. The files

| File | What it is |
|---|---|
| `Setup.bat` | **THE ONLY FILE THE OWNER EVER RUNS.** Downloads the latest `Setup.ps1` from the canonical repo (URL inside) and runs it elevated. Must stay CRLF. |
| `Setup.ps1` | Install, update, repair and self-test in one. Banner should say **v16**. Downloads every file, builds `KeyboardLighting.exe` with the C# compiler shipped with Windows, creates Start-menu/Desktop shortcuts, registers the logon task `KeyboardLighting` in Task Scheduler, sets G-Helper's `skip_aura`, then tests everything. On failure it copies a diagnostic report to the clipboard. `-TestOnly` runs just the checks. |
| `KeyboardLighting.exe` | Launcher stub, compiled on the owner's machine. Never in git. |
| `Tray.ps1` | The application: window, tray icon, settings, updater. Starts the engine with `CreateNoWindow` so no console ever appears. |
| `ui_controls.cs.txt` | The WinForms interface, compiled at startup. |
| `Aura-Background.ps1` | The lighting engine: PowerShell 5.1 setup that discovers the keyboard's HID reports, then a compiled C# render loop (Add-Type) on its own thread. v3 architecture. |
| `MyEffect.ps1` | The owner's custom effect. The template for writing new ones. |
| `Zones.bat` / `Zones.ps1` | Zone-map checker and corrector (see "If the owner reports a bug"). |
| `tools/paramcheck.py` | Cross-script parameter check. Catches calls another script's `param()` would reject. Runs anywhere. |
| `tools/cslint.py` | Tree-sitter lint of the embedded C#. Catches what regex cannot. Needs pip packages. |
| `app.ico`, `README.md` | Icon; the manual. |

**Canonical source is a different repo.** `Setup.bat` and `Setup.ps1`
download from `ajani190819-ops/HSEQB`, branch `arena/01a0a5d4-hseqb`, path
`keyboard-lighting`. This copy in the Tests repo is **storage**. A change
made only here never reaches the owner's machine — shipping means putting it
in the HSEQB repo too. **Never push there without the owner asking for it.**

## 5. Hard-won facts — do not re-derive these

* The keyboard is a **HID LampArray** with **16 lamps**: 4 keyboard zones
  plus 12 light-bar lamps physically grouped in clusters of three (not
  evenly spaced). The engine treats them as **two independent render
  groups** with separate pattern, palette, speed, brightness and phase.
* **Report layouts are not hardcoded.** At startup the engine enumerates the
  device's HID reports and locates the multi-update report by usage
  (`0x61`/`0x62` lamp-id range). If it cannot resolve the layout it prints a
  diagnostic block and asks the owner to paste it back — never guess offsets.
* **HID usage `0x71` is autonomous mode**: 0 = the host is in charge of the
  lamps, 1 = the keyboard's own firmware drives them. The app sends 0 at
  startup and on every heartbeat; `-Restore` sends 1 and exits (the "let
  the keyboard take over" option).
* **The laptop's EC silently takes the keyboard back** — lid close, Modern
  Standby, display off — **without the USB device ever disappearing.** No
  write fails, so nothing else notices. Two mechanisms exist because of
  this, and **neither may be removed**:
  * the **heartbeat**: re-send the 51-byte "host is in charge" control
    report every 3 seconds;
  * the **wake detector**: a frame gap > 1.5 s means suspended/frozen and
    forces a full repaint (works where `PowerModeChanged` does not).
* **Heartbeat history (keep it):** it was once removed while chasing a
  flicker. The flicker turned out to be brightness quantisation, and the
  lights then never came back after opening the lid. The heartbeat also
  used to invalidate the previous-colour cache — that repaint was a real,
  visible cost. It no longer does; only a detected wake does (`force`).
* **Write-fail handling:** 8 consecutive failed frames mark the device lost,
  then a reopen loop (400 ms waits). Not fewer — single hiccups must not
  count.
* **Frame pacing:** sleep-then-spin to land frames on time. A missed
  deadline is counted, **not** acted on. Back off only when the writes
  themselves exceed 3/4 of the frame budget for 120 straight frames (a real
  capacity limit), in 20% steps, floor 15 fps. An earlier version backed
  off on ordinary jitter and permanently degraded a device that had slack —
  do not repeat it.
* **Only one program can drive the keyboard at a time.** G-Helper must keep
  `skip_aura: 1` (in `%AppData%\GHelper\config.json`); Windows **Dynamic
  Lighting must be off** (Settings > Personalization > Dynamic Lighting) or
  Windows fights the app; `Zones` Identify needs the lighting app fully
  closed.
* **The app has no idle detection at all.** It pushes colour frames
  continuously (up to 60 fps) and re-asserts ownership every 3 s. If lights
  go dark on their own after a short idle, the cause is outside this app —
  G-Helper's backlight timeout, the EC, or the display turning off — so
  diagnose with the checklist below before touching engine code.

## 6. Hard rules — breaking these breaks the owner's machine

1. **`Setup.bat` stays the only file the owner runs.** Keep it CRLF, keep
   its download-then-run flow intact, and never let it be able to damage
   anything: it repairs and reports, or refuses in plain English.
2. **PowerShell 5.1** (`#requires -Version 5.1`), Windows PowerShell, not
   PS7. All C# is embedded and compiled at runtime via Add-Type.
3. **Run the checkers after edits.** Embedded C# changed → `cslint` (a
   `volatile` on a `double`, CS0677, killed v16 once). Any script calling
   another → `paramcheck` (`-Effect zonetest` shipped broken because the
   `ValidateSet` did not list it).
4. **Never reorganize this folder wholesale.** One logical change at a
   time; no renames of the files `Setup.ps1` downloads (its `$Files` list
   and the .bat's URL must stay in agreement).
5. **New effects obey the contract:** a scriptblock with `param($t, $N)`
   (`$t` = seconds since start, `$N` = zone count 16) calling
   `Set-Zone <index> <r> <g> <b>` (0–255); helpers `Convert-Hsv` and
   `ConvertFrom-Hex`; runnable via `-Custom MyEffect.ps1`. Keep the
   `ValidateSet` in `Aura-Background.ps1` in sync with what actually works.
6. **`theme.json` and `panel.json` are different things.** `theme.json` is
   the live panel→engine state file (changes apply without restart);
   `panel.json` (in `%LOCALAPPDATA%\KeyboardLighting\`) is the persisted
   settings. Never merge their jobs.
7. **Zone-map corrections are saved separately from settings** and must
   survive updates untouched. Never overwrite the owner's correction.
8. **Logs stay capped at 256 KB** (`logs/panel.log`, `logs/engine.log`,
   next to `Setup.bat`). Repo-wide, `*.log` is gitignored — never commit
   runtime output.
9. **Nothing ships to HSEQB without the owner asking.**
10. **Say what you could not test.** Every engine change is untested on
    hardware until the owner runs it. No exceptions from this sandbox.

## 7. If the owner reports a bug

Get the evidence before theorizing:

1. **Right-click tray icon → Open logs folder.** `engine.log` and
   `panel.log`, both capped at 256 KB. The engine writes a timing line a
   few seconds after start (`device write <µs>/frame (worst <n>), budget
   <n> us at <fps> fps, late frames <n>`) — that line is the smoothness
   check.
2. **Run `Setup.bat`** — it re-downloads, rebuilds, self-tests, and on
   failure copies a diagnostic to the clipboard. The banner must say v16.
3. **Ask whether the preview strip in the window still shows colours.**
   If the strip shows the pattern but the keyboard is dark, something
   outside the app took the lamps (EC, G-Helper timeout, display off). If
   the strip dims and says OFF, the engine stopped — look in the logs.
4. **Check the single-driver rule**: G-Helper `skip_aura` still 1? Windows
   Dynamic Lighting off?
5. **Dark-after-idle specifically** is never this app (see §5, last fact).
   G-Helper's Extra settings hold two backlight timeouts (plugged / on
   battery — check BOTH, they are independent), and the EC takes the
   keyboard back when the display turns off, so ask whether the screen was
   still on.

## 8. Glossary

* **HID** — Human Interface Device; the USB protocol class the keyboard
  speaks.
* **LampArray** — the HID standard for RGB lighting: lamps addressed by
  index, updated via reports.
* **Report** — a fixed-format message to the device. *Feature reports*
  set modes; *output reports* carry data (the colours). The app discovers
  the layouts from the device rather than hardcoding them.
* **Usage** — a numbered field inside a report. `0x71` is autonomous mode.
* **EC** — embedded controller; the laptop's always-on firmware that owns
  the keyboard hardware and can silently take it back.
* **Heartbeat** — the 3-secondly re-send of "host is in charge."
* **DeviceLost** — the engine's state after 8 consecutive failed frames;
  triggers the reopen loop.
* **Render group** — keyboard zones or light bar; the two independently
  patterned sets.
* **Zone map** — the physical order of lamps; the owner can correct it
  with `Zones.bat`, and the correction is kept forever.
* **G-Helper** — the lightweight open-source Armoury Crate replacement the
  owner uses for fans, power modes and Fn keys.
* **Dynamic Lighting** — Windows 11's own RGB control; it fights this app
  and must stay off.
