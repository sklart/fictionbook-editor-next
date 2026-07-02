<#
.SYNOPSIS
Проверяет, что релизные плагины и batch-конвертеры не зависят от динамического CRT.

.DESCRIPTION
Для чистой Windows 7 и portable-сценариев GUI-плагины FBE должны загружаться без
обязательной предварительной установки Visual C++ Redistributable. Этот тест
ловит возврат `MultiThreadedDLL` в Release-конфигурациях плагинов и batch-
утилит.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$projects = @(
    "src\export-html\ExportHTML.vcxproj",
    "src\export-docx\ExportDOCX.vcxproj",
    "src\export-epub\ExportEPUB.vcxproj",
    "src\import-epub\ImportEPUB.vcxproj",
    "src\import-epub\ImportEPUBLunaSVG.vcxproj",
    "src\export-docx\ExportDOCXBatch.vcxproj",
    "src\export-epub\ExportEPUBBatch.vcxproj",
    "src\import-epub\ImportEPUBBatch.vcxproj"
)

foreach ($relativePath in $projects) {
    $path = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Не найден проект для проверки CRT: $path"
    }

    $text = Get-Content -Raw -LiteralPath $path
    if ($text -match "<RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>") {
        throw "Release-проект $relativePath использует динамический CRT (/MD). Для Win7/portable ожидается MultiThreaded (/MT)."
    }
}

Write-Host "Проверка статического CRT для плагинов прошла успешно."
Write-Host "  Проектов: $($projects.Count)"
