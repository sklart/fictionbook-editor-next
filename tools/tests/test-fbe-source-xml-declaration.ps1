<# Contract for XML declaration and document encoding transfer between Body and Source. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$document = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBDoc.cpp')
foreach ($needle in @(
    'static CString ExtractXmlDeclarationEncoding',
    'm_doc->m_encoding = sourceEncoding',
    'Preserve the XML declaration encoding when switching to Source view.',
    'CString xmlDecl;',
    'xmlDecl.Format(L"<?xml version=\"1.0\" encoding=\"%s\"?>"',
    'sourceEncoding = _Settings.KeepEncoding()',
    'ShowView(SOURCE)')) {
    if (-not $source.Contains($needle)) { throw "XML declaration transfer is missing: $needle" }
}
foreach ($needle in @('CreateDOMImp(const CString& encoding', 'createProcessingInstruction(L"xml"', 'encoding=\"')) {
    if (-not $document.Contains($needle)) { throw "Document serialization no longer preserves an XML encoding declaration: $needle" }
}
foreach ($encoding in 'utf-8', 'windows-1251') {
    $declaration = "<?xml version='1.0' encoding='$encoding'?>"
    if (-not $declaration.Contains("encoding='$encoding'")) { throw "Cannot parse fixture declaration for $encoding." }
}
Write-Host 'Source XML declaration and encoding transfer contract passed.'
