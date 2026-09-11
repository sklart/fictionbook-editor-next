[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source=Get-Content -Raw (Join-Path $root 'src\fbe\image\ImageDocumentInserter.cpp')
$header=Get-Content -Raw (Join-Path $root 'src\fbe\image\ImageDocumentInserter.h')
$view=Get-Content -Raw (Join-Path $root 'src\fbe\FBEview.cpp')
foreach($forbidden in @('CFBEView','FBEview.h','CMainFrame','mainfrm.h','Settings','ModernFileDialog')) { if($source.Contains($forbidden) -or $header.Contains($forbidden)){throw "Forbidden dependency: $forbidden"} }
foreach($required in @('SafeArrayCreateVector','SafeArrayAccessData','apiAddBinary','FillCoverList','InsImage','InsInlineImage','ImagePlacement','insertedElement')) { if(-not $source.Contains($required)){throw "Missing insertion contract: $required"} }
if(-not $header.Contains('insertedElement')) { throw 'Image insertion result does not expose the inserted DOM element.' }
foreach($required in @('::SysAllocString(L"")', 'return Finish(out, E_INVALIDARG);', 'return Finish(out, E_OUTOFMEMORY);', 'return Finish(out, hr);')) { if(-not $source.Contains($required)){throw "Missing result/physical-path contract: $required"} }
foreach($legacy in @('SafeArrayCreateVector','SafeArrayAccessData','InvokeN(L"apiAddBinary"')) { if($view.Contains($legacy)){throw "FBEview retains insertion implementation: $legacy"} }
if(-not $view.Contains('FbeVisualDom::BubbleUp(node, L"DIV")')) { throw 'Block image insertion no longer relocates the returned DOM node.' }
Write-Host 'Image document inserter boundary passed.'
