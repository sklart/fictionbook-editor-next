[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Text($path) { Get-Content -Raw (Join-Path $root $path) }
function Require($text, $pattern, $message) { if($text -notmatch $pattern) { throw $message } }

$dialogs = Text 'src\fbe\document\ui\DocumentFileDialogs.cpp'
$frame = Text 'src\fbe\mainfrm.cpp'
$project = Text 'src\fbe\FBE.vcxproj'

Require $dialogs 'FictionBook files \(\*\.fb2;\*\.fbd;\*\.zip;\*\.rar\)' 'Open filters must be owned by document/ui.'
Require $dialogs 'FD101' 'Open dialog diagnostic must remain in document/ui.'
Require $dialogs 'FD102' 'Save dialog diagnostic must remain in document/ui.'
Require $dialogs 'overwritePrompt = true' 'Save overwrite prompt must remain in document/ui.'
Require $dialogs 'AddFictionBookExtensionIfMissing' 'Save extension normalization must remain in document/ui.'
Require $dialogs 'filterIndex == 2' 'Save FBD filter mapping must remain in document/ui.'
if($dialogs -match '_Settings|CMainFrame|FB::Doc') { throw 'Document file dialog module must not depend on settings, main frame, or document model.' }
if($frame -match 'FictionBook files \(\*\.fb2;\*\.fbd;\*\.zip;\*\.rar\)') { throw 'Main frame still owns FictionBook open filters.' }
if($frame -match 'IDS_ENCODINGS[\s\S]{0,1200}ModernFileDialog::Show') { throw 'Main frame still configures the Save dialog.' }
Require $frame 'DocumentFileDialogs::ShowOpen' 'Main frame must route Open through the document dialog module.'
Require $frame 'DocumentFileDialogs::ShowSave' 'Main frame must route Save As through the document dialog module.'
Require $frame 'FBE_NEXT_TEST_SAVE_PATH' 'RAR Save As runtime override must remain outside the dialog module.'
Require $project 'document\\ui\\DocumentFileDialogs\.cpp' 'Dialog implementation must be compiled.'
Write-Host 'Document file dialog boundary contract passed.'
