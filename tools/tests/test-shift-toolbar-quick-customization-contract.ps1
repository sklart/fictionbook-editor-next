<#
.SYNOPSIS
Guards the legacy Shift quick-customization path for FBE toolbars.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')

function MustContain([string] $text, [string] $value, [string] $description) {
    if ($text.IndexOf($value, [StringComparison]::Ordinal) -lt 0) {
        throw "${description}: '$value'"
    }
}

foreach ($required in @(
    'BeginToolbarQuickCustomize',
    'TrackToolbarQuickCustomize',
    'CompleteToolbarQuickCustomize',
    'ApplyToolbarQuickCustomizeItems',
    '(wParam & MK_SHIFT) != 0',
    'TB_HITTEST',
    'SM_CXDRAG',
    'SM_CYDRAG',
    'items.insert(items.begin() + source, separator)',
    'items.erase(items.begin() + source)',
    'if(sourceSeparator) return false',
    'if(!::PtInRect(&client, point)) return false',
    'control.GetItemRect(destination, &targetRect)',
    'const bool afterTarget = point.x > midpoint || (point.x == midpoint && destination > source)',
    'if(source < insertionPoint) --insertionPoint;',
    'UpdateCommandToolbarItems(items)',
    'UpdateScriptToolbarItems(runtime->definition.id, items)',
    'FbeScripts::ScriptToolbarPersistence::Save(layout)',
    'ToolbarLayoutAdapter::Apply(toolbar, previous, catalog)',
    'return ::DefSubclassProc(window, message, wParam, lParam);'
)) {
    MustContain $source $required 'Shift quick-customization implementation is incomplete'
}

function MoveToolbarItem([string[]] $items, [int] $source, [int] $destination, [bool] $afterTarget) {
    $insertionPoint = $destination + $(if ($afterTarget) { 1 } else { 0 })
    if ($insertionPoint -eq $source -or $insertionPoint -eq ($source + 1)) { return @($items) }
    $result = [System.Collections.Generic.List[string]]::new($items)
    $moved = $result[$source]
    $result.RemoveAt($source)
    if ($source -lt $insertionPoint) { --$insertionPoint }
    $result.Insert($insertionPoint, $moved)
    return @($result)
}

$cases = @(
    @{ Name = 'A->B'; Source = 0; Destination = 1; After = $true; Expected = 'B,A,C' },
    @{ Name = 'B->A'; Source = 1; Destination = 0; After = $false; Expected = 'B,A,C' },
    @{ Name = 'A->C'; Source = 0; Destination = 2; After = $true; Expected = 'B,C,A' },
    @{ Name = 'C->A'; Source = 2; Destination = 0; After = $false; Expected = 'C,A,B' }
)
foreach ($case in $cases) {
    $actual = (MoveToolbarItem @('A', 'B', 'C') $case.Source $case.Destination $case.After) -join ','
    if ($actual -ne $case.Expected) { throw "Shift-drag insertion point failed for $($case.Name): $actual" }
}

foreach ($required in @(
    'HWND m_quickToolbarWindow;',
    'int m_quickToolbarSourceIndex;',
    'bool m_quickToolbarSourceSeparator;',
    'bool m_quickToolbarDragging;'
)) {
    MustContain $header $required 'Quick-customization state is incomplete'
}

if ($source -match 'DeleteButton\(.*outside|AddScriptToToolbar\(.*quick') {
    throw 'Shift quick-customization must not add scripts or delete buttons outside a toolbar.'
}

Write-Host 'Shift toolbar quick-customization contract passed.'