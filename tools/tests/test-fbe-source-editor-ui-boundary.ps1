<# Guards the Scintilla presentation boundary while lifecycle remains in CMainFrame. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\ui\SourceEditorControl.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\ui\SourceEditorControl.cpp')
$config = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\ui\SourceEditorConfig.h')
$project = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.vcxproj')

foreach($forbidden in @('mainfrm\.h', 'FBDoc', 'FB::Doc', 'MSHTML', 'DocumentSession', 'CSettings', '_Settings')) {
    if($header -match $forbidden -or $source -match $forbidden -or $config -match $forbidden) { throw "Source editor UI must not depend on $forbidden." }
}
foreach($required in @('class\s+SourceEditorControl', 'Create\s*\(', 'Destroy\s*\(', 'ApplyConfiguration\s*\(', 'UpdateLineNumberMargin\s*\(', 'ConfigureSpecialCharacterRepresentations\s*\(')) {
    if($header -notmatch $required) { throw "Source editor boundary missing: $required" }
}
if($project -notmatch 'source\\ui\\SourceEditorControl\.cpp' -or $project -notmatch 'source\\ui\\SourceEditorConfig\.h') { throw 'FBE project must include SourceEditorControl sources.' }
Write-Host 'Source editor UI boundary contract passed.'
