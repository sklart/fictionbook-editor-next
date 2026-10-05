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
$highlight = [regex]::Match($source, 'void\s+CTreeView::HighlightItemAtPos[\s\S]*?(?=\n\s*(?:static\s+)?(?:void|LRESULT)\s+)').Value
if(-not $highlight -or $highlight -match 'ClearSelection\(' -or $highlight -match 'SelectItem\(') { throw 'Body synchronisation must not replace the user selection.' }
Require "wParam == 'A'.*GetKeyState\(VK_CONTROL\)" 'Ctrl+A must be handled only by the tree.'
Require 'GetNextVisibleItem\(item\)' 'Ctrl+A must select only visible structure items.'
Require 'std::vector<HTREEITEM> selectedItems' 'Ctrl+Click must preserve the complete logical selection.'
Require 'for\(size_t index = 0; index < selectedItems\.size\(\); \+\+index\)' 'Ctrl+Click must restore each pre-existing selection.'
Write-Host 'Document tree selection contract passed.'
