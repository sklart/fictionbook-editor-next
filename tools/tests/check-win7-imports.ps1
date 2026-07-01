<#
.SYNOPSIS
Проверяет релизные EXE/DLL на прямые импорты WinAPI, отсутствующие в Windows 7.

.DESCRIPTION
Скрипт не доказывает полную поддержку Windows 7, но ловит самые опасные случаи:
если бинарник напрямую импортирует функцию, которой нет в Windows 7, загрузчик
Windows остановит программу ещё до старта. Отдельно выводятся предупреждения по
Universal CRT api-ms-win-crt-*.dll: они допустимы только при наличии UCRT/VC
Redistributable на целевой системе или при локальной поставке runtime DLL.
#>

[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [switch]$TreatCrtApiSetAsError
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outDir = Join-Path $repoRoot "out\$Configuration"

if (-not (Test-Path -LiteralPath $outDir)) {
    throw "Каталог релизных бинарников не найден: $outDir"
}

& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

$blockedPatterns = @(
    "GetSystemTimePreciseAsFileTime",
    "PathCch",
    "SetDefaultDllDirectories",
    "AddDllDirectory",
    "RemoveDllDirectory",
    "GetDpiForWindow",
    "GetDpiForMonitor",
    "SetProcessDpiAwareness",
    "SetProcessDpiAwarenessContext",
    "GetThreadDescription",
    "SetThreadDescription",
    "GetOverlappedResultEx"
)

$warningPatterns = @(
    "api-ms-win-crt-"
)

$files = Get-ChildItem -LiteralPath $outDir -File |
    Where-Object { $_.Extension -in ".exe", ".dll" } |
    Sort-Object Name

$errors = New-Object System.Collections.Generic.List[string]
$warnings = New-Object System.Collections.Generic.List[string]

foreach ($file in $files) {
    $imports = & dumpbin.exe /imports $file.FullName 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin /imports завершился с кодом $LASTEXITCODE для $($file.FullName)."
    }

    foreach ($pattern in $blockedPatterns) {
        $matches = $imports | Select-String -SimpleMatch $pattern
        foreach ($match in $matches) {
            $errors.Add(("{0}: {1}" -f $file.Name, $match.Line.Trim()))
        }
    }

    foreach ($pattern in $warningPatterns) {
        $matches = $imports | Select-String -SimpleMatch $pattern
        foreach ($match in $matches) {
            $warnings.Add(("{0}: {1}" -f $file.Name, $match.Line.Trim()))
        }
    }
}

if ($warnings.Count -gt 0) {
    Write-Warning "Найдены зависимости Universal CRT api-ms-win-crt-*.dll:"
    foreach ($warning in $warnings) {
        Write-Warning "  $warning"
    }
}

if ($TreatCrtApiSetAsError -and $warnings.Count -gt 0) {
    foreach ($warning in $warnings) {
        $errors.Add("UCRT dependency: $warning")
    }
}

if ($errors.Count -gt 0) {
    Write-Host "Найдены импорты, несовместимые с Windows 7:"
    foreach ($errorItem in $errors) {
        Write-Host "  $errorItem"
    }
    throw "Проверка Win7-совместимости импортов завершилась с ошибкой."
}

Write-Host "Проверка Win7-совместимости импортов прошла успешно."
