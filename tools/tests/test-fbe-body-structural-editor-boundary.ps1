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
    if($view -notmatch "BodyStructuralEditor editor\(Document\(\), m_mk_srv\);\s*const FbeStructure::StructuralOperationResult result = editor\.$name\(fCheck\);\s*if \(!fCheck && FAILED\(result.error\)\) U::ReportError\(result.error\);\s*return result.IsApplied\(\);") { throw "CFBEView::$name does not surface BodyStructuralEditor failures at the UI boundary." }
}
if($header -notmatch 'StructuralOperationResult SplitContainer\(bool checkOnly\);') { throw 'BodyStructuralEditor does not expose SplitContainer result.' }
if($header -notmatch 'bool documentChanged;') { throw 'Structural operation result does not report committed document changes.' }
Write-Host 'BodyStructuralEditor boundary passed.'
