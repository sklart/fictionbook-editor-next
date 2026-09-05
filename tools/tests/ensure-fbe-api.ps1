[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('Win32')]
    [string]$Platform = 'Win32',
    [string]$PlatformToolset = 'v143'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

$null = & (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') `
    -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset -VcVarsVersion '14.44'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "vswhere.exe was not found: $vswhere"
}
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -version '[17.0,18.0)' -find 'MSBuild\Current\Bin\MSBuild.exe' | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($msbuild)) {
    throw 'Visual Studio 2022 MSBuild was not found.'
}

$null = & $msbuild (Join-Path $repoRoot 'src\contracts\FBEContracts.vcxproj') `
    /t:Build "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:PlatformToolset=$PlatformToolset" /v:minimal /nologo
if ($LASTEXITCODE -ne 0) {
    throw 'FBE API contract generation failed.'
}

$apiDirectory = Join-Path $repoRoot "build\generated\$Platform\$Configuration\fbe-api"
foreach ($name in 'FBE.h', 'FBE_i.c', 'FBE.tlb') {
    if (-not (Test-Path -LiteralPath (Join-Path $apiDirectory $name) -PathType Leaf)) {
        throw "FBE API generator did not produce $name."
    }
}

Write-Output $apiDirectory
