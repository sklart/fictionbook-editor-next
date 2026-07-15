[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourcePath = Join-Path $repoRoot 'src\fbe\mainfrm.cpp'
$source = [System.Text.Encoding]::GetEncoding(1251).GetString([System.IO.File]::ReadAllBytes($sourcePath))

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

Write-Host 'Двусторонний перенос выделения между Body и Source закреплён проверкой.'
