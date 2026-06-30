<#
.SYNOPSIS
Собирает статическую библиотеку Hunspell из локального проекта libhunspell.vcxproj.
#>

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "ThirdPartySources.ps1")

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$projectPath = Join-Path $repoRoot "build\hunspell\libhunspell.vcxproj"
$solutionDir = $repoRoot
if (-not $solutionDir.EndsWith("\")) {
    $solutionDir += "\"
}

if (-not (Test-Path -LiteralPath $projectPath)) {
    throw "Не найден проект сборки Hunspell: $projectPath"
}

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Не найден vswhere.exe. Установите Visual Studio с инструментами сборки C++."
}

$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
    -find "MSBuild\Current\Bin\MSBuild.exe" | Select-Object -First 1

if (-not $msbuild) {
    throw "Не найден MSBuild.exe."
}

& $msbuild $projectPath /m /t:Build `
    "/p:Configuration=$Configuration" `
    "/p:Platform=Win32" `
    "/p:SolutionDir=$solutionDir" `
    /v:minimal /nologo

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host (Format-ThirdPartyText "SHVuc3BlbGwg0L/QvtC00LPQvtGC0L7QstC70LXQvSDQsiBidWlsZFxcaHVuc3BlbGxcXGxpYlxcezB9" $Configuration)
