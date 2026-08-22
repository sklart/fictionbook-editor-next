<#
.SYNOPSIS
Проверяет JSON/generation/подключение малых DIALOGEX-диалогов FBE.

.DESCRIPTION
Тест страхует перенос малых DIALOGEX-диалогов FBE на JSON→generated pipeline:
проверяет JSON-каталог на 12 языков, regenerated `.rc2`, наличие DIALOGEX-
ресурсов и подключение generated-файла вместо ручных блоков в русской и
украинской `FBE.rc`.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$catalogPath = Join-Path $repoRoot "localization\app-ui\fbe-small-dialogs.json"
$catalog = Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json -Depth 40
$expectedLanguages = @('en-US','ru-RU','uk-UA','de-DE','fr-FR','es-ES','it-IT','pl-PL','pt-PT','nl-NL','cs-CZ','bg-BG')
if ((Compare-Object -ReferenceObject $expectedLanguages -DifferenceObject @($catalog.targetLanguages)).Count -ne 0) { throw "Набор языков каталога малых диалогов FBE не совпадает с ожидаемым." }
$entries = @($catalog.strings.PSObject.Properties)
$expectedEntryCount = 164
if ($entries.Count -ne $expectedEntryCount) { throw "Ожидалось $expectedEntryCount строк малых диалогов FBE, получено $($entries.Count)." }
foreach ($entry in $entries) { foreach ($language in $expectedLanguages) { $translation = $entry.Value.translations.PSObject.Properties[$language]; if (-not $translation -or [string]::IsNullOrWhiteSpace([string]$translation.Value)) { throw "У строки $($entry.Name) нет перевода для $language." } } }
& (Join-Path $repoRoot "tools\localization\update-fbe-small-dialog-resources.ps1")
if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw "update-fbe-small-dialog-resources.ps1 завершился с кодом $LASTEXITCODE." }
$files = @(
    @{ Language = "ru-RU"; Rc = Join-Path $repoRoot "src\locales\res_rus\FBE.rc"; Generated = Join-Path $repoRoot "src\locales\res_rus\FBESmallDialogs.generated.rc2" },
    @{ Language = "uk-UA"; Rc = Join-Path $repoRoot "src\locales\res_ukr\FBE.rc"; Generated = Join-Path $repoRoot "src\locales\res_ukr\FBESmallDialogs.generated.rc2" }
)
$cp1251=[Text.Encoding]::GetEncoding(1251)
$utf16=[Text.UnicodeEncoding]::new($false,$true)
$baseRcText = [IO.File]::ReadAllText((Join-Path $repoRoot "src\fbe\FBE.rc"),$cp1251)
if ($baseRcText -notmatch 'COMBOBOX\s+IDC_LANG,\d+,\d+,1[0-9]{2},\d+') {
    throw "Базовый FBE.rc должен оставлять достаточно широкое поле IDC_LANG для 'Определяется системой'."
}
$settingsDialogIds = @('IDD_TOOLS_SETTINGS', 'IDD_OPTIONS', 'IDD_SETTING_OTHER', 'IDD_HOTKEYS', 'IDD_SETTINGS_WORDS', 'IDD_SETTING_NEXT')
function Assert-SettingsDialogFont([string]$text, [string]$sourceName) {
    foreach ($dialogId in $settingsDialogIds) {
        $dialog = [regex]::Match($text, "(?ms)^$dialogId DIALOGEX.*?^END")
        if (-not $dialog.Success) {
            throw "В $sourceName не найден диалог настроек $dialogId."
        }
        if ($dialog.Value -notmatch 'FONT 8, "Tahoma", 400, 0, 0x1') {
            throw "В $sourceName для $dialogId должен использоваться шрифт-эталон Tahoma 8."
        }
        if ($dialog.Value -match 'DS_FIXEDSYS') {
            throw "В $sourceName у $dialogId не должен использоваться DS_FIXEDSYS: он меняет шрифт страницы."
        }
    }
}
Assert-SettingsDialogFont $baseRcText 'базовом FBE.rc'
foreach($file in $files){
    $rcText=[IO.File]::ReadAllText($file.Rc,$cp1251)
    if($rcText -notmatch '#include\s+"FBESmallDialogs\.generated\.rc2"'){ throw "В $($file.Language) FBE.rc не подключён FBESmallDialogs.generated.rc2." }
    foreach($resource in @('IDD_TABLE','IDD_INPUTBOX','IDD_ADDIMAGE','IDD_TOOLS_SETTINGS','IDD_ABOUTBOX','IDD_CUSTOMSAVEDLG','IDD_SETTINGS_WORDS','IDD_HOTKEYS','IDD_FIND','IDD_REPLACE','IDD_SPELL_CHECK','IDD_WORDS','IDD_SETTING_OTHER','IDD_SETTING_NEXT','IDD_OPTIONS')){ if($rcText -match "(?m)^\s*$resource\s+DIALOGEX\s+"){ throw "В $($file.Language) FBE.rc остался ручной $resource DIALOGEX." } }
    $bytes=[IO.File]::ReadAllBytes($file.Generated)
    if($bytes.Length -lt 2 -or $bytes[0] -ne 0xFF -or $bytes[1] -ne 0xFE){ throw "Generated-файл малых диалогов должен быть UTF-16 LE BOM: $($file.Generated)" }
    $generatedText=[IO.File]::ReadAllText($file.Generated,$utf16)
    Assert-SettingsDialogFont $generatedText "generated-ресурсе $($file.Language)"
    foreach($resource in @('IDD_TABLE DIALOGEX','IDD_INPUTBOX DIALOGEX','IDD_ADDIMAGE DIALOGEX','IDD_TOOLS_SETTINGS DIALOGEX','IDD_ABOUTBOX DIALOGEX','IDD_CUSTOMSAVEDLG DIALOGEX','IDD_SETTINGS_WORDS DIALOGEX','IDD_HOTKEYS','IDD_FIND','IDD_REPLACE','IDD_SPELL_CHECK','IDD_WORDS','IDD_SETTING_OTHER','IDD_SETTING_NEXT','IDD_OPTIONS','IDC_CHECK_TABLE_TITLE','IDC_EDIT_TABLE_COLUMNS','IDC_ADDIMAGE_ASKAGAIN','IDC_UPDATE','IDC_ENCODING','IDC_CHECK_SHOW_EXCLUSIONS','IDC_BUTTON_HOTKEY_ASSIGN','ID_FIND_NEXT','IDC_REPLACE_ALL','IDC_SPELL_IGNOREALL','IDC_BUTTON_REMOVEHLREPL','IDC_WORDS_FR_BTN_REPL','IDC_DEFAULT_SCRIPTS_FOLDER','IDC_OPTIONS_CLEARIMGS','IDC_CREATE_BACKUP_FILE','IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE','IDC_FBE_NEXT_WINDOW_TITLE_GROUP','IDC_SHOWLINENUMBERS','IDC_BACKGROUNDSPELLCHECK')){ if($generatedText -notmatch [regex]::Escape($resource)){ throw "В generated малых диалогов $($file.Language) нет $resource." } }
    if($generatedText -notmatch 'COMBOBOX\s+IDC_LANG,\d+,\d+,1[0-9]{2},\d+'){
        throw "Generated IDD_OPTIONS $($file.Language) должен оставлять достаточно широкое поле IDC_LANG для 'Определяется системой'."
    }
    if($generatedText -notmatch 'IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS'){
        throw "В generated малых диалогов $($file.Language) нет переключателя невидимых символов FBE Next."
    }
	foreach($imageImportControl in @('IDC_IMAGE_IMPORT_FORMAT','IDC_IMAGE_IMPORT_JPEG_QUALITY','IDC_IMAGE_IMPORT_JPEG_SPIN','IDC_IMAGE_IMPORT_KEEP_SUPPORTED')) {
		if($generatedText -notmatch $imageImportControl) { throw "В generated малых диалогов $($file.Language) нет $imageImportControl." }
	}
	if($generatedText -notmatch 'IDC_OPTIONS_SOURCE_SPECIAL_CHARS_STYLE'){
		throw "В generated малых диалогов $($file.Language) нет выбора стиля невидимых символов FBE Next."
	}
	$resourceHeader = Get-Content -Raw -LiteralPath (Join-Path (Split-Path -Parent $file.Rc) 'resource.h')
	if($resourceHeader -notmatch '#define\s+IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS\s+1145'){
		throw "В resource.h локали $($file.Language) отсутствует идентификатор IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS."
	}
	if($resourceHeader -notmatch '#define\s+IDC_OPTIONS_SOURCE_SPECIAL_CHARS_STYLE\s+1153'){
		throw "В resource.h локали $($file.Language) отсутствует идентификатор IDC_OPTIONS_SOURCE_SPECIAL_CHARS_STYLE."
	}
	foreach($imageImportId in @('IDC_IMAGE_IMPORT_FORMAT\s+1104','IDC_IMAGE_IMPORT_JPEG_QUALITY\s+1105','IDC_IMAGE_IMPORT_JPEG_SPIN\s+1106','IDC_IMAGE_IMPORT_KEEP_SUPPORTED\s+1107')) {
		if($resourceHeader -notmatch ('#define\s+' + $imageImportId)) { throw "В resource.h локали $($file.Language) отсутствует $imageImportId." }
	}
}
Write-Host "Малые DIALOGEX-диалоги FBE прошли проверку."
Write-Host "  Каталог: $catalogPath"
Write-Host "  Строк: $($entries.Count)"
Write-Host "  Языков: $($expectedLanguages.Count)"
