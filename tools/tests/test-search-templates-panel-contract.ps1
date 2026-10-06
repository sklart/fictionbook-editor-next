[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SearchReplace.h')
$runtime = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\testing\RuntimeTestEditorAndExport.inl')
$resources = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.rc')
function Require([string]$pattern, [string]$description) { if ($source -notmatch $pattern) { throw "Missing $description." } }
function RequireRuntime([string]$pattern, [string]$description) { if ($runtime -notmatch $pattern) { throw "Missing $description." } }
Require 'SetSearchTemplatesPanelPinned\(pinned, true\)' 'pin persistence'
Require 'struct PresetPanelMetrics' 'shared panel metrics'
Require 'PreviewHeightForCurrentSelection' 'bounded preview measurement'
Require 'lineHeight \* 4' 'four-line preview cap'
Require 'LocalizedButtonWidth' 'localized Apply measurement'
Require 'GetTextExtentPoint32W' 'button font measurement'
Require 'const int footerHeight' 'reserved footer'
Require 'const int contentHeight' 'available panel content calculation'
Require 'metrics\.previewHeight = \(std::min\)' 'preview shrinks before tree'
Require 'const int panelBottom' 'bottom-up panel layout'
Require 'const int row2 = panelBottom - metrics\.buttonHeight' 'bottom action row anchor'
Require 'const int buttonsTop = row2 - margin - metrics\.buttonHeight' 'top action row anchor'
Require 'const int descriptionBottom = buttonsTop - margin' 'preview stops before footer'
Require 'const int treeBottom' 'tree consumes only remaining space'
Require 'InvalidateRect\(preview, NULL, TRUE\)' 'preview repaint after text/relayout'
foreach($control in @('IDC_FIND_PRESET_APPLY','IDC_FIND_PRESET_SAVE','IDC_FIND_PRESET_UPDATE','IDC_FIND_PRESET_RENAME','IDC_FIND_PRESET_DELETE')) { if(-not $source.Contains('::SetWindowPos(GetDlgItem(' + $control + ')')) { throw "Missing layout for $control." } }
if($resources -notmatch 'EDITTEXT\s+IDC_FIND_PRESET_DESCRIPTION.*ES_MULTILINE.*ES_READONLY.*ES_AUTOVSCROLL.*WS_VSCROLL') { throw 'Preset preview must be a clipping read-only multiline edit control.' }
foreach($forbidden in @('PresetPinMaskResource', 'GetDIBits', 'DrawFocusRect')) { if($source -match [regex]::Escape($forbidden)) { throw "Legacy Templates implementation remains: $forbidden" } }
RequireRuntime 'verifyLongPreviewLayout' 'runtime long regexp fixture'
RequireRuntime 'IDC_FIND_PRESET_APPLY, IDC_FIND_PRESET_SAVE, IDC_FIND_PRESET_UPDATE, IDC_FIND_PRESET_RENAME, IDC_FIND_PRESET_DELETE' 'runtime footer action bounds check'
RequireRuntime 'IntersectRect\(&overlap, &actionRect, &previewRect\)' 'runtime preview/footer non-overlap check'
RequireRuntime 'EqualRect\(&before, &after\)' 'runtime pin geometry check'
Write-Host 'Templates pin and bottom-up preview layout contract passed.'