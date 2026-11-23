[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe)
$ErrorActionPreference='Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') { throw 'Theme capture requires a disposable GitHub-hosted runner.' }
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repoRoot 'out/tests/rc3-theme-capture'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$uia=Start-Job -ScriptBlock {
    try {
        Add-Type -AssemblyName UIAutomationClient
        Add-Type -AssemblyName UIAutomationTypes
        Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class Rc3ThemeDpi {
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
}
'@
        'UIA_READY'
        $deadline=[DateTime]::UtcNow.AddMinutes(3)
        while([DateTime]::UtcNow -lt $deadline) {
            foreach($process in @(Get-Process -Name FBE -ErrorAction SilentlyContinue)) {
                try {
                    $handle=$process.MainWindowHandle
                    if($handle -eq [IntPtr]::Zero){continue}
                    $root=[System.Windows.Automation.AutomationElement]::FromHandle($handle)
                    if(-not $root){continue}
                    $condition=[System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty,[System.Windows.Automation.ControlType]::ComboBox)
                    $items=$root.FindAll([System.Windows.Automation.TreeScope]::Descendants,$condition)
                    "utc=$([DateTime]::UtcNow.ToString('o')) pid=$($process.Id) dpi=$([Rc3ThemeDpi]::GetDpiForWindow($handle)) root=$($root.Current.BoundingRectangle) combos=$($items.Count)"
                    foreach($item in $items) {
                        try {
                            $current=$item.Current
                            "  name=$($current.Name) class=$($current.ClassName) id=$($current.AutomationId) enabled=$($current.IsEnabled) offscreen=$($current.IsOffscreen) bounds=$($current.BoundingRectangle)"
                        } catch { "  UIA_COMBO_ERROR=$($_.Exception.Message)" }
                    }
                } catch { "UIA_WINDOW_ERROR=$($_.Exception.Message)" }
            }
            Start-Sleep -Seconds 2
        }
    } catch { "UIA_UNAVAILABLE=$($_.Exception.Message)" }
}
$previous=$env:FBE_NEXT_TEST_APPEARANCE_ARTIFACT_DIR
try {
    $env:FBE_NEXT_TEST_APPEARANCE_ARTIFACT_DIR=$evidence
    & (Join-Path $repoRoot 'tools/tests/test-fbe-theme-application-runtime.ps1') -FbeExe $FbeExe
} finally {
    $env:FBE_NEXT_TEST_APPEARANCE_ARTIFACT_DIR=$previous
    if($uia.State -in @('Running','NotStarted')) { Stop-Job -Job $uia }
    Receive-Job -Job $uia -ErrorAction Continue *>&1 | Out-File -LiteralPath (Join-Path $evidence 'uia.txt') -Encoding utf8
    Remove-Job -Job $uia -Force -ErrorAction SilentlyContinue
}
