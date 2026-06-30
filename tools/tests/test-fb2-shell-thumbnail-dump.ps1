[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Fb2Path,

    [string]$OutputBmp = "",

    [int]$Size = 256,

    [switch]$ForceExtraction,

    [switch]$ExtractDoNotCache,

    [switch]$ExtractInProc,

    [ValidateSet("Win32", "x64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$invocationLocation = (Get-Location).Path

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

if ($Platform -eq "x64") {
    Import-VsDevEnvironmentX64
} else {
    & (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64
}

$testDir = Join-Path $repoRoot "out\tests\fb2-shell-thumbnail-dump\$Platform"
$testExe = Join-Path $testDir "fb2-shell-thumbnail-dump.exe"
$fb2InputPath = if ([System.IO.Path]::IsPathRooted($Fb2Path)) {
    $Fb2Path
} else {
    Join-Path $invocationLocation $Fb2Path
}

$fb2InputPath = (Resolve-Path -LiteralPath $fb2InputPath -ErrorAction Stop).Path

$outputPath = if ([string]::IsNullOrWhiteSpace($OutputBmp)) {
    Join-Path $testDir "dump.bmp"
} elseif ([System.IO.Path]::IsPathRooted($OutputBmp)) {
    $OutputBmp
} else {
    Join-Path $invocationLocation $OutputBmp
}

$outputDirectory = Split-Path -Parent $outputPath
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

$outputPath = [System.IO.Path]::GetFullPath($outputPath)
$flags = 0
if ($ForceExtraction) {
    $flags = $flags -bor 0x20
}
if ($ExtractDoNotCache) {
    $flags = $flags -bor 0x40
}
if ($ExtractInProc) {
    $flags = $flags -bor 0x80
}

New-Item -ItemType Directory -Path $testDir -Force | Out-Null

& cl.exe /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE /MD /W3 `
    "/Fo$($testDir)\\" `
    (Join-Path $PSScriptRoot "fb2-shell-thumbnail-dump.cpp") `
    "/link" "/SUBSYSTEM:CONSOLE" "ole32.lib" "shell32.lib" "gdi32.lib" "user32.lib" "msimg32.lib" "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Push-Location $testDir
try {
    & $testExe $fb2InputPath $outputPath $Size $flags
    if ($LASTEXITCODE -ne 0) {
        throw "Shell thumbnail dump завершился с кодом $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

Write-Host "Shell thumbnail dump прошёл успешно."
Write-Host "Файл дампа: $outputPath"
