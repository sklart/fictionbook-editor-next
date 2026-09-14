<# Keeps Design-mode search runtime separate from CFBEView UI/MSHTML mutation. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$controllerHeaderPath = Join-Path $repoRoot 'src\fbe\search\DesignSearchController.h'
$controllerSourcePath = Join-Path $repoRoot 'src\fbe\search\DesignSearchController.cpp'
$viewHeaderPath = Join-Path $repoRoot 'src\fbe\FBEview.h'
$viewSourcePath = Join-Path $repoRoot 'src\fbe\FBEview.cpp'
$project = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj')
$filters = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj.filters')

foreach ($path in @($controllerHeaderPath, $controllerSourcePath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Design search controller is missing: $path" }
}
foreach ($entry in @('search\DesignSearchController.cpp', 'search\DesignSearchController.h')) {
    if ($project -notmatch [regex]::Escape($entry)) { throw "FBE project does not include $entry" }
    if ($filters -notmatch [regex]::Escape($entry)) { throw "FBE filters do not include $entry" }
}

$controller = (Get-Content -Raw -LiteralPath $controllerHeaderPath) + (Get-Content -Raw -LiteralPath $controllerSourcePath)
foreach ($api in @('Coordinator\(', 'Advance\(', 'SetScope\(', 'ShouldSkipZeroLength\(', 'RecordHit\(', 'SetReplacePreview\(', 'SetReplaceAllCompletion\(')) {
    if ($controller -notmatch $api) { throw "DesignSearchController lacks required runtime API: $api" }
}
if ($controller -match 'FBEview|mainfrm|FBDoc|EditorViewController|CSearchHighlightOverlay|PostMessage|MessageBox') {
    throw 'DesignSearchController unexpectedly depends on view/frame/UI presentation.'
}
if ($controller -match '#define\s+m_|State\s*\(\)\s*\{\s*return\s+m_') {
    throw 'DesignSearchController exposes a transitional state-access layer.'
}

$viewHeader = Get-Content -Raw -LiteralPath $viewHeaderPath
$viewSource = Get-Content -Raw -LiteralPath $viewSourcePath
if ($viewHeader -notmatch 'DesignSearchController\s+m_design_search') {
    throw 'CFBEView does not own the explicit DesignSearchController adapter.'
}
foreach ($oldField in @('m_document_search', 'm_search_document_generation', 'm_find_scope_range', 'm_has_find_scope_range', 'm_last_zero_length_hit', 'm_replace_preview_', 'm_controlled_replace_all_mutation', 'm_replace_all_completion_pending')) {
    if ($viewHeader -match $oldField) { throw "CFBEView still owns duplicated search runtime state: $oldField" }
}
if ($viewSource -match '#define\s+m_') { throw 'CFBEView uses a transitional search-state macro layer.' }
if ($viewSource -notmatch 'm_design_search\.Coordinator\(\)') {
    throw 'CFBEView no longer delegates Design search mapping to its controller.'
}

Write-Host 'FBE Design search boundary passed.'
