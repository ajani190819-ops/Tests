# Compile the unmodified embedded engine with Framework C# 5, then execute its
# actual output and scheduler methods with synthetic I/O/time. No devices or UI.
param([string]$Root = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$tokens = $null; $errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $Root 'keyboard-lighting/Aura-Background.ps1'), [ref]$tokens, [ref]$errors)
if ($errors) { throw 'Lighting source parse failed' }
$blocks = @($ast.FindAll({ param($n)
    $n -is [System.Management.Automation.Language.StringConstantExpressionAst] -and
    $n.Value.Contains('public class LampEngine {')
}, $true))
if ($blocks.Count -ne 1) { throw 'Missing/duplicate embedded LampEngine' }
$source = $blocks[0].Value.Replace("`r`n", "`n")
$csc = "$env:WinDir\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
$folder = Join-Path ([IO.Path]::GetTempPath()) ('lighting-colour-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($folder)
try {
    $original = Join-Path $folder 'original.cs'
    [IO.File]::WriteAllText($original, $source, [Text.Encoding]::ASCII)
    & $csc /nologo /target:library /langversion:5 /optimize+ /r:System.dll /r:System.Core.dll "/out:$folder\original.dll" $original
    if ($LASTEXITCODE -ne 0) { throw 'Unmodified Lighting engine C# 5 compile failed' }

    $native = "  [DllImport(`"hid.dll`", SetLastError=true)]`n  static extern bool HidD_SetFeature(IntPtr h, byte[] b, int len);"
    if ([regex]::Matches($source, [regex]::Escape($native)).Count -ne 1) { throw 'HID test seam drifted' }
    $shim = @'
  public static int ColourTestWrites, ColourTestFailAt;
  public static readonly object ColourTestIOLock = new object();
  public static System.Action<byte[]> ColourTestBeforeWrite;
  public static System.Collections.Generic.List<int> ColourTestEvents = new System.Collections.Generic.List<int>();
  public static System.Collections.Generic.List<int> ColourTestKinds = new System.Collections.Generic.List<int>();
  public static System.Collections.Generic.List<byte[]> ColourTestReports = new System.Collections.Generic.List<byte[]>();
  static bool HidD_SetFeature(IntPtr h, byte[] b, int len) {
    if (len != b.Length) throw new Exception("wrong HID report length");
    bool ok;
    lock (ColourTestIOLock) {
      ColourTestEvents.Add(++ColourTestWrites);
      ColourTestKinds.Add(b[0] == 0x5a ? 3 : (b[0] == 9 ? 4 : b[1] + 1));
      ColourTestReports.Add((byte[])b.Clone());
      ok = ColourTestWrites != ColourTestFailAt;
    }
    // Wait outside the recording lock, so a broken production caller really
    // can interleave here. The fixture must not accidentally serialize it.
    if (ColourTestBeforeWrite != null) ColourTestBeforeWrite(b);
    return ok;
  }
'@
    $mock = $source.Replace($native, $shim)
    $pace = [regex]'(?s)  void Pace\(int us\) \{.*?\n  \}'
    if ($pace.Matches($mock).Count -ne 1) { throw 'Pacing test seam drifted' }
    $mock = $pace.Replace($mock, '  void Pace(int us) { lock (ColourTestIOLock) { ColourTestEvents.Add(-us); } }')
    # Execute the full production Loop with virtual time and synthetic work.
    # Keep its decisions/conditions/arithmetic intact; never launch Start().
    $loopPattern = [regex]'(?s)  void Loop\(\) \{.*?\n  \}'
    if ($loopPattern.Matches($mock).Count -ne 1) { throw 'Scheduler test seam drifted' }
    $loop = $loopPattern.Match($mock).Value
    $replayLoop = $loop.Replace('Stopwatch', 'LightingReplayClock').Replace('Thread.Sleep', 'LightingReplayClock.Sleep').Replace('Thread.SpinWait', 'LightingReplayClock.SpinWait')
    $replayLoop = $replayLoop.Replace('timeBeginPeriod(1)', 'LightingReplayClock.Begin(1)').Replace('timeEndPeriod(1)', 'LightingReplayClock.End(1)')
    $replayLoop = $replayLoop.Replace('        Frame(', '        CadenceTestFrame(').Replace('        Push();', '        CadenceTestPush();').Replace('        PublishFrame();', '        CadenceTestPublish();')
    $mock = $mock.Replace($loop, $replayLoop).Replace('public class LampEngine {', 'public partial class LampEngine {')
    $mockPath = Join-Path $folder 'replay.cs'
    [IO.File]::WriteAllText($mockPath, $mock, [Text.Encoding]::ASCII)
    $harness = Join-Path $Root 'tests/native/lighting_colour_harness.cs.txt'
    $cadence = Join-Path $Root 'tests/native/lighting_cadence_harness.cs.txt'
    & $csc /nologo /target:library /langversion:5 /optimize+ /r:System.dll /r:System.Core.dll "/out:$folder\replay.dll" $mockPath $harness $cadence
    if ($LASTEXITCODE -ne 0) { throw 'Lighting C# colour replay compile failed' }
    [void][Reflection.Assembly]::LoadFrom((Join-Path $folder 'replay.dll'))
    [LightingColourReplay]::Run()
    [LightingCadenceReplay]::Run()
} finally {
    # Framework keeps a loaded DLL locked until this short-lived CI process ends.
    Get-ChildItem -LiteralPath $folder -Filter '*.cs' | Remove-Item -Force
    Remove-Item -LiteralPath (Join-Path $folder 'original.dll') -Force -ErrorAction SilentlyContinue
}
