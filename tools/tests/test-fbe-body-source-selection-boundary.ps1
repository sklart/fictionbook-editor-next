<# Guards ownership of BODY/SOURCE selection mapping. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\BodySourceSelectionCoordinator.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\BodySourceSelectionCoordinator.cpp')
$session = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceViewSession.cpp')
$main = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$mapperHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\dom\SelectionRangeMapper.h')
$mapperSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\dom\SelectionRangeMapper.cpp')
$view = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBEview.cpp')
$unit = $header + $source

foreach($required in @('class\s+BodySourceSelectionCoordinator', 'IBodySourceSelectionMapper', 'EditorSelectionState', 'SourceDocumentTransfer', 'U::DomPath', 'MapSourceSelectionToBody', 'MapBodySelectionToSource')) {
    if($unit -notmatch $required) { throw "Selection coordinator is missing: $required" }
}
foreach($forbidden in @('mainfrm\.h', 'CMainFrame', 'DocumentSession', 'DocumentSave', 'Recovery', 'RecentDocuments', 'PluginExecution', 'PluginUi')) {
    if($unit -match $forbidden) { throw "Selection coordinator must not own $forbidden." }
}
if($session -match 'FindVisibleXmlTextRange|FindXmlBodyRangeByIndex|GetNodeFromText') { throw 'Source view session retains selection mapping.' }
if($main -match 'FindVisibleXmlTextRange|FindXmlBodyRangeByIndex|GetNodeFromText') { throw 'Main frame retains selection mapping.' }
foreach($required in @('namespace\s+FbeDom', 'class\s+SelectionRangeMapper', 'GetRangePos', 'GetSelectionInfo', 'SetSelection', 'GetRelationalCharPos', 'GetRealCharPos', 'CountNodeChars', 'IHTMLControlRangePtr', 'coll->item\(0\)')) {
    if(($mapperHeader + $mapperSource) -notmatch $required) { throw "Selection range mapper is missing: $required" }
}
foreach($forbidden in @('CFBEView', 'CMainFrame', '_Settings', 'DocumentSession', 'EditorViewController', 'SourceViewSession', 'WM_COMMAND', 'MessageBox')) {
    if(($mapperHeader + $mapperSource) -match $forbidden) { throw "Selection range mapper leaks forbidden dependency: $forbidden" }
}
foreach($legacy in @('CFBEView::GetRangePos', 'CFBEView::GetSelectionInfo', 'CFBEView::SetSelection', 'CFBEView::GetRelationalCharPos', 'CFBEView::GetRealCharPos', 'CFBEView::CountNodeChars')) {
    if($view -match [regex]::Escape($legacy)) { throw "CFBEView still owns selection range mapping: $legacy" }
}
if($source -notmatch 'FbeDom::SelectionRangeMapper::GetSelectionInfo' -or $source -notmatch 'FbeDom::SelectionRangeMapper::SetSelection') {
    throw 'BodySourceSelectionCoordinator must use SelectionRangeMapper for both transfer directions.'
}
Write-Host 'BODY/SOURCE selection boundary contract passed.'
