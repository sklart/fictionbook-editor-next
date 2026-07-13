<#
.SYNOPSIS
Генерирует английский fallback продуктовых строк для дополнительных языков NSIS.

.DESCRIPTION
NSIS требует определения каждого LangString в каждой подключённой языковой
таблице и не подставляет английскую строку автоматически. Скрипт читает
базовый English.nsh и создаёт generated include для девяти европейских языков
мастера установки. Стандартные страницы NSIS остаются локализованными штатными
переводами NSIS, а продуктовые строки безопасно используют английский fallback
до появления вычитанных переводов.
#>
[CmdletBinding()]
param(
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$englishPath = Join-Path $repoRoot "packaging\nsis\Installer\Localization\English.nsh"
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

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("; Английский fallback продуктовых строк для дополнительных языков NSIS.")
$lines.Add("; Сгенерировано из Localization\\English.nsh.")
$lines.Add("; Не редактируйте вручную: запускайте tools/localization/export-nsis-installer-fallbacks.ps1.")
$lines.Add("")
$lines.Add("!macro FBE_DEFINE_ENGLISH_INSTALLER_FALLBACK LanguageId")
foreach ($line in $stringLines) {
    $lines.Add(($line -replace '\$\{LANG_ENGLISH\}', '${LanguageId}'))
}
$lines.Add("!macroend")
$lines.Add("")

$languages = @("GERMAN", "FRENCH", "SPANISH", "ITALIAN", "POLISH", "PORTUGUESE", "DUTCH", "CZECH", "BULGARIAN")
foreach ($language in $languages) {
    $lines.Add("!insertmacro FBE_DEFINE_ENGLISH_INSTALLER_FALLBACK `${LANG_$language}")
}

New-Item -ItemType Directory -Path (Split-Path -Parent $OutputPath) -Force | Out-Null
[IO.File]::WriteAllText($OutputPath, ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))

Write-Host "English fallback для дополнительных языков NSIS подготовлен."
Write-Host "  Файл: $OutputPath"
Write-Host "  Строк: $($stringLines.Count)"
Write-Host "  Языков: $($languages.Count)"
