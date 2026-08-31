[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw (Join-Path $root 'src\fbe\FBEview.cpp')

function Handler([string]$name) {
    $signature = "VARIANT_BOOL CFBEView::$name(IDispatch* evt)"
    $start = $source.IndexOf($signature, [StringComparison]::Ordinal)
    if($start -lt 0) { throw "Missing $name handler" }

    $openBrace = $source.IndexOf('{', $start)
    if($openBrace -lt 0) { throw "Missing opening brace for $name handler" }

    $depth = 0
    for($i = $openBrace; $i -lt $source.Length; ++$i) {
        if($source[$i] -eq '{') { ++$depth }
        elseif($source[$i] -eq '}') {
            --$depth
            if($depth -eq 0) {
                return $source.Substring($openBrace + 1, $i - $openBrace - 1)
            }
        }
    }

    throw "Unterminated $name handler"
}

$down = Handler 'OnMouseDown'
$move = Handler 'OnMouseMove'
$up = Handler 'OnMouseUp'

foreach($contract in @(
    @{ Body = $down; Text = 'if (!eventObject || eventObject->button != 1) return VARIANT_TRUE;' },
    @{ Body = $down; Text = 'if (!cell) return VARIANT_TRUE;' },
    @{ Body = $down; Text = 'return VARIANT_TRUE;' },
    @{ Body = $move; Text = 'if (!m_table_selection_dragging || !m_table_selection_anchor) return VARIANT_TRUE;' },
    @{ Body = $move; Text = 'if (!cell || cell == m_table_selection_anchor' },
    @{ Body = $move; Text = 'return VARIANT_TRUE;' },
    @{ Body = $up; Text = 'if (!m_table_selection_dragging) return VARIANT_TRUE;' },
    @{ Body = $up; Text = 'if (!tableSelectionHandled || !eventObject) return VARIANT_TRUE;' })) {
    if(-not $contract.Body.Contains($contract.Text)) { throw "Missing MSHTML mouse passthrough contract: $($contract.Text)" }
}

foreach($handler in @($move, $up)) {
    if(-not $handler.Contains('eventObject->returnValue = VARIANT_FALSE;') -or -not $handler.Contains('return VARIANT_FALSE;')) {
        throw 'Custom multi-cell selection must explicitly suppress only its own MSHTML gesture.'
    }
}

if($down.Contains('return VARIANT_FALSE;')) {
    throw 'OnMouseDown must never cancel MSHTML before a multi-cell selection exists.'
}

if($up.Contains('m_table_selection_anchor.Release();')) {
    throw 'OnMouseUp must retain table context until the next Body mouse down so toolbar commands can resolve their cell.'
}

foreach($handler in @($move, $up)) {
    $selectionStart = $handler.IndexOf('SelectTableCellRange', [StringComparison]::Ordinal)
    $cancelStart = $handler.IndexOf('return VARIANT_FALSE;', [StringComparison]::Ordinal)
    if($selectionStart -lt 0 -or $cancelStart -lt $selectionStart) {
        throw 'MSHTML mouse cancellation must occur only after SelectTableCellRange().'
    }
}

Write-Host 'MSHTML Body mouse-selection passthrough contract passed.'
