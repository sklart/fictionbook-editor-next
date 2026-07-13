<#
.SYNOPSIS
Проверяет, что инвентарь оставшихся FBE DIALOGEX UI-литералов строится корректно.

.DESCRIPTION
Тест запускает tools/localization/analyze-fbe-rc-ui-literals.ps1 во временный каталог
и проверяет базовый контракт отчёта: ручных MENU/DIALOGEX UI-литералов больше
не осталось, а уже generated MENU/DIALOGEX-ресурсы не возвращаются в ручной инвентарь.
Это страховка для дальнейшего переноса диалогов в JSON/Weblate pipeline.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$tmpDir = Join-Path ([IO.Path]::GetTempPath()) ("fbe-rc-ui-literals-{0}" -f $PID)
New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null
$outPath = Join-Path $tmpDir "fbe-rc-ui-literals.json"

& (Join-Path $repoRoot "tools\localization\analyze-fbe-rc-ui-literals.ps1") -OutputPath $outPath
if (-not $?) {
    throw "analyze-fbe-rc-ui-literals.ps1 завершился с ошибкой."
}

if (-not (Test-Path -LiteralPath $outPath)) {
    throw "Файл отчёта не создан: $outPath"
}

$report = Get-Content -Raw -LiteralPath $outPath | ConvertFrom-Json -Depth 20
$resourceTypes = @($report.byResourceType | ForEach-Object { $_.resourceType })
if ($resourceTypes -contains "DIALOGEX") {
    throw "DIALOGEX больше не должен быть в ручном инвентаре: все FBE DIALOGEX подключены из generated .rc2."
}
if ($resourceTypes -contains "MENU") {
    throw "MENU больше не должен быть в ручном инвентаре: все FBE MENU подключены из generated .rc2."
}

$resources = @($report.byResource | ForEach-Object { $_.resource })
foreach ($generatedMenu in @("IDR_MAINFRAME", "IDR_DOCUMENT_TREE", "IDR_TOOLBAR_MENU")) {
    if ($resources -contains $generatedMenu) {
        throw "$generatedMenu больше не должен быть в ручном инвентаре: FBE MENU подключены из generated .rc2."
    }
}
foreach ($generatedDialog in @("IDD_TABLE", "IDD_INPUTBOX", "IDD_ADDIMAGE", "IDD_TOOLS_SETTINGS", "IDD_ABOUTBOX", "IDD_CUSTOMSAVEDLG", "IDD_SETTINGS_WORDS", "IDD_HOTKEYS", "IDD_FIND", "IDD_REPLACE", "IDD_SPELL_CHECK", "IDD_WORDS", "IDD_SETTING_OTHER", "IDD_OPTIONS")) {
    if ($resources -contains $generatedDialog) {
        throw "$generatedDialog больше не должен быть в ручном инвентаре: малые DIALOGEX подключены из generated .rc2."
    }
}

if ([int]$report.totals.items -ne 0) {
    throw "Ожидалось 0 оставшихся FBE UI-литералов, получено $($report.totals.items)."
}

Write-Host "Инвентарь UI-литералов FBE прошёл проверку."
Write-Host "  Файл: $outPath"
Write-Host "  Литералов: $($report.totals.items)"
Write-Host "  Ресурсов: $($report.totals.resources)"
