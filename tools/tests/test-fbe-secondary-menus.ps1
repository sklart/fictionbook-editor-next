<#
.SYNOPSIS
Проверяет JSON/generation/подключение малых меню FBE.

.DESCRIPTION
Тест страхует перенос `IDR_DOCUMENT_TREE` и `IDR_TOOLBAR_MENU` на JSON→generated
pipeline: проверяет JSON-каталог на 12 языков, regenerated `.rc2`, наличие
обоих MENU-ресурсов и подключение generated-файла вместо ручных блоков в
русской и украинской `FBE.rc`.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$catalogPath = Join-Path $repoRoot "localization\app-ui\fbe-secondary-menus.json"
$catalog = Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json -Depth 40
$expectedLanguages = @('en-US','ru-RU','uk-UA','de-DE','fr-FR','es-ES','it-IT','pl-PL','pt-PT','nl-NL','cs-CZ','bg-BG')
if ((Compare-Object -ReferenceObject $expectedLanguages -DifferenceObject @($catalog.targetLanguages)).Count -ne 0) {
    throw "Набор языков каталога малых меню FBE не совпадает с ожидаемым."
}
$entries = @($catalog.strings.PSObject.Properties)
if ($entries.Count -ne 7) { throw "Ожидалось 7 пунктов малых меню FBE, получено $($entries.Count)." }
foreach ($entry in $entries) {
    foreach ($language in $expectedLanguages) {
        $translation = $entry.Value.translations.PSObject.Properties[$language]
        if (-not $translation -or [string]::IsNullOrWhiteSpace([string]$translation.Value)) {
            throw "У пункта $($entry.Name) нет перевода для $language."
        }
    }
}

& (Join-Path $repoRoot "tools\localization\update-fbe-secondary-menu-resources.ps1")
if ($LASTEXITCODE -ne 0) { throw "update-fbe-secondary-menu-resources.ps1 завершился с кодом $LASTEXITCODE." }

$files = @(
    @{ Language = "ru-RU"; Rc = Join-Path $repoRoot "src\locales\res_rus\FBE.rc"; Generated = Join-Path $repoRoot "src\locales\res_rus\FBESecondaryMenus.generated.rc2" },
    @{ Language = "uk-UA"; Rc = Join-Path $repoRoot "src\locales\res_ukr\FBE.rc"; Generated = Join-Path $repoRoot "src\locales\res_ukr\FBESecondaryMenus.generated.rc2" }
)
$cp1251 = [Text.Encoding]::GetEncoding(1251)
$utf16 = [Text.UnicodeEncoding]::new($false, $true)
foreach ($file in $files) {
    $rcText = [IO.File]::ReadAllText($file.Rc, $cp1251)
    if ($rcText -notmatch '#include\s+"FBESecondaryMenus\.generated\.rc2"') {
        throw "В $($file.Language) FBE.rc не подключён FBESecondaryMenus.generated.rc2."
    }
    foreach ($resource in @('IDR_DOCUMENT_TREE','IDR_TOOLBAR_MENU')) {
        if ($rcText -match "(?m)^\s*$resource\s+MENU\s*$") {
            throw "В $($file.Language) FBE.rc остался ручной $resource MENU."
        }
    }
    $bytes = [IO.File]::ReadAllBytes($file.Generated)
    if ($bytes.Length -lt 2 -or $bytes[0] -ne 0xFF -or $bytes[1] -ne 0xFE) {
        throw "Generated-файл малых меню должен быть UTF-16 LE BOM: $($file.Generated)"
    }
    $generatedText = [IO.File]::ReadAllText($file.Generated, $utf16)
    foreach ($resource in @('IDR_DOCUMENT_TREE MENU','IDR_TOOLBAR_MENU MENU','ID_DT_VIEW','ID_DT_DELETE','ID_TOOLS_CUSTOMIZE')) {
        if ($generatedText -notmatch [regex]::Escape($resource)) {
            throw "В generated малых меню $($file.Language) нет $resource."
        }
    }
}

Write-Host "Малые меню FBE прошли проверку."
Write-Host "  Каталог: $catalogPath"
Write-Host "  Пунктов: $($entries.Count)"
Write-Host "  Языков: $($expectedLanguages.Count)"