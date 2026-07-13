<#
.SYNOPSIS
Проверяет, что главное меню FBE подключено из generated `.rc2`.

.DESCRIPTION
Тест страхует runtime-переход `IDR_MAINFRAME MENU` на JSON→generated pipeline:
в русской и украинской `FBE.rc` не должно оставаться ручного блока главного меню,
а `FBEIdrMainframeMenu.generated.rc2` должен быть подключён через `#include`.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$files = @(
    @{ Language = "ru-RU"; Path = Join-Path $repoRoot "src\locales\res_rus\FBE.rc" },
    @{ Language = "uk-UA"; Path = Join-Path $repoRoot "src\locales\res_ukr\FBE.rc" }
)
$encoding = [Text.Encoding]::GetEncoding(1251)

foreach ($file in $files) {
    $text = [IO.File]::ReadAllText($file.Path, $encoding)
    if ($text -notmatch '#include\s+"FBEIdrMainframeMenu\.generated\.rc2"') {
        throw "В $($file.Language) FBE.rc не подключён FBEIdrMainframeMenu.generated.rc2."
    }
    if ($text -match '(?m)^\s*IDR_MAINFRAME\s+MENU\s*$') {
        throw "В $($file.Language) FBE.rc остался ручной IDR_MAINFRAME MENU."
    }
}

Write-Host "Главное меню FBE подключено из generated .rc2."
foreach ($file in $files) {
    Write-Host "  $($file.Language): $($file.Path)"
}