[CmdletBinding()]
param(
    [string]$FilePath = "",
    [ValidateSet("Win32", "x64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Import-VsEnvironmentForTest {
    param(
        [Parameter(Mandatory)]
        [ValidateSet("Win32", "x64")]
        [string]$Platform
    )

    if ($Platform -eq "Win32") {
        & (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64
        return
    }

    $sentinelName = "FBE_VSDEV_x64_x64_INITIALIZED"
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
        throw "Не найдены инструменты сборки Visual Studio C++ для x64."
    }

    $vsDevCmd = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"
    $environment = & cmd.exe /d /s /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Не удалось инициализировать среду сборки Visual Studio для x64."
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
}

Import-VsEnvironmentForTest -Platform $Platform

$testDir = Join-Path $repoRoot ("out\tests\fb2-shell-properties\" + $Platform)
$testExe = Join-Path $testDir "fb2-shell-properties-query.exe"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

if ([string]::IsNullOrWhiteSpace($FilePath)) {
    $FilePath = Join-Path $PSScriptRoot "fb2-metadata-smoke.fb2"
} else {
    $FilePath = (Resolve-Path -LiteralPath $FilePath).Path
}

& cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /MD `
    "/Fo$($testDir)\\" `
    (Join-Path $PSScriptRoot "fb2-shell-properties-query.cpp") `
    "/link" "/SUBSYSTEM:CONSOLE" "ole32.lib" "shell32.lib" "propsys.lib" "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $testExe $FilePath
if ($LASTEXITCODE -ne 0) {
    throw "Запрос shell-свойств завершился с кодом $LASTEXITCODE."
}
