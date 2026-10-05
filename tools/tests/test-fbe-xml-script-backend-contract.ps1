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
$pendingIdl = Get-Content -Raw (Join-Path $root 'docs\architecture\xml-scripting-api-idl-pending.md')
$idl = Get-Content -Raw (Join-Path $root 'src\contracts\fbe.idl')
function Require([string]$text, [string]$token, [string]$message) { if($text -notmatch $token) { throw $message } }
Require $header 'class XmlScriptBackend' 'Internal XML backend is missing.'
Require $backend 'SourceDocumentTransfer::ReadSourceText' 'Active Source must be read from Scintilla.'
Require $backend 'CreateDOM\(m_document->m_encoding\)' 'BODY XML must use production serialization.'
Require $backend 'SetXMLAndValidate\(m_source\.m_hWnd, true' 'Validation must use the production validation path.'
Require $backend 'SetXMLAndValidate\(m_source\.m_hWnd, false' 'Apply must use the production LoadFromDOM path.'
Require $backend 'GetSourceText\(previous\)' 'Apply must capture a rollback snapshot before mutation.'
Require $backend 'm_undoSnapshots\.push_back' 'Apply must create one logical undo snapshot.'
Require $backend 'UndoLastApply' 'XML backend must expose rollback/undo behavior.'
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
Require $pendingIdl '\[id\(32\).*GetSourceText' 'Pending IDL diff must reserve a new GetSourceText DISPID.'
Require $pendingIdl '\[id\(33\).*ValidateSourceText' 'Pending IDL diff must reserve a new ValidateSourceText DISPID.'
Require $pendingIdl '\[id\(34\).*GetLastSourceDiagnostic' 'Pending IDL diff must define COM-friendly diagnostics.'
Require $pendingIdl '\[id\(35\).*ApplySourceText' 'Pending IDL diff must reserve a new ApplySourceText DISPID.'
if($idl -match 'GetSourceText|ValidateSourceText|ApplySourceText') { throw 'COM IDL must remain unchanged while the public API is blocked.' }
Write-Host 'Internal XML scripting backend contract passed.'
