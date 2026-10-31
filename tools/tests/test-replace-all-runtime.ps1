[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe,
      [ValidateSet('none', 'before-first', 'after-one', 'middle', 'format', 'end')][string]$Fault = 'none',
      [string]$ArtifactDirectory = (Join-Path $PSScriptRoot '..\..\build\community-feedback-replace-runtime'))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'replace-all'
$names = @('FBE_NEXT_TEST_MODE', 'FBE_NEXT_TEST_SCENARIO', 'FBE_NEXT_TEST_REPLACE_OUTPUT', 'FBE_NEXT_TEST_REPLACE_FAULT')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name) }
$passed = $false
try {
    New-IsolatedFbeSettingsSeed -Isolation $iso | Out-Null
    $fixture = Join-Path $iso.DataPath 'input.fb2'
    $fb2 = @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
<description><title-info><genre>prose</genre><author><first-name>Test</first-name><last-name>Author</last-name></author><book-title>Replace test</book-title><lang>ru</lang></title-info><document-info><author><first-name>Test</first-name><last-name>Author</last-name></author><date>2026</date><id>replace-test</id><version>1.0</version></document-info></description>
<body><section id="section-one"><title><p>1.0 — создание файла</p></title><section id="nested"><title><p>Раздел 6</p></title><p id="main-p">До <emphasis>12</emphasis> и <strong>3</strong>: blank.fb2 <a l:href="#note-a" type="note">4</a>.</p><empty-line/><p>Пробел здесь 5.</p><p>7 в начале, середина 8.</p></section></section><section id="section-two"><p>9 второй раздел.</p></section></body>
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
    $report = Join-Path $iso.DataPath 'report.txt'
    $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden `
        -ArgumentList @('--portable', '-b', ('"' + $report + '"'), ('"' + $fixture + '"')) -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'replace-all-runtime' -Report $report -TimeoutSeconds 120
    $text = if (Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing>' }
    $ArtifactDirectory = Join-Path ([IO.Path]::GetFullPath($ArtifactDirectory)) $Fault
    New-Item -ItemType Directory -Force -Path $ArtifactDirectory | Out-Null
    foreach ($name in @('report.txt', 'before.fb2', 'after.fb2')) {
        $path = Join-Path $iso.DataPath $name
        if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination (Join-Path $ArtifactDirectory $name) -Force }
    }
    if ($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Native Replace All failed: $text" }
    $before = [Xml.XmlDocument]::new(); $before.Load((Join-Path $iso.DataPath 'before.fb2'))
    $after = [Xml.XmlDocument]::new(); $after.Load((Join-Path $iso.DataPath 'after.fb2'))
    $ns = [Xml.XmlNamespaceManager]::new($before.NameTable); $ns.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $afterNs = [Xml.XmlNamespaceManager]::new($after.NameTable); $afterNs.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $beforeText = @($before.SelectNodes('/fb:FictionBook/fb:body//text()', $ns))
    $afterText = @($after.SelectNodes('/fb:FictionBook/fb:body//text()', $afterNs))
    if ($beforeText.Count -ne $afterText.Count) { throw 'Text-node structure changed.' }
    $replaced = 0
    for ($i = 0; $i -lt $beforeText.Count; ++$i) {
        $original = $beforeText[$i].Value; $actual = $afterText[$i].Value
        if ($original.Length -ne $actual.Length) { throw 'Text length changed unexpectedly.' }
        for ($j = 0; $j -lt $original.Length; ++$j) {
            if ($original[$j] -ceq $actual[$j]) { continue }
            if ($original[$j] -notmatch '[0-9]' -or $actual[$j] -cne '+') { throw 'Unexpected text mutation.' }
            ++$replaced
        }
        $beforeText[$i].Value = $actual
    }
    $expectedCount = switch ($Fault) { 'before-first' { 0 } 'after-one' { 1 } 'middle' { 9 } 'format' { 1 } default { 18 } }
    if ($replaced -ne $expectedCount) { throw "Expected $expectedCount replacements, got $replaced." }
    if ($before.OuterXml -cne $after.OuterXml) { throw 'Saved FB2 differs from independently transformed original DOM.' }
    if ($Fault -in @('none', 'end') -and ($after.SelectSingleNode('//fb:title/fb:p', $afterNs).InnerText -cne '+.+ — создание файла' -or
        $after.SelectSingleNode('//fb:p[@id="main-p"]', $afterNs).InnerText -notlike '*blank.fb+*')) { throw 'Expected text replacements are absent.' }
    $reader = [Xml.XmlReader]::Create((Join-Path $iso.DataPath 'after.fb2'), $settings)
    try { while ($reader.Read()) {} } finally { $reader.Dispose() }
    $passed = $true
    $history = if ($Fault -eq 'before-first') { 'Undo/Redo not needed before mutation' } else { 'Undo/Redo verified' }
    [IO.File]::AppendAllText((Join-Path $ArtifactDirectory 'report.txt'), "validated-replacements=$replaced`nindependent-xml=1`nxsd=1`n")
    Write-Host "Replace All native DOM/XML, $history, Save/Reopen and XSD: PASS ($Fault; replacements=$replaced)."
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name]) }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
