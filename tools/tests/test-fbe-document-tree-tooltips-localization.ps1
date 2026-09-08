<# Guards runtime localization of document-tree toolbar tooltips. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\DocumentTree.h') -Encoding UTF8
$catalog = Get-Content -Raw -LiteralPath (Join-Path $root 'localization\app-ui\fbe-secondary-menus.json') -Encoding UTF8 | ConvertFrom-Json
$languages = @($catalog.targetLanguages)
$required = @{
    'fbe.tooltip.document_tree.move_left' = 'ID_DT_LEFT'
    'fbe.tooltip.document_tree.delete' = 'ID_DT_DELETE'
    'fbe.tooltip.document_tree.move_right' = 'ID_DT_RIGHT_ONE'
    'fbe.tooltip.document_tree.make_child' = 'ID_DT_RIGHT_SMART'
    'fbe.tooltip.document_tree.merge' = 'ID_DT_MERGE'
}

foreach ($entry in $required.GetEnumerator()) {
    if ($header -notlike "*$($entry.Key)*" -or $header -notlike "*$($entry.Value)*") {
        throw "Document tree tooltip does not map $($entry.Value) to runtime key $($entry.Key)."
    }
    $translation = $catalog.strings.PSObject.Properties[$entry.Key].Value
    if ($null -eq $translation -or $translation.targetId -ne $entry.Value) {
        throw "Localization catalog does not describe $($entry.Key) for $($entry.Value)."
    }
    foreach ($language in $languages) {
        if ([string]::IsNullOrWhiteSpace([string]$translation.translations.PSObject.Properties[$language].Value)) {
            throw "Missing $language translation for $($entry.Key)."
        }
    }
}
if ($header -match 'TTF_DI_SETITEM') {
    throw 'Document tree tooltip text must not be cached across an interface-language change.'
}
Write-Host 'Document tree toolbar tooltip localization contract passed.'
