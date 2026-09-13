<# Guards the persistence-only document save boundary. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\document\DocumentSaveController.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\document\DocumentSaveController.cpp')
$frame = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$plan = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\document\DocumentSavePlan.cpp')

foreach($required in @('DocumentSaveFailureKind', 'Serialization', 'NormalWrite', 'ArchiveWrite', 'DocumentSaveAsRequest', 'SaveCurrent\s*\(', 'SaveAsNormal\s*\(')) {
    if($header -notmatch $required) { throw "Save result contract is missing: $required" }
}
foreach($forbidden in @('mainfrm\.h', 'Recovery', 'RuntimeTests', 'ShowError', 'MessageBox', 'DocumentFileDialogs')) {
    if($source -match $forbidden) { throw "Save controller crossed its persistence boundary: $forbidden" }
}
if($header -match 'Cancelled') { throw 'Persistence-only save controller must not expose Cancelled.' }
foreach($required in @('DocumentSavePlan::Create', 'DocumentSaveController', 'DocumentSaveFailureKind::ArchiveWrite', 'CommitSuccessfulSave')) {
    if($frame -notmatch $required) { throw "Frame save boundary is missing: $required" }
}
$saveFile = [regex]::Match($frame, 'CMainFrame::FILE_OP_STATUS CMainFrame::SaveFile[\s\S]*?(?=void CMainFrame::CommitSuccessfulSave)').Value
if($saveFile -notmatch 'm_recentDocuments\.OnSavedAsNormal' -or $saveFile -match 'RememberNormalMruRecord\(m_recentDocuments\.List\(') {
    throw 'SaveFile must update Save As MRU through RecentDocumentsController.'
}
foreach($required in @('CurrentFile', 'CurrentArchive', 'SaveAs')) {
    if($plan -notmatch $required) { throw "DocumentSavePlan target is missing: $required" }
}
Write-Host 'Document save boundary contract passed.'
