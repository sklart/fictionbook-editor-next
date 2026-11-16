<# Exercises the actual Source and Validate commands on invalid FB2 nesting.
   The supplied original is copied into a disposable portable runtime. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FbeExe,
    [string]$OriginalBook,
    [int]$TimeoutSeconds = 45,
    [switch]$KeepArtifacts
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$cases = [ordered]@{
    'valid-control' = '<p>Valid text with <strong>inline formatting</strong>.</p><p>Second paragraph.</p>'
    'subtitle-strong-p' = '<subtitle><strong><p>Nested</p></strong></subtitle>'
    'section-strong-p' = '<strong><p>Nested</p></strong>'
    'subtitle-emphasis-p' = '<subtitle><emphasis><p>Nested</p></emphasis></subtitle>'
    'paragraph-strong-p' = '<p><strong><p>Nested</p></strong></p>'
}
if ($OriginalBook) { $cases['user-book'] = $null }
foreach ($case in $cases.GetEnumerator()) {
    foreach ($operation in 'source', 'validate') {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'issue72'
        $passed = $false
        try {
            $null = New-IsolatedFbeSettingsSeed -Isolation $iso
            $fixture = Join-Path $iso.Root 'input.fb2'
            $report = Join-Path $iso.Root 'report.txt'
            if ($case.Key -eq 'user-book') {
                Copy-Item -LiteralPath $OriginalBook -Destination $fixture
            } else {
                $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Issue72</book-title><lang>en</lang></title-info><document-info><id>issue72-test</id><version>1.0</version></document-info></description><body><section>' + $case.Value + '<p>Neighbour</p></section></body></FictionBook>'
                [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
            }
            $before = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
            $sourceDump = Join-Path $iso.Root 'source-snapshot.fb2'
            $oldMode, $oldScenario, $oldDump = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_ISSUE72_DUMP
            try {
                $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = "issue72-$operation"
                $env:FBE_NEXT_TEST_ISSUE72_DUMP = $sourceDump
                $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList @('--portable', '-b', $report, $fixture) -PassThru -WindowStyle Hidden
                $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario "issue72-$operation" -Report $report -TimeoutSeconds $TimeoutSeconds
            } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_ISSUE72_DUMP = $oldMode, $oldScenario, $oldDump }
            $text = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing>' }
            if ($exitCode -ne 0 -or $text -notmatch '(?m)^stage=command-returned\r?$' -or
                $text -notmatch '(?m)^same-document=1\r?$' -or $text -notmatch '(?m)^dirty-before=1\r?$' -or
                $text -notmatch '(?m)^dirty-after=1\r?$' -or $text -notmatch '(?m)^marker=1\r?$' -or
                $text -notmatch '(?m)^window-enabled=1\r?$' -or $text -notmatch '(?m)^subsequent-edit=1\r?$' -or
                $text -notmatch '(?m)^memory-available=1\r?$') {
                throw "Issue72 $($case.Key)/$operation failed: exit=$exitCode; report=$text"
            }
            if ($text -match '(?m)^source-active=0\r?$' -and $text -notmatch '(?m)^body-unchanged=1\r?$') {
                throw "Issue72 $($case.Key)/$operation changed the visual document during refusal: $text"
            }
            if ($text -match '(?m)^source-active=1\r?$') {
                function Get-BodyText([string]$path) {
                    $settings = [Xml.XmlReaderSettings]::new()
                    $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
                    $settings.XmlResolver = $null
                    $reader = [Xml.XmlReader]::Create($path, $settings)
                    try { $document = [Xml.XmlDocument]::new(); $document.XmlResolver = $null; $document.Load($reader) }
                    finally { $reader.Close() }
                    $bodyNodes = $document.SelectNodes("/*[local-name()='FictionBook' and namespace-uri()='http://www.gribuser.ru/xml/fictionbook/2.0']/*[local-name()='body' and namespace-uri()='http://www.gribuser.ru/xml/fictionbook/2.0']")
                    if ($bodyNodes.Count -eq 0) { throw "No FB2 body in $path" }
                    $builder = [Text.StringBuilder]::new()
                    foreach ($node in $bodyNodes) { [void]$builder.Append($node.InnerText) }
                    return [regex]::Replace($builder.ToString().Replace('ISSUE72_UNSAVED_MARKER', ''), '\s+', '')
                }
                if (!(Test-Path -LiteralPath $sourceDump -PathType Leaf)) { throw 'Source snapshot was not written.' }
                $expectedBody = Get-BodyText $fixture
                $actualBody = Get-BodyText $sourceDump
                if ($expectedBody -cne $actualBody) { throw "Issue72 $($case.Key)/$operation changed body text: expected $($expectedBody.Length) characters, actual $($actualBody.Length)." }
                if ($case.Key -eq 'section-strong-p') {
                    $settings = [Xml.XmlReaderSettings]::new()
                    $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
                    $settings.XmlResolver = $null
                    $reader = [Xml.XmlReader]::Create($sourceDump, $settings)
                    try { $document = [Xml.XmlDocument]::new(); $document.XmlResolver = $null; $document.Load($reader) }
                    finally { $reader.Close() }
                    $ns = [Xml.XmlNamespaceManager]::new($document.NameTable)
                    $ns.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
                    $sections = $document.SelectNodes('/fb:FictionBook/fb:body/fb:section', $ns)
                    if ($sections.Count -ne 1) { throw 'Issue72 section-strong-p: expected one section.' }
                    $elements = $sections[0].SelectNodes('*')
                    if ($elements.Count -ne 2 -or $elements[0].LocalName -cne 'p' -or
                        $elements[0].SelectNodes('fb:strong', $ns).Count -ne 1 -or
                        $elements[0].InnerText -cne 'Nested' -or $elements[1].LocalName -cne 'p' -or
                        $elements[1].InnerText -cne 'Neighbour') {
                        throw 'Issue72 section-strong-p: paragraph structure or strong formatting changed.'
                    }
                }
            }
            if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $before) { throw 'Original fixture changed on disk.' }
            $passed = $true
            Write-Host "Issue72 $($case.Key)/$operation returned and preserved the unsaved edit."
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed -KeepArtifacts:$KeepArtifacts }
    }
}
