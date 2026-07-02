<#
.SYNOPSIS
Проверяет контракт будущей runtime-локализации JSON.

.DESCRIPTION
Скрипт валидирует `localization/runtime/contract.json`: основной формат должен
оставаться JSON, каталог runtime-локализации — `Lang`, fallback-язык должен
совпадать с инвентарём языковых пакетов, а shell/MUI-ресурсы должны планироваться
в `Lang/Shell`. Контракт пока не подключён к запуску программы.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$contractPath = Join-Path $repoRoot "localization\runtime\contract.json"
$packsPath = Join-Path $repoRoot "localization\language-packs.json"

$contract = Get-Content -Raw -LiteralPath $contractPath | ConvertFrom-Json -Depth 30
$packs = Get-Content -Raw -LiteralPath $packsPath | ConvertFrom-Json -Depth 30

if ($contract.catalogFormat -ne "json") {
    throw "Runtime-контракт должен использовать JSON как основной формат локализации."
}

if ($contract.runtimeRoot -ne "Lang") {
    throw "Runtime-контракт должен использовать каталог Lang."
}

if ($contract.fallbackLanguage -ne $packs.fallbackLanguage) {
    throw "Fallback-язык runtime-контракта не совпадает с language-packs.json."
}

$lookupOrder = @($contract.lookupOrder)
foreach ($required in @("Lang/{locale}/{module}.json", "Lang/en-US/{module}.json", "embedded:{module}")) {
    if ($lookupOrder -notcontains $required) {
        throw "В lookupOrder отсутствует обязательный fallback-шаг: $required"
    }
}

if ([string]::IsNullOrWhiteSpace([string]$contract.missingFilePolicy) -or
    [string]::IsNullOrWhiteSpace([string]$contract.missingKeyPolicy)) {
    throw "В runtime-контракте должны быть явно описаны missingFilePolicy и missingKeyPolicy."
}

if ($contract.shellMui.hostModule -ne "Lang/Shell/FBVVerbResources.dll") {
    throw "Shell MUI host должен планироваться как Lang/Shell/FBVVerbResources.dll."
}

if ($contract.shellMui.localizedMuiPattern -ne "Lang/Shell/{locale}/FBVVerbResources.dll.mui") {
    throw "Shell MUI satellite pattern должен планироваться в Lang/Shell/{locale}."
}

$moduleNames = @($contract.modules | ForEach-Object { $_.module })
foreach ($module in @("fbe", "fbv", "export-html", "export-docx", "export-epub", "import-epub")) {
    if ($module -notin $moduleNames) {
        throw "В runtime-контракте отсутствует модуль: $module"
    }
}

foreach ($module in @($contract.modules)) {
    if ($module.embeddedFallback -ne $true) {
        throw "Для модуля $($module.module) должен быть включён embeddedFallback."
    }
    if ([IO.Path]::GetExtension([string]$module.file) -ne ".json") {
        throw "Модуль $($module.module) должен использовать JSON-файл: $($module.file)"
    }
}

Write-Host "Контракт runtime-локализации JSON прошёл проверку."
Write-Host "  Файл: $contractPath"
Write-Host "  Runtime root: $($contract.runtimeRoot)"
Write-Host "  Fallback: $($contract.fallbackLanguage)"
