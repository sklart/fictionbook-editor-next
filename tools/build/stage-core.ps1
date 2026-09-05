[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$EditorRuntimeDirectory = '',
    [string]$BatchOutputDirectory = '',
    [string]$ArchHandlerOutputDirectory = '',
    [string]$ProvenanceDirectory = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $PSScriptRoot 'PackageLayout.ps1')
$layout = Get-FbePackageLayout -RepositoryRoot $repoRoot
$buildOutput = Join-Path $repoRoot "out\$Configuration"
$editorRuntime = if ($EditorRuntimeDirectory) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EditorRuntimeDirectory) } else { Join-Path $repoRoot 'runtime' }
$batchOutput = if ($BatchOutputDirectory) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BatchOutputDirectory) } else { $buildOutput }
$archOutput = if ($ArchHandlerOutputDirectory) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ArchHandlerOutputDirectory) } else { Join-Path $repoRoot "out\archhandler\Win32\$Configuration" }
$stage = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)

foreach ($path in @($buildOutput, $editorRuntime, $batchOutput, $archOutput)) {
    if (-not (Test-Path -LiteralPath $path -PathType Container)) { throw "Не найден подготовленный input Core: $path" }
}
$commonSource = $buildOutput
$commonPlugins = Join-Path $commonSource 'Plugins'
$commonProvenanceArguments = @{ Action = 'Validate'; Kind = 'CommonCore'; Configuration = $Configuration; CommonDirectory = $commonSource }
$runtimeProvenanceArguments = @{ Action = 'Validate'; Kind = 'Runtime'; Configuration = $Configuration; ProfileDirectory = $editorRuntime; BatchDirectory = $batchOutput; ArchHandlerDirectory = $archOutput }
if ($ProvenanceDirectory) {
    $commonProvenanceArguments.ProvenanceDirectory = $ProvenanceDirectory
    $runtimeProvenanceArguments.ProvenanceDirectory = $ProvenanceDirectory
}
& (Join-Path $PSScriptRoot 'build-provenance.ps1') @commonProvenanceArguments
& (Join-Path $PSScriptRoot 'build-provenance.ps1') @runtimeProvenanceArguments

if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null
$sourceRoots = @{
    runtime = Join-Path $repoRoot 'runtime'
    editorRuntime = $editorRuntime
    common = $commonSource
    commonPlugins = $commonPlugins
    batch = $batchOutput
    arch = $archOutput
    repository = $repoRoot
    thirdParty = Join-Path $repoRoot 'third_party'
    lunaSvg = Join-Path $repoRoot 'src\import-epub\thirdparty\lunasvg'
    plutoVg = Join-Path $repoRoot 'src\import-epub\thirdparty\lunasvg\plutovg'
}
Copy-FbePackageLayoutEntries -Entries $layout.core.copy -SourceRoots $sourceRoots -StageDirectory $stage
Copy-FbePackageLayoutAliases -Aliases $layout.core.aliases -StageDirectory $stage
foreach ($relativePath in @($layout.core.remove)) {
    $target = Join-Path $stage $relativePath
    if ($relativePath -like '*/*') { Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction SilentlyContinue }
    else { Remove-Item -LiteralPath $target -Force -ErrorAction SilentlyContinue }
}
& (Join-Path $repoRoot 'tools\localization\export-runtime-lang.ps1') -RepositoryRoot $repoRoot -OutputDirectory (Join-Path $stage 'Lang') -Clean
& (Join-Path $PSScriptRoot 'verify-package-stage.ps1') -Kind Core -StageDirectory $stage
Write-Host "Core stage prepared: $stage"
