<# Opens About in four representative UI locales and checks its real text geometry. #>
[CmdletBinding()]
param([string]$FbeExe)
$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'about-layout'
$passed = $false
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class AboutLayoutWindow {
    public delegate bool Callback(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback, IntPtr data);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
    [DllImport("user32.dll")] static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr window, uint message, IntPtr wp, IntPtr lp);
    public static IntPtr Find(int processId, bool about) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, data) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner != (uint)processId) return true;
            if (about) {
                if (!IsWindowVisible(window) || GetDlgItem(window, 1070) == IntPtr.Zero) return true;
            } else {
                var name = new StringBuilder(128);
                GetClassName(window, name, name.Capacity);
                if (name.ToString() != "FictionBookEditorFrame") return true;
            }
            found = window;
            return false;
        }, IntPtr.Zero);
        return found;
    }
    public static void OpenAbout(IntPtr frame) {
        ShowWindow(frame, 5);
        PostMessage(frame, 0x0111, (IntPtr)0xE140, IntPtr.Zero); // ID_APP_ABOUT
    }
    public static void Close(IntPtr window) { PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero); }
}
'@
try {
    & pwsh -NoProfile -File (Join-Path $PSScriptRoot '..\localization\export-runtime-lang.ps1') -OutputDirectory (Join-Path $isolation.Runtime 'Lang') -Clean | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Runtime localization export failed.' }
    $fixture = (Resolve-Path (Join-Path $PSScriptRoot 'fb2-validate-valid-smoke.fb2')).Path
    $languageIds = @{ 'ru-RU'=0xFBE002; 'en-US'=0xFBE001; 'de-DE'=0xFBE004; 'uk-UA'=0xFBE003 }
    $settingsDirectory = Join-Path $isolation.DataPath 'Settings'
    New-Item -ItemType Directory -Path $settingsDirectory -Force | Out-Null
    $settingsTemplate = [IO.File]::ReadAllText((Join-Path $isolation.Runtime 'Data\Settings\Settings.xml'))
    $pattern = [regex]::new('<IntefaceLangID>[0-9]+</IntefaceLangID>')
    if (-not $pattern.IsMatch($settingsTemplate)) { throw 'The settings template lacks the interface language.' }
    foreach ($locale in 'ru-RU', 'en-US', 'de-DE', 'uk-UA') {
        $process = $null
        try {
            $settings = $pattern.Replace($settingsTemplate, '<IntefaceLangID>' + $languageIds[$locale] + '</IntefaceLangID>', 1)
            [IO.File]::WriteAllText((Join-Path $settingsDirectory 'Settings.xml'), $settings, [Text.UTF8Encoding]::new($false))
            $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', ('"' + $fixture + '"')) -PassThru
            $frame = [IntPtr]::Zero
            for ($attempt=0; $attempt -lt 60 -and $frame -eq [IntPtr]::Zero; ++$attempt) {
                Start-Sleep -Milliseconds 100
                if ($process.HasExited) { throw "$locale FBE exited before opening About." }
                $frame = [AboutLayoutWindow]::Find($process.Id, $false)
            }
            if ($frame -eq [IntPtr]::Zero) { throw "$locale main frame missing." }
            [AboutLayoutWindow]::OpenAbout($frame)
            $about = [IntPtr]::Zero
            for ($attempt=0; $attempt -lt 60 -and $about -eq [IntPtr]::Zero; ++$attempt) {
                Start-Sleep -Milliseconds 100
                $about = [AboutLayoutWindow]::Find($process.Id, $true)
            }
            if ($about -eq [IntPtr]::Zero) { throw "$locale About dialog missing." }
            $report = Join-Path $isolation.Root "about-layout-$locale.json"
            & (Join-Path $PSScriptRoot '..\ui\invoke-ui-layout-audit.ps1') -ProcessId $process.Id -Language $locale -OutputPath $report | Out-Null
            $audit = Get-Content -Raw -Encoding UTF8 -LiteralPath $report | ConvertFrom-Json
            $aboutHandle = '0x' + $about.ToInt64().ToString('X')
            $children = @($audit.controls | Where-Object Parent -eq $aboutHandle)
            $buildLabel = @($children | Where-Object Id -eq 1108)
            if ($buildLabel.Count -ne 1 -or $buildLabel[0].TextMayClip) { throw "$locale About build label is missing or clipped." }
            $checkedHandles = @($children | Where-Object Id -in 1108, 1023, 1058 | ForEach-Object Hwnd)
            $issues = @($audit.issues | Where-Object { -not $_.ignored -and $checkedHandles -contains $_.hwnd })
            if ($issues.Count) { throw "$locale About build/link row has layout findings: $($issues.kind -join ', ')." }
            Write-Host "$locale About build/link row fits."
            [AboutLayoutWindow]::Close($about)
            [AboutLayoutWindow]::Close($frame)
            [void]$process.WaitForExit(5000)
        }
        finally {
            if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
        }
    }
    $passed = $true
    Write-Host 'About dialog layout passed for ru/en/de/uk.'
}
finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
