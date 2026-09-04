<# Verifies that Batch EXEs show console help, and pause only in their own console. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sources = @(
    'src\export-docx\ExportDOCXBatch.cpp',
    'src\export-epub\ExportEPUBBatch.cpp',
    'src\import-epub\ImportEPUBBatch.cpp'
)

foreach ($relativePath in $sources) {
    $source = Get-Content -Raw -LiteralPath (Join-Path $root $relativePath)
    if ($source -match 'MessageBoxW\s*\(') { throw "$relativePath still displays a MessageBox for interactive Batch help." }
    foreach ($required in @('ShowInteractiveLaunchHelp', 'PrintUsage|Usage\(', 'GetConsoleMode', 'GetConsoleProcessList', 'ReadConsoleInputW', 'Нажмите любую клавишу для выхода')) {
        if ($source -notmatch $required) { throw "$relativePath is missing interactive console behavior: $required" }
    }
    if ($source -match 'GetConsoleWindow|IsWindowVisible') { throw "$relativePath must not require a visible console window for interactive Batch help." }
    if ($source -match 'system\s*\(\s*"pause|cmd\s*/k|wt\.exe') { throw "$relativePath uses a prohibited pause launcher." }
}

Write-Host 'Batch interactive-launch source checks passed.'
