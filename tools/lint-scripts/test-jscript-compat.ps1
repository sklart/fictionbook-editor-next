$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$parser = Join-Path $PSScriptRoot 'jscript-compat.js'
$fixtures = Join-Path $PSScriptRoot 'fixtures'
if(-not (Get-Command cscript.exe -ErrorAction SilentlyContinue)) { throw 'Windows Script Host cscript.exe is required for the JScript regression test.' }

# The parser accepts a runtime-shaped folder; use a temporary isolated copy so
# a modern token is verified by the real engine without executing it.
$temporary = Join-Path ([System.IO.Path]::GetTempPath()) ("fbe-jscript-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    Copy-Item -LiteralPath (Join-Path $fixtures 'jscript-valid.js') -Destination (Join-Path $temporary 'valid.js')
    & cscript.exe //nologo $parser $temporary
    if($LASTEXITCODE -ne 0) { throw 'The compatible ES3 fixture was rejected by Microsoft JScript.' }
    Copy-Item -LiteralPath (Join-Path $fixtures 'jscript-modern-syntax.js') -Destination (Join-Path $temporary 'modern.js')
    & cscript.exe //nologo $parser $temporary 2>$null
    if($LASTEXITCODE -eq 0) { throw 'Microsoft JScript must reject the modern-syntax fixture.' }
} finally {
    Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host 'Microsoft JScript modern-syntax regression passed.'
