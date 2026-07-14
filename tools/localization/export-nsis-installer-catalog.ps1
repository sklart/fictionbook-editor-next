<#
.SYNOPSIS
    Собирает редактируемый JSON-каталог продуктовых строк установщика NSIS.
.DESCRIPTION
    Извлекает общие LangString из уже вычитанных English/Russian/Ukrainian.nsh
    в Weblate-friendly JSON. Каталог является источником будущих переводов
    дополнительных языков и пока не меняет поведение установщика напрямую.
#>

[CmdletBinding()]
param(
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$localizationDirectory = Join-Path $repoRoot "packaging\nsis\Installer\Localization"

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot "localization\installer-ui\catalog.json"
}

function Get-NsisLanguageStrings {
    param(
        [Parameter(Mandatory = $true)][string]$Path
    )

    $result = [ordered]@{}
    $lines = Get-Content -LiteralPath $Path
    for ($index = 0; $index -lt $lines.Count; $index++) {
        $line = $lines[$index]
        if ($line -notmatch '^\s*LangString\s+(?<name>\S+)\s+\$\{LANG_[A-Z]+\}\s+"(?<value>.*)$') {
            continue
        }

        $name = $Matches.name
        $value = $Matches.value
        while (-not $value.EndsWith('"')) {
            $index++
            if ($index -ge $lines.Count) {
                throw "Незавершённая LangString '$name' в $Path."
            }
            $value += "`n" + $lines[$index]
        }

        $value = $value.Substring(0, $value.Length - 1)
        $result[$name] = $value
    }
    return $result
}

$sources = [ordered]@{
    "en-US" = Get-NsisLanguageStrings -Path (Join-Path $localizationDirectory "English.nsh")
    "ru-RU" = Get-NsisLanguageStrings -Path (Join-Path $localizationDirectory "Russian.nsh")
    "uk-UA" = Get-NsisLanguageStrings -Path (Join-Path $localizationDirectory "Ukrainian.nsh")
}

$strings = [ordered]@{}
foreach ($name in $sources['en-US'].Keys) {
    $translations = [ordered]@{}
    foreach ($locale in $sources.Keys) {
        if (-not $sources[$locale].Contains($name)) {
            throw "В $locale отсутствует NSIS-строка $name."
        }
        $translations[$locale] = $sources[$locale][$name]
    }

    $strings["nsis.$name"] = [ordered]@{
        source = $sources['en-US'][$name]
        component = "installer.nsi"
        nsisName = $name
        translations = $translations
    }
}

$catalog = [ordered]@{
    formatVersion = 1
    description = "Weblate-friendly каталог собственных строк установщика FictionBook Editor Next. Английский, русский и украинский импортированы из действующих NSIS-файлов; остальные языки добавляются после вычитки."
    targetLanguages = @("en-US", "ru-RU", "uk-UA", "de-DE", "fr-FR", "es-ES", "it-IT", "pl-PL", "pt-PT", "nl-NL", "cs-CZ", "bg-BG")
    strings = $strings
}

New-Item -ItemType Directory -Path (Split-Path -Parent $OutputPath) -Force | Out-Null
[IO.File]::WriteAllText($OutputPath, ($catalog | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))

Write-Host "Каталог строк установщика NSIS подготовлен."
Write-Host "  Файл: $OutputPath"
Write-Host "  Строк: $($strings.Count)"
Write-Host "  Базовых локалей: $($sources.Count)"
