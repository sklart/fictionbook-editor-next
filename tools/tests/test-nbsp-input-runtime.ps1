[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe,
      [ValidateSet('square', 'small-square', 'circle', 'none')][string[]]$Modes = @('square', 'small-square', 'circle', 'none'),
      [ValidateSet('plain', 'emphasis', 'strong', 'link')][string]$Context = 'plain',
      [string]$ArtifactDirectory,
      [switch]$CheckDisplayToggle,
      [switch]$CheckSingleDisplayToggle,
      [switch]$CheckSingleEmptyRedo,
      [switch]$EmptyRedoControl,
      [switch]$ToggleControl,
      [switch]$CheckRepeatedApply,
      [ValidateSet('none','small-square')][string]$RepeatDisplay = 'none')
$ErrorActionPreference = 'Stop'
if(@(@($CheckDisplayToggle, $CheckSingleDisplayToggle, $CheckSingleEmptyRedo, $EmptyRedoControl, $ToggleControl, $CheckRepeatedApply) | Where-Object { $_ }).Count -gt 1) { throw 'Choose only one NBSP diagnostic mode.' }
if($CheckRepeatedApply -and (@($Modes).Count -ne 1 -or $Modes[0] -ne 'square')) { throw 'Repeated-apply regression requires -Modes square.' }
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $ArtifactDirectory) { $ArtifactDirectory = Join-Path $root 'build\community-feedback-nbsp' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$ArtifactDirectory = [IO.Path]::GetFullPath($ArtifactDirectory)
New-Item -ItemType Directory -Force -Path $ArtifactDirectory | Out-Null
$names = @('FBE_NEXT_TEST_MODE', 'FBE_NEXT_TEST_SCENARIO', 'FBE_NEXT_TEST_NBSP_OUTPUT', 'FBE_NEXT_TEST_NBSP_TOGGLE')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name) }
$failures = @()
try {
    foreach ($mode in @(@('square', 0x25A1), @('small-square', 0x25AB), @('circle', 0x25E6), @('none', 0x00A0))) {
        $name = $mode[0]; $marker = [char]$mode[1]
        if ($name -notin $Modes) { continue }
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name "nbsp-$name"
        $passed = $false
        try {
            $settingsFile = New-IsolatedFbeSettingsSeed -Isolation $iso
            $settings = [IO.File]::ReadAllText($settingsFile).Replace('</Settings>', "<NBSPChar>$marker</NBSPChar></Settings>")
            [IO.File]::WriteAllText($settingsFile, $settings, [Text.UTF8Encoding]::new($false))
            $fixture = Join-Path $iso.DataPath 'input.fb2'
            $markup = switch ($Context) { 'plain' { 'word1word2' } 'emphasis' { '<emphasis>word1word2</emphasis>' } 'strong' { '<strong>word1word2</strong>' } 'link' { '<a l:href="#nbsp-probe">word1word2</a>' } }
            $fb2 = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>Test</first-name><last-name>Author</last-name></author><book-title>NBSP test</book-title><lang>en</lang></title-info><document-info><author><first-name>Test</first-name><last-name>Author</last-name></author><date>2026</date><id>nbsp-test</id><version>1.0</version></document-info></description><body><section><p id="nbsp-prior">prior</p><p id="nbsp-probe">' + $markup + '</p></section></body></FictionBook>'
            [IO.File]::WriteAllText($fixture, $fb2, [Text.UTF8Encoding]::new($false))
            $saved = Join-Path $iso.DataPath 'saved.fb2'
            $report = Join-Path $iso.DataPath 'report.txt'
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'nbsp-input-runtime'
            $env:FBE_NEXT_TEST_NBSP_TOGGLE = if($CheckDisplayToggle) { '1' } elseif($CheckSingleDisplayToggle) { '5' } elseif($CheckSingleEmptyRedo) { '6' } elseif($EmptyRedoControl) { '7' } elseif($ToggleControl) { '2' } elseif($CheckRepeatedApply -and $RepeatDisplay -eq 'small-square') { '4' } elseif($CheckRepeatedApply) { '3' } else { $null }
            $env:FBE_NEXT_TEST_NBSP_OUTPUT = $saved
            $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime `
                -ArgumentList @('--portable', '-b', ('"' + $report + '"'), ('"' + $fixture + '"')) -PassThru
            $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'nbsp-input-runtime' -Report $report -TimeoutSeconds 120
            $text = if (Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing>' }
            if (Test-Path -LiteralPath $report) { Copy-Item -LiteralPath $report -Destination (Join-Path $ArtifactDirectory "$Context-$name.txt") -Force }
            if (Test-Path -LiteralPath $saved) { Copy-Item -LiteralPath $saved -Destination (Join-Path $ArtifactDirectory "$Context-$name.fb2") -Force }
            if ($CheckRepeatedApply -and (Test-Path -LiteralPath "$saved.repeat.fb2")) { Copy-Item -LiteralPath "$saved.repeat.fb2" -Destination (Join-Path $ArtifactDirectory "$Context-$name-repeat.fb2") -Force }
            $expectedMode = '{0:X4}' -f [int]$marker
            if (-not (Test-Path -LiteralPath $saved)) { throw "$name failed before save: $text" }
            $xml = [Xml.XmlDocument]::new(); $xml.Load($saved)
            $ns = [Xml.XmlNamespaceManager]::new($xml.NameTable); $ns.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
            $value = $xml.SelectSingleNode('//fb:p[@id="nbsp-probe"]', $ns).InnerText
            if ($Context -eq 'emphasis' -and -not $xml.SelectSingleNode('//fb:p[@id="nbsp-probe"]/fb:emphasis', $ns)) { throw 'Emphasis was lost.' }
            if ($Context -eq 'strong') {
                $strong = $xml.SelectSingleNode('//fb:p[@id="nbsp-probe"]/fb:strong', $ns)
                if (-not $strong -or $strong.InnerText -cne $value) { throw 'Strong formatting did not retain the complete text.' }
            }
            if ($Context -eq 'link') {
                $link = $xml.SelectSingleNode('//fb:p[@id="nbsp-probe"]/fb:a', $ns)
                if (-not $link -or $link.GetAttribute('href', 'http://www.w3.org/1999/xlink') -cne '#nbsp-probe') { throw 'Link or target was lost.' }
            }
            $expectedSuffix = if($CheckSingleEmptyRedo -or $EmptyRedoControl) { 'wOrd2' } else { 'word2' }
            if ($value.Length -ne 12 -or $value.Substring(0,5) -cne 'word1' -or $value.Substring(7) -cne $expectedSuffix -or
                @($value.Substring(5,2).ToCharArray() | Where-Object { $_ -ne ' ' -and $_ -ne [char]0x00A0 }).Count -ne 0) { throw "$name saved unexpected characters" }
            if (@($value.Substring(5,2).ToCharArray() | Where-Object { $_ -eq [char]0x00A0 }).Count -ne 1) { throw "$name did not save exactly one real NBSP" }
            if ($marker -ne [char]0x00A0 -and $value.Contains([string]$marker)) { throw "$name saved its display marker" }
            $readerSettings = [Xml.XmlReaderSettings]::new()
            $readerSettings.ValidationType = [Xml.ValidationType]::Schema
            $readerSettings.XmlResolver = [Xml.XmlUrlResolver]::new()
            $readerSettings.Schemas.XmlResolver = [Xml.XmlUrlResolver]::new()
            [void]$readerSettings.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0', (Join-Path $root 'runtime\FictionBook.xsd'))
            $reader = [Xml.XmlReader]::Create($saved, $readerSettings)
            try { while ($reader.Read()) {} } finally { $reader.Dispose() }
            if($CheckRepeatedApply) {
                $repeatPath = "$saved.repeat.fb2"
                $repeatXml = [Xml.XmlDocument]::new(); $repeatXml.Load($repeatPath)
                $repeatNs = [Xml.XmlNamespaceManager]::new($repeatXml.NameTable); $repeatNs.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
                $repeatText = $repeatXml.SelectSingleNode('//fb:p[@id="nbsp-probe"]', $repeatNs).InnerText
                if(-not $repeatText.EndsWith([string][char]0x25A1)) { throw 'The typed square was not preserved in the saved FB2.' }
                $repeatReader = [Xml.XmlReader]::Create($repeatPath, $readerSettings)
                try { while($repeatReader.Read()) {} } finally { $repeatReader.Dispose() }
            }
            Write-Host "NBSP saved XML and independent XSD: PASS ($Context/$name)."
            $toggleExpected = if($CheckDisplayToggle -or $CheckSingleDisplayToggle -or $CheckSingleEmptyRedo -or $EmptyRedoControl -or $ToggleControl) { '1' } else { '0' }
            if($text -notmatch "(?m)^toggle-tested=$toggleExpected\r?$") { throw "$name did not report the requested display-toggle mode: $text" }
            $appliedExpected = if($CheckDisplayToggle -or $CheckSingleDisplayToggle -or $CheckSingleEmptyRedo) { '1' } else { '0' }
            if($text -notmatch "(?m)^toggle-applied=$appliedExpected\r?$") { throw "$name did not report the requested display change: $text" }
            if(($CheckSingleEmptyRedo -or $EmptyRedoControl) -and ($text -notmatch '(?m)^empty-redo-tested=1\r?$' -or $text -notmatch '(?m)^empty-redo-ready=1\r?$')) { throw "$name did not establish an empty Redo stack: $text" }
            if($CheckRepeatedApply -and $text -notmatch '(?m)^repeat-applied=1\r?$') { throw "$name did not run the repeated-settings diagnostic: $text" }
            if ($code -ne 0 -or $text -notmatch "(?m)^mode=$expectedMode\r?$" -or $text -notmatch '(?m)^result=pass\r?$') { throw "$name failed: $text" }
            $passed = $true
            if($CheckRepeatedApply) { Write-Host "Repeated settings preserve typed marker and save/reopen: PASS ($Context/$name/$RepeatDisplay)." }
            else { Write-Host "NBSP input/Undo/Redo: PASS ($Context/$name)." }
        } catch {
            $failures += "$name`: $($_.Exception.Message)"
            Write-Warning $failures[-1]
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
    }
    if ($failures.Count) { throw "NBSP runtime failures: $($failures.Count) of $($Modes.Count); see saved reports." }
} finally { foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name]) } }
