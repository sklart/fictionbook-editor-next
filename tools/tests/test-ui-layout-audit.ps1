[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Add-Type -AssemblyName System.Windows.Forms
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class UiAuditFixture
{
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateWindowEx(int exStyle, string className, string text, int style,
        int x, int y, int width, int height, IntPtr parent, IntPtr id, IntPtr instance, IntPtr parameter);
}
'@
$form = New-Object System.Windows.Forms.Form
$form.Text = 'UI Layout Audit fixture'
$form.Width = 360
$form.Height = 180
$directory = Join-Path $root 'out\tests\ui-layout-audit'
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$exceptionPath = Join-Path $directory 'exceptions.json'
[IO.File]::WriteAllText($exceptionPath, '{"ignoredOverlapPairs":["112:113"]}', [Text.UTF8Encoding]::new($false))
$reportPath = Join-Path $directory 'report.json'
try {
    $form.Show()
    $parent = $form.Handle
    $visible = 0x50000000 # WS_CHILD | WS_VISIBLE
    [void][UiAuditFixture]::CreateWindowEx(0, 'STATIC', 'A long label intentionally clipped', $visible,
        20, 20, 55, 18, $parent, [IntPtr]111, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][UiAuditFixture]::CreateWindowEx(0, 'BUTTON', 'One', $visible,
        20, 70, 90, 30, $parent, [IntPtr]112, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][UiAuditFixture]::CreateWindowEx(0, 'BUTTON', 'Two', $visible,
        60, 75, 90, 30, $parent, [IntPtr]113, [IntPtr]::Zero, [IntPtr]::Zero)
    [Windows.Forms.Application]::DoEvents()
    & (Join-Path $root 'tools\ui\invoke-ui-layout-audit.ps1') -ProcessId $PID -Language en-US `
        -OutputPath $reportPath -ExceptionsPath $exceptionPath
    $report = Get-Content -Raw -Encoding UTF8 -LiteralPath $reportPath | ConvertFrom-Json
    if ($report.language -ne 'en-US' -or @($report.windows).Count -lt 1) { throw 'Window or language missing from UI audit.' }
    $label = @($report.controls | Where-Object { $_.Id -eq 111 })
    if ($label.Count -ne 1 -or -not $label[0].TextMayClip -or $label[0].Dpi -lt 96 -or -not $label[0].Bounds.Right) {
        throw 'Native label clipping or geometry was not measured.'
    }
    $overlap = @($report.issues | Where-Object { $_.kind -eq 'possible-overlap' })
    if ($overlap.Count -ne 1 -or -not $overlap[0].ignored) { throw 'Intentional overlap exception was not recorded.' }
    Write-Host 'UI Layout Audit native fixture passed.'
}
finally {
    $form.Close()
    $form.Dispose()
}
