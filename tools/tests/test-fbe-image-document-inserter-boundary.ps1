[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source=Get-Content -Raw (Join-Path $root 'src\fbe\image\ImageDocumentInserter.cpp')
$header=Get-Content -Raw (Join-Path $root 'src\fbe\image\ImageDocumentInserter.h')
$view=Get-Content -Raw (Join-Path $root 'src\fbe\FBEview.cpp')
foreach($forbidden in @('CFBEView','FBEview.h','CMainFrame','mainfrm.h','Settings','ModernFileDialog')) { if($source.Contains($forbidden) -or $header.Contains($forbidden)){throw "Forbidden dependency: $forbidden"} }
foreach($required in @('SafeArrayCreateVector','SafeArrayAccessData','apiAddBinary','FillCoverList','InsImage','InsInlineImage','ImagePlacement')) { if(-not $source.Contains($required)){throw "Missing insertion contract: $required"} }
foreach($legacy in @('SafeArrayCreateVector','SafeArrayAccessData','InvokeN(L"apiAddBinary"')) { if($view.Contains($legacy)){throw "FBEview retains insertion implementation: $legacy"} }
Write-Host 'Image document inserter boundary passed.'
