[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable',
    [switch]$TitlesOnly,
    [string]$ArtifactDirectory,
    [switch]$TracePaint,
    [switch]$FocusTree,
    [Alias("LegacyScreenRead")][switch]$BackgroundDiagnostic,
    [int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'tree-window'
$passed = $false
if(!$ArtifactDirectory){$ArtifactDirectory=Join-Path $PSScriptRoot ('../../out/tests/tree-marker/'+$Mode+'-'+[guid]::NewGuid().ToString('N'))}
New-Item -ItemType Directory -Force -Path $ArtifactDirectory | Out-Null
$ArtifactDirectory=(Resolve-Path $ArtifactDirectory).Path
$report = Join-Path $iso.Root 'tree-window.log'
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TEST_ARTIFACT_DIR','FBE_NEXT_TEST_TREE_TRACE_PAINT','FBE_NEXT_TEST_TREE_FOCUS','FBE_NEXT_TEST_TREE_BACKGROUND')
$saved = @{}; foreach($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
try {
    $settingsSeed = New-IsolatedFbeSettingsSeed $iso
    $book = [Text.StringBuilder]::new()
    [void]$book.Append('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose_contemporary</genre><author><first-name>Tree</first-name><last-name>Test</last-name></author><book-title>Nested structure</book-title><lang>en</lang></title-info><document-info><author><nickname>Tree runtime</nickname></author><id>tree-window-test</id><version>1.0</version></document-info></description><body>')
    $heads=if($TitlesOnly){1}else{12};$children=if($TitlesOnly){1}else{4};$leaves=if($TitlesOnly){1}else{2}
    for($head = 0; $head -lt $heads; ++$head) {
        [void]$book.Append("<section id='head$head'><title><p id='head-title$head'>Head $head</p><p id='head-text$head'>Heading text</p></title>")
        for($child = 0; $child -lt $children; ++$child) {
            $childId = $head * 4 + $child
            [void]$book.Append("<section id='child$childId'><title><p>Child $childId</p><p id='child-text$childId'>Child heading</p></title>")
            for($leaf = 0; $leaf -lt $leaves; ++$leaf) {
                $leafId = $childId * 2 + $leaf
                [void]$book.Append("<section id='leaf$leafId'><title><p id='leaf-title$leafId'>Leaf $leafId</p></title><p id='leaf-text$leafId'>Ordinary paragraph $leafId — Юникод</p></section>")
            }
            [void]$book.Append('</section>')
        }
        [void]$book.Append('</section>')
    }
    # The second merge candidate intentionally has no ID or title: the
    # production command must refuse to discard either kind of metadata.
    [void]$book.Append('<section id="merge-owner"><title><p>Merge fixture</p></title><section id="merge-first"><p>First text</p></section><section><p>Second text</p></section></section>')
    [void]$book.Append('<section id="title-unicode"><title><p>СТАДИЯ <strong>ПЕРВАЯ</strong></p><p>Глава <emphasis>первая</emphasis></p></title><p>Text</p></section><section id="title-empty"><title><p> </p></title><p>Text</p></section><section id="title-parent-empty"><section><title><p>Only child title</p></title><p>Text</p></section></section></body></FictionBook>')
    if($TitlesOnly) {
        $book.Replace('</body></FictionBook>', ('<section id="long-title"><title><p>' + ('X' * 600) + '</p></title><p>Long title fixture</p></section></body></FictionBook>')) | Out-Null
    }
    $fixture = Join-Path $iso.Root 'nested.fb2'
    [IO.File]::WriteAllText($fixture, $book.ToString(), [Text.UTF8Encoding]::new($false))
    [ordered]@{
        executable = (Get-FileHash -LiteralPath $iso.Exe -Algorithm SHA256).Hash
        fixture = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
        settings = (Get-FileHash -LiteralPath $settingsSeed -Algorithm SHA256).Hash
        mode = $Mode
        compatibilityLayer = $env:__COMPAT_LAYER
        backgroundDiagnostic = [bool]$BackgroundDiagnostic
        focusTree = [bool]$FocusTree
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $ArtifactDirectory 'inputs.json') -Encoding utf8
    $env:FBE_NEXT_TEST_TREE_BACKGROUND=if($BackgroundDiagnostic){'1'}else{$null}
    $env:FBE_NEXT_TEST_TREE_FOCUS=if($FocusTree){'1'}else{$null}
    $env:FBE_NEXT_TEST_TREE_TRACE_PAINT=if($TracePaint){'1'}else{$null}
    $env:FBE_NEXT_TEST_ARTIFACT_DIR=$ArtifactDirectory
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = if($TitlesOnly){'document-tree-titles-runtime'}else{'document-tree-window-runtime'}
    if($Mode -eq 'installed') { $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = Join-Path $iso.DataPath 'Settings' }
    else { $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $null }
    Set-IsolatedRuntimeStage $iso "document-tree-window-runtime/$Mode"
    $process = Start-Process $iso.Exe -ArgumentList @("--$Mode", '-b', $report, $fixture) -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru
    $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'document-tree-window-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
    $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
    if(Test-Path $report){Copy-Item -LiteralPath $report -Destination (Join-Path $ArtifactDirectory 'runtime.log')}
    if($exitCode -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Document Tree window runtime failed ($Mode), exit=$exitCode`n$text" }
    if(!$TitlesOnly) {
    if(([regex]::Matches($text, '(?m)^marker_text_width_theme_\d+=1\r?$')).Count -ne 2) { throw "Document Tree marker width coverage is missing ($Mode)." }
    foreach($check in @('toggle_100','section_title_extraction','title_normal_fast_layout','title_source_roundtrip','title_edit_undo_redo','clone_section_caption_undo_redo','title_save_reopen_xml')) {
        if($text -notmatch ('(?m)^' + [regex]::Escape($check) + '=1\r?$')) { throw "Document Tree title coverage is missing ($Mode): $check" }
    }
    }
    Write-Host $text
    Write-Host "Document Tree window runtime: PASS ($Mode)."
    $passed = $true
} finally {
    if(Test-Path -LiteralPath $report) { Copy-Item -LiteralPath $report -Destination (Join-Path $ArtifactDirectory 'runtime.log') }
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
