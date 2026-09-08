[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('Win32')]
    [string]$Platform = 'Win32',
    [string]$PlatformToolset = 'v143'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $PSScriptRoot 'Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset -VcVarsVersion 14.44
$msbuild = Join-Path ${env:VSINSTALLDIR} 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) { throw "MSBuild not found: $msbuild" }

$releaseFiles = @(
    'FBE.exe',
    'FBV.exe',
    'FBShell.dll',
    'Plugins\ExportHTML.dll',
    'Plugins\ExportDOCX.dll',
    'Plugins\ExportEPUB.dll',
    'Plugins\ImportEPUB.dll',
    'Plugins\ImportEPUBLunaSVG.dll'
)
$releaseDirectory = Join-Path $repoRoot "out\$Configuration"
$batchDirectory = Join-Path $repoRoot 'out\target-batches'
$batchFiles = @('ExportDOCXBatch.exe', 'ExportEPUBBatch.exe', 'ImportEPUBBatch.exe')
$strictOutRoot = Join-Path $repoRoot 'out\strict-warnings'
$strictIntRoot = Join-Path $repoRoot 'build\strict-warnings'

function Get-ArtifactHashes {
    param([string]$Directory, [string[]]$Names, [string]$Kind)

    $hashes = @{}
    foreach ($name in $Names) {
        $path = Join-Path $Directory $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required $Kind artifact is missing before the strict gate: $path"
        }
        $hashes[$name] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    }
    return $hashes
}

function Assert-ArtifactHashesUnchanged {
    param([hashtable]$Expected, [string]$Directory, [string]$Kind)

    foreach ($entry in $Expected.GetEnumerator()) {
        $path = Join-Path $Directory $entry.Key
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "$Kind artifact was removed by the strict gate: $path"
        }
        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($actual -ne $entry.Value) {
            throw "$Kind artifact was changed by the strict gate: $path"
        }
    }
}

$releaseHashes = Get-ArtifactHashes -Directory $releaseDirectory -Names $releaseFiles -Kind 'Release'
$batchHashes = Get-ArtifactHashes -Directory $batchDirectory -Names $batchFiles -Kind 'target-batches'

# The IDL contract has no C/C++ compiler inputs, but is checked separately so
# every first-party project boundary is covered by the gate.  Dependencies for
# native projects must be prepared by the normal build job;
# BuildProjectReferences=false avoids a second vendor/dependency build in CI.
$contractName = 'FBEContracts'
$contractProject = Join-Path $repoRoot "src\contracts\$contractName.vcxproj"
if (-not (Test-Path -LiteralPath $contractProject)) { throw "First-party contract project not found: $contractProject" }
Write-Host 'Strict first-party IDL: src\contracts\FBEContracts.vcxproj'
& $msbuild $contractProject /t:Rebuild /m:1 /v:minimal /nologo `
    "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:PlatformToolset=$PlatformToolset" `
    "/p:FbeStrictWarningsOutRoot=$strictOutRoot\" "/p:FbeStrictWarningsIntRoot=$strictIntRoot\" `
    /p:FbeStrictWarnings=true /p:BuildProjectReferences=false
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$projects = @(
    'src\fbe\FBE.vcxproj',
    'src\fbshell\FBShell.vcxproj',
    'src\fbv\FBV.vcxproj',
    'src\export-html\ExportHTML.vcxproj',
    'src\export-docx\ExportDOCX.vcxproj',
    'src\export-epub\ExportEPUB.vcxproj',
    'src\import-epub\ImportEPUB.vcxproj',
    'src\export-docx\ExportDOCXBatch.vcxproj',
    'src\export-epub\ExportEPUBBatch.vcxproj',
    'src\import-epub\ImportEPUBBatch.vcxproj',
    'src\import-epub\ImportEPUBLunaSVG.vcxproj'
)

foreach ($relativeProject in $projects) {
    $project = Join-Path $repoRoot $relativeProject
    if (-not (Test-Path -LiteralPath $project)) { throw "First-party project not found: $project" }
    Write-Host "Strict first-party warnings: $relativeProject"
    & $msbuild $project /t:Rebuild /m:1 /v:minimal /nologo `
        "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:PlatformToolset=$PlatformToolset" `
        "/p:FbeStrictWarningsOutRoot=$strictOutRoot\" "/p:FbeStrictWarningsIntRoot=$strictIntRoot\" `
        /p:FbeStrictWarnings=true /p:BuildProjectReferences=false
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Assert-ArtifactHashesUnchanged -Expected $releaseHashes -Directory $releaseDirectory -Kind 'Release'
Assert-ArtifactHashesUnchanged -Expected $batchHashes -Directory $batchDirectory -Kind 'target-batches'
Write-Host 'Strict first-party gate preserved Release and target-batches artifacts.'
