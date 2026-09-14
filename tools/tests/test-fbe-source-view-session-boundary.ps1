<# Guards the source-view document exchange session boundary. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceViewSession.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceViewSession.cpp')
$mainHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.h')
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$unit = $header + $source

foreach($required in @('class\s+SourceViewSession', 'IEditorSourceExchange', 'SourceDocumentTransfer', 'CommitSourceDocument', 'PrepareSourceDocument', 'm_savedXml')) {
    if($unit -notmatch $required) { throw "Source view session is missing: $required" }
}
foreach($forbidden in @('mainfrm\.h', 'CMainFrame', 'DocumentSession', 'DocumentSave', 'Recovery', 'RecentDocuments', 'PluginExecution', 'PluginUi')) {
    if($unit -match $forbidden) { throw "Source view session must not own $forbidden." }
}
if($mainHeader -match 'm_saved_xml') { throw 'Main frame retains the source XML snapshot.' }
foreach($conversion in @('PrepareSerializedSource', 'ApplySourceDocument')) {
    if($mainSource -match $conversion) { throw "Main frame retains source conversion: $conversion" }
}
$showView = [regex]::Match($mainSource, '(?s)void\s+CMainFrame::ShowView\(.*?(?=void\s+CMainFrame::ApplyEditorViewCommandUi)').Value
if([string]::IsNullOrEmpty($showView)) { throw 'Unable to locate CMainFrame::ShowView.' }
if($showView -notmatch 'm_editor_view_controller\.ChangeView\(m_editor_view_state,\s*\*this,\s*presentation,\s*vt\)') {
    throw 'View lifecycle must use the CMainFrame application source adapter.'
}
if($showView -match 'ChangeView\(m_editor_view_state,\s*m_source_view_session') {
    throw 'View lifecycle bypasses the application source adapter.'
}
$commit = [regex]::Match($mainSource, '(?s)EditorSourceOperationResult\s+CMainFrame::CommitSourceDocument\(\).*?(?=bool\s+CMainFrame::SourceToHTML\()').Value
if($commit -notmatch 'm_document_tree\.GetDocumentStructure\(m_doc->m_body\.Document\(\)\)') {
    throw 'Successful Source commit must refresh the document tree.'
}
Write-Host 'Source view session boundary contract passed.'
