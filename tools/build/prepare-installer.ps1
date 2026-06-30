[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [switch]$SkipPortablePackage
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$portableScript = Join-Path $PSScriptRoot "package-portable.ps1"
$buildExperimentalShellScript = Join-Path $PSScriptRoot "build-experimental-property-handler.ps1"
$verifyPackageStageScript = Join-Path $PSScriptRoot "verify-package-stage.ps1"
$portableDir = Join-Path $repoRoot "out\package\FictionBookEditor"
$uacThirdPartyDir = Join-Path $repoRoot "third_party\uac"
$uacNsisDir = Join-Path $repoRoot "packaging\nsis\NSIS"

if (-not $SkipPortablePackage) {
    foreach ($platform in @("Win32", "x64")) {
        & $buildExperimentalShellScript -Configuration $Configuration -Platform $platform
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    & $portableScript -Configuration $Configuration -RequireWin32PropertyHandler -RequireX64ShellExtension
}

if (-not (Test-Path -LiteralPath $portableDir)) {
    throw "Portable-пакет не найден: $portableDir"
}

& $verifyPackageStageScript -StageDirectory $portableDir

if (Test-Path -LiteralPath $uacThirdPartyDir) {
    $uacUnicodePluginDir = Join-Path $uacNsisDir "Plugins\x86-unicode"
    New-Item -ItemType Directory -Path $uacNsisDir -Force | Out-Null
    New-Item -ItemType Directory -Path $uacUnicodePluginDir -Force | Out-Null

    Copy-Item -LiteralPath (Join-Path $uacThirdPartyDir "UAC.nsh") `
        -Destination (Join-Path $uacNsisDir "UAC.nsh") -Force
    Copy-Item -LiteralPath (Join-Path $uacThirdPartyDir "Plugins\x86-unicode\UAC.dll") `
        -Destination (Join-Path $uacNsisDir "UAC.dll") -Force
    Copy-Item -LiteralPath (Join-Path $uacThirdPartyDir "Plugins\x86-unicode\UAC.dll") `
        -Destination (Join-Path $uacUnicodePluginDir "UAC.dll") -Force

    Write-Host "UAC-плагин синхронизирован из third_party\uac."
}

Write-Host "NSIS будет читать входные файлы напрямую из $portableDir"
