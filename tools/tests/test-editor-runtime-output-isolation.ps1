<# Guards the separation between tracked runtime dependencies and build output. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$scintillaBuild = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\build\build-scintilla.ps1')
$mainBuild = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\build\build.ps1')
$updatePipeline = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\build\apply-third-party-update-and-test.ps1')
$runtimeTest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\tests\test-xml-source-cache-runtime.ps1')
$trackedRuntimeGuard = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\tests\test-tracked-editor-runtime-clean.ps1')

if ($scintillaBuild -notmatch '\[switch\]\$UpdateTrackedRuntime') {
    throw 'build-scintilla.ps1 must require an explicit switch before updating tracked runtime DLLs.'
}
if ($scintillaBuild -notmatch 'if \(\$UpdateTrackedRuntime\)') {
    throw 'Tracked runtime DLL copying must be gated by UpdateTrackedRuntime.'
}
if ($scintillaBuild -match '(?s)if \(\$ReusePreparedRuntime -and \(Test-PreparedRuntimeFingerprint\)\) \{\s*foreach .*?\$runtimeDir') {
    throw 'A prepared runtime cache must not overwrite tracked runtime DLLs by default.'
}
if ($mainBuild -notmatch 'function Copy-EditorRuntimeToDevelopmentOutput' -or
    $mainBuild -notmatch '-EditorRuntimeDirectory \$editorRuntimeDirectory' -or
    $mainBuild -notmatch '-OutputDirectory \$commonOutput') {
    throw 'The normal build must copy editor runtime DLLs only into its development output.'
}
if ($updatePipeline -notmatch 'build-scintilla\.ps1"\) -UpdateTrackedRuntime') {
    throw 'Only the explicit Scintilla/Lexilla update pipeline may update tracked runtime DLLs.'
}
if ($runtimeTest -match 'runtime\\Scintilla\.dll' -or
    $runtimeTest -notmatch 'out\\editor-runtime') {
    throw 'Runtime smoke tests must consume the built editor runtime, not tracked dependency DLLs.'
}
if ($trackedRuntimeGuard -notmatch 'git -C \$repoRoot diff --exit-code -- @trackedRuntimeDlls') {
    throw 'The tracked editor runtime guard must use git diff --exit-code on both DLLs.'
}
if ($mainBuild -notmatch 'test-tracked-editor-runtime-clean\.ps1') {
    throw 'The ordinary build must verify that tracked editor runtime DLLs remain unchanged.'
}
if ((Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\build\verify-release.ps1')) -notmatch 'test-tracked-editor-runtime-clean\.ps1') {
    throw 'FAST validation must verify that tracked editor runtime DLLs remain unchanged.'
}

Write-Host 'Editor runtime output isolation contract passed.'
