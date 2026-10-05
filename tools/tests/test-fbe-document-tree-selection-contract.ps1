<# Verifies independent Body current-item highlighting and multi-selection semantics. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$header = Get-Content -Raw (Join-Path $root 'src\fbe\TreeView.h')
$source = Get-Content -Raw (Join-Path $root 'src\fbe\TreeView.cpp')
function Require([string]$pattern, [string]$message) { if($source -notmatch $pattern -and $header -notmatch $pattern) { throw $message } }

Require 'm_current_item' 'Current Body position must be kept independently from selection.'
Require 'OnCustomDraw' 'Current Body position needs a dedicated visual marker.'
Require 'ThemeManager::IsHighContrast' 'Current-item custom colouring must preserve High Contrast.'
Require 'ThemeManager::HoverColor' 'Current item marker must use the interface theme palette.'
Require 'CDDS_ITEMPOSTPAINT' 'Current item must receive an overlay after the native selection is drawn.'
Require 'ThemeManager::AccentColor' 'The current-item overlay must remain visible on selected items.'
Require 'UiMetrics::ScaleForDpi\(3, UiMetrics::DpiForWindow\(m_hWnd\)\)' 'Current-item overlay width must be DPI aware.'
if($source -match 'm_current_item\s*&&\s*!\(GetItemState\(m_current_item, TVIS_SELECTED\)') { throw 'Current item marker must not disappear when the item is selected.' }
$highlight = [regex]::Match($source, 'void\s+CTreeView::HighlightItemAtPos[\s\S]*?(?=\n\s*(?:static\s+)?(?:void|LRESULT)\s+)').Value
if(-not $highlight -or $highlight -match 'ClearSelection\(' -or $highlight -match 'SelectItem\(') { throw 'Body synchronisation must not replace the user selection.' }
Require "wParam == 'A'.*GetKeyState\(VK_CONTROL\)" 'Ctrl+A must be handled only by the tree.'
Require 'SelectAllVisibleItems\(\)' 'Ctrl+A must use the shared visible-item selection path.'
Require 'GetNextVisibleItem\(item\)' 'Ctrl+A must select only visible structure items.'
Require 'std::vector<HTREEITEM> selectedItems' 'Ctrl+Click must preserve the complete logical selection.'
Require 'for\(size_t index = 0; index < selectedItems\.size\(\); \+\+index\)' 'Ctrl+Click must restore each pre-existing selection.'
$runtime = Get-Content -Raw (Join-Path $root 'src\fbe\testing\RuntimeTestEditorAndExport.inl')
$runtimeScript = Join-Path $root 'tools\tests\test-fbe-document-tree-selection-runtime.ps1'
foreach($required in @('document-tree-selection-runtime', 'multiSelectPreserved', 'removeOne', 'SelectAllVisibleItems', 'currentInsideSelection')) {
    if(-not $runtime.Contains($required)) { throw "Runtime coverage is missing: $required" }
}
if(-not (Test-Path -LiteralPath $runtimeScript)) { throw 'Document Tree selection runtime launcher is missing.' }
Write-Host 'Document tree selection contract passed.'
