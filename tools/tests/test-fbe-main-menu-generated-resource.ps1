<#
.SYNOPSIS
Проверяет generated `.rc2` главного меню FBE.

.DESCRIPTION
Тест запускает генератор `update-fbe-main-menu-resource.ps1` и проверяет, что
для ru-RU и uk-UA созданы Win32 MENU-фрагменты `IDR_MAINFRAME` с ожидаемой
структурой: верхние POPUP-пункты, количество MENUITEM и ключевые command id.
Это подготовительный контракт перед заменой ручного MENU-блока в локализованных
`FBE.rc`.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
& (Join-Path $repoRoot "tools\localization\update-fbe-main-menu-resource.ps1")
if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "update-fbe-main-menu-resource.ps1 завершился с кодом $LASTEXITCODE."
}

$files = @(
    @{ Language = "ru-RU"; Path = Join-Path $repoRoot "src\locales\res_rus\FBEIdrMainframeMenu.generated.rc2" },
    @{ Language = "uk-UA"; Path = Join-Path $repoRoot "src\locales\res_ukr\FBEIdrMainframeMenu.generated.rc2" }
)

foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath $file.Path)) {
        throw "Generated MENU-файл не найден: $($file.Path)"
    }

    $bytes = [IO.File]::ReadAllBytes($file.Path)
    if ($bytes.Length -lt 2 -or $bytes[0] -ne 0xFF -or $bytes[1] -ne 0xFE) {
        throw "Generated MENU-файл должен быть UTF-16 LE BOM: $($file.Path)"
    }

    $text = [IO.File]::ReadAllText($file.Path, [Text.UnicodeEncoding]::new($false, $true))
    if ($text -notmatch 'IDR_MAINFRAME\s+MENU') {
        throw "В generated MENU-файле нет IDR_MAINFRAME MENU: $($file.Path)"
    }

    $menuItemCount = ([regex]::Matches($text, '(?m)^\s*MENUITEM\s+"')).Count
    if ($menuItemCount -ne 62) {
        throw "Ожидалось 62 MENUITEM в $($file.Language), получено $menuItemCount."
    }

    $popupCount = ([regex]::Matches($text, '(?m)^\s*POPUP\s+"')).Count
    if ($popupCount -ne 12) {
        throw "Ожидалось 12 POPUP в $($file.Language), получено $popupCount."
    }    $topLevelPopupCount = ([regex]::Matches($text, '(?m)^    POPUP\s+"')).Count
    if ($topLevelPopupCount -ne 8) {
        throw "Ожидалось 8 верхних POPUP в $($file.Language), получено $topLevelPopupCount."
    }
    if ($text -notmatch '(?m)^        POPUP\s+".+"\r?$') {
        throw "В $($file.Language) отсутствует вложенное подменю Диагностика."
    }

    foreach ($id in @('ID_FILE_OPEN','ID_FILE_SAVE','ID_EDIT_REPLACE','ID_VIEW_BODY','ID_INSERT_TABLE','ID_STYLE_LINK','ID_TOOLS_SPELLCHECK','ID_TOOLS_DIAGNOSTIC_TRACE','ID_TOOLS_OPEN_DIAGNOSTIC_LOG','ID_TOOLS_OPEN_DIAGNOSTIC_FOLDER','ID_TOOLS_COPY_DIAGNOSTIC_LOG_PATH','ID_TOOLS_CLEAR_DIAGNOSTIC_LOGS','ID_APP_ABOUT')) {
        if ($text -notmatch [regex]::Escape($id)) {
            throw "В generated MENU-файле $($file.Language) нет $id."
        }
    }
}

Write-Host "Generated MENU-ресурс главного меню FBE прошёл проверку."
foreach ($file in $files) {
    Write-Host "  $($file.Language): $($file.Path)"
}
