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

# The IDL contract has no C/C++ compiler inputs, but is checked separately so
# every first-party project boundary is covered by the gate.  Dependencies for
# native projects must be prepared by the normal build job;
# BuildProjectReferences=false avoids a second vendor/dependency build in CI.
$contractProject = Join-Path $repoRoot 'src\contracts\FBEContracts.vcxproj'
if (-not (Test-Path -LiteralPath $contractProject)) { throw "First-party contract project not found: $contractProject" }
Write-Host 'Strict first-party IDL: src\contracts\FBEContracts.vcxproj'
& $msbuild $contractProject /t:Rebuild /m:1 /v:minimal /nologo `
    "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:PlatformToolset=$PlatformToolset" `
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
        /p:FbeStrictWarnings=true /p:BuildProjectReferences=false
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
