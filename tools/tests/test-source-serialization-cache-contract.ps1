[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceDocumentTransfer.h')
$transfer = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceDocumentTransfer.cpp')
$session = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceViewSession.cpp')

foreach ($required in @(
    'struct\s+SourceSerializedTextCache',
    'MSXML2::IXMLDOMDocumentPtr\s+xml',
    'CString\s+encoding',
	'int\s+documentType\s*=\s*-1',
    'CString\s+text',
    'serializedCache\.xml\s*!=\s*cachedXml',
    'serializedCache\.encoding\s*!=\s*encoding',
	'serializedCache\.documentType\s*!=\s*documentType',
    'serializedCache\.xml\s*=\s*cachedXml',
    'serializedCache\.encoding\s*=\s*encoding',
	'serializedCache\.documentType\s*=\s*documentType',
    'sourceText\s*=\s*serializedCache\.text',
    'm_serializedSourceCache'
)) {
    if (($header + $transfer + $session) -notmatch $required) {
        throw "Не реализован кэш сериализованного SOURCE: $required"
    }
}

if ($transfer -notmatch '(?s)document\.DocRelChanged\(\).*?cachedXml\s*=\s*candidate') {
    throw 'Изменение BODY должно заменять DOM-снимок и тем самым инвалидировать текстовый кэш.'
}

Write-Host 'Кэш сериализованного SOURCE закреплён контрактом.'
