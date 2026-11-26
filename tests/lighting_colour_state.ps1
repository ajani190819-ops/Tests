# Compile the unmodified embedded engine with Framework C# 5, then execute its
# actual output methods with only HID/pacing replaced. No devices or UI opened.
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
  public static System.Collections.Generic.List<int> ColourTestEvents = new System.Collections.Generic.List<int>();
  public static System.Collections.Generic.List<byte[]> ColourTestReports = new System.Collections.Generic.List<byte[]>();
  static bool HidD_SetFeature(IntPtr h, byte[] b, int len) {
    if (len != b.Length) throw new Exception("wrong HID report length");
    ColourTestEvents.Add(++ColourTestWrites);
    ColourTestReports.Add((byte[])b.Clone());
    return ColourTestWrites != ColourTestFailAt;
  }
'@
    $mock = $source.Replace($native, $shim)
    $pace = [regex]'(?s)  void Pace\(int us\) \{.*?\n  \}'
    if ($pace.Matches($mock).Count -ne 1) { throw 'Pacing test seam drifted' }
    $mock = $pace.Replace($mock, '  void Pace(int us) { ColourTestEvents.Add(-us); }')
    $mockPath = Join-Path $folder 'replay.cs'
    [IO.File]::WriteAllText($mockPath, $mock, [Text.Encoding]::ASCII)
    $harness = Join-Path $Root 'tests/native/lighting_colour_harness.cs.txt'
    & $csc /nologo /target:library /langversion:5 /optimize+ /r:System.dll /r:System.Core.dll "/out:$folder\replay.dll" $mockPath $harness
    if ($LASTEXITCODE -ne 0) { throw 'Lighting C# colour replay compile failed' }
    [void][Reflection.Assembly]::LoadFrom((Join-Path $folder 'replay.dll'))
    [LightingColourReplay]::Run()
} finally {
    # Framework keeps a loaded DLL locked until this short-lived CI process ends.
    Get-ChildItem -LiteralPath $folder -Filter '*.cs' | Remove-Item -Force
    Remove-Item -LiteralPath (Join-Path $folder 'original.dll') -Force -ErrorAction SilentlyContinue
}
