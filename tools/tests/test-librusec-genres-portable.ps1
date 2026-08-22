<# Exercises the genre resolver through FBE.exe in a clean portable copy. #>
[CmdletBinding()]
param([string]$FbeExecutable)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $FbeExecutable) { $FbeExecutable = Join-Path $root 'out\Release\FBE.exe' }
$FbeExecutable = (Resolve-Path -LiteralPath $FbeExecutable).Path
$manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\package-manifest.json') | ConvertFrom-Json
foreach ($name in @('genres.txt', 'genres.rus.txt', 'genres.ukr.txt', 'genres.librusec.txt', 'genres.rus.librusec.txt')) {
    if ($manifest.core.required -notcontains $name) { throw "Package manifest does not require $name." }
}

$testDirectory = Join-Path $root 'out\tests\librusec-genres-portable'
Remove-Item -LiteralPath $testDirectory -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $testDirectory | Out-Null
Copy-Item -LiteralPath $FbeExecutable -Destination (Join-Path $testDirectory 'FBE.exe')
foreach ($name in @('genres.txt', 'genres.rus.txt', 'genres.ukr.txt', 'genres.txt_L', 'genres.rus.txt_L')) {
    Copy-Item -LiteralPath (Join-Path $root "runtime\$name") -Destination (Join-Path $testDirectory $name)
}
Copy-Item -LiteralPath (Join-Path $root 'runtime\genres.txt_L') -Destination (Join-Path $testDirectory 'genres.librusec.txt')
Copy-Item -LiteralPath (Join-Path $root 'runtime\genres.rus.txt_L') -Destination (Join-Path $testDirectory 'genres.rus.librusec.txt')

function Set-Catalog([string]$Catalog, [int]$Language) {
    $settingsDir = Join-Path $testDirectory 'Data\Settings'
    New-Item -ItemType Directory -Path $settingsDir -Force | Out-Null
    @"
<?xml version="1.0" encoding="utf-8"?>
<FBE><Settings ID="0"><IntefaceLangID>$Language</IntefaceLangID><GenreCatalog>$Catalog</GenreCatalog></Settings></FBE>
"@ | Set-Content -LiteralPath (Join-Path $settingsDir 'Settings.xml') -Encoding UTF8
}

function Invoke-Resolver() {
    $stdout = Join-Path $testDirectory 'resolver.stdout'
    $stderr = Join-Path $testDirectory 'resolver.stderr'
    Remove-Item -LiteralPath $stdout,$stderr -Force -ErrorAction SilentlyContinue
    $process = Start-Process -FilePath (Join-Path $testDirectory 'FBE.exe') -ArgumentList @('--portable', '--print-genre-catalog') -WorkingDirectory $testDirectory -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if ($process.ExitCode -ne 0) { throw "Genre resolver exited with $($process.ExitCode): $(Get-Content -LiteralPath $stderr -Raw -ErrorAction SilentlyContinue)" }
    return (Get-Content -LiteralPath $stdout -Raw | ConvertFrom-Json)
}

# Explicit IDs avoid dependence on the test machine's UI language.
$english = 0xFBE001
$russian = 0xFBE002
$ukrainian = 0xFBE003

Set-Catalog Standard $english
$result = Invoke-Resolver
if ($result.catalog -ne 'Standard' -or $result.resolvedFile -ne 'genres.txt') { throw 'Standard English catalog was not resolved.' }

Set-Catalog Librusec $english
$result = Invoke-Resolver
if ($result.catalog -ne 'Librusec' -or $result.resolvedFile -ne 'genres.librusec.txt') { throw 'Librusec English catalog was not resolved.' }
# The setting survives a restart because the same clean portable Data file is read again.
$result = Invoke-Resolver
if ($result.catalog -ne 'Librusec' -or $result.resolvedFile -ne 'genres.librusec.txt') { throw 'Portable Librusec selection did not persist.' }

Set-Catalog Librusec $russian
$result = Invoke-Resolver
if ($result.resolvedFile -ne 'genres.rus.librusec.txt') { throw 'Librusec Russian catalog was not resolved.' }

Set-Catalog Librusec $ukrainian
$result = Invoke-Resolver
if ($result.resolvedFile -ne 'genres.ukr.txt' -or -not [string]::IsNullOrEmpty($result.legacyFile)) { throw 'Ukrainian Librusec fallback policy is not explicit.' }

Set-Catalog Librusec $english
Remove-Item -LiteralPath (Join-Path $testDirectory 'genres.librusec.txt') -Force
$result = Invoke-Resolver
if ($result.resolvedFile -ne 'genres.txt_L') { throw 'Legacy Librusec fallback was not resolved.' }

Write-Host 'Librusec genre resolver behavior passed.'
