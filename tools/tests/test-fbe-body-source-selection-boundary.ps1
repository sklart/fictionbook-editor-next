<# Guards ownership of BODY/SOURCE selection mapping. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\BodySourceSelectionCoordinator.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\BodySourceSelectionCoordinator.cpp')
$session = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceViewSession.cpp')
$main = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$unit = $header + $source

foreach($required in @('class\s+BodySourceSelectionCoordinator', 'IBodySourceSelectionMapper', 'EditorSelectionState', 'SourceDocumentTransfer', 'U::DomPath', 'MapSourceSelectionToBody', 'MapBodySelectionToSource')) {
    if($unit -notmatch $required) { throw "Selection coordinator is missing: $required" }
}
foreach($forbidden in @('mainfrm\.h', 'CMainFrame', 'DocumentSession', 'DocumentSave', 'Recovery', 'RecentDocuments', 'PluginExecution', 'PluginUi')) {
    if($unit -match $forbidden) { throw "Selection coordinator must not own $forbidden." }
}
if($session -match 'FindVisibleXmlTextRange|FindXmlBodyRangeByIndex|GetNodeFromText') { throw 'Source view session retains selection mapping.' }
if($main -match 'FindVisibleXmlTextRange|FindXmlBodyRangeByIndex|GetNodeFromText') { throw 'Main frame retains selection mapping.' }
Write-Host 'BODY/SOURCE selection boundary contract passed.'
