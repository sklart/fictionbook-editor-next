[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$coordinatorHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\DocumentSearchCoordinator.h')
$coordinatorSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\DocumentSearchCoordinator.cpp')
$adapterHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\SearchDocumentAdapter.h')
$adapterSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\SearchDocumentAdapter.cpp')

foreach ($required in @(
    'void\s+EnsureSnapshot\(MSHTML::IHTMLDocument2Ptr\s+document,\s*std::uint64_t\s+documentGeneration\)',
    'm_snapshotDocument\s*==\s*document',
    'm_snapshot\.DocumentGeneration\s*==\s*documentGeneration',
    'm_adapter\.BuildSnapshot\(document,\s*documentGeneration\)',
    'm_snapshotDocument\s*=\s*NULL',
    'std::unordered_map<std::uint64_t,\s*std::size_t>\s+m_sourceIdIndexes',
    'std::unordered_map<long,\s*std::size_t>\s+m_sourceElementIndexes',
    'm_sourceIdIndexes\.find\(id\)',
    'm_sourceElementIndexes\.find\(element->sourceIndex\)'
)) {
    if (($coordinatorHeader + $coordinatorSource + $adapterHeader + $adapterSource) -notmatch $required) {
        throw "Не реализован контракт кэша search snapshot/index: $required"
    }
}

$rebuild = [regex]::Match($coordinatorSource, '(?s)bool\s+DocumentSearchCoordinator::Rebuild\(.*?(?=void\s+DocumentSearchCoordinator::EnsureSnapshot)').Value
if ($rebuild -notmatch 'EnsureSnapshot\(document,\s*documentGeneration\)' -or $rebuild -match 'm_adapter\.BuildSnapshot') {
    throw 'Rebuild должен переиспользовать snapshot через EnsureSnapshot, а не строить его для каждого query.'
}

Write-Host 'Кэш search snapshot и индексы source ranges закреплены контрактом.'
