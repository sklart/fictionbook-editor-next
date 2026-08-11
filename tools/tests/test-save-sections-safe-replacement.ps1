$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'runtime\Utilities\Save Sections As Separate Documents\SaveSectionsAsSeparateDocuments.hta')
foreach ($pattern in @('BuildPath\(folder,"\.save-sections-', 'parkedPath', 'FBE_NEXT_TEST_MODE', 'SAVE_SECTIONS_FAIL_REPLACE', 'fso\.MoveFile\(parkedPath, pathForSaving\)')) {
    if ($source -notmatch $pattern) { throw "Не найдена обязательная защита безопасной замены: $pattern" }
}
if ($source -match 'DeleteFile\(pathForSaving') { throw 'Целевой файл нельзя удалять до успешной замены.' }
Write-Host 'Save Sections safe replacement contract passed.'
