[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable',
    [int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'tree-window'
$passed = $false
$report = Join-Path $iso.Root 'tree-window.log'
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$saved = @{}; foreach($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
try {
    $null = New-IsolatedFbeSettingsSeed $iso
    $book = [Text.StringBuilder]::new()
    [void]$book.Append('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose_contemporary</genre><author><first-name>Tree</first-name><last-name>Test</last-name></author><book-title>Nested structure</book-title><lang>en</lang></title-info><document-info><author><nickname>Tree runtime</nickname></author><id>tree-window-test</id><version>1.0</version></document-info></description><body>')
    for($head = 0; $head -lt 12; ++$head) {
        [void]$book.Append("<section id='head$head'><title><p id='head-title$head'>Head $head</p><p id='head-text$head'>Heading text</p></title>")
        for($child = 0; $child -lt 4; ++$child) {
            $childId = $head * 4 + $child
            [void]$book.Append("<section id='child$childId'><title><p>Child $childId</p><p id='child-text$childId'>Child heading</p></title>")
            for($leaf = 0; $leaf -lt 2; ++$leaf) {
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
    $fixture = Join-Path $iso.Root 'nested.fb2'
    [IO.File]::WriteAllText($fixture, $book.ToString(), [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'document-tree-window-runtime'
    if($Mode -eq 'installed') { $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = Join-Path $iso.DataPath 'Settings' }
    else { $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $null }
    Set-IsolatedRuntimeStage $iso "document-tree-window-runtime/$Mode"
    $process = Start-Process $iso.Exe -ArgumentList @("--$Mode", '-b', $report, $fixture) -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru
    $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'document-tree-window-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
    $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
    if($exitCode -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Document Tree window runtime failed ($Mode), exit=$exitCode`n$text" }
    if(([regex]::Matches($text, '(?m)^marker_text_width_theme_\d+=1\r?$')).Count -ne 2) { throw "Document Tree marker width coverage is missing ($Mode)." }
    foreach($check in @('section_title_extraction','title_normal_fast_layout','title_source_roundtrip','title_edit_undo_redo','clone_section_caption_undo_redo','title_save_reopen_xml')) {
        if($text -notmatch ('(?m)^' + [regex]::Escape($check) + '=1\r?$')) { throw "Document Tree title coverage is missing ($Mode): $check" }
    }
    Write-Host $text
    Write-Host "Document Tree window runtime: PASS ($Mode)."
    $passed = $true
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
