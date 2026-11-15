[CmdletBinding()]
param([string]$BatchPath)
$ErrorActionPreference='Stop'
if (!$BatchPath) { $BatchPath = Join-Path $PSScriptRoot '../../out/Release/ImportEPUBBatch.exe' }
if (!(Test-Path -LiteralPath $BatchPath -PathType Leaf)) {throw "Missing ImportEPUBBatch.exe: $BatchPath"}
$launcher=Get-Command py.exe -ErrorAction SilentlyContinue
if ($launcher) {
    & $launcher.Source -3 (Join-Path $PSScriptRoot 'test-import-epub-secure-zip.py') --batch $BatchPath -v
} elseif ($env:FBE_TEST_PYTHON) {
    if (!(Test-Path -LiteralPath $env:FBE_TEST_PYTHON -PathType Leaf)) { throw 'FBE_TEST_PYTHON must name a Python 3 executable.' }
    & $env:FBE_TEST_PYTHON (Join-Path $PSScriptRoot 'test-import-epub-secure-zip.py') --batch $BatchPath -v
} else {
    throw 'The short EPUB fixtures require py.exe or a Python 3 executable in FBE_TEST_PYTHON.'
}
if ($LASTEXITCODE -ne 0) {throw 'EPUB ZIP security regressions failed'}
