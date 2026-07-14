<#
.SYNOPSIS
    Проверяет экспортируемый каталог пользовательских строк установщика NSIS.
.DESCRIPTION
    Контролирует, что английская, русская и украинская версии содержат одинаковый
    набор product LangString и могут служить безопасной базой для Weblate.
#>

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDirectory = Join-Path ([IO.Path]::GetTempPath()) "fbe-nsis-installer-catalog-$PID"
$outputPath = Join-Path $outputDirectory "catalog.json"

try {
    & (Join-Path $repoRoot "tools\localization\export-nsis-installer-catalog.ps1") -OutputPath $outputPath | Out-Host
    $catalog = Get-Content -Raw -LiteralPath $outputPath | ConvertFrom-Json -Depth 20
    $entries = @($catalog.strings.PSObject.Properties)
    if ($entries.Count -lt 60) {
        throw "В каталоге установщика слишком мало строк: $($entries.Count)."
    }

    foreach ($locale in @("en-US", "ru-RU", "uk-UA")) {
        foreach ($entry in $entries) {
            $value = $entry.Value.translations.PSObject.Properties[$locale].Value
            if ([string]::IsNullOrWhiteSpace([string]$value)) {
                throw "Пустой перевод $locale для $($entry.Name)."
            }
        }
    }

    Write-Host "Каталог строк установщика NSIS прошёл проверку."
    Write-Host "  Строк: $($entries.Count)"
} finally {
    Remove-Item -LiteralPath $outputDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
