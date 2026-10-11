<#
.SYNOPSIS
Captures a read-only geometry/text audit of visible windows in one process.
.EXAMPLE
./tools/ui/invoke-ui-layout-audit.ps1 -ProcessId $fbe.Id -Language ru-RU
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][int]$ProcessId,
    [Parameter(Mandatory = $true)][string]$Language,
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\..\out\tests\ui-layout-audit.json'),
    [string]$ExceptionsPath
)
$ErrorActionPreference = 'Stop'
if ($ProcessId -le 0) { throw 'ProcessId must be positive.' }
if (-not (Get-Process -Id $ProcessId -ErrorAction SilentlyContinue)) { throw "Process $ProcessId is not running." }
if (-not ('UiLayoutAudit' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'UiLayoutAudit.cs') }
$rows = @([UiLayoutAudit]::Capture($ProcessId))
if ($rows.Count -eq 0) { throw "Process $ProcessId has no visible top-level windows." }

$excludedIds = @()
$excludedPairs = @()
if ($ExceptionsPath) {
    $exceptions = Get-Content -Raw -Encoding UTF8 -LiteralPath $ExceptionsPath | ConvertFrom-Json
    $excludedIds = @($exceptions.ignoredControlIds | ForEach-Object { [int]$_ })
    $excludedPairs = @($exceptions.ignoredOverlapPairs | ForEach-Object { [string]$_ })
}
$issues = [Collections.Generic.List[object]]::new()
foreach ($row in $rows) {
    if ($row.OutsideWorkArea) {
        $issues.Add([pscustomobject]@{ kind = 'outside-work-area'; hwnd = $row.Hwnd; otherHwnd = $null; potential = $false; ignored = ($excludedIds -contains $row.Id) })
    }
    if ($row.TextMayClip) {
        $issues.Add([pscustomobject]@{ kind = 'text-may-clip'; hwnd = $row.Hwnd; otherHwnd = $null; potential = $true; ignored = ($excludedIds -contains $row.Id) })
    }
}
# Only siblings can overlap unintentionally. Group boxes and decorative controls
# commonly contain other elements and are excluded from this heuristic.
for ($leftIndex = 0; $leftIndex -lt $rows.Count; ++$leftIndex) {
    $left = $rows[$leftIndex]
    if ($left.Parent -eq '0x0' -or $left.Decorative -or $left.Class -notin @('Static', 'Button', 'Edit', 'ComboBox')) { continue }
    if ($left.Class -eq 'Button' -and $left.Text -eq '') { continue }
    for ($rightIndex = $leftIndex + 1; $rightIndex -lt $rows.Count; ++$rightIndex) {
        $right = $rows[$rightIndex]
        if ($right.Parent -cne $left.Parent -or $right.Decorative -or $right.Class -notin @('Static', 'Button', 'Edit', 'ComboBox')) { continue }
        if ($right.Class -eq 'Button' -and $right.Text -eq '') { continue }
        $intersectionWidth = [Math]::Min($left.Bounds.Right, $right.Bounds.Right) - [Math]::Max($left.Bounds.Left, $right.Bounds.Left)
        $intersectionHeight = [Math]::Min($left.Bounds.Bottom, $right.Bounds.Bottom) - [Math]::Max($left.Bounds.Top, $right.Bounds.Top)
        if ($intersectionWidth -le 4 -or $intersectionHeight -le 4) { continue }
        $pair = (@($left.Id, $right.Id) | Sort-Object) -join ':'
        $issues.Add([pscustomobject]@{
            kind = 'possible-overlap'; hwnd = $left.Hwnd; otherHwnd = $right.Hwnd;
            potential = $true; ignored = ($excludedIds -contains $left.Id -or $excludedIds -contains $right.Id -or $excludedPairs -contains $pair)
        })
    }
}
$report = [ordered]@{
    schemaVersion = 1
    processId = $ProcessId
    language = $Language
    capturedAtUtc = [DateTime]::UtcNow.ToString('o')
    windows = @($rows | Where-Object { $_.Parent -eq '0x0' } | Select-Object Hwnd, Dialog, Dpi, Bounds, WorkArea)
    controls = $rows
    issues = @($issues)
}
$OutputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
$directory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $directory -Force | Out-Null
[IO.File]::WriteAllText($OutputPath, ($report | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
Write-Host "UI Layout Audit: $($rows.Count) visible windows/controls, $($issues.Count) findings; $OutputPath"
