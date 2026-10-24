[CmdletBinding()]
param([Parameter(Mandatory)][string]$CaseFile)
$ErrorActionPreference='Stop'
$case=Get-Content -Raw -LiteralPath $CaseFile | ConvertFrom-Json
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outputDir=$case.outputDirectory; $batchOutputDir=$outputDir
$Configuration='Release'; $PlatformToolset='v143'
$env:MSBUILDDISABLENODEREUSE='1'
Set-Location -LiteralPath $repoRoot
try {
    if($env:VCToolsVersion -notlike '14.44*' -or $env:VSCMD_ARG_TGT_ARCH -ne 'x86') { throw 'Sanitizer worker requires the prepared VC 14.44 x86 environment.' }
    # Harnesses request v143, latest, or explicit 14.44 through the shared
    # importer. All three must reuse this pinned diagnostic environment.
    # Re-running VsDevCmd inside every worker also spawns short-lived registry
    # probes unrelated to the runtime scenario under observation.
    foreach($version in @('v143','latest','14.44')) { [Environment]::SetEnvironmentVariable("FBE_VSDEV_x86_x64_${version}_INITIALIZED",'1','Process') }
    $global:LASTEXITCODE=0
    & ([scriptblock]::Create($case.command))
    if ($LASTEXITCODE -ne 0) { throw "Runtime harness native exit: $LASTEXITCODE" }
} catch { Write-Error $_; exit 1 }
