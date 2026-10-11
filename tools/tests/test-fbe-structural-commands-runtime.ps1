[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 90,
    [ValidateSet('clone','clone-poem','clone-stanza','clone-cite','clone-epigraph','merge','merge-table','merge-cite','merge-stanza','remove','body','merge-id','merge-mixed','cite-author','remove-metadata','remove-id')]
    [string]$CaseId,
    [switch]$KeepArtifacts
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe was not found: $FbeExe" }
$testRoot = [IO.Path]::GetTempPath()
$directory = Join-Path $testRoot ('fbe-structural-' + [guid]::NewGuid().ToString('N'))
$namespace = 'http://www.gribuser.ru/xml/fictionbook/2.0'
$cases = @(
    @{ id='clone'; body='<section id="clone-source" xml:lang="en"><p>Alpha <strong>Ω</strong></p></section>'; reopen='clone-source' },
    @{ id='clone-poem'; body='<section id="clone-poem-root"><poem id="clone-poem-source" xml:lang="en"><title><p>Poem</p></title><stanza><v>Line</v></stanza></poem></section>'; reopen='clone-poem-root' },
    @{ id='clone-stanza'; body='<section id="clone-stanza-root"><poem><stanza xml:lang="en"><v id="clone-stanza-line">Line</v></stanza></poem></section>'; reopen='clone-stanza-root' },
    @{ id='clone-cite'; body='<section id="clone-cite-root"><cite id="clone-cite-source" xml:lang="en"><p>Quote</p></cite></section>'; reopen='clone-cite-root' },
    @{ id='clone-epigraph'; body='<section id="clone-epigraph-root"><epigraph id="clone-epigraph-source"><p>Quote</p></epigraph><p>Text</p></section>'; reopen='clone-epigraph-root' },
    @{ id='merge'; body='<section id="merge-first"><p>First <strong>Ω</strong></p></section><section><p>Second</p></section>'; reopen='merge-first' },
    @{ id='merge-table'; body='<section id="merge-table-first"><p>First</p></section><section><table><tr><td>Cell</td></tr></table></section>'; reopen='merge-table-first' },
    @{ id='merge-cite'; body='<section id="merge-cite-root"><cite id="merge-cite-first"><p>First</p></cite><cite><p>Second</p><text-author>Author</text-author></cite></section>'; reopen='merge-cite-root' },
    @{ id='merge-stanza'; body='<section id="merge-stanza-root"><poem><stanza><v id="merge-stanza-line">First</v></stanza><stanza><v>Second</v></stanza></poem></section>'; reopen='merge-stanza-root' },
    @{ id='remove'; body='<section><section id="inner-first"><p>First</p></section><section id="inner-second"><p>Second</p></section></section>'; reopen='inner-first' },
    @{ id='body'; body='<section><section id="inner-first"><p>First</p></section></section>' },
    @{ id='merge-id'; body='<section id="merge-id-first"><p><a l:href="#merge-id-second">First</a></p></section><section id="merge-id-second"><p>Second</p></section>' },
    @{ id='merge-mixed'; body='<section id="merge-mixed-first"><p>Text</p></section><section><section><p>Nested</p></section></section>' },
    @{ id='cite-author'; body='<section><cite id="cite-first"><p>First</p><text-author>Author</text-author></cite><cite><p>Second</p></cite></section>' },
    @{ id='remove-metadata'; body='<section><title><p>Title</p></title><epigraph><p>Quote</p></epigraph><image l:href="#cover"/><annotation><p>Note</p></annotation><section id="metadata-inner"><p>Text</p></section></section>' },
    @{ id='remove-id'; body='<section><p><a l:href="#outer-id">Backlink</a></p></section><section id="outer-id"><section id="id-inner"><p>Text</p></section></section>' }
)

function Run-Fbe([string]$Scenario, [string]$Action, [string]$Report, [string]$Fixture, [string]$ReopenId) {
    $previous = @($env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_STRUCTURE_ACTION, $env:FBE_NEXT_TEST_SPLIT_CONTAINER_ID, $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY, $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB)
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = $Scenario
        $env:FBE_NEXT_TEST_STRUCTURE_ACTION = $Action
        $env:FBE_NEXT_TEST_SPLIT_CONTAINER_ID = $ReopenId
        $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $directory
        $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB = Join-Path $directory ($Action + '.startup.txt')
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $Report, $Fixture) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $process.Id -Force
            $phase = if(Test-Path -LiteralPath $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB) { Get-Content -LiteralPath $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB -Tail 1 } else { '<no startup breadcrumb>' }
            throw "FBE timed out: $Scenario / $Action after $phase"
        }
        if($process.ExitCode -ne 0) { throw "FBE failed: $Scenario / $Action, exit $($process.ExitCode)" }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_STRUCTURE_ACTION, $env:FBE_NEXT_TEST_SPLIT_CONTAINER_ID, $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY, $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB = $previous
    }
    $row = Import-Csv -LiteralPath $Report -Delimiter "`t"
    if(@($row).Count -ne 1 -or $row.result -ne 'pass') { throw "Runtime assertion failed: $Scenario / $Action : $($row | ConvertTo-Json -Compress)" }
    return $row
}

function Assert-Schema([string]$Fixture) {
    $cache = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $cache.add($namespace, (Join-Path $root 'runtime\FictionBook.xsd'))
    $document = New-Object -ComObject Msxml2.DOMDocument.6.0
    $document.async = $false
    if(-not $document.load($Fixture)) { throw "Could not load FB2: $($document.parseError.reason)" }
    $document.schemas = $cache
    $validation = $document.validate()
    if($validation.errorCode -ne 0) { throw "FB2 XSD failed: $($validation.reason)" }
}

try {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    # A readable settings document avoids the legacy serializer's modal error
    # path on a completely empty per-test settings directory.
    [IO.File]::WriteAllText((Join-Path $directory 'Settings.xml'), '<FBE><Settings ID="0"/></FBE>', (New-Object Text.UTF8Encoding($false)))
    $selected = @($cases | Where-Object { -not $CaseId -or $_.id -eq $CaseId })
    $passed = 0
    foreach($case in $selected) {
        $fixture = Join-Path $directory ($case.id + '.fb2')
        $report = Join-Path $directory ($case.id + '.tsv')
        $body = $case.body
        $xml = "<?xml version=`"1.0`" encoding=`"utf-8`"?><FictionBook xmlns=`"$namespace`" xmlns:l=`"http://www.w3.org/1999/xlink`"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Structural</book-title><lang>en</lang></title-info><document-info><author><first-name>T</first-name><last-name>T</last-name></author><program-used>test</program-used><date>2026-10-11</date><id>structural-$($case.id)</id><version>1.0</version></document-info></description><body>$body</body><binary id=`"cover`" content-type=`"image/png`">iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aO7sAAAAASUVORK5CYII=</binary></FictionBook>"
        [IO.File]::WriteAllText($fixture, $xml, (New-Object Text.UTF8Encoding($false)))
        Assert-Schema $fixture
        $row = Run-Fbe 'structural-commands' $case.id $report $fixture $null
        if($row.check_unchanged -ne '1') { throw "Preflight changed the DOM: $($case.id)" }
        if($case.ContainsKey('reopen')) {
            foreach($field in @('check_allowed','applied','changed','undo_restored','redo_restored','saved')) {
                if($row.$field -ne '1') { throw "$($case.id): $field failed" }
            }
            Assert-Schema $fixture
            [xml]$saved = Get-Content -Raw -LiteralPath $fixture
            $sections = @($saved.SelectNodes('//*[local-name()="body"]/*[local-name()="section"]'))
            switch($case.id) {
                clone {
                    if($sections.Count -ne 2 -or @($saved.SelectNodes('//*[@id="clone-source"]')).Count -ne 1 -or $sections[1].GetAttribute('id') -or $sections[1].GetAttribute('lang','http://www.w3.org/XML/1998/namespace') -ne 'en') { throw 'Clone ID or xml:lang contract failed.' }
                }
                'clone-poem' {
                    $poems = @($saved.SelectNodes('//*[local-name()="poem"]'))
                    if($poems.Count -ne 2 -or @($saved.SelectNodes('//*[@id="clone-poem-source"]')).Count -ne 1 -or $poems[1].GetAttribute('id') -or $poems[1].GetAttribute('lang','http://www.w3.org/XML/1998/namespace') -ne 'en' -or @($poems[1].SelectNodes('./*[local-name()="stanza"]/*[local-name()="v"]')).Count -ne 1) { throw 'Poem clone contract failed.' }
                }
                'clone-stanza' {
                    $stanzas = @($saved.SelectNodes('//*[local-name()="stanza"]'))
                    if($stanzas.Count -ne 2 -or @($saved.SelectNodes('//*[@id="clone-stanza-line"]')).Count -ne 1 -or $stanzas[1].GetAttribute('lang','http://www.w3.org/XML/1998/namespace') -ne 'en' -or @($stanzas[1].SelectNodes('./*[local-name()="v"]')).Count -ne 1) { throw 'Stanza clone contract failed.' }
                }
                'clone-cite' {
                    $cites = @($saved.SelectNodes('//*[local-name()="cite"]'))
                    if($cites.Count -ne 2 -or @($saved.SelectNodes('//*[@id="clone-cite-source"]')).Count -ne 1 -or $cites[1].GetAttribute('id') -or $cites[1].GetAttribute('lang','http://www.w3.org/XML/1998/namespace') -ne 'en') { throw 'Cite clone contract failed.' }
                }
                'clone-epigraph' {
                    $epigraphs = @($saved.SelectNodes('//*[local-name()="epigraph"]'))
                    if($epigraphs.Count -ne 2 -or @($saved.SelectNodes('//*[@id="clone-epigraph-source"]')).Count -ne 1 -or $epigraphs[1].GetAttribute('id')) { throw 'Epigraph clone contract failed.' }
                }
                merge {
                    if($sections.Count -ne 1 -or @($sections[0].SelectNodes('./*[local-name()="p"]')).Count -ne 2 -or $sections[0].InnerText -notmatch 'First.*Second') { throw 'Merge content/order contract failed.' }
                }
                merge-table {
                    if($sections.Count -ne 1 -or @($sections[0].SelectNodes('./*[local-name()="table"]')).Count -ne 1 -or $sections[0].InnerText -notmatch 'First.*Cell') { throw 'Merge table contract failed.' }
                }
                'merge-cite' {
                    $cites = @($saved.SelectNodes('//*[local-name()="cite"]'))
                    if($cites.Count -ne 1 -or @($cites[0].SelectNodes('./*[local-name()="p"]')).Count -ne 2 -or @($cites[0].SelectNodes('./*[local-name()="text-author"]')).Count -ne 1 -or $cites[0].InnerText -notmatch 'First.*Second.*Author') { throw 'Merge citation contract failed.' }
                }
                'merge-stanza' {
                    $stanzas = @($saved.SelectNodes('//*[local-name()="stanza"]'))
                    if($stanzas.Count -ne 1 -or @($stanzas[0].SelectNodes('./*[local-name()="v"]')).Count -ne 2 -or $stanzas[0].InnerText -notmatch 'First.*Second') { throw 'Merge stanza contract failed.' }
                }
                remove {
                    if($sections.Count -ne 2 -or $sections[0].GetAttribute('id') -ne 'inner-first' -or $sections[1].GetAttribute('id') -ne 'inner-second') { throw 'Remove Outer hierarchy/order contract failed.' }
                }
            }
            $reopen = Join-Path $directory ($case.id + '.reopen.tsv')
            $null = Run-Fbe 'split-container-reopen' $case.id $reopen $fixture $case.reopen
        } else {
            if($row.check_allowed -ne '0' -or $row.applied -ne '0') { throw "Unsafe operation was enabled: $($case.id)" }
        }
        $passed++
    }
    Write-Host "Structural MSHTML runtime: selected=$($selected.Count) passed=$passed"
} finally {
    if($KeepArtifacts) { Write-Host "Artifacts: $directory" }
    else {
        $safeRoot = [IO.Path]::GetFullPath($testRoot).TrimEnd('\') + '\'
        $safeDirectory = [IO.Path]::GetFullPath($directory)
        if(-not $safeDirectory.StartsWith($safeRoot, [StringComparison]::OrdinalIgnoreCase)) { throw "Unsafe test cleanup path: $safeDirectory" }
        Remove-Item -LiteralPath $safeDirectory -Recurse -Force -ErrorAction SilentlyContinue
    }
}
