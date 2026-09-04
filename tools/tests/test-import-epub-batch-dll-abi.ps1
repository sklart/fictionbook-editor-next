<#
.SYNOPSIS
Verifies the C ABI boundary between ImportEPUBBatch and ImportEPUB.dll.
#>
[CmdletBinding()]
param(
    [string]$DllPath,
    [string]$DumpbinPath,
    [string]$BatchPath,
    [string]$SmokeEpubPath
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$batchProject = Get-Content -Raw (Join-Path $root 'src\import-epub\ImportEPUBBatch.vcxproj')
$pluginProject = Get-Content -Raw (Join-Path $root 'src\import-epub\ImportEPUB.vcxproj')
$batchSource = Get-Content -Raw (Join-Path $root 'src\import-epub\ImportEPUBBatch.cpp')
$apiHeader = Get-Content -Raw (Join-Path $root 'src\import-epub\ImportEPUBApi.h')
$def = Get-Content -Raw (Join-Path $root 'src\import-epub\ImportEPUB.def')

if ($batchProject -match 'EpubImport\.cpp') { throw 'ImportEPUBBatch.vcxproj must not compile EpubImport.cpp.' }
if ($pluginProject -notmatch '<ClCompile Include="EpubImport\.cpp"') { throw 'ImportEPUB.dll must compile EpubImport.cpp.' }
if ($batchSource -match 'BuildFb2XmlFromEpub|GetLastEpubImportRuntimeStats|#include "EpubImport\.h"') { throw 'ImportEPUBBatch still uses the importer C++ API.' }
if ($batchSource -notmatch 'LoadLibraryW' -or $batchSource -notmatch 'GetProcAddress' -or $batchSource -notmatch 'Plugins' -or $batchSource -notmatch 'ImportEPUB\.dll') { throw 'ImportEPUBBatch does not dynamically load Plugins\ImportEPUB.dll.' }
if ($batchSource -notmatch 'was not found' -or $batchSource -notmatch 'same Win32/x86 architecture' -or $batchSource -notmatch 'does not export ImportEPUB_BuildFb2XmlW') { throw 'ImportEPUBBatch is missing a required DLL load diagnostic.' }
if ($apiHeader -notmatch 'extern "C"' -or $apiHeader -notmatch 'HRESULT WINAPI ImportEPUB_BuildFb2XmlW' -or $apiHeader -notmatch 'BSTR\* fb2Xml' -or $apiHeader -notmatch 'BSTR\* errorText' -or $apiHeader -notmatch 'ImportEpubRuntimeStatsV1') { throw 'The stable BSTR-based C ImportEPUB ABI declaration is incomplete.' }
if ($batchSource -match 'requiredFb2Cch|ERROR_INSUFFICIENT_BUFFER|fb2Buffer') { throw 'ImportEPUBBatch retains the two-pass output-buffer ABI.' }
if (([regex]::Matches($batchSource, 'g_importEpubBuild\(inputPath')).Count -ne 1) { throw 'ImportEPUBBatch must dispatch exactly one DLL import per EPUB.' }
if ($batchSource -notmatch 'SysFreeString\(apiFb2\)' -or $batchSource -notmatch 'SysFreeString\(apiError\)') { throw 'ImportEPUBBatch does not release BSTR results with SysFreeString.' }
if ($def -notmatch '(?m)^\s*ImportEPUB_BuildFb2XmlW\s*$') { throw 'ImportEPUB.def does not export ImportEPUB_BuildFb2XmlW.' }

if ($DllPath) {
    if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) { throw "ImportEPUB DLL not found: $DllPath" }
    $dumpbin = if ($DumpbinPath) { Get-Command $DumpbinPath -ErrorAction SilentlyContinue } else { Get-Command dumpbin.exe -ErrorAction SilentlyContinue }
    if (-not $dumpbin -and -not $DumpbinPath) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
            if ($installation) {
                $dumpbinCandidate = Get-ChildItem -LiteralPath (Join-Path $installation 'VC\Tools\MSVC') -Directory -ErrorAction SilentlyContinue |
                    Sort-Object Name -Descending |
                    ForEach-Object { Join-Path $_.FullName 'bin\Hostx86\x86\dumpbin.exe' } |
                    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
                    Select-Object -First 1
                if ($dumpbinCandidate) { $dumpbin = Get-Command $dumpbinCandidate -ErrorAction SilentlyContinue }
            }
        }
    }
    if (-not $dumpbin) { throw "dumpbin.exe is required to inspect the built ImportEPUB export: $DumpbinPath" }
    $exports = & $dumpbin.Source /exports $DllPath 2>&1
    if ($LASTEXITCODE -ne 0 -or (($exports -join "`n") -notmatch 'ImportEPUB_BuildFb2XmlW')) { throw 'Built ImportEPUB.dll does not expose ImportEPUB_BuildFb2XmlW.' }
}

if ($BatchPath -or $SmokeEpubPath) {
    if (-not $BatchPath -or -not $SmokeEpubPath) { throw 'BatchPath and SmokeEpubPath must be supplied together.' }
    if (-not (Test-Path -LiteralPath $BatchPath -PathType Leaf)) { throw "ImportEPUBBatch executable not found: $BatchPath" }
    if (-not (Test-Path -LiteralPath $SmokeEpubPath -PathType Leaf)) { throw "Smoke EPUB not found: $SmokeEpubPath" }

    $testDirectory = Join-Path $root 'out\tests\import-epub-batch-dll-abi'
    New-Item -ItemType Directory -Force -Path $testDirectory | Out-Null
    $normalOutput = Join-Path $testDirectory ('normal-plugin-load-' + [guid]::NewGuid().ToString('N') + '.fb2')
    & $BatchPath $SmokeEpubPath $normalOutput
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $normalOutput -PathType Leaf)) { throw 'Batch EPUB-to-FB2 smoke test through Plugins\ImportEPUB.dll failed.' }

    $missingDirectory = Join-Path $testDirectory 'no-dll'
    New-Item -ItemType Directory -Force -Path $missingDirectory | Out-Null
    $isolatedBatch = Join-Path $missingDirectory 'ImportEPUBBatch.exe'
    Copy-Item -LiteralPath $BatchPath -Destination $isolatedBatch -Force
    Push-Location $missingDirectory
    try {
        $missingOutput = Join-Path $missingDirectory 'missing-plugin.fb2'
        & $isolatedBatch $SmokeEpubPath $missingOutput
        if ($LASTEXITCODE -ne 1) { throw 'Batch did not return code 1 when ImportEPUB.dll was unavailable.' }
        if (Test-Path -LiteralPath $missingOutput -PathType Leaf) { throw 'Batch created FB2 output without ImportEPUB.dll.' }
    }
    finally {
        Pop-Location
    }
}

Write-Host 'ImportEPUB Batch DLL ABI checks passed.'
