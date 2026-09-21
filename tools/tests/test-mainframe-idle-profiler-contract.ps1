$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$runtimeUi = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ui\MainFrameRuntimeUi.inl')
foreach($token in @(
    'struct IdleProfile',
    'StartupTrace::Enabled()',
    'idle-count=%llu',
    'total-ms=%llu',
    'max-ms=%llu',
    'command-state-updates=%llu',
    'selection-context-updates=%llu',
    'selection-context-ms=%llu',
    'command-state-ms=%llu',
    'ui-update-view-cmd-calls=%llu',
    'source-ui-updates=%llu',
    'source-ui-ms=%llu',
	'link-table-attribute-bars-updates=%llu',
	'link-table-attribute-bars-ms=%llu',
    'file-fingerprint-ms=%llu',
    'toolbar-ms=%llu',
    'toolbar-localization-updates=%llu',
    'toolbar-localization-ms=%llu',
    'status-updates=%llu',
    'status-ms=%llu',
    'tree-ms=%llu',
    'spell-ms=%llu',
    'title-ms=%llu',
    'file-fingerprint-checks=%llu',
    'toolbar-updates=%llu',
    'tree-updates=%llu',
    'P410')) {
    if($source.IndexOf($token, [StringComparison]::Ordinal) -lt 0) {
        throw "OnIdle profiler contract is missing '$token'."
    }
}

if($source.IndexOf('const bool profileIdle = StartupTrace::Enabled();', [StringComparison]::Ordinal) -lt 0) {
    throw 'OnIdle profiler must leave the normal path gated by the existing diagnostic trace.'
}
foreach($token in @('g_uiUpdateViewCmdCount', 'g_idleProfile.sourceUpdates', 'g_idleProfile.attributeBarsUpdates', 'g_idleProfile.statusUpdates')) {
    if($source.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Profiler counter is not connected: '$token'." }
}
foreach($token in @('HighlightItemAtPos(m_selection_context.container)', 'g_idleProfile.treeUpdates', 'g_idleProfile.treeMilliseconds')) {
    if($source.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Tree selection profiler is not connected: '$token'." }
}
foreach($token in @('StartupTrace::Enabled()', 'g_idleProfile.toolbarLocalizationUpdates')) {
    if($runtimeUi.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Toolbar-localization profiler is not connected: '$token'." }
}

Write-Host 'Main-frame idle profiler contract passed.'
