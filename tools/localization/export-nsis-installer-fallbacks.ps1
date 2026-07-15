<#
.SYNOPSIS
Генерирует fallback и стартовые европейские переводы продуктовых строк NSIS.

.DESCRIPTION
NSIS требует определения каждого LangString в каждой подключённой языковой
таблице и не подставляет английскую строку автоматически. Скрипт читает
базовый English.nsh и создаёт generated include для девяти европейских языков
мастера установки. Затем поверх английского fallback накладываются стартовые
переводы видимых продуктовых строк из localization\installer-ui\european-overrides.json.
Файл JSON остаётся удобной точкой последующей вычитки через Weblate.
#>
[CmdletBinding()]
param(
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$englishPath = Join-Path $repoRoot "packaging\nsis\Installer\Localization\English.nsh"
$overridePath = Join-Path $repoRoot "localization\installer-ui\european-overrides.json"
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot "packaging\nsis\Installer\Generated\EuropeanFallback.generated.nsh"
}

$englishLines = Get-Content -LiteralPath $englishPath
$stringLines = New-Object System.Collections.Generic.List[string]
for ($index = 0; $index -lt $englishLines.Count; $index++) {
    $line = $englishLines[$index]
    if ($line -notmatch '^\s*(LicenseLangString|LangString)\s+') {
        continue
    }

    # В NSIS строка может продолжаться на следующей строке обратной косой
    # чертой. Сохраняем такой блок целиком, иначе следующая директива будет
    # ошибочно воспринята как часть незакрытого LangString.
    $stringLines.Add($line)
    while ($line.TrimEnd().EndsWith('\')) {
        $index++
        if ($index -ge $englishLines.Count) {
            throw "Незавершённая многострочная строка NSIS в $englishPath."
        }

        $line = $englishLines[$index]
        $stringLines.Add($line)
    }
}
if ($stringLines.Count -eq 0) {
    throw "В $englishPath не найдены English LangString для fallback."
}

if (-not (Test-Path -LiteralPath $overridePath -PathType Leaf)) {
    throw "Не найден каталог стартовых переводов NSIS: $overridePath"
}

$overrides = Get-Content -Raw -LiteralPath $overridePath | ConvertFrom-Json -Depth 20
$languageIds = [ordered]@{
    "de-DE" = "GERMAN"
    "fr-FR" = "FRENCH"
    "es-ES" = "SPANISH"
    "it-IT" = "ITALIAN"
    "pl-PL" = "POLISH"
    "pt-PT" = "PORTUGUESE"
    "nl-NL" = "DUTCH"
    "cs-CZ" = "CZECH"
    "bg-BG" = "BULGARIAN"
}

# Стартовые переводы не должны дублировать fallback в одной таблице NSIS:
# предупреждение 6030 не влияет на файл, но скрывает реальные предупреждения сборки.
$overrideNames = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($locale in $languageIds.Keys) {
    $localeOverrides = $overrides.locales.PSObject.Properties[$locale]
    if ($null -eq $localeOverrides) {
        throw "В $overridePath отсутствует секция $locale."
    }
    foreach ($entry in $localeOverrides.Value.PSObject.Properties) {
        [void]$overrideNames.Add($entry.Name)
    }
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("; Английский fallback продуктовых строк для дополнительных языков NSIS.")
$lines.Add("; Сгенерировано из Localization\\English.nsh.")
$lines.Add("; Не редактируйте вручную: запускайте tools/localization/export-nsis-installer-fallbacks.ps1.")
$lines.Add("")
$lines.Add("!macro FBE_DEFINE_ENGLISH_INSTALLER_FALLBACK LanguageId")
foreach ($line in $stringLines) {
    if ($line -match '^\s*LangString\s+(?<name>\S+)\s+' -and $overrideNames.Contains($Matches['name'])) {
        continue
    }
    $lines.Add(($line -replace '\$\{LANG_ENGLISH\}', '${LanguageId}'))
}
$lines.Add("!macroend")
$lines.Add("")

$languages = @("GERMAN", "FRENCH", "SPANISH", "ITALIAN", "POLISH", "PORTUGUESE", "DUTCH", "CZECH", "BULGARIAN")
foreach ($language in $languages) {
    $lines.Add("!insertmacro FBE_DEFINE_ENGLISH_INSTALLER_FALLBACK `${LANG_$language}")
}

$lines.Add("")
$lines.Add("; Стартовые переводы видимых product strings из localization\\installer-ui\\european-overrides.json.")
foreach ($locale in $languageIds.Keys) {
    $localeOverrides = $overrides.locales.PSObject.Properties[$locale]
    if ($null -eq $localeOverrides) {
        throw "В $overridePath отсутствует секция $locale."
    }

    foreach ($entry in $localeOverrides.Value.PSObject.Properties) {
        $name = $entry.Name
        $value = [string]$entry.Value
        $matchingSourceLines = @($stringLines | Where-Object {
            $_ -match ('^\s*LangString\s+' + [regex]::Escape($name) + '\s+')
        })
        if ($matchingSourceLines.Count -eq 0) {
            throw "Стартовый перевод $locale ссылается на неизвестную NSIS-строку $name."
        }
        $lines.Add(('LangString {0} ${{LANG_{1}}} "{2}"' -f $name, $languageIds[$locale], $value))
    }
}

New-Item -ItemType Directory -Path (Split-Path -Parent $OutputPath) -Force | Out-Null
[IO.File]::WriteAllText($OutputPath, ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))

Write-Host "English fallback для дополнительных языков NSIS подготовлен."
Write-Host "  Файл: $OutputPath"
Write-Host "  Строк: $($stringLines.Count)"
Write-Host "  Языков: $($languages.Count)"
