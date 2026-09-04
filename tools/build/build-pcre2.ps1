<#
.SYNOPSIS
Собирает статическую библиотеку PCRE2 и устанавливает её в build\pcre2\install.
#>

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [string]$PlatformToolset,

    [switch]$ReusePreparedPcre2,

    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$sourceDir = Join-Path $repoRoot "third_party\pcre2"
$buildRoot = Join-Path $repoRoot "build\pcre2"
$installDir = Join-Path $buildRoot "install\$Configuration"

function Get-CMakeVisualStudioGenerator {
    param(
        [string]$Toolset,

        [string]$VisualStudioProductLineVersion
    )

    # Генератор определяется по реально найденной Visual Studio: v143 может
    # быть установлен как в VS 2022, так и в VS 2026.
    if ($VisualStudioProductLineVersion -eq "18") {
        return "Visual Studio 18 2026"
    }

    if ($VisualStudioProductLineVersion -eq "17") {
        return "Visual Studio 17 2022"
    }

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

$vswhereArguments = @("-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64")
if ($PlatformToolset -eq "v143") {
    # v143 release должен использовать VS 2022, а не случайный newest instance.
    $vswhereArguments += @("-version", "[17.0,18.0)")
}

$installationPath = & $vswhere @vswhereArguments -property installationPath
if (-not $installationPath) {
    throw "Не найдены инструменты сборки Visual Studio C++ для x86."
}

$visualStudioProductLineVersion = & $vswhere @vswhereArguments -property catalog_productLineVersion

# Приоритет у CMake из той же Visual Studio, которую выбрал vswhere. Это
# исключает случайный CMake из Python/другой SDK, не знающий генератор VS.
$cmake = Get-ChildItem (Join-Path $installationPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin") `
    -Filter cmake.exe -Recurse -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty FullName -First 1
if (-not $cmake) {
    $cmake = (Get-Command cmake.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1)
}
if (-not $cmake) {
    throw "Не найден cmake.exe."
}

$generator = Get-CMakeVisualStudioGenerator -Toolset $PlatformToolset -VisualStudioProductLineVersion $visualStudioProductLineVersion
$generatorSuffix = if ($generator -eq "Visual Studio 17 2022") { "vs2022" } else { "vs2026" }
$toolsetSuffix = if ($PlatformToolset) { $PlatformToolset } else { "default-toolset" }
$buildDir = Join-Path $buildRoot "$Configuration-$generatorSuffix-$toolsetSuffix"
$metadataPath = Join-Path $installDir "fbe-pcre2-fingerprint.json"
$mutexName = "Global\FBeditor-build-pcre2-$Configuration-$generatorSuffix-$toolsetSuffix"
$pcre2Commit = (git -C $sourceDir rev-parse HEAD).Trim()
$pcre2CodeUnitWidth = 16
$pcre2Unicode = $true
$pcre2Jit = $true

Write-Host "PCRE2: конфигурация $Configuration"
Write-Host "PCRE2: PlatformToolset = $PlatformToolset"
Write-Host "PCRE2: CMake generator = $generator"
Write-Host "PCRE2: cmake.exe = $cmake"
Write-Host "PCRE2: каталог сборки = $buildDir"
Write-Host "PCRE2: каталог установки = $installDir"

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

    $processStartInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $processStartInfo.FileName = $FilePath
    $processStartInfo.UseShellExecute = $false
    $processStartInfo.RedirectStandardOutput = $true
    $processStartInfo.RedirectStandardError = $true

    foreach ($argument in $ArgumentList) {
        [void]$processStartInfo.ArgumentList.Add($argument)
    }

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $processStartInfo

    try {
        [void]$process.Start()
        $standardOutput = $process.StandardOutput.ReadToEnd()
        $standardError = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        $exitCode = [int]$process.ExitCode
    }
    finally {
        $process.Dispose()
    }

    if ((-not $QuietOutput) -or $exitCode -ne 0) {
        if (-not [string]::IsNullOrWhiteSpace($standardOutput)) {
            $standardOutput -split "`r?`n" | Where-Object { $_ } | ForEach-Object { Write-Host $_ }
        }
        if (-not [string]::IsNullOrWhiteSpace($standardError)) {
            $standardError -split "`r?`n" | Where-Object { $_ } | ForEach-Object { Write-Host $_ }
        }
    }

    if ($exitCode -ne 0) {
        Write-Warning "Сборка PCRE2 завершилась с ошибкой."
    }

    return $exitCode
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

function Copy-Pcre2RequiredFile {
    param(
        [Parameter(Mandatory)]
        [string]$FileName,

        [Parameter(Mandatory)]
        [string]$DestinationDirectory,

        [Parameter(Mandatory)]
        [string[]]$CandidatePaths
    )

    $destinationPath = Join-Path $DestinationDirectory $FileName
    foreach ($candidatePath in $CandidatePaths) {
        if (Test-Path -LiteralPath $candidatePath) {
            New-Item -ItemType Directory -Path $DestinationDirectory -Force | Out-Null
            Copy-Item -LiteralPath $candidatePath -Destination $destinationPath -Force
            Write-Host "PCRE2: разложен файл $destinationPath из $candidatePath"
            return
        }
    }

    Copy-Pcre2OutputIfMissing -FileName $FileName -DestinationDirectory $DestinationDirectory
}

function Assert-Pcre2Prepared {
    $requiredPaths = @(
        (Join-Path $installDir "include\pcre2.h"),
        (Join-Path $installDir "lib\pcre2-16-static.lib")
    )

    $missingPaths = @($requiredPaths | Where-Object { -not (Test-Path -LiteralPath $_) })
    if ($missingPaths.Count -eq 0) {
        return
    }

    Write-Warning "PCRE2 не разложен полностью. Найденные файлы build\\pcre2:"
    Get-ChildItem -LiteralPath $buildRoot -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "pcre2*.h" -or $_.Name -like "pcre2*.lib" } |
        Select-Object -ExpandProperty FullName |
        ForEach-Object { Write-Warning "  $_" }

    throw ("PCRE2 не подготовлен, отсутствуют обязательные файлы: {0}" -f ($missingPaths -join "; "))
}

function Test-PreparedPcre2Fingerprint {
    Assert-Pcre2Prepared
    if (-not (Test-Path -LiteralPath $metadataPath -PathType Leaf)) {
        throw "PCRE2 cache не содержит fingerprint: $metadataPath"
    }
    $metadata = Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json
    if ($metadata.configuration -ne $Configuration -or $metadata.generator -ne $generator -or
        $metadata.platformToolset -ne $PlatformToolset -or $metadata.commit -ne $pcre2Commit -or
        $metadata.codeUnitWidth -ne $pcre2CodeUnitWidth -or $metadata.unicode -ne $pcre2Unicode -or
        $metadata.jit -ne $pcre2Jit) {
        throw "PCRE2 cache имеет несовместимый fingerprint (configuration/generator/toolset/commit/width/unicode/jit)."
    }
}

try {
    $mutexAcquired = $buildMutex.WaitOne([TimeSpan]::FromMinutes(10))
    if (-not $mutexAcquired) {
        throw "Не удалось дождаться блокировки сборки PCRE2 за 10 минут."
    }

    if ($ReusePreparedPcre2) {
        Test-PreparedPcre2Fingerprint
        Write-Host "PCRE2: exact validated cache hit; CMake configure/build/install пропущены."
        return
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
        "-D", "PCRE2_BUILD_PCRE2_8=OFF",
        "-D", "PCRE2_BUILD_PCRE2_16=ON",
        "-D", "PCRE2_BUILD_PCRE2_32=OFF",
        "-D", "PCRE2_BUILD_PCRE2GREP=OFF",
        "-D", "PCRE2_BUILD_TESTS=OFF",
        "-D", "PCRE2_SUPPORT_UNICODE=ON",
        "-D", "PCRE2_SUPPORT_JIT=ON"
    )
    if ($PlatformToolset) {
        $configureArgs += @("-T", $PlatformToolset)
    }

    Write-Host "PCRE2: конфигурация CMake"
    $exitCode = Invoke-ExternalCommand -FilePath $cmake -ArgumentList $configureArgs -QuietOutput:$Quiet
    if ($exitCode -ne 0) {
        throw "Конфигурация PCRE2 завершилась с кодом $exitCode."
    }

    foreach ($target in @("pcre2-16-static")) {
        Write-Host "PCRE2: сборка цели $target"
        $buildArgs = @("--build", $buildDir, "--config", $Configuration, "--target", $target)
        $exitCode = Invoke-ExternalCommand -FilePath $cmake -ArgumentList $buildArgs -QuietOutput:$Quiet
        if ($exitCode -ne 0) {
            throw "Сборка цели PCRE2 $target завершилась с кодом $exitCode."
        }
    }

    Write-Host "PCRE2: установка в $installDir"
    $installArgs = @("--install", $buildDir, "--config", $Configuration)
    $exitCode = Invoke-ExternalCommand -FilePath $cmake -ArgumentList $installArgs -QuietOutput:$Quiet
    if ($exitCode -ne 0) {
        throw "Установка PCRE2 завершилась с кодом $exitCode."
    }

    Copy-Pcre2RequiredFile -FileName "pcre2.h" -DestinationDirectory (Join-Path $installDir "include") -CandidatePaths @(
        (Join-Path $buildDir "interface\pcre2.h"),
        (Join-Path $sourceDir "src\pcre2.h"),
        (Join-Path $sourceDir "pcre2.h")
    )
    Copy-Pcre2RequiredFile -FileName "pcre2-16-static.lib" -DestinationDirectory (Join-Path $installDir "lib") -CandidatePaths @(
        (Join-Path $buildDir "$Configuration\pcre2-16-static.lib"),
        (Join-Path $buildDir "$Configuration\pcre2-16-staticd.lib"),
        (Join-Path $buildDir "pcre2-16-static.lib")
    )

    Assert-Pcre2Prepared
    [ordered]@{
        configuration = $Configuration
        generator = $generator
        platformToolset = $PlatformToolset
        commit = $pcre2Commit
        codeUnitWidth = $pcre2CodeUnitWidth
        unicode = $pcre2Unicode
        jit = $pcre2Jit
    } |
        ConvertTo-Json | Set-Content -LiteralPath $metadataPath -Encoding UTF8
    Write-Host "PCRE2 подготовлен в каталоге $installDir"
}
finally {
    if ($mutexAcquired) {
        $buildMutex.ReleaseMutex() | Out-Null
    }
    $buildMutex.Dispose()
}
