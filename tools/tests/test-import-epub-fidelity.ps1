[CmdletBinding()]
param([string]$BatchPath=(Join-Path $PSScriptRoot '../../out/Release/ImportEPUBBatch.exe'))
$ErrorActionPreference='Stop'
if (!(Test-Path -LiteralPath $BatchPath -PathType Leaf)) {throw "Missing ImportEPUBBatch.exe: $BatchPath"}
$launcher=Get-Command py.exe -ErrorAction SilentlyContinue
if (!$launcher) {throw 'The short EPUB fixtures require the Windows Python 3 launcher (py.exe)'}
& $launcher.Source -3 (Join-Path $PSScriptRoot 'test-import-epub-fidelity.py') --batch $BatchPath -v
if ($LASTEXITCODE -ne 0) {throw 'EPUB content fidelity regressions failed'}
