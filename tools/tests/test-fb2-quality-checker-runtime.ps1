[CmdletBinding()]
param(
    [string]$FbeExe,
    [string]$ArtifactDirectory,
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
if (-not $ArtifactDirectory) { $ArtifactDirectory = Join-Path $PSScriptRoot '..\..\out\tests\fb2-quality-ui' }
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$qualityIconPath = Join-Path $repositoryRoot 'src\fbe\res\Fb2Quality.ico'
Add-Type -AssemblyName System.Drawing
$iconBytes = [IO.File]::ReadAllBytes($qualityIconPath)
$iconCount = [BitConverter]::ToUInt16($iconBytes, 4)
$iconSizes = @{}
for ($index = 0; $index -lt $iconCount; $index++) {
    $entry = 6 + 16 * $index
    $iconSizes[[int]$iconBytes[$entry]] = $true
}
foreach ($size in 32, 40, 48, 64) {
    if (-not $iconSizes.ContainsKey($size)) { throw "FB2 quality icon is missing a $size px source." }
    $entry = 6 + 16 * (0..($iconCount - 1) | Where-Object { $iconBytes[6 + 16 * $_] -eq $size } | Select-Object -First 1)
    $offset = [BitConverter]::ToUInt32($iconBytes, $entry + 12)
    $length = [BitConverter]::ToUInt32($iconBytes, $entry + 8)
    $stream = [IO.MemoryStream]::new($iconBytes, [int]$offset, [int]$length)
    try {
        if ($size -eq 32) {
            $icon = [Drawing.Icon]::new($qualityIconPath, [Drawing.Size]::new($size, $size))
            try { $bitmap = $icon.ToBitmap() } finally { $icon.Dispose() }
        } else {
            # .NET Framework's Icon.ToBitmap cannot decode PNG ICO entries above 32 px.
            $bitmap = [Drawing.Bitmap]::new($stream)
        }
        try {
            if ($bitmap.Width -ne $size -or $bitmap.Height -ne $size -or $bitmap.GetPixel(0, 0).A -ne 0) {
                throw "FB2 quality icon has invalid size or alpha at $size px."
            }
        } finally { $bitmap.Dispose() }
    } finally { $stream.Dispose() }
}
$catalog = Get-Content -LiteralPath (Join-Path $repositoryRoot 'localization\app-ui\fbe-small-dialogs.json') -Raw | ConvertFrom-Json -AsHashtable
$locales = (Get-Content -LiteralPath (Join-Path $repositoryRoot 'localization\app-ui\catalog.json') -Raw | ConvertFrom-Json -AsHashtable).targetLanguages
$runtimeRoot = Join-Path (Split-Path -Parent $FbeExe) 'Lang'
foreach ($locale in $locales) {
    $runtimeFile = Join-Path $runtimeRoot "$locale\fbe.json"
    if (-not (Test-Path -LiteralPath $runtimeFile)) { throw "Runtime locale missing: $locale" }
    $runtime = (Get-Content -LiteralPath $runtimeFile -Raw | ConvertFrom-Json -AsHashtable).strings
    foreach ($key in 'fbe.quality.analysis.progress', 'fbe.quality.analysis.cancel', 'fbe.quality.analysis.cancelling', 'fbe.quality.report.clean', 'fbe.file_filter.books_archives', 'fbe.file_filter.books', 'fbe.file_filter.archives', 'fbe.file_filter.all', 'fbe.file_filter.fb2', 'fbe.file_filter.fbd') {
        $expected = $catalog.strings[$key].translations[$locale]
        if ([string]::IsNullOrWhiteSpace($expected) -or $runtime[$key] -cne $expected) { throw "Runtime locale mismatch: $locale/$key" }
    }
}
$ArtifactDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ArtifactDirectory)
New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-quality-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $fixture = Join-Path $root 'fixture.fb2'
    $report = Join-Path $root 'report.txt'
@'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
  <description><title-info><genre>prose</genre><author><first-name>Test</first-name><last-name>Author</last-name></author><book-title>Quality smoke</book-title><lang>en</lang></title-info><document-info><author><first-name>Test</first-name><last-name>Author</last-name></author><id>quality-smoke</id><version>1.0</version></document-info></description>
  <body><section><p id="quality-para">Unchanged editor document.</p><p>See <a l:href="#quality-note" type="note">Note</a>.</p><table id="quality-table"><tr><td>one</td><td>two</td></tr></table></section></body>
  <body name="notes"><section id="quality-note"><p>Note text.</p></section></body>
  <binary id="quality-image" content-type="image/png">AQID</binary>
</FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $fixtureHash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
    $oldMode, $oldScenario, $oldTrace, $oldSettings, $oldArtifacts, $oldBreadcrumb = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE, $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY, $env:FBE_NEXT_TEST_ARTIFACT_DIR, $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'fb2-quality-checker-runtime'
        $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $root
        $env:FBE_NEXT_TEST_ARTIFACT_DIR = $ArtifactDirectory
        $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB = Join-Path $root 'startup-breadcrumb.txt'
        $env:FBE_NEXT_TRACE = '1'
        $process = Start-Process -FilePath $FbeExe -ArgumentList '--portable','-b',("`"$report`""),("`"$fixture`"") -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $process.Id -Force
            $phase = if (Test-Path -LiteralPath $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB) { (Get-Content -LiteralPath $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB -Tail 1) } else { '<no startup breadcrumb>' }
            throw "FB2 quality runtime timed out after: $phase"
        }
        if ($process.ExitCode -ne 0) { $detail = if (Test-Path $report) { Get-Content $report -Raw } else { '<report missing>' }; throw "FB2 quality runtime failed: $($process.ExitCode); $detail" }
    } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE, $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY, $env:FBE_NEXT_TEST_ARTIFACT_DIR, $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB = $oldMode, $oldScenario, $oldTrace, $oldSettings, $oldArtifacts, $oldBreadcrumb }
    $rows = @{}
    Get-Content -LiteralPath $report | ForEach-Object { $pair = $_ -split '=', 2; if ($pair.Count -eq 2) { $rows[$pair[0]] = $pair[1] } }
    if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $fixtureHash) { throw 'FB2 quality runtime changed the original fixture.' }
    foreach ($key in 'unchanged', 'undo_preserved', 'binary_table_preserved', 'snapshot_current', 'snapshot_note_href', 'snapshot_note_accepted', 'failed_snapshot_isolated', 'subsequent_save', 'links', 'binaries', 'metadata', 'empty', 'malformed', 'xlink_rules', 'note_graph', 'binary_rules', 'structure_rules', 'nesting_rules', 'valid_metadata', 'valid_image_placements', 'large_document', 'cancellation', 'progress_completion', 'exact_links', 'exact_id', 'missing_attribute', 'no_stale_jump', 'malformed_end', 'unicode_offset', 'body_to_source', 'unicode_selection', 'locations', 'report_formats', 'report_files', 'report_save_failure', 'dialog_layout', 'dialog_visual', 'empty_dialog_visual', 'open_dialog_visual', 'save_dialog_visual', 'save_fbd_dialog_visual', 'report_save_dialog_visual', 'report_save_html_dialog_visual', 'quality_toolbar', 'quality_toolbar_visual', 'file_menu_localized', 'diagnostic_presentation') {
        if ($rows[$key] -ne '1') { throw "FB2 quality runtime: $key=$($rows[$key])" }
    }
    if ($rows['result'] -ne 'pass') { throw "FB2 quality runtime result=$($rows['result'])" }
    foreach ($name in 'quality-results-after.bmp', 'quality-results-empty-after.bmp', 'file-open-after.bmp', 'file-save-after.bmp', 'file-save-fbd-after.bmp', 'report-save-after.bmp', 'report-save-html-after.bmp', 'quality-toolbar-after.bmp') {
        $screenshot = Join-Path $ArtifactDirectory $name
        $minimumBytes = if ($name -eq 'quality-toolbar-after.bmp') { 2000 } else { 10000 }
        if (-not (Test-Path -LiteralPath $screenshot -PathType Leaf) -or (Get-Item -LiteralPath $screenshot).Length -lt $minimumBytes) { throw "GUI screenshot is missing or empty: $name" }
    }
    if ((Get-FileHash (Join-Path $ArtifactDirectory 'quality-results-after.bmp') -Algorithm SHA256).Hash -eq
        (Get-FileHash (Join-Path $ArtifactDirectory 'quality-results-empty-after.bmp') -Algorithm SHA256).Hash) { throw 'Populated and empty result captures are identical.' }
    if ((Get-FileHash (Join-Path $ArtifactDirectory 'report-save-after.bmp') -Algorithm SHA256).Hash -eq
        (Get-FileHash (Join-Path $ArtifactDirectory 'report-save-html-after.bmp') -Algorithm SHA256).Hash) { throw 'TXT and HTML save dialog captures are identical.' }
    $bitmap = [Drawing.Bitmap]::FromFile((Join-Path $ArtifactDirectory 'quality-results-after.bmp'))
    try {
        $sampleY = [int]($bitmap.Height * 0.55)
        $boundaries = 0
        $previous = $bitmap.GetPixel(20, $sampleY).ToArgb()
        for ($x = 21; $x -lt $bitmap.Width - 20; $x++) {
            $current = $bitmap.GetPixel($x, $sampleY).ToArgb()
            if ($current -ne $previous) { $boundaries++ }
            $previous = $current
        }
        if ($boundaries -gt 20) { throw "Vertical line artifacts remain in empty result area: $boundaries color transitions." }
    } finally { $bitmap.Dispose() }
    Write-Host 'FB2 quality checker runtime passed.'
} finally {
    $resolved = [IO.Path]::GetFullPath($root)
    $temp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolved.StartsWith($temp, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
    }
}
