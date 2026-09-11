<# Guards BODY <-> SOURCE transition ownership boundaries. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$selection = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\BodySourceSelectionTransfer.h')
$state = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\BodySourceSelectionState.h')
$transferHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceDocumentTransfer.h')
$transferSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceDocumentTransfer.cpp')
$editorHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\ui\SourceEditorControl.h')
$editorSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\ui\SourceEditorControl.cpp')
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$mainHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.h')

foreach($unit in @($selection, $state, $transferHeader, $transferSource)) {
    foreach($forbidden in @('mainfrm\.h', 'CMainFrame')) { if($unit -match $forbidden) { throw "Transition source component must not depend on $forbidden." } }
}
foreach($unit in @($editorHeader, $editorSource)) {
    foreach($forbidden in @('FBDoc', 'FB::Doc', 'MSHTML', 'DocumentSession')) { if($unit -match $forbidden) { throw "SourceEditorControl must not depend on $forbidden." } }
}
foreach($required in @('BodySourceSelectionState', 'bodyToSourceTransferred', 'sourceToBodyTransferred', 'sourceStart', 'sourceEnd', 'Reset\s*\(')) {
    if($state -notmatch $required) { throw "Selection transfer state is missing: $required" }
}
if($transferHeader -notmatch 'enum class SourceTransitionResult' -or $transferHeader -notmatch 'ReadSourceText') { throw 'SourceDocumentTransfer boundary is incomplete.' }
foreach($legacyHelper in @('static\s+int\s+FindXmlNodeTextPosition', 'static\s+bool\s+FindVisibleXmlTextRange', 'static\s+bool\s+FindEnclosingXmlElementRange', 'static\s+bool\s+FindXmlBodyRangeByIndex', 'static\s+CString\s+ExtractVisibleXmlText', 'static\s+MSHTML::IHTMLTxtRangePtr\s+FindBodyTextRange')) {
    if($mainSource -match $legacyHelper) { throw "CMainFrame retains transition mapping helper: $legacyHelper" }
}
foreach($legacy in @('m_body_selection_transferred', 'm_source_selection_transferred', 'm_source_selection_start', 'm_source_selection_end')) {
    if($mainHeader -match $legacy) { throw "CMainFrame retains legacy transfer state: $legacy" }
}
Write-Host 'BODY/SOURCE transition boundary contract passed.'
