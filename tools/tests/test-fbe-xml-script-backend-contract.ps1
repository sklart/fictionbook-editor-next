<# Guards the non-COM XML scripting backend and its production boundaries. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$backend = Get-Content -Raw (Join-Path $root 'src\fbe\scripts\XmlScriptBackend.cpp')
$header = Get-Content -Raw (Join-Path $root 'src\fbe\scripts\XmlScriptBackend.h')
$doc = Get-Content -Raw (Join-Path $root 'src\fbe\FBDoc.cpp')
$frame = Get-Content -Raw (Join-Path $root 'src\fbe\mainfrm.cpp')
$runtime = Get-Content -Raw (Join-Path $root 'src\fbe\testing\RuntimeTestEditorAndExport.inl')
$idl = Get-Content -Raw (Join-Path $root 'src\contracts\fbe.idl')
$external = Get-Content -Raw (Join-Path $root 'src\fbe\ExternalHelper.cpp')
function Require([string]$text, [string]$token, [string]$message) { if($text -notmatch $token) { throw $message } }
Require $header 'class XmlScriptBackend' 'Internal XML backend is missing.'
Require $backend 'SourceDocumentTransfer::ReadSourceText' 'Active Source must be read from Scintilla.'
Require $backend 'CreateDOM\(m_document->m_encoding\)' 'BODY XML must use production serialization.'
Require $backend 'SetXMLAndValidate\(m_source\.m_hWnd, true' 'Validation must use the production validation path.'
Require $backend 'SetXMLAndValidate\(m_source\.m_hWnd, false' 'Apply must use the production LoadFromDOM path.'
Require $backend 'GetSourceText\(previous\)' 'Apply must capture a rollback snapshot before mutation.'
Require $backend 'public IOleUndoUnit' 'Apply must use the standard MSHTML undo manager.'
Require $backend 'manager->Add\(unit\)' 'Apply must register exactly one standard undo unit.'
Require $backend 'GetDescription' 'The standard undo unit must expose its operation name.'
Require $backend 'm_description = operationName' 'The caller supplied operation name must describe the undo unit.'
if($backend -match 'm_undoSnapshots|UndoLastApply') { throw 'XML backend must not maintain a parallel undo stack.' }
Require $backend 'documentWasDirty' 'Undo must restore the captured dirty-state contract.'
Require $backend 'm_document->ResetSavePoint' 'Successful XML application must mark the document dirty.'
Require $backend 'm_synchronize\(text\)' 'Successful XML application must request UI synchronization.'
Require $doc 'BSTR sourceOverride' 'Production validation must accept an internal candidate without a second parser.'
Require $frame 'SynchronizeAfterXmlScriptApply' 'Frame must synchronize a successful XML application.'
Require $frame 'SCI_SETSAVEPOINT' 'Source cache must be synchronized after XML application.'
Require $frame 'GetDocumentStructure\(m_doc->m_body.Document\(\)\)' 'Document Tree must be refreshed after XML application.'
Require $frame 'MarkRecoveryDirty\(\)' 'XML application must update recovery/dirty UI state.'
Require $runtime 'xml-script-backend-runtime' 'A real editor runtime scenario is required.'
Require $runtime 'invalid_rejected' 'Runtime scenario must cover rejected XML.'
Require $runtime 'ID_EDIT_UNDO' "Runtime scenario must invoke FBE's normal Undo command."
Require $runtime 'ID_EDIT_REDO' "Runtime scenario must invoke FBE's normal Redo command."
Require $idl '\[id\(32\).*GetSourceText' 'GetSourceText must use DISPID 32.'
Require $idl '\[id\(33\).*ValidateSourceText' 'ValidateSourceText must use DISPID 33.'
Require $idl '\[id\(34\).*GetLastSourceDiagnostic' 'GetLastSourceDiagnostic must use DISPID 34.'
Require $idl '\[id\(35\).*ApplySourceText' 'ApplySourceText must use DISPID 35.'
Require $external 'JsonEscape' 'Source diagnostic JSON must escape parser text.'
Require $external 'WM_XML_SCRIPT_API' 'COM adapter must delegate XML work to the editor backend.'
Require $runtime 'xml-script-com-runtime' 'A real JScript window.external runtime scenario is required.'
Require $runtime 'GetLastSourceDiagnostic' 'Runtime scenario must parse the public diagnostic.'
Write-Host 'XML scripting backend and COM adapter contract passed.'
