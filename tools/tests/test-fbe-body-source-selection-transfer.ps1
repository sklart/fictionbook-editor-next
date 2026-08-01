[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourcePath = Join-Path $repoRoot 'src\fbe\mainfrm.cpp'
$tracePath = Join-Path $repoRoot 'src\fbe\StartupTrace.cpp'
$documentPath = Join-Path $repoRoot 'src\fbe\FBDoc.cpp'
$source = [System.Text.Encoding]::GetEncoding(1251).GetString([System.IO.File]::ReadAllBytes($sourcePath))
$trace = [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($tracePath))
$document = [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($documentPath))

function Assert-Contains {
    param(
        [Parameter(Mandatory)]
        [string]$Text,

        [Parameter(Mandatory)]
        [string]$Expected,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if (-not $Text.Contains($Expected)) {
        throw "Не выполнено требование переноса выделения Body/Source: $Description"
    }
}

Assert-Contains $source 'm_doc->m_body.GetSelectionInfo(' `
    'переход Body → Source должен читать границы выделения из HTML DOM'
Assert-Contains $source 'SCI_SETSELECTIONSTART,savedPosBegin' `
    'переход Body → Source должен устанавливать начало выделения в Scintilla'
Assert-Contains $source 'SCI_SETSELECTIONEND,savedPosEnd' `
    'переход Body → Source должен устанавливать конец выделения в Scintilla'
Assert-Contains $source 'm_body_selection_transferred = selection_mapped_to_source;' `
    'переход Body → Source должен сохранять результат сопоставления диапазона'
Assert-Contains $source 'm_view.ActivateWnd(m_source);' `
    'переход Body → Source должен активировать окно Scintilla'
Assert-Contains $source 'SCI_SETSELECTIONSTART, m_source_selection_start' `
    'после активации Source должно восстанавливаться начало перенесённого выделения'
Assert-Contains $source 'SCI_SETSELECTIONEND, m_source_selection_end' `
    'после активации Source должно восстанавливаться конец перенесённого выделения'
Assert-Contains $source 'SCI_SETSEL, m_source_selection_start' `
    'после установки фокуса Source должен повторно применяться весь диапазон выделения'
Assert-Contains $source 'PostMessage(m_source, SCI_SCROLLCARET' `
    'после завершения смены режима Source должен отложенно прокручиваться к выделению'
Assert-Contains $source 'FindVisibleXmlTextRange(srcText, selectedText' `
    'переход Body → Source должен сначала сопоставлять фактически выделенный текст'

Assert-Contains $source 'SCI_GETSELECTIONSTART' `
    'переход Source → Body должен читать начало выделения Scintilla'
Assert-Contains $source 'SCI_GETSELECTIONEND' `
    'переход Source → Body должен читать конец выделения Scintilla'
Assert-Contains $source 'bool selection_path_available = path_begin.CreatePathFromText' `
    'переход Source → Body должен проверять преобразование позиции в DOM-путь'
Assert-Contains $source 'm_source_selection_transferred = (bool)m_body_selection;' `
    'переход Source → Body должен фиксировать успешное создание HTML-выделения'
Assert-Contains $source 'if(m_source_selection_transferred && (bool)m_body_selection)' `
    'переход Source → Body должен применять только подтверждённое выделение'
Assert-Contains $source 'm_body_selection->select();' `
    'после активации Body должно восстанавливаться перенесённое выделение'
Assert-Contains $source 'ExtractVisibleXmlText(selectedSourceXml)' `
    'при переходе Source → Body выделение должно преобразовываться в отображаемый текст'
Assert-Contains $source 'FindBodyTextRange(htmlBody,' `
    'при отказе DOM-пути Source → Body должен искать текстовый диапазон в Body'

Assert-Contains $source 'FindXmlNodeTextPosition(srcText, xml_selected_begin' `
    'при отказе DomPath переход Body → Source должен использовать XML выбранного узла'
Assert-Contains $source 'FindXmlNodeTextPosition(srcText, xml_selected_end' `
    'конец выделения Body → Source должен сопоставляться по XML конечного узла'

Assert-Contains $source 'SourceToHTML: source bytes=' `
    'диагностический журнал должен фиксировать исходные позиции выделения Source'
Assert-Contains $source 'ShowSource: mapping-by-text=' `
    'диагностический журнал должен фиксировать способ переноса Body → Source'
Assert-Contains $source 'ShowView: Source final bytes=' `
    'диагностический журнал должен фиксировать итоговую прокрутку Source'
Assert-Contains $trace 'FBE_NEXT_TRACE' `
	'диагностический журнал должен включаться переменной окружения'
if ($trace.Contains('FBE_NEXT_STARTUP_TRACE') -or $trace.Contains('FBE_NEXT_SELECTION_TRACE')) {
	throw 'В диагностическом журнале не должны оставаться отдельные переменные startup/selection.'
}
Assert-Contains $trace 'fbe-trace-' `
	'диагностический журнал должен записываться в файл FBE Next'
Assert-Contains $source 'StartupTrace::Event(L"selection", L"E200", message);' `
	'записи переноса выделения должны иметь категорию selection'
Assert-Contains $trace 'void StartupTrace::Event' `
	'журнал должен принимать события нескольких диагностических категорий'
Assert-Contains $document 'StartupTrace::Event(L"document", code, trace);' `
	'создание XML DOM должно фиксироваться в диагностическом журнале'
Assert-Contains $document 'StartupTrace::HResult(L"com", L"X191", e.Error(), L"CreateDOM");' `
	'ошибка создания XML DOM должна фиксироваться как COM-событие'
Assert-Contains $document 'L"book load started"' `
	'журнал должен фиксировать начало открытия книги'
Assert-Contains $document 'L"book save started"' `
	'общий журнал должен фиксировать начало сохранения книги'
Assert-Contains $document 'L"script execution started"' `
	'общий журнал должен фиксировать ручной запуск пользовательского скрипта'
Assert-Contains $document 'L"recovery"' `
	'автосохранение должно иметь отдельную категорию recovery'
Assert-Contains $trace 'FBE_VERSION_WSTRING' `
	'шапка диагностического журнала должна содержать версию FBE'
Assert-Contains $source 'ID_TOOLS_DIAGNOSTIC_TRACE' `
	'меню должно содержать команду включения диагностического журнала'
Assert-Contains $source 'fbe.trace.warning' `
	'при запуске с диагностическим журналом должно выводиться предупреждение'
Assert-Contains $source 'fbe.trace.title_suffix' `
	'заголовок окна должен отмечать диагностический режим'
Assert-Contains $source 'TraceMainFrameCommand(wParam, lParam);' `
	'общий журнал должен фиксировать команды главного окна'
Assert-Contains $source 'StartupTrace::Event(L"command", L"C100", trace);' `
	'команды должны иметь отдельную категорию command'
Assert-Contains $source 'menu/hotkey/internal' `
	'для команд из меню и горячих клавиш должен фиксироваться источник WM_COMMAND'
Assert-Contains $source 'TraceMainFrameHotkey(pMsg);' `
	'общий журнал должен фиксировать нажатую горячую клавишу до трансляции акселератора'
Assert-Contains $source 'virtual-key=%u' `
	'запись горячей клавиши должна содержать её сочетание и назначенную команду'

Write-Host 'Двусторонний перенос выделения между Body и Source закреплён проверкой.'
