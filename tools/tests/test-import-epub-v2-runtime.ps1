[CmdletBinding()]
param([string]$Configuration = 'Release', [string]$RuntimeDirectory)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $RuntimeDirectory) { $RuntimeDirectory = Join-Path $root "out\$Configuration" }
$dll = Join-Path $RuntimeDirectory "Plugins\ImportEPUB.dll"
if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) { throw "Missing ImportEPUB.dll: $dll" }

& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$apiDirectory = & (Join-Path $PSScriptRoot 'ensure-fbe-api.ps1') -Configuration $Configuration
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'import-epub-v2' -RuntimePayload
$exe = Join-Path $scratch.Root 'import-epub-v2-runtime.exe'
$objectDir = Join-Path $scratch.Root 'obj'
$fixture = Join-Path $scratch.Root 'cyrillic.epub'
$passed = $false
$oldTemp, $oldTmp = $env:TEMP, $env:TMP
try {
    $env:TEMP = $scratch.Root; $env:TMP = $scratch.Root
    New-Item -ItemType Directory -Path $objectDir -ErrorAction Stop | Out-Null
    & (Join-Path $PSScriptRoot 'test-export-epub-cyrillic.ps1') -Configuration $Configuration -DllPath (Join-Path $RuntimeDirectory 'Plugins\ExportEPUB.dll') -OutputPath $fixture
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $fixture -PathType Leaf)) { throw 'Failed to prepare the ImportEPUB runtime fixture.' }
    & cl.exe /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE "/I$apiDirectory" "/Fo:$objectDir\\" (Join-Path $PSScriptRoot 'import-epub-v2-runtime-harness.cpp') (Join-Path $apiDirectory 'FBE_i.c') /link ole32.lib oleaut32.lib msxml6.lib "/OUT:$exe"
    if ($LASTEXITCODE -ne 0) { throw 'ImportEPUB v2 runtime harness did not compile.' }
    & $exe $dll $fixture
    if ($LASTEXITCODE -ne 0) { throw "ImportEPUB v2 runtime harness failed: $LASTEXITCODE" }
    $passed = $true
}
catch {
    [IO.File]::WriteAllText((Join-Path $scratch.Root 'error.txt'), ($_ | Out-String), [Text.UTF8Encoding]::new($false))
    throw
}
finally {
    $env:TEMP = $oldTemp; $env:TMP = $oldTmp
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed
}
Write-Host 'ImportEPUB v2 runtime checks passed.'
