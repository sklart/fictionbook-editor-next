<# Prevents local tooling and developer by-products from returning to plugin source roots. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$pluginDirectories = @(
    'src\export-docx',
    'src\export-epub',
    'src\export-html',
    'src\import-epub'
)
$forbiddenFilePatterns = @(
    '*.bat', '*.cmd', 'TEST_*', 'RUN_*TEST*', 'TODO_*.txt', 'PVS_*.txt',
    '*_FIX_????????.txt', 'STABILIZATION_*.txt', 'LARGE_TEST_*.txt',
    'PROBLEM_FILES_*.txt', '*build*.log', '*.plog'
)
$forbiddenDirectoryNames = @('out', 'build_logs')
$violations = [Collections.Generic.List[string]]::new()

foreach ($relativeDirectory in $pluginDirectories) {
    $directory = Join-Path $repoRoot $relativeDirectory
    foreach ($item in Get-ChildItem -LiteralPath $directory -Force) {
        if ($item.PSIsContainer) {
            if ($item.Name -in $forbiddenDirectoryNames) { $violations.Add("$relativeDirectory/$($item.Name)") }
            continue
        }
        foreach ($pattern in $forbiddenFilePatterns) {
            if ($item.Name -like $pattern) {
                $violations.Add("$relativeDirectory/$($item.Name)")
                break
            }
        }
    }
}

if ($violations.Count) { throw "Forbidden plugin-source layout entries: $($violations -join ', ')" }
Write-Host 'Plugin source layout policy passed.'
