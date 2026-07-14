<#
.SYNOPSIS
    Проверяет, что аудит зашитой кириллицы корректно обрабатывает экранированные пути.
.DESCRIPTION
    Fixture содержит C++-строку с обратными слешами и русский комментарий. Комментарий
    не должен ошибочно считаться частью строкового литерала при проверке локализации.
#>

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$auditScript = Join-Path $repoRoot "tools\localization\analyze-product-hardcoded-cyrillic.ps1"

& $auditScript -Roots @("tools\tests\fixtures\localization-cyrillic-audit") -FailOnFindings

Write-Host "Регрессионная проверка аудита зашитой кириллицы прошла успешно."
