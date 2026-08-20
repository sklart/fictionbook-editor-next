[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$BuildOutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildOutput = if ($BuildOutputDirectory) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildOutputDirectory) } else { Join-Path $repoRoot "out\$Configuration" }
$shellRoot = Join-Path $repoRoot 'out\package\shell-build'
$stage = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
foreach ($path in @((Join-Path $shellRoot "Win32\$Configuration\FBShell.dll"),(Join-Path $shellRoot "x64\$Configuration\FBShell.dll"),(Join-Path $buildOutput 'Lang\Shell\FBVVerbResources.dll'))) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Не найден подготовленный Integration artifact: $path" } }
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath (Join-Path $shellRoot "Win32\$Configuration\FBShell.dll") -Destination (Join-Path $stage 'FBShell.dll')
Copy-Item -LiteralPath (Join-Path $shellRoot "x64\$Configuration\FBShell.dll") -Destination (Join-Path $stage 'FBShell64.dll')
Copy-Item -LiteralPath (Join-Path $repoRoot 'packaging\property-schema\FBE.Sequence.propdesc') -Destination $stage
$tools = Join-Path $stage 'InstallerTools'; New-Item -ItemType Directory -Path $tools | Out-Null
foreach ($name in @('register-sequence-property-schema.ps1','register-modern-property-handler.ps1','unregister-modern-property-handler.ps1')) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $tools }
Copy-Item -LiteralPath (Join-Path $buildOutput 'Lang\Shell') -Destination (Join-Path $stage 'Lang\Shell') -Recurse -Force
& (Join-Path $PSScriptRoot 'verify-package-stage.ps1') -Kind Integration -StageDirectory $stage
Write-Host "Integration stage prepared: $stage"
