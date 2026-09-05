[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$BuildOutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $PSScriptRoot 'PackageLayout.ps1')
$layout = Get-FbePackageLayout -RepositoryRoot $repoRoot
$buildOutput = if ($BuildOutputDirectory) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildOutputDirectory) } else { Join-Path $repoRoot "out\$Configuration" }
$shellRoot = Join-Path $repoRoot 'out\package\shell-build'
$stage = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
foreach ($path in @((Join-Path $shellRoot "Win32\$Configuration\FBShell.dll"),(Join-Path $shellRoot "x64\$Configuration\FBShell.dll"),(Join-Path $buildOutput 'Lang\Shell\FBVVerbResources.dll'))) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Не найден подготовленный Integration artifact: $path" } }
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null
$sourceRoots = @{
    shellWin32 = Join-Path $shellRoot "Win32\$Configuration"
    shellX64 = Join-Path $shellRoot "x64\$Configuration"
    repository = $repoRoot
    tools = $PSScriptRoot
    build = $buildOutput
}
Copy-FbePackageLayoutEntries -Entries $layout.integration.copy -SourceRoots $sourceRoots -StageDirectory $stage
& (Join-Path $PSScriptRoot 'verify-package-stage.ps1') -Kind Integration -StageDirectory $stage
Write-Host "Integration stage prepared: $stage"
