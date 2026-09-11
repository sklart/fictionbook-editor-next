[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-backup-file-commit-' + [guid]::NewGuid().ToString('N'))
$output = Join-Path $testDirectory 'fbe-backup-file-commit.exe'
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
try {
    & cl.exe /nologo /EHsc /W4 /WX /DUNICODE /D_UNICODE /I (Join-Path $repoRoot 'src\fbe') "/Fo:$testDirectory\\" (Join-Path $PSScriptRoot 'backup-file-commit-test.cpp') /Fe$output
    if ($LASTEXITCODE -ne 0) { throw 'Не удалось скомпилировать filesystem backup regression test.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Filesystem backup regression test failed.' }
    Write-Host 'Filesystem backup regression test passed.'
}
finally { Remove-Item -LiteralPath $testDirectory -Recurse -Force -ErrorAction SilentlyContinue }
