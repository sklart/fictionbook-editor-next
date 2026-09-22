<# Verifies the O(depth) sourceIndex selection path and its guarded fallback. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$header = Get-Content -Raw (Join-Path $root 'src\fbe\TreeView.h')
$source = Get-Content -Raw (Join-Path $root 'src\fbe\TreeView.cpp')
foreach($token in @('std::map<long, HTREEITEM>', 'm_source_index', 'RebuildSourceIndex', 'IndexTreeItem', 'm_tree_index_lookup_count', 'm_tree_linear_fallback_count')) {
    if($header -notmatch [regex]::Escape($token) -and $source -notmatch [regex]::Escape($token)) { throw "Document-tree index contract is missing: $token" }
}
$lookup = [regex]::Match($source, 'CTreeItem CTreeView::LocatePosition[\s\S]*?(?=void\s+CTreeView::IndexTreeItem)').Value
if(-not $lookup -or $lookup -notmatch 'm_source_index\.find\(current->sourceIndex\)' -or $lookup -notmatch 'current = current->parentElement') { throw 'Selection lookup does not walk sourceIndex ancestors.' }
if($lookup.IndexOf('m_source_index.find') -gt $lookup.IndexOf('SearchUnder')) { throw 'Linear SearchUnder runs before the indexed lookup.' }
if($lookup -notmatch '\+\+m_tree_linear_fallback_count') { throw 'Linear fallback is not diagnosed.' }
foreach($method in @('GetDocumentStructure', 'UpdateDocumentStructure')) { if(([regex]::Match($source, "void\s+CTreeView::$method[\s\S]*?(?=\nvoid|\z)").Value) -notmatch 'RebuildSourceIndex\(\)') { throw "$method does not rebuild the source index." } }
Write-Host 'Document tree sourceIndex contract passed.'
