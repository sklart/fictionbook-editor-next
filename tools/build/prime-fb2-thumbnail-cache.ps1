[CmdletBinding()]
param(
    [Parameter(Mandatory, Position = 0, ValueFromRemainingArguments)]
    [string[]]$FilePath,

    [Parameter()]
    [ValidateSet("Win32", "x64")]
    [string]$Platform = "x64",

    [switch]$SkipBuild,

    [switch]$NoExplorerRefresh
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Import-VsDevEnvironmentX64 {
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

function Ensure-PrimeThumbnailSmokeExe {
    param(
        [Parameter(Mandatory)]
        [string]$Platform
    )

    if ($Platform -eq "x64") {
        Import-VsDevEnvironmentX64
    } else {
        & (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64
    }

    $testDir = Join-Path $repoRoot "out\tests\fb2-shell-thumbnail-prime\$Platform"
    $testExe = Join-Path $testDir "fb2-shell-thumbnail-prime-smoke.exe"
    $sourceFile = Join-Path $repoRoot "tools\tests\fb2-shell-thumbnail-prime-smoke.cpp"

    if ($SkipBuild -and (Test-Path -LiteralPath $testExe -PathType Leaf)) {
        return $testExe
    }

    New-Item -ItemType Directory -Path $testDir -Force | Out-Null

    & cl.exe /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE /MD /W3 `
        "/Fo$($testDir)\\" `
        $sourceFile `
        "/link" "/SUBSYSTEM:CONSOLE" "ole32.lib" "shell32.lib" "gdi32.lib" "/OUT:$testExe" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    return $testExe
}

function Ensure-ShellNotifyType {
    if (-not ("FbeShellNotifyNative" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class FbeShellNotifyNative {
    [DllImport("shell32.dll")]
    public static extern void SHChangeNotify(uint wEventId, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@
    }
}

function Invoke-ShellAssociationRefresh {
    Ensure-ShellNotifyType
    [FbeShellNotifyNative]::SHChangeNotify(0x08000000u, 0x0000u, [IntPtr]::Zero, [IntPtr]::Zero)
}

$testExe = Ensure-PrimeThumbnailSmokeExe -Platform $Platform

$resolvedFiles = foreach ($path in $FilePath) {
    if ([string]::IsNullOrWhiteSpace($path)) {
        continue
    }

    $resolved = (Resolve-Path -LiteralPath $path -ErrorAction SilentlyContinue)?.Path
    if (-not $resolved) {
        throw "Не найден файл для обновления миниатюры: $path"
    }

    if (-not [IO.Path]::GetExtension($resolved).Equals(".fb2", [StringComparison]::OrdinalIgnoreCase)) {
        throw "Ожидался файл .fb2, а получено: $resolved"
    }

    $resolved
}

if ($resolvedFiles.Count -eq 0) {
    throw "Не передан ни один файл .fb2 для принудительного обновления миниатюры."
}

Write-Host "Принудительное обновление thumbnail cache для .fb2"
Write-Host "  Платформа утилиты: $Platform"
Write-Host "  Smoke-утилита: $testExe"

foreach ($resolved in $resolvedFiles) {
    Write-Host "  Файл: $resolved"
}

Push-Location (Split-Path -Path $testExe -Parent)
try {
    foreach ($resolved in $resolvedFiles) {
        & $testExe $resolved
        if ($LASTEXITCODE -ne 0) {
            throw "Не удалось обновить миниатюру для файла: $resolved"
        }
    }
}
finally {
    Pop-Location
}

if (-not $NoExplorerRefresh) {
    Invoke-ShellAssociationRefresh
    Write-Host "Проводник уведомлён об обновлении shell-кэша."
}

Write-Host "Принудительное обновление миниатюр завершено."
