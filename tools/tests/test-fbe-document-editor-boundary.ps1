<# Guards the narrow document-to-editor dependency boundary. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$docHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBDoc.h')
$docSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBDoc.cpp')
$hostHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\document\DocumentEditorHost.h')
$hostSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\document\DocumentEditorHost.cpp')
$project = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj')
$filters = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj.filters')

foreach ($entry in @('document\DocumentEditorHost.cpp', 'document\DocumentEditorHost.h')) {
    if ($project -notmatch [regex]::Escape($entry) -or $filters -notmatch [regex]::Escape($entry)) {
        throw "Document editor boundary is not registered: $entry"
    }
}
if ($docHeader -notmatch 'DocumentEditorHost\s+m_editor') {
    throw 'FB::Doc does not retain its explicit document-editor boundary.'
}
if ($docSource -match '\bm_body\s*\.\s*') {
    throw 'FB::Doc still calls CFBEView directly outside DocumentEditorHost.'
}
foreach ($legacyField in @('m_body_ver!=m_body\.', 'm_body_cp!=m_body\.', 'm_body\.IsForm')) {
    if ($docHeader -match $legacyField) { throw "FB::Doc still owns view-specific state access: $legacyField" }
}
if ($hostHeader -match 'mainfrm|FBDoc\.h|DocumentLifecycleController|DocumentSaveController|RecoveryController|MessageBox|PostMessage|SendMessage|CDialog') {
    throw 'DocumentEditorHost unexpectedly depends on application/UI/document-controller concerns.'
}
if ($hostSource -match 'mainfrm|FBDoc\.h|DocumentLifecycleController|DocumentSaveController|RecoveryController|MessageBox|PostMessage|SendMessage|CDialog') {
    throw 'DocumentEditorHost implementation unexpectedly depends on application/UI/document-controller concerns.'
}
if ($hostHeader -notmatch 'GetVersionNumber|IsFormChanged|ResetFormChanged|Document\(\)|LoadTransformedHtml') {
    throw 'DocumentEditorHost lacks required lifecycle/content contract operations.'
}

Write-Host 'FBE document editor boundary passed.'
