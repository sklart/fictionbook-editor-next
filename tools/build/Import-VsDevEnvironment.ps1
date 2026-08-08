<#
.SYNOPSIS
Подготавливает переменные окружения Visual Studio для дальнейшей сборки из PowerShell.
#>

[CmdletBinding()]
param(
    [ValidateSet("x86")]
    [string]$Arch = "x86",

    [ValidateSet("x64")]
    [string]$HostArch = "x64",

    [string]$PlatformToolset,

    [string]$VcVarsVersion
)

$ErrorActionPreference = "Stop"

$sentinelVersion = if ($VcVarsVersion) { $VcVarsVersion } elseif ($PlatformToolset) { $PlatformToolset } else { "latest" }
$sentinelName = "FBE_VSDEV_${Arch}_${HostArch}_${sentinelVersion}_INITIALIZED"
if ([Environment]::GetEnvironmentVariable($sentinelName, "Process") -eq "1" -and
    (Get-Command nmake.exe -ErrorAction SilentlyContinue)) {
    return
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Не найден vswhere.exe. Установите Visual Studio с инструментами сборки C++."
}

$vswhereArguments = @("-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64")
if ($PlatformToolset -eq "v143") {
    $vswhereArguments += @("-version", "[17.0,18.0)")
}
$installationPath = & $vswhere @vswhereArguments -property installationPath
if (-not $installationPath) {
    throw "Не найдены инструменты сборки Visual Studio C++ для x86."
}

$vsDevCmd = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"
$vcVarsVersionArgument = if ($VcVarsVersion) { " -vcvars_ver=$VcVarsVersion" } else { "" }
$environment = & cmd.exe /d /s /c "`"$vsDevCmd`" -arch=$Arch -host_arch=$HostArch$vcVarsVersionArgument >nul && set"
if ($LASTEXITCODE -ne 0) {
    if ($VcVarsVersion) {
        throw "Не удалось инициализировать среду сборки Visual Studio для $Arch с vcvars_ver=$VcVarsVersion."
    }
    throw "Не удалось инициализировать среду сборки Visual Studio для $Arch."
}

# PowerShell 7 может сохранить два различающихся регистром ключа PATH/Path
# при наследовании окружения. MSBuild передаёт их в ProcessStartInfo как один
# case-insensitive dictionary и из-за этого не может запустить cl.exe.
# Удаляем оба варианта до импорта и записываем единственный канонический Path.
Get-ChildItem Env: |
    Where-Object { $_.Name -ieq "Path" } |
    ForEach-Object { Remove-Item -LiteralPath ("Env:" + $_.Name) -ErrorAction SilentlyContinue }

foreach ($line in $environment) {
    $separator = $line.IndexOf("=")
    if ($separator -gt 0) {
        $name = $line.Substring(0, $separator)
        if ($name -ieq "Path") {
            $name = "Path"
        }
        [Environment]::SetEnvironmentVariable(
            $name,
            $line.Substring($separator + 1),
            "Process")
    }
}

[Environment]::SetEnvironmentVariable($sentinelName, "1", "Process")

$clPath = Get-Command cl.exe -ErrorAction Stop | Select-Object -ExpandProperty Source
Write-Host "VS toolchain: installation=$installationPath; PlatformToolset=$PlatformToolset; cl.exe=$clPath; VCToolsVersion=$env:VCToolsVersion"
