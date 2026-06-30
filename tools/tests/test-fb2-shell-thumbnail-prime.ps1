[CmdletBinding()]
param(
    [ValidateSet("Win32", "x64")]
    [string]$Platform = "x64",

    [string]$FilePath = ""
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

if ($Platform -eq "x64") {
    Import-VsDevEnvironmentX64
} else {
    & (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64
}

$testDir = Join-Path $repoRoot "out\tests\fb2-shell-thumbnail-prime\$Platform"
$testExe = Join-Path $testDir "fb2-shell-thumbnail-prime-smoke.exe"
$sourceFile = Join-Path $PSScriptRoot "fb2-shell-thumbnail-prime-smoke.cpp"
$fixture = if ([string]::IsNullOrWhiteSpace($FilePath)) {
    Join-Path $repoRoot "tools\tests\fb2-cover-visible-smoke.fb2"
} elseif ([System.IO.Path]::IsPathRooted($FilePath)) {
    $FilePath
} else {
    Join-Path $repoRoot $FilePath
}

if (-not (Test-Path -LiteralPath $fixture -PathType Leaf)) {
    throw "Не найден файл для проверки: $fixture"
}

$fixture = (Resolve-Path -LiteralPath $fixture).Path

New-Item -ItemType Directory -Path $testDir -Force | Out-Null

& cl.exe /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE /MD /W3 `
    "/Fo$($testDir)\\" `
    $sourceFile `
    "/link" "/SUBSYSTEM:CONSOLE" "ole32.lib" "shell32.lib" "gdi32.lib" "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Push-Location $testDir
try {
    & $testExe $fixture
    if ($LASTEXITCODE -ne 0) {
        throw "Prime shell thumbnail smoke завершился с кодом $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

Write-Host "Prime shell thumbnail smoke прошёл успешно."
