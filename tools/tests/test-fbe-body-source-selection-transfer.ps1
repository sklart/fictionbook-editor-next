[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourcePath = Join-Path $repoRoot 'src\fbe\mainfrm.cpp'
$selectionCoordinatorPath = Join-Path $repoRoot 'src\fbe\source\BodySourceSelectionCoordinator.cpp'
$sourceSessionPath = Join-Path $repoRoot 'src\fbe\source\SourceViewSession.cpp'
$transferPath = Join-Path $repoRoot 'src\fbe\source\SourceDocumentTransfer.cpp'
$presentationPath = Join-Path $repoRoot 'src\fbe\view\ui\EditorViewPresentationHost.cpp'
$tracePath = Join-Path $repoRoot 'src\fbe\StartupTrace.cpp'
$documentPath = Join-Path $repoRoot 'src\fbe\FBDoc.cpp'
$source = [System.Text.Encoding]::GetEncoding(1251).GetString([System.IO.File]::ReadAllBytes($sourcePath)) +
    [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($selectionCoordinatorPath)) +
    [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($sourceSessionPath)) +
    [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($transferPath)) +
    [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($presentationPath))
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

Assert-Contains $source 'document.m_body.GetSelectionInfo(' `
	'переход Body → Source должен получать live selection через editor adapter'
Assert-Contains $source 'FbeDom::SelectionRangeMapper::SetSelection(document.m_body.Document()' `
	'переход Source → Body должен восстанавливать диапазон через DOM mapper'
Assert-Contains $source 'SCI_SETSELECTIONSTART, beginByte' `
    'переход Body → Source должен устанавливать начало выделения в Scintilla'
Assert-Contains $source 'SCI_SETSELECTIONEND, endByte' `
    'переход Body → Source должен устанавливать конец выделения в Scintilla'
Assert-Contains $source 'selection.BodySource().bodyToSourceTransferred = true;' `
    'переход Body → Source должен сохранять результат сопоставления диапазона'
Assert-Contains $source 'm_context.view.ActivateWnd(m_context.source);' `
    'переход Body → Source должен активировать окно Scintilla'
Assert-Contains $source 'SCI_SETSEL, m_context.selection.BodySource().sourceStart' `
    'после активации Source должно восстанавливаться начало перенесённого выделения'
Assert-Contains $source 'm_context.selection.BodySource().sourceEnd' `
    'после активации Source должно восстанавливаться конец перенесённого выделения'
Assert-Contains $source 'SCI_SETSEL, m_context.selection.BodySource().sourceStart' `
    'после установки фокуса Source должен повторно применяться весь диапазон выделения'
Assert-Contains $source 'PostMessage(m_context.source, SCI_SCROLLCARET' `
    'после завершения смены режима Source должен отложенно прокручиваться к выделению'
Assert-Contains $source 'SourceDocumentTransfer::FindVisibleXmlTextRange(serialized, selectedText' `
    'переход Body → Source должен сначала сопоставлять фактически выделенный текст'
Assert-Contains $source 'SourceDocumentTransfer::FindXmlBodyRangeByIndex(serialized, selectedBodyIndex, bodyRange)' `
    'переход Body → Source при отказе DomPath должен ограничить fallback соответствующим XML body'
Assert-Contains $source 'bodyRange.start, bodyRange.end, expectedBegin, visible' `
    'переход Body → Source должен применять безопасный text fallback в границах body'

Assert-Contains $source 'ReadSourceText' `
    'переход Source → Body должен читать начало выделения Scintilla'
Assert-Contains $source 'ReadSourceText' `
    'переход Source → Body должен читать конец выделения Scintilla'
Assert-Contains $source 'bool pathAvailable = beginPath.CreatePathFromText' `
    'переход Source → Body должен проверять преобразование позиции в DOM-путь'
Assert-Contains $source 'selection.BodySource().sourceToBodyTransferred = (bool)selection.BodyRange();' `
    'переход Source → Body должен фиксировать успешное создание HTML-выделения'
Assert-Contains $source 'm_context.selection.BodyRange()->select();' `
	'после активации Body должно восстанавливаться перенесённое выделение'
Assert-Contains $source 'ExtractVisibleXmlText(selectedXml)' `
    'при переходе Source → Body выделение должно преобразовываться в отображаемый текст'
Assert-Contains $source 'SourceDocumentTransfer::FindBodyTextRange(' `
	'при отказе DOM-пути Source → Body должен искать текстовый диапазон в Body'
Assert-Contains $source 'FindXmlBodyIndexAtPosition(source.text, begin)' `
	'при отказе DomPath Source → Body должен определять соответствующий XML body по позиции Source'
Assert-Contains $source 'int bodyIndex = selectedBodyIndex >= 0 ? selectedBodyIndex : 0;' `
	'fallback Source → Body должен иметь безопасный основной body даже без DomPath'
Assert-Contains $source 'scope = element;' `
	'соответствующий HTML body должен быть базовой областью поиска fallback'
Assert-Contains $source 'MSHTML::IHTMLElementPtr refined' `
	'DomPath может только уточнять область поиска fallback, а не отключать её'
Assert-Contains $source 'target == BODY && previous == SOURCE && m_context.selection.BodySource().sourceToBodyTransferred' `
	'после окончательной установки фокуса Body должно применяться только подтверждённое перенесённое выделение'

Assert-Contains $trace 'FBE_NEXT_TRACE' `
	'диагностический журнал должен включаться переменной окружения'
if ($trace.Contains('FBE_NEXT_STARTUP_TRACE') -or $trace.Contains('FBE_NEXT_SELECTION_TRACE')) {
	throw 'В диагностическом журнале не должны оставаться отдельные переменные startup/selection.'
}
Assert-Contains $trace 'fbe-trace-' `
	'диагностический журнал должен записываться в файл FBE Next'
Assert-Contains $source 'StartupTrace::Event(L"selection", code, message);' `
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
