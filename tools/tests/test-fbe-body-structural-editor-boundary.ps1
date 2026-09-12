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
foreach($name in @('InsertCite', 'InsertPoem')) {
    if($view -notmatch "BodyStructuralEditor editor\(Document\(\), m_mk_srv\);\s*return editor\.$name\(fCheck\);") { throw "CFBEView::$name does not delegate to BodyStructuralEditor." }
}
Write-Host 'BodyStructuralEditor boundary passed.'
