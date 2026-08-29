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
# A Codex/CI process can already inherit a different Visual Studio environment.
# VsDevCmd otherwise treats that installation as active and leaves its ATL paths
# ahead of the selected toolset.
foreach ($name in @("VSINSTALLDIR", "VCINSTALLDIR", "VCToolsInstallDir", "VCToolsRedistDir", "VisualStudioVersion", "VSCMD_VER", "VSCMD_ARG_app_plat", "VSCMD_ARG_HOST_ARCH", "VSCMD_ARG_TGT_ARCH")) {
    Remove-Item -LiteralPath ("Env:" + $name) -ErrorAction SilentlyContinue
}
# GitHub-hosted runners can accumulate a PATH longer than cmd.exe can expand
# while VsDevCmd appends its own tool directories.  Start the batch file from
# a minimal system PATH; the resulting VS environment is then imported below.
$systemPath = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\Wbem"
# PowerShell 7 can retain both PATH and Path in its process environment.  cmd.exe
# may emit the stale, long variant after VsDevCmd finishes, so remove every
# spelling before starting the child shell.
$originalPathEntry = Get-ChildItem Env: |
    Where-Object { $_.Name -ceq "Path" } |
    Select-Object -First 1
if (-not $originalPathEntry) {
    $originalPathEntry = Get-ChildItem Env: |
        Where-Object { $_.Name -ieq "Path" } |
        Select-Object -First 1
}
$originalPath = if ($originalPathEntry) { $originalPathEntry.Value } else { "" }
Get-ChildItem Env: |
    Where-Object { $_.Name -ieq "Path" } |
    ForEach-Object { Remove-Item -LiteralPath ("Env:" + $_.Name) -ErrorAction SilentlyContinue }
[Environment]::SetEnvironmentVariable("Path", $systemPath, "Process")
$environment = & cmd.exe /d /s /c "set `"PATH=$systemPath`" && call `"$vsDevCmd`" -arch=$Arch -host_arch=$HostArch$vcVarsVersionArgument >nul && set"
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

# Keep command-line tools provided by the runner (git, pwsh, etc.) available
# to the build after importing the short VS PATH.  Writing one canonical Path
# here avoids the duplicate PATH/Path pair that broke the cmd.exe invocation.
if ($originalPath) {
    [Environment]::SetEnvironmentVariable("Path", ($env:Path + ";" + $originalPath), "Process")
}

[Environment]::SetEnvironmentVariable($sentinelName, "1", "Process")

$clPath = Get-Command cl.exe -ErrorAction Stop | Select-Object -ExpandProperty Source
Write-Host "VS toolchain: installation=$installationPath; PlatformToolset=$PlatformToolset; cl.exe=$clPath; VCToolsVersion=$env:VCToolsVersion"
