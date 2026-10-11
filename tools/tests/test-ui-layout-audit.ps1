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
    [StructLayout(LayoutKind.Sequential)] public struct ScrollInfo {
        public uint Size, Mask;
        public int Min, Max;
        public uint Page;
        public int Position, TrackPosition;
    }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateWindowEx(int exStyle, string className, string text, int style,
        int x, int y, int width, int height, IntPtr parent, IntPtr id, IntPtr instance, IntPtr parameter);
    [DllImport("user32.dll")] public static extern int SetScrollInfo(IntPtr window, int bar, ref ScrollInfo info, bool redraw);
}
'@
$form = New-Object System.Windows.Forms.Form
$form.Text = 'UI Layout Audit fixture'
$form.Width = 360
$form.Height = 180
$prefix = Join-Path ([IO.Path]::GetTempPath()) ('fbe-ui-layout-audit-' + [guid]::NewGuid().ToString('N'))
$exceptionPath = $prefix + '-exceptions.json'
[IO.File]::WriteAllText($exceptionPath, '{"ignoredOverlapPairs":["112:113"]}', [Text.UTF8Encoding]::new($false))
$reportPath = $prefix + '-report.json'
$passed = $false
try {
    $form.Show()
    $parent = $form.Handle
    $visible = 0x50000000 # WS_CHILD | WS_VISIBLE
    [void][UiAuditFixture]::CreateWindowEx(0, 'STATIC', 'A long label intentionally clipped', $visible,
        20, 20, 55, 18, $parent, [IntPtr]111, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][UiAuditFixture]::CreateWindowEx(0, 'BUTTON', '&One', $visible,
        20, 70, 90, 30, $parent, [IntPtr]112, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][UiAuditFixture]::CreateWindowEx(0, 'BUTTON', 'One', $visible,
        60, 75, 90, 30, $parent, [IntPtr]113, [IntPtr]::Zero, [IntPtr]::Zero)
    $scrollHost = [UiAuditFixture]::CreateWindowEx(0, 'STATIC', '', ($visible -bor 0x00200000),
        180, 20, 100, 50, $parent, [IntPtr]114, [IntPtr]::Zero, [IntPtr]::Zero)
    $scrollInfo = New-Object UiAuditFixture+ScrollInfo
    $scrollInfo.Size = [Runtime.InteropServices.Marshal]::SizeOf($scrollInfo)
    $scrollInfo.Mask = 0x7 # SIF_RANGE | SIF_PAGE | SIF_POS
    $scrollInfo.Min = 0
    $scrollInfo.Max = 1049
    $scrollInfo.Page = 50
    $scrollInfo.Position = 1000
    [void][UiAuditFixture]::SetScrollInfo($scrollHost, 1, [ref]$scrollInfo, $true)
    [void][UiAuditFixture]::CreateWindowEx(0, 'STATIC', 'X', $visible,
        5, -1000, 120, 18, $scrollHost, [IntPtr]115, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][UiAuditFixture]::CreateWindowEx(0, 'STATIC', 'Z', $visible,
        -1000, 5, 120, 18, $scrollHost, [IntPtr]117, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][UiAuditFixture]::CreateWindowEx(0, 'STATIC', 'Y', $visible,
        -1000, 110, 140, 18, $parent, [IntPtr]116, [IntPtr]::Zero, [IntPtr]::Zero)
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
    $mnemonic = @($report.controls | Where-Object { $_.Id -eq 112 })[0]
    $plain = @($report.controls | Where-Object { $_.Id -eq 113 })[0]
    if ($mnemonic.MeasuredTextWidth -ne $plain.MeasuredTextWidth) { throw 'Button mnemonic marker was measured as visible text.' }
    $scrolled = @($report.controls | Where-Object { $_.Id -eq 115 })
    $unreachable = @($report.controls | Where-Object { $_.Id -eq 116 })
    $wrongAxis = @($report.controls | Where-Object { $_.Id -eq 117 })
    if ($scrolled.Count -ne 1 -or $scrolled[0].OutsideWorkArea) {
        throw 'Content reachable through a parent scrollbar was reported outside the work area.'
    }
    if ($unreachable.Count -ne 1 -or -not $unreachable[0].OutsideWorkArea) {
        throw 'A non-scrollable offscreen control was not reported outside the work area.'
    }
    if ($wrongAxis.Count -ne 1 -or -not $wrongAxis[0].OutsideWorkArea) {
        throw 'A vertical scrollbar must not excuse a horizontally unreachable control.'
    }
    Write-Host 'UI Layout Audit native fixture passed.'
    $passed = $true
}
finally {
    $form.Close()
    $form.Dispose()
    if ($passed) { Remove-Item -LiteralPath $exceptionPath, $reportPath -Force -ErrorAction SilentlyContinue }
    else { Write-Host "UI Layout Audit failure artifacts: $exceptionPath, $reportPath" }
}
