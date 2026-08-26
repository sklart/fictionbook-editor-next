<#
.SYNOPSIS
Проверяет контракт первого этапа диагностики ошибок пользовательских скриптов FBE.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$scriptSource = Get-Content -Raw -Encoding Default -LiteralPath (Join-Path $repoRoot 'src\fbe\script.cpp')
$diagnostics = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ScriptDiagnostics.h')
$resources = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\resource.h')
$catalog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'localization\app-ui\catalog.json')

foreach ($contract in @(
    @{ Text = '#include "ScriptDiagnostics.h"'; Source = $scriptSource; Name = 'подключение диагностики' },
    @{ Text = 'FbeScriptDiagnostics::SetContext\(filename,'; Source = $scriptSource; Name = 'контекст загружаемого скрипта' },
    @{ Text = 'FbeScriptDiagnostics::Show\(m_frame->m_hWnd, ei, line, column\)'; Source = $scriptSource; Name = 'диагностика ошибки JScript' },
    @{ Text = 'FbeScriptDiagnostics::ShowLoad\(GetActiveWindow\(\), filename, em,'; Source = $scriptSource; Name = 'диагностика ошибки чтения файла' },
    @{ Text = 'SourceContext\('; Source = $diagnostics; Name = 'вывод строки исходного кода' },
    @{ Text = 'SetClipboardData\(CF_UNICODETEXT'; Source = $diagnostics; Name = 'копирование сведений в буфер обмена' },
    @{ Text = 'TaskDialogIndirect'; Source = $diagnostics; Name = 'диалог с дополнительными действиями' },
    @{ Text = 'IDS_SCRIPT_PARSE_DIAGNOSTIC_MSG'; Source = $resources; Name = 'ресурс синтаксической ошибки' },
    @{ Text = 'IDS_SCRIPT_RUNTIME_DIAGNOSTIC_MSG'; Source = $resources; Name = 'ресурс ошибки выполнения' },
    @{ Text = 'IDS_SCRIPT_LOAD_DIAGNOSTIC_MSG'; Source = $resources; Name = 'ресурс ошибки загрузки' },
    @{ Text = 'IDS_SCRIPT_CLOSE_DETAILS'; Source = $resources; Name = 'встроенный английский fallback identifier' },
    @{ Text = 'fbe.script.diagnostic_parse'; Source = $catalog; Name = 'JSON-локализация синтаксической ошибки' },
    @{ Text = 'fbe.script.copy_details'; Source = $catalog; Name = 'JSON-локализация кнопки копирования' }
)) {
    if ($contract.Source -notmatch $contract.Text) {
        throw "Не найден обязательный контракт: $($contract.Name)."
    }
}

foreach ($contract in @(
    @{ Text = 'TraceMetadata'; Source = $diagnostics; Name = 'metadata-only trace for script errors' },
    @{ Text = 'file=%s; line=%lu; column=%ld; hr=0x%08lX; description-present=%d'; Source = $diagnostics; Name = 'safe script trace fields' },
    @{ Text = 'StartupTrace::Error(L"script-diagnostics", L"SD100"'; Source = $diagnostics; Name = 'script trace error severity' }
)) { if ($contract.Source -notlike "*$($contract.Text)*") { throw "Missing contract: $($contract.Name)" } }
Write-Host 'Проверка первого этапа диагностики ошибок пользовательских скриптов прошла успешно.'
