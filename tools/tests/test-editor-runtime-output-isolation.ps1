<# Guards the separation between tracked runtime dependencies and build output. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$scintillaBuild = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\build\build-scintilla.ps1')
$mainBuild = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\build\build.ps1')
$updatePipeline = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\build\apply-third-party-update-and-test.ps1')
$runtimeTest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\tests\test-xml-source-cache-runtime.ps1')

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

Write-Host 'Editor runtime output isolation contract passed.'
