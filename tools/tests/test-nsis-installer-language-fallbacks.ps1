<#
.SYNOPSIS
Проверяет fallback продуктовых строк для дополнительных языков NSIS.

.DESCRIPTION
Сценарий генерирует include во временный каталог и убеждается, что каждый
дополнительный язык получает полный английский fallback вместо пустых строк.
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

    foreach ($name in @("Main", "LanguagePacksGroup", "FinishPageTitle", "FinishPageText", "FinishPageRunText", "UninstAskSettings")) {
        if ($text -notmatch ('LangString\s+' + [regex]::Escape($name) + '\s+\$\{LanguageId\}')) {
            throw "В generated fallback отсутствует продуктовая строка: $name."
        }
    }

    Write-Host "Fallback продуктовых строк для дополнительных языков NSIS прошёл проверку."
}
finally {
    Remove-Item -LiteralPath $outputDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
