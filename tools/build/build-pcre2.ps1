<#
.SYNOPSIS
Собирает статическую библиотеку PCRE2 и устанавливает её в build\pcre2\install.
#>

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [string]$PlatformToolset,

    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$sourceDir = Join-Path $repoRoot "third_party\pcre2"
$buildRoot = Join-Path $repoRoot "build\pcre2"
$buildDir = Join-Path $buildRoot $Configuration
$installDir = Join-Path $buildRoot "install\$Configuration"
$mutexName = "Global\FBeditor-build-pcre2-$Configuration"

function Get-CMakeVisualStudioGenerator {
    param(
        [string]$Toolset
    )

    if ($Toolset -eq "v143") {
        return "Visual Studio 17 2022"
    }

    return "Visual Studio 18 2026"
}

if (-not (Test-Path -LiteralPath $sourceDir)) {
    throw "Не найден каталог с исходниками PCRE2: $sourceDir"
}

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Не найден vswhere.exe. Установите Visual Studio с инструментами сборки C++."
}

$installationPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $installationPath) {
    throw "Не найдены инструменты сборки Visual Studio C++ для x86."
}

$cmake = (Get-Command cmake.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1)
if (-not $cmake) {
    $cmake = Get-ChildItem (Join-Path $installationPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin") `
        -Filter cmake.exe -Recurse -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName -First 1
}
if (-not $cmake) {
    throw "Не найден cmake.exe."
}

$generator = Get-CMakeVisualStudioGenerator -Toolset $PlatformToolset

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
New-Item -ItemType Directory -Path $installDir -Force | Out-Null

$buildMutex = [System.Threading.Mutex]::new($false, $mutexName)
$mutexAcquired = $false

function Invoke-ExternalCommand {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,

        [Parameter(Mandatory)]
        [string[]]$ArgumentList,

        [switch]$QuietOutput
    )

    if (-not $QuietOutput) {
        & $FilePath @ArgumentList
        return $LASTEXITCODE
    }

    $logPath = Join-Path ([IO.Path]::GetTempPath()) ("build-pcre2-{0}-{1}.log" -f $PID, [guid]::NewGuid().ToString("N"))
    try {
        & $FilePath @ArgumentList *> $logPath
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0) {
            Write-Warning "Сборка PCRE2 завершилась с ошибкой. Вывожу сохранённый лог:"
            Get-Content -LiteralPath $logPath
        }
        return $exitCode
    }
    finally {
        Remove-Item -LiteralPath $logPath -Force -ErrorAction SilentlyContinue
    }
}

function Copy-Pcre2OutputIfMissing {
    param(
        [Parameter(Mandatory)]
        [string]$FileName,

        [Parameter(Mandatory)]
        [string]$DestinationDirectory
    )

    $destinationPath = Join-Path $DestinationDirectory $FileName
    if (Test-Path -LiteralPath $destinationPath) {
        return
    }

    $sourcePath = Get-ChildItem -LiteralPath $buildDir -Recurse -File -Filter $FileName -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName -First 1
    if (-not $sourcePath) {
        throw "Сборка PCRE2 не создала обязательный файл $FileName."
    }

    New-Item -ItemType Directory -Path $DestinationDirectory -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
    Write-Host "PCRE2: восстановлен файл $destinationPath из $sourcePath"
}

try {
    $mutexAcquired = $buildMutex.WaitOne([TimeSpan]::FromMinutes(10))
    if (-not $mutexAcquired) {
        throw "Не удалось дождаться блокировки сборки PCRE2 за 10 минут."
    }

    $configureArgs = @(
        "-S", $sourceDir,
        "-B", $buildDir,
        "-G", $generator,
        "-A", "Win32",
        "-D", "CMAKE_INSTALL_PREFIX=$installDir",
        "-D", "CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>",
        "-D", "BUILD_SHARED_LIBS=OFF",
        "-D", "BUILD_STATIC_LIBS=ON",
        "-D", "PCRE2_BUILD_PCRE2_8=ON",
        "-D", "PCRE2_BUILD_PCRE2_16=OFF",
        "-D", "PCRE2_BUILD_PCRE2_32=OFF",
        "-D", "PCRE2_BUILD_PCRE2GREP=OFF",
        "-D", "PCRE2_BUILD_TESTS=OFF",
        "-D", "PCRE2_SUPPORT_UNICODE=ON",
        "-D", "PCRE2_SUPPORT_JIT=OFF"
    )

    $exitCode = Invoke-ExternalCommand -FilePath $cmake -ArgumentList $configureArgs -QuietOutput:$Quiet
    if ($exitCode -ne 0) {
        exit $exitCode
    }

    $buildArgs = @("--build", $buildDir, "--config", $Configuration, "--target", "pcre2-8-static", "pcre2-posix-static")
    $exitCode = Invoke-ExternalCommand -FilePath $cmake -ArgumentList $buildArgs -QuietOutput:$Quiet
    if ($exitCode -ne 0) {
        exit $exitCode
    }

    $installArgs = @("--install", $buildDir, "--config", $Configuration)
    $exitCode = Invoke-ExternalCommand -FilePath $cmake -ArgumentList $installArgs -QuietOutput:$Quiet
    if ($exitCode -ne 0) {
        exit $exitCode
    }

    Copy-Pcre2OutputIfMissing -FileName "pcre2.h" -DestinationDirectory (Join-Path $installDir "include")
    Copy-Pcre2OutputIfMissing -FileName "pcre2posix.h" -DestinationDirectory (Join-Path $installDir "include")
    Copy-Pcre2OutputIfMissing -FileName "pcre2-8-static.lib" -DestinationDirectory (Join-Path $installDir "lib")
    Copy-Pcre2OutputIfMissing -FileName "pcre2-posix-static.lib" -DestinationDirectory (Join-Path $installDir "lib")

    Write-Host "PCRE2 подготовлен в каталоге $installDir"
}
finally {
    if ($mutexAcquired) {
        $buildMutex.ReleaseMutex() | Out-Null
    }
    $buildMutex.Dispose()
}
