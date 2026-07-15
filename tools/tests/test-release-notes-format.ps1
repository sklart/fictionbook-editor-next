<#
.SYNOPSIS
Проверяет формат человекочитаемых заметок GitHub Release.

.DESCRIPTION
Release notes должны быть удобны для чтения на GitHub: один пункт списка — одна
физическая строка Markdown, без ручных переносов внутри bullet. Генератор также
должен добавлять ссылку на версионируемый `docs/release-notes/<version>.md` при
запуске в GitHub Actions окружении.
#>
[CmdletBinding()]
param(
    [string]$Version
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($Version)) {
    $versionHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\version.h")
    $versionMatch = [regex]::Match($versionHeader, '#define\s+FBE_VERSION_STRING\s+"(?<version>\d+\.\d+\.\d+)"')
    if (-not $versionMatch.Success) {
        throw "В src\\version.h не найден FBE_VERSION_STRING."
    }
    $Version = $versionMatch.Groups["version"].Value
}
$notesPath = Join-Path $repoRoot "docs\release-notes\$Version.md"
if (-not (Test-Path -LiteralPath $notesPath)) {
    throw "Не найден curated release notes файл: $notesPath"
}

$lines = Get-Content -LiteralPath $notesPath
$insideFence = $false
for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    if ($line -match '^\s*```') {
        $insideFence = -not $insideFence
        continue
    }
    if ($insideFence) {
        continue
    }
    if ($line -match '^\s{2,}\S' -and $i -gt 0 -and $lines[$i - 1] -match '^\s*[-*+]\s+') {
        throw "В $notesPath найден ручной перенос внутри пункта списка на строке $($i + 1)."
    }
}

$outputPath = Join-Path ([IO.Path]::GetTempPath()) "fbe-release-notes-$Version-$PID.md"
$oldRepository = $env:GITHUB_REPOSITORY
$oldRefName = $env:GITHUB_REF_NAME
try {
    $env:GITHUB_REPOSITORY = "sklart/fictionbook-editor-next"
    $env:GITHUB_REF_NAME = "v$Version"
    & (Join-Path $repoRoot "tools\build\new-release-notes.ps1") -Version $Version -OutputPath $outputPath | Out-Host

    $generated = Get-Content -Raw -LiteralPath $outputPath
    $expectedSourceUrl = "https://github.com/sklart/fictionbook-editor-next/blob/v$Version/docs/release-notes/$Version.md"
    if ($generated -notmatch [regex]::Escape($expectedSourceUrl)) {
        throw "Сгенерированные release notes не содержат ссылку на исходник заметок: $expectedSourceUrl"
    }
}
finally {
    $env:GITHUB_REPOSITORY = $oldRepository
    $env:GITHUB_REF_NAME = $oldRefName
    Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
}

Write-Host "Проверка формата release notes прошла успешно."
Write-Host "  Версия: $Version"
