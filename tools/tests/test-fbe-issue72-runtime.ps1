<# Exercises the actual Source and Validate commands on invalid FB2 nesting.
   The supplied original is copied into a disposable portable runtime. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FbeExe,
    [string]$OriginalBook,
    [int]$TimeoutSeconds = 45,
    [switch]$KeepArtifacts,
    [string]$CaseName,
    [Alias('Operation')][ValidateSet('both', 'source', 'validate')][string]$OperationFilter = 'both',
    [switch]$RequireSource
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$cases = [ordered]@{
    'valid-control' = '<p>Valid text with <strong>inline formatting</strong>.</p><p>Second paragraph.</p>'
    'subtitle-strong-p' = '<subtitle><strong><p>Nested</p></strong></subtitle>'
    'subtitle-paragraph' = '<subtitle><p>Nested</p></subtitle>'
    'subtitle-strong-multiple' = '<subtitle><strong><p>First</p><empty-line/><p>Second</p><empty-line/><p>Third</p></strong></subtitle>'
    'section-strong-p' = '<strong><p>Nested</p></strong>'
    'section-emphasis-p' = '<emphasis><p>Nested</p></emphasis>'
    'subtitle-emphasis-p' = '<subtitle><emphasis><p>Nested</p></emphasis></subtitle>'
    'subtitle-sub-p' = '<subtitle><sub><p>Nested</p></sub></subtitle>'
    'subtitle-strong-sub-p' = '<subtitle><strong>Heading </strong><sub><p><emphasis>Nested</emphasis></p></sub></subtitle>'
    'subtitle-empty-p-tail' = '<subtitle><p/>Tail text</subtitle>'
    'annotation-strong-p' = '<p>Body control.</p>'
    'legacy-links-binary' = '<p><a l:href="#target">Link</a></p><image l:href="#cover"/>'
    'conflicting-xlink-attributes' = '<p><a std:href="#target" l:href="#different">Link</a></p>'
    'paragraph-strong-p' = '<p><strong><p>Nested</p></strong></p>'
}
if ($OriginalBook) { $cases['user-book'] = $null }
if ($CaseName -and !$cases.Contains($CaseName)) { throw "Unknown Issue72 case: $CaseName" }
foreach ($case in $cases.GetEnumerator()) {
    if ($CaseName -and $case.Key -ne $CaseName) { continue }
    foreach ($operation in 'source', 'validate') {
        if ($OperationFilter -ne 'both' -and $operation -ne $OperationFilter) { continue }
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
                if ($case.Key -eq 'annotation-strong-p') { $xml = $xml.Replace('<lang>en</lang>', '<annotation><strong><p>Nested annotation.</p></strong></annotation><lang>en</lang>') }
                if ($case.Key -eq 'legacy-links-binary') {
                    $xml = $xml.Replace('<FictionBook xmlns=', '<FictionBook xmlns:l="http://www.w3.org/1999/l" xmlns=')
                    $xml = $xml.Replace('<lang>en</lang>', '<coverpage><image l:href="#cover"/></coverpage><lang>en</lang>')
                    $xml = $xml.Replace('<body><section>', '<body><section id="target">')
                    $xml = $xml.Replace('</FictionBook>', '<binary id="cover" content-type="image/png">iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+a3ioAAAAASUVORK5CYII=</binary></FictionBook>')
                }
                if ($case.Key -eq 'conflicting-xlink-attributes') {
                    $xml = $xml.Replace('<FictionBook xmlns=', '<FictionBook xmlns:l="http://www.w3.org/1999/l" xmlns:std="http://www.w3.org/1999/xlink" xmlns=')
                    $xml = $xml.Replace('<body><section>', '<body><section id="target">')
                }
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
            if (($RequireSource -or $case.Key -eq 'valid-control') -and $operation -eq 'source' -and $text -notmatch '(?m)^source-active=1\r?$') {
                throw "Issue72 $($case.Key)/$operation did not open Source: $text"
            }
            if ($text -match '(?m)^source-active=1\r?$') {
                function Get-VisibleText([string]$path) {
                    $settings = [Xml.XmlReaderSettings]::new()
                    $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
                    $settings.XmlResolver = $null
                    $reader = [Xml.XmlReader]::Create($path, $settings)
                    try { $document = [Xml.XmlDocument]::new(); $document.XmlResolver = $null; $document.Load($reader) }
                    finally { $reader.Close() }
                    $book = "/*[local-name()='FictionBook' and namespace-uri()='http://www.gribuser.ru/xml/fictionbook/2.0']"
                    $bodyNodes = $document.SelectNodes("$book/*[local-name()='body' and namespace-uri()='http://www.gribuser.ru/xml/fictionbook/2.0']")
                    if ($bodyNodes.Count -eq 0) { throw "No FB2 body in $path" }
                    $builder = [Text.StringBuilder]::new()
                    $groups = @(
                        "$book/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='annotation']",
                        "$book/*[local-name()='description']/*[local-name()='src-title-info']/*[local-name()='annotation']",
                        "$book/*[local-name()='description']/*[local-name()='document-info']/*[local-name()='history']",
                        "$book/*[local-name()='body']"
                    )
                    foreach ($group in $groups) {
                        [void]$builder.Append([char]1)
                        foreach ($node in $document.SelectNodes($group)) { [void]$builder.Append($node.InnerText) }
                    }
                    return [regex]::Replace($builder.ToString().Replace('ISSUE72_UNSAVED_MARKER', ''), '\s+', '')
                }
                if (!(Test-Path -LiteralPath $sourceDump -PathType Leaf)) { throw 'Source snapshot was not written.' }
                $expectedText = Get-VisibleText $fixture
                $actualText = Get-VisibleText $sourceDump
                if ($expectedText -cne $actualText) { throw "Issue72 $($case.Key)/$operation changed visible text: expected $($expectedText.Length) characters, actual $($actualText.Length)." }
                function Get-LinkTargets([string]$path) {
                    $settings = [Xml.XmlReaderSettings]::new(); $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit; $settings.XmlResolver = $null
                    $reader = [Xml.XmlReader]::Create($path, $settings)
                    try { $document = [Xml.XmlDocument]::new(); $document.XmlResolver = $null; $document.Load($reader) }
                    finally { $reader.Close() }
                    $targets = [Collections.Generic.List[string]]::new()
                    foreach ($node in $document.SelectNodes("//*[local-name()='a' or local-name()='image']")) {
                        foreach ($attribute in $node.Attributes) {
                            if ($attribute.LocalName -ceq 'href') { $targets.Add($node.LocalName + ':' + $attribute.Value) }
                        }
                    }
                    return [string]::Join([string][char]1, $targets)
                }
                if ((Get-LinkTargets $fixture) -cne (Get-LinkTargets $sourceDump)) { throw "Issue72 $($case.Key)/$operation changed or lost link targets." }
                function Get-BinaryFingerprints([string]$path) {
                    $settings = [Xml.XmlReaderSettings]::new(); $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit; $settings.XmlResolver = $null
                    $reader = [Xml.XmlReader]::Create($path, $settings)
                    try { $document = [Xml.XmlDocument]::new(); $document.XmlResolver = $null; $document.Load($reader) }
                    finally { $reader.Close() }
                    $hash = [Security.Cryptography.SHA256]::Create()
                    try {
                        $items = foreach ($node in $document.SelectNodes("/*[local-name()='FictionBook']/*[local-name()='binary']")) {
                            $bytes = [Convert]::FromBase64String($node.InnerText)
                            $digest = [BitConverter]::ToString($hash.ComputeHash($bytes)).Replace('-', '')
                            $node.GetAttribute('id') + ':' + $node.GetAttribute('content-type') + ':' + $digest
                        }
                        return [string]::Join([string][char]1, @($items))
                    } finally { $hash.Dispose() }
                }
                if ((Get-BinaryFingerprints $fixture) -cne (Get-BinaryFingerprints $sourceDump)) { throw "Issue72 $($case.Key)/$operation changed binary IDs, media types or bytes." }
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
                if ($case.Key -eq 'subtitle-paragraph') {
                    $settings = [Xml.XmlReaderSettings]::new()
                    $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
                    $settings.XmlResolver = $null
                    $reader = [Xml.XmlReader]::Create($sourceDump, $settings)
                    try { $document = [Xml.XmlDocument]::new(); $document.XmlResolver = $null; $document.Load($reader) }
                    finally { $reader.Close() }
                    $ns = [Xml.XmlNamespaceManager]::new($document.NameTable)
                    $ns.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
                    $sections = $document.SelectNodes('/fb:FictionBook/fb:body/fb:section', $ns)
                    if ($sections.Count -ne 1) { throw 'Issue72 subtitle-paragraph: expected one section.' }
                    $elements = $sections[0].SelectNodes('*')
                    if ($elements.Count -ne 2 -or $elements[0].LocalName -cne 'subtitle' -or
                        $elements[0].InnerText -cne 'Nested' -or $elements[1].LocalName -cne 'p' -or
                        $elements[1].InnerText -cne 'Neighbour') {
                        throw 'Issue72 subtitle-paragraph: subtitle or paragraph structure changed.'
                    }
                }
                if ($case.Key -in @('section-emphasis-p', 'subtitle-sub-p', 'subtitle-strong-sub-p', 'subtitle-empty-p-tail', 'annotation-strong-p')) {
                    $settings = [Xml.XmlReaderSettings]::new(); $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit; $settings.XmlResolver = $null
                    $reader = [Xml.XmlReader]::Create($sourceDump, $settings)
                    try { $document = [Xml.XmlDocument]::new(); $document.XmlResolver = $null; $document.Load($reader) }
                    finally { $reader.Close() }
                    $ns = [Xml.XmlNamespaceManager]::new($document.NameTable); $ns.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
                    $sections = $document.SelectNodes('/fb:FictionBook/fb:body/fb:section', $ns)
                    if ($sections.Count -ne 1) { throw "Issue72 $($case.Key): expected one section." }
                    $elements = $sections[0].SelectNodes('*')
                    if ($case.Key -eq 'section-emphasis-p' -and ($elements.Count -ne 2 -or $elements[0].LocalName -cne 'p' -or $elements[0].SelectNodes('fb:emphasis', $ns).Count -ne 1 -or $elements[0].InnerText -cne 'Nested' -or $elements[1].InnerText -cne 'Neighbour')) { throw 'Issue72 section-emphasis-p: paragraph or formatting changed.' }
                    if ($case.Key -eq 'subtitle-sub-p' -and ($elements.Count -ne 2 -or $elements[0].LocalName -cne 'subtitle' -or $elements[0].SelectNodes('fb:sub', $ns).Count -ne 1 -or $elements[0].InnerText -cne 'Nested' -or $elements[1].InnerText -cne 'Neighbour')) { throw 'Issue72 subtitle-sub-p: subtitle or subscript changed.' }
                    if ($case.Key -eq 'subtitle-strong-sub-p' -and ($elements.Count -ne 2 -or $elements[0].LocalName -cne 'subtitle' -or $elements[0].SelectNodes('fb:strong', $ns).Count -ne 1 -or $elements[0].SelectNodes('fb:sub/fb:emphasis', $ns).Count -ne 1 -or $elements[0].InnerText -cne 'Heading Nested' -or $elements[1].InnerText -cne 'Neighbour')) { throw 'Issue72 subtitle-strong-sub-p: subtitle, subscript or inline formatting changed.' }
                    if ($case.Key -eq 'subtitle-empty-p-tail' -and ($elements.Count -ne 3 -or $elements[0].LocalName -cne 'empty-line' -or $elements[1].LocalName -cne 'subtitle' -or $elements[1].InnerText -cne 'Tail text' -or $elements[2].InnerText -cne 'Neighbour')) { throw 'Issue72 subtitle-empty-p-tail: empty paragraph or subtitle changed.' }
                    if ($case.Key -eq 'annotation-strong-p') {
                        $annotation = $document.SelectNodes('/fb:FictionBook/fb:description/fb:title-info/fb:annotation/fb:p[fb:strong]', $ns)
                        if ($annotation.Count -ne 1 -or $annotation[0].InnerText.Replace('ISSUE72_UNSAVED_MARKER', '').Trim() -cne 'Nested annotation.') { throw 'Issue72 annotation-strong-p: annotation structure or formatting changed.' }
                    }
                }
            }
            if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $before) { throw 'Original fixture changed on disk.' }
            $passed = $true
            Write-Host "Issue72 $($case.Key)/$operation returned and preserved the unsaved edit."
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed -KeepArtifacts:$KeepArtifacts }
    }
}
