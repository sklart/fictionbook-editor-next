<# Checks actual MSHTML BODY/table colors and document invariants across theme switches. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'body-theme'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'body-theme.fb2'
    $report = Join-Path $isolation.Root 'body-theme.tsv'
    $paragraphs = (1..100 | ForEach-Object { "<p>Theme target paragraph $_.</p>" }) -join ''
    $fb2 = '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Body theme</book-title><lang>en</lang></title-info><document-info><author><first-name>T</first-name><last-name>T</last-name></author><date>2026</date><id>body-theme-test</id><version>1.0</version></document-info></description><body><section>' + $paragraphs + '<table><tr><th>Heading</th><td>Cell</td></tr></table></section></body></FictionBook>'
    [IO.File]::WriteAllText($fixture, $fb2, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText("$fixture.open.fb2", $fb2.Replace('Theme target paragraph', 'Opened second paragraph'), [Text.UTF8Encoding]::new($false))
    $before = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'body-theme-runtime'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'body-theme-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
        if($exitCode -ne 0) { throw "BODY theme scenario exited with $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario
    }
    $appearance = Get-Content -Raw -LiteralPath "$report.appearance"
    Write-Host "Combined appearance Undo/Redo:`n$appearance"
    foreach($case in @('light-16-32', 'sepia-24-20', 'custom-32-32')) {
        if($appearance -notmatch ("(?m)^" + [regex]::Escape($case) + "=1\r?$")) { throw "Combined appearance Undo/Redo failed: $case`n$appearance" }
    }
    if($appearance -notmatch '(?m)^dark-32-16=(1|unsupported)\r?$') { throw "Dark combined appearance Undo/Redo failed:`n$appearance" }
    $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    $expected = @(
        @('light-auto', '0', '4278190080', '4278190080'),
        @('dark-auto', '1', '4278190080', '4278190080'),
        @('dark-ui-light-body', '0', '0', '16777215'),
        @('light-ui-dark-body', '1', '16777215', '1315860'),
        @('light-auto-again', '0', '4278190080', '4278190080'),
        @('dark-explicit-again', '0', '0', '16777215'),
        @('named-sepia', '0', '0', '16777215'),
        @('named-latte', '0', '0', '16777215'),
        @('named-custom', '0', '0', '16777215')
    )
    if(@($rows | Where-Object phase -eq 'named-graphite').Count) { $expected += ,@('named-graphite', '0', '0', '16777215') }
    if($rows.Count -ne $expected.Count) { throw "Expected $($expected.Count) BODY phases, got $($rows.Count)." }
    foreach($phase in $expected) {
        $matching = @($rows | Where-Object phase -eq $phase[0])
        if($matching.Count -ne 1) { throw "Missing BODY phase $($phase[0])." }
        $row = $matching[0]
        if($row.table_dark -ne $phase[1] -or $row.configured_fg -ne $phase[2] -or $row.configured_bg -ne $phase[3] -or
           $row.no_important -ne '1' -or $row.state_ok -ne '1') {
            throw "BODY theme mismatch in $($phase[0]): $($row | ConvertTo-Json -Compress)"
        }
        if(-not $row.foreground -or -not $row.background) { throw "BODY colors missing in $($phase[0])." }
    }
    $beforeSave = "$fixture.before-save"
    if((Get-FileHash -LiteralPath $beforeSave -Algorithm SHA256).Hash -ne $before) { throw 'Theme-only changes modified the FB2 file before Save.' }
    $savedXml = [Xml.XmlDocument]::new(); $savedXml.Load($fixture)
    $ns = [Xml.XmlNamespaceManager]::new($savedXml.NameTable); $ns.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $savedParagraphs = @($savedXml.SelectNodes('/fb:FictionBook/fb:body/fb:section/fb:p', $ns))
    if($savedParagraphs.Count -ne 100 -or $savedParagraphs[0].InnerText -cne 'Edited target paragraph 1.') { throw 'Save did not persist the edited first paragraph.' }
    for($i = 1; $i -lt $savedParagraphs.Count; ++$i) { if($savedParagraphs[$i].InnerText -cne "Theme target paragraph $($i + 1).") { throw "Save changed paragraph $($i + 1)." } }
    $table = $savedXml.SelectSingleNode('/fb:FictionBook/fb:body/fb:section/fb:table/fb:tr', $ns)
    if(-not $table -or $table.SelectSingleNode('fb:th', $ns).InnerText -cne 'Heading' -or $table.SelectSingleNode('fb:td', $ns).InnerText -cne 'Cell') { throw 'Save changed the table.' }
    $readerSettings = [Xml.XmlReaderSettings]::new(); $readerSettings.ValidationType = [Xml.ValidationType]::Schema
    $readerSettings.XmlResolver = [Xml.XmlUrlResolver]::new(); $readerSettings.Schemas.XmlResolver = [Xml.XmlUrlResolver]::new()
    [void]$readerSettings.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0', (Join-Path $PSScriptRoot '..\..\runtime\FictionBook.xsd'))
    $reader = [Xml.XmlReader]::Create($fixture, $readerSettings)
    try { while($reader.Read()) {} } finally { $reader.Dispose() }
    $passed = $true
    Write-Host 'BODY themes, document invariants, toolbar Open, Save, search, modes and Undo/Redo passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
