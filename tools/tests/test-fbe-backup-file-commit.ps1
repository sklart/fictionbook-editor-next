[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$output = Join-Path ([IO.Path]::GetTempPath()) ('fbe-backup-file-commit-' + [guid]::NewGuid().ToString('N') + '.exe')
try {
    & cl.exe /nologo /EHsc /W4 /WX /DUNICODE /D_UNICODE /I (Join-Path $repoRoot 'src\fbe') (Join-Path $PSScriptRoot 'backup-file-commit-test.cpp') /Fe$output
    if ($LASTEXITCODE -ne 0) { throw 'Не удалось скомпилировать filesystem backup regression test.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Filesystem backup regression test failed.' }
    Write-Host 'Filesystem backup regression test passed.'
}
finally { Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue }
