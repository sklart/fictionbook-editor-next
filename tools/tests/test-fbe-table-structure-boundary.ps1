$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$grid = Join-Path $root 'src\fbe\table\TableGrid.cpp'
$editor = Join-Path $root 'src\fbe\table\TableStructuralEditor.cpp'
$view = Join-Path $root 'src\fbe\FBEview.cpp'

foreach ($path in @($grid, $editor)) {
    $text = Get-Content -Raw $path
    foreach ($forbidden in @('FBEview.h', 'CFBEView', 'mainfrm.h', 'CMainFrame')) {
        if ($text -match [regex]::Escape($forbidden)) { throw "$path must not depend on $forbidden" }
    }
}

$viewText = Get-Content -Raw $view
foreach ($legacy in @('struct LogicalTableCell', 'struct LogicalTableGrid', 'BuildLogicalTableGrid')) {
    if ($viewText -match [regex]::Escape($legacy)) { throw "FBEview.cpp still owns $legacy" }
}

Write-Host 'PASS: table structure boundary'
