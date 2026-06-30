<#
.SYNOPSIS
Подготавливает переменные окружения Visual Studio для дальнейшей сборки из PowerShell.
#>

[CmdletBinding()]
param(
    [ValidateSet("x86")]
    [string]$Arch = "x86",

    [ValidateSet("x64")]
    [string]$HostArch = "x64"
)

$ErrorActionPreference = "Stop"

$sentinelName = "FBE_VSDEV_${Arch}_${HostArch}_INITIALIZED"
if ([Environment]::GetEnvironmentVariable($sentinelName, "Process") -eq "1") {
    return
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Не найден vswhere.exe. Установите Visual Studio с инструментами сборки C++."
}

$installationPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $installationPath) {
    throw "Не найдены инструменты сборки Visual Studio C++ для x86."
}

$vsDevCmd = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"
$environment = & cmd.exe /d /s /c "`"$vsDevCmd`" -arch=$Arch -host_arch=$HostArch >nul && set"
if ($LASTEXITCODE -ne 0) {
    throw "Не удалось инициализировать среду сборки Visual Studio для $Arch."
}

foreach ($line in $environment) {
    $separator = $line.IndexOf("=")
    if ($separator -gt 0) {
        [Environment]::SetEnvironmentVariable(
            $line.Substring(0, $separator),
            $line.Substring($separator + 1),
            "Process")
    }
}

[Environment]::SetEnvironmentVariable($sentinelName, "1", "Process")
