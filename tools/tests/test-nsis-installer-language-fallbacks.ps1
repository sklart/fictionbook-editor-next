<#
.SYNOPSIS
Проверяет fallback и стартовые переводы продуктовых строк для языков NSIS.

.DESCRIPTION
Сценарий генерирует include во временный каталог и убеждается, что каждый
дополнительный язык получает полный английский fallback без пустых строк, а
видимые строки компонентов и финальной страницы перекрываются переводами.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDirectory = Join-Path ([IO.Path]::GetTempPath()) "fbe-nsis-installer-fallbacks-$PID"
$outputPath = Join-Path $outputDirectory "EuropeanFallback.generated.nsh"

try {
    & (Join-Path $repoRoot "tools\localization\export-nsis-installer-fallbacks.ps1") -OutputPath $outputPath | Out-Host
    $text = Get-Content -Raw -LiteralPath $outputPath

    foreach ($language in @("GERMAN", "FRENCH", "SPANISH", "ITALIAN", "POLISH", "PORTUGUESE", "DUTCH", "CZECH", "BULGARIAN")) {
        if ($text -notmatch [regex]::Escape('!insertmacro FBE_DEFINE_ENGLISH_INSTALLER_FALLBACK ${LANG_' + $language + '}')) {
            throw "В generated fallback отсутствует язык NSIS: $language."
        }
    }

    # Видимые строки компонентов перекрываются переводами ниже. Здесь
    # проверяем именно технические строки, которые остаются английским fallback.
    foreach ($name in @("EnglishDict", "DESC_Main", "ErrCheckMSXMLVersion", "UacRetryInstaller")) {
        if ($text -notmatch ('LangString\s+' + [regex]::Escape($name) + '\s+\$\{LanguageId\}')) {
            throw "В generated fallback отсутствует продуктовая строка: $name."
        }
    }

    $expectedOverrides = @{
        "GERMAN" = "Installation abgeschlossen"
        "FRENCH" = "Installation terminée"
        "SPANISH" = "Instalación completada"
        "ITALIAN" = "Installazione completata"
        "POLISH" = "Instalacja zakończona"
        "PORTUGUESE" = "Instalação concluída"
        "DUTCH" = "Installatie voltooid"
        "CZECH" = "Instalace dokončena"
        "BULGARIAN" = "Инсталирането завърши"
    }
    foreach ($language in $expectedOverrides.Keys) {
        $pattern = 'LangString\s+FinishPageTitle\s+\$\{LANG_' + $language + '\}\s+"' + [regex]::Escape($expectedOverrides[$language]) + '"'
        if ($text -notmatch $pattern) {
            throw "В generated include отсутствует перевод финальной страницы для $language."
        }
    }

    Write-Host "Fallback и стартовые переводы продуктовых строк NSIS прошли проверку."
}
finally {
    Remove-Item -LiteralPath $outputDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
