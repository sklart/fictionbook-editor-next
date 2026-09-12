<# Guards the editor-only boundary of BodyStructuralEditor. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$body = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\structure\BodyStructuralEditor.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\structure\BodyStructuralEditor.h')
$view = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBEview.cpp')
foreach($forbidden in @('CFBEView', 'FBEview.h', 'CMainFrame', 'Settings')) {
    if($body.Contains($forbidden) -or $header.Contains($forbidden)) { throw "BodyStructuralEditor leaks $forbidden." }
}
if($body.Contains('GetEnvironmentVariable')) { throw 'BodyStructuralEditor reads test environment state.' }
if($header -notmatch 'StructuralTrace\* trace = nullptr') { throw 'BodyStructuralEditor trace must remain nullable by default.' }
if($view.Contains('StructuralTrace')) { throw 'Production CFBEView wrappers must not construct StructuralTrace.' }
foreach($name in @('InsertCite', 'InsertPoem', 'SplitContainer')) {
    if($view -notmatch "BodyStructuralEditor editor\(Document\(\), m_mk_srv\);\s*return editor\.$name\(fCheck\);") { throw "CFBEView::$name does not delegate to BodyStructuralEditor." }
}
if($header -notmatch 'bool SplitContainer\(bool checkOnly\);') { throw 'BodyStructuralEditor does not expose SplitContainer.' }
Write-Host 'BodyStructuralEditor boundary passed.'
