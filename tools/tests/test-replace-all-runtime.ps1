#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe,
      [ValidateSet('none', 'before-first', 'after-one', 'middle', 'format', 'format-command', 'format-italic-command', 'end')][string]$Fault = 'none',
      [ValidateSet('digits', 'no-match', 'literal', 'zero-length', 'stale-preview', 'scope-selection', 'scope-section', 'cross-inline', 'format-success', 'format-italic')][string]$Mode = 'digits',
      [string]$ArtifactDirectory)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $ArtifactDirectory) { $ArtifactDirectory = Join-Path $root 'build\community-feedback-replace-runtime' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'replace-all'
$names = @('FBE_NEXT_TEST_MODE', 'FBE_NEXT_TEST_SCENARIO', 'FBE_NEXT_TEST_REPLACE_OUTPUT', 'FBE_NEXT_TEST_REPLACE_FAULT', 'FBE_NEXT_TEST_REPLACE_MODE')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name) }
$passed = $false
try {
    New-IsolatedFbeSettingsSeed -Isolation $iso | Out-Null
    $fixture = Join-Path $iso.DataPath 'input.fb2'
    $fb2 = @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
<description><title-info><genre>prose</genre><author><first-name>Test</first-name><last-name>Author</last-name></author><book-title>Replace test</book-title><lang>ru</lang></title-info><document-info><author><first-name>Test</first-name><last-name>Author</last-name></author><date>2026</date><id>replace-test</id><version>1.0</version></document-info></description>
<body><section id="section-one"><title><p>1.0 — создание файла</p></title><section id="nested"><title><p>Раздел 6</p></title><p id="main-p">До <emphasis>12</emphasis> и <strong>3</strong>: blank.fb2 <a l:href="#note-a" type="note">4</a>.</p><p id="format-boundary">A<emphasis>BC</emphasis>D</p><empty-line/><p>Пробел здесь 5.</p><p>7 в начале, середина 8.</p></section></section><section id="section-two"><p>9 второй раздел.</p></section></body>
<body name="notes"><section id="note-a"><title><p>Примечание 10</p></title><p>Текст <emphasis>11</emphasis> и <a l:href="#section-one">ссылка 12</a>.</p></section></body>
</FictionBook>
'@
    [IO.File]::WriteAllText($fixture, $fb2, [Text.UTF8Encoding]::new($false))
    $settings = [Xml.XmlReaderSettings]::new(); $settings.ValidationType = [Xml.ValidationType]::Schema
    $settings.Schemas.XmlResolver = [Xml.XmlUrlResolver]::new()
    [void]$settings.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0', (Join-Path $root 'runtime\FictionBook.xsd'))
    $reader = [Xml.XmlReader]::Create($fixture, $settings)
    try { while ($reader.Read()) {} } finally { $reader.Dispose() }
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'replace-all-runtime'
    $env:FBE_NEXT_TEST_REPLACE_OUTPUT = $iso.DataPath
    $env:FBE_NEXT_TEST_REPLACE_FAULT = if ($Fault -eq 'none') { $null } else { $Fault }
    $env:FBE_NEXT_TEST_REPLACE_MODE = $Mode
    if ($Mode -ne 'digits' -and $Fault -ne 'none') { throw 'Fault injection requires digits mode.' }
    $report = Join-Path $iso.DataPath 'report.txt'
    $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden `
        -ArgumentList @('--portable', '-b', ('"' + $report + '"'), ('"' + $fixture + '"')) -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'replace-all-runtime' -Report $report -TimeoutSeconds 120
    $text = if (Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing>' }
    $ArtifactDirectory = Join-Path ([IO.Path]::GetFullPath($ArtifactDirectory)) $(if ($Mode -eq 'digits') { $Fault } else { $Mode })
    New-Item -ItemType Directory -Force -Path $ArtifactDirectory | Out-Null
    foreach ($name in @('report.txt', 'before.fb2', 'after.fb2')) {
        $path = Join-Path $iso.DataPath $name
        if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination (Join-Path $ArtifactDirectory $name) -Force }
    }
    if ($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Native Replace All failed: $text" }
    if ($Mode -in @('cross-inline', 'format-success', 'format-italic') -and $text -notmatch '(?m)^count=1\r?$') { throw "Expected one match: $text" }
    $before = [Xml.XmlDocument]::new(); $before.Load((Join-Path $iso.DataPath 'before.fb2'))
    $after = [Xml.XmlDocument]::new(); $after.Load((Join-Path $iso.DataPath 'after.fb2'))
    $ns = [Xml.XmlNamespaceManager]::new($before.NameTable); $ns.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $afterNs = [Xml.XmlNamespaceManager]::new($after.NameTable); $afterNs.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    if ($Mode -eq 'cross-inline') {
        $target = $before.SelectSingleNode('//fb:p[@id="format-boundary"]', $ns)
        if (-not $target -or $target.InnerText -cne 'ABCD') { throw 'Invalid formatting-boundary fixture.' }
        $target.FirstChild.Value = 'xy'
        $target.SelectSingleNode('fb:emphasis', $ns).InnerText = 'C'
    }
    if ($Mode -in @('format-success', 'format-italic')) {
        $target = $before.SelectSingleNode('//fb:section[@id="section-two"]/fb:p', $ns)
        if (-not $target -or -not $target.FirstChild.Value.StartsWith('9')) { throw 'Invalid format-success fixture.' }
        $target.FirstChild.Value = $target.FirstChild.Value.Substring(1)
        $tag = if ($Mode -eq 'format-italic') { 'emphasis' } else { 'strong' }
        $strong = $before.CreateElement($tag, 'http://www.gribuser.ru/xml/fictionbook/2.0')
        $strong.InnerText = '+'
        [void]$target.PrependChild($strong)
    }
    $beforeText = @($before.SelectNodes('/fb:FictionBook/fb:body//text()', $ns))
    $afterText = @($after.SelectNodes('/fb:FictionBook/fb:body//text()', $afterNs))
    if ($Mode -eq 'stale-preview') {
        $target = $before.SelectSingleNode('//fb:p[@id="main-p"]/text()[contains(., "blank.fb2")]', $ns)
        if (-not $target) { throw 'Missing stale preview fixture text.' }
        $target.Value = $target.Value.Replace('blank.fb2', '!blank.fb2')
    }
    if ($beforeText.Count -ne $afterText.Count) { throw 'Text-node structure changed.' }
    $replaced = if ($Mode -in @('cross-inline', 'format-success', 'format-italic')) { 1 } else { 0 }
    for ($i = 0; $i -lt $beforeText.Count; ++$i) {
        $original = $beforeText[$i].Value; $actual = $afterText[$i].Value
        if ($Mode -eq 'zero-length') {
            $expected = $original.Replace('blank', '+blank')
            if ($actual -cne $expected) { throw 'Unexpected zero-length replacement.' }
            if ($expected -cne $original) { ++$replaced }
            $beforeText[$i].Value = $expected
            continue
        }
        if ($original.Length -ne $actual.Length) { throw 'Text length changed unexpectedly.' }
        for ($j = 0; $j -lt $original.Length; ++$j) {
            if ($original[$j] -ceq $actual[$j]) { continue }
            if ($original[$j] -notmatch '[0-9]' -or $actual[$j] -cne '+') { throw 'Unexpected text mutation.' }
            if ($Mode -eq 'literal' -and $original[$j] -cne '2') { throw 'Literal mode changed a different digit.' }
            if ($Mode -eq 'scope-selection' -and ($original[$j] -cne '2' -or $original -notlike '*blank.fb2*')) { throw 'Replacement escaped selected text.' }
            if ($Mode -eq 'scope-section' -and ($original[$j] -cne '9' -or $original -notlike '9*')) { throw 'Replacement escaped current section.' }
            ++$replaced
        }
        $beforeText[$i].Value = $actual
    }
    $expectedCount = switch ($Mode) { 'no-match' { 0 } 'stale-preview' { 0 } 'cross-inline' { 1 } 'format-success' { 1 } 'format-italic' { 1 } 'literal' { 3 } 'zero-length' { 1 } 'scope-selection' { 1 } 'scope-section' { 1 } default { switch ($Fault) { 'before-first' { 0 } 'after-one' { 1 } 'middle' { 9 } 'format' { 1 } 'format-command' { 1 } 'format-italic-command' { 1 } default { 18 } } } }
    if ($replaced -ne $expectedCount) { throw "Expected $expectedCount replacements, got $replaced." }
    if ($before.OuterXml -cne $after.OuterXml) { throw 'Saved FB2 differs from independently transformed original DOM.' }
    if ($Mode -eq 'digits' -and $Fault -in @('none', 'end') -and ($after.SelectSingleNode('//fb:title/fb:p', $afterNs).InnerText -cne '+.+ — создание файла' -or
        $after.SelectSingleNode('//fb:p[@id="main-p"]', $afterNs).InnerText -notlike '*blank.fb+*')) { throw 'Expected text replacements are absent.' }
    $reader = [Xml.XmlReader]::Create((Join-Path $iso.DataPath 'after.fb2'), $settings)
    try { while ($reader.Read()) {} } finally { $reader.Dispose() }
    $passed = $true
    $history = if ($Mode -eq 'stale-preview') { 'Undo/Redo not tested for external edit' } elseif ($Fault -eq 'before-first' -or $Mode -eq 'no-match') { 'Undo/Redo not needed without mutation' } else { 'Undo/Redo verified' }
    [IO.File]::AppendAllText((Join-Path $ArtifactDirectory 'report.txt'), "validated-replacements=$replaced`nindependent-xml=1`nxsd=1`n")
    Write-Host "Replace All native DOM/XML, $history, Save/Reopen and XSD: PASS ($Fault; replacements=$replaced)."
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name]) }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
