[CmdletBinding()]
param([switch]$Runtime, [string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourcePath = Join-Path $repoRoot 'src\fbe\mainfrm.cpp'
$selectionCoordinatorPath = Join-Path $repoRoot 'src\fbe\source\BodySourceSelectionCoordinator.cpp'
$sourceSessionPath = Join-Path $repoRoot 'src\fbe\source\SourceViewSession.cpp'
$transferPath = Join-Path $repoRoot 'src\fbe\source\SourceDocumentTransfer.cpp'
$presentationPath = Join-Path $repoRoot 'src\fbe\view\ui\EditorViewPresentationHost.cpp'
$tracePath = Join-Path $repoRoot 'src\fbe\StartupTrace.cpp'
$documentPath = Join-Path $repoRoot 'src\fbe\FBDoc.cpp'
$utf8 = New-Object System.Text.UTF8Encoding($false, $true)
$source = $utf8.GetString([System.IO.File]::ReadAllBytes($sourcePath)) +
    $utf8.GetString([System.IO.File]::ReadAllBytes($selectionCoordinatorPath)) +
    $utf8.GetString([System.IO.File]::ReadAllBytes($sourceSessionPath)) +
    $utf8.GetString([System.IO.File]::ReadAllBytes($transferPath)) +
    $utf8.GetString([System.IO.File]::ReadAllBytes($presentationPath))
$trace = $utf8.GetString([System.IO.File]::ReadAllBytes($tracePath))
$document = $utf8.GetString([System.IO.File]::ReadAllBytes($documentPath))

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

Assert-Contains $source 'XmlSelectionPosition positions' `
    'позиции должны вычисляться по структуре сериализованного XML'
Assert-Contains $source 'getElementById(L"fbw_body")' `
    'HTML-корень должен находиться по явному ID'
Assert-Contains $source 'XmlForHtml(document, root' `
    'HTML-элемент должен сопоставляться с соответствующей XML-областью'
Assert-Contains $source 'HtmlForXml(document, root' `
    'XML-элемент должен сопоставляться с соответствующей HTML-областью'
Assert-Contains $source 'selection.BodySource().bodyToSourceTransferred = true;' `
    'успешный перенос должен фиксироваться после проверки обоих концов диапазона'
Assert-Contains $source 'source.SendMessage(SCI_SETSELECTIONSTART, beginByte)' `
    'Scintilla должна получать UTF-8 смещение начала'
Assert-Contains $source 'source.SendMessage(SCI_SETSELECTIONEND, endByte)' `
    'Scintilla должна получать UTF-8 смещение конца'
Assert-Contains $source 'sourceToBodyApproximate' `
    'приближённая граница разметки должна отличаться от точного переноса'
Assert-Contains $source 'Unavailable(L"BS201")' `
    'неудача Body -> Source должна диагностироваться'
Assert-Contains $source 'Unavailable(L"BS202")' `
    'неудача Source -> Body должна диагностироваться'
if($source.Contains('beginPath.GetNodeFromText(') -or $source.Contains('int bodyIndex = selectedBodyIndex >= 0 ? selectedBodyIndex : 0;')) {
    throw 'Нельзя возвращать ошибку DomPath как нулевую позицию или выбирать первый body при отказе сопоставления.'
}
Assert-Contains $source 'target == BODY && previous == SOURCE && m_context.selection.BodySource().sourceToBodyTransferred' `
    'после установки фокуса Body должно восстанавливаться подтверждённое выделение'
Assert-Contains $source 'm_document_tree.HighlightItemAtPos(m_doc->m_body.SelectionContainer());' `
    'маркер дерева должен следовать восстановленному диапазону'

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
$diagnosticDialog = $utf8.GetString([IO.File]::ReadAllBytes((Join-Path $repoRoot 'src\fbe\diagnostics\DiagnosticDialog.h')))
Assert-Contains $diagnosticDialog 'StartupTrace::Enabled() ? L"extended" : L"normal_mode"' `
	'окно диагностики должно явно показывать действующий расширенный режим журнала'
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

if($Runtime) {
    & (Join-Path $PSScriptRoot 'test-fbe-body-source-selection-transfer-behavior.ps1')
    & (Join-Path $PSScriptRoot 'test-fbe-community-selection-runtime.ps1') -FbeExe $FbeExe
}
