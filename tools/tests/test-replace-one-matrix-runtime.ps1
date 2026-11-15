[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [string[]]$Cases,
      [string]$ArtifactDirectory = (Join-Path $PSScriptRoot '..\..\build\replace-one-matrix'))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$matrix = @(
    @{name='start'; pattern='^Alpha'; replacement='Q'; body='Alpha omega'; expected='Q omega'},
    @{name='end'; pattern='omega$'; replacement='Z'; body='Alpha omega'; expected='Alpha Z'},
    @{name='grow'; pattern='23'; replacement='длиннее'; body='до 23 после'; expected='до длиннее после'},
    @{name='shrink'; pattern='123'; replacement='x'; body='до 123 после'; expected='до x после'},
    @{name='delete'; pattern='123'; replacement=''; body='до 123 после'; expected='до  после'},
    @{name='spaces-baseline'; pattern='X'; replacement='Y'; body='до  после X'; expected='до  после Y'},
    @{name='spaces-inline'; pattern='X'; replacement='Y'; body='до <emphasis> </emphasis>после X'; expected='до  после Y'; tag='emphasis'},
    @{name='spaces-three'; pattern='X'; replacement='Y'; body='до   после X'; expected='до   после Y'},
    @{name='spaces-search'; pattern='  '; replacement='_'; body='до  после'; expected='до_после'},
    @{name='spaces-nbsp'; pattern='X'; replacement='Y'; body="до `u{00a0} после X"; expected="до `u{00a0} после Y"},
    @{name='spaces-private-use'; pattern='X'; replacement='Y'; body="до `u{e000}  X"; expected="до `u{e000}  Y"},
    @{name='custom-xsl-spaces'; pattern='X'; replacement='Y'; body='до  после X'; expected='match до  после Y'; customXsl=$true},
    @{name='groups'; pattern='(кот) (дом)'; replacement='$2 $1'; body='кот дом дальше'; expected='дом кот дальше'},
    @{name='lookahead'; pattern='(?=слово)'; replacement='+'; body='до слово после'; expected='до +слово после'},
    @{name='cyrillic'; pattern='\bкот\b'; replacement='пёс'; body='кот здесь'; expected='пёс здесь'},
    @{name='nbsp'; pattern="`u{00a0}"; replacement='_'; body="один`u{00a0}два"; expected='один_два'},
    @{name='nbsp-escape'; pattern='\x{a0}'; replacement='_'; body="один`u{00a0}два"; expected='один_два'},
    @{name='space-not-nbsp'; pattern=' '; replacement='_'; body="один`u{00a0}два три"; expected="один`u{00a0}два_три"},
    @{name='nbsp-inline'; pattern='\x{a0}'; replacement='_'; body="один<emphasis>`u{00a0}</emphasis>два"; expected='один_два'},
    @{name='nbsp-square'; pattern="`u{00a0}"; replacement='_'; body="один`u{00a0}два"; expected='один_два'; marker='□'},
    @{name='nbsp-escape-square'; pattern='\x{a0}'; replacement='_'; body="один`u{00a0}два"; expected='один_два'; marker='□'},
    @{name='nbsp-class-square'; pattern='один\sдва'; replacement='один_два'; body="один`u{00a0}два"; expected='один_два'; marker='□'},
    @{name='nbsp-bracket-square'; pattern='один[\s]два'; replacement='один_два'; body="один`u{00a0}два"; expected='один_два'; marker='□'},
    @{name='nbsp-posix-class'; pattern='один[[:digit:]\s]два'; replacement='один_два'; body="один`u{00a0}два"; expected='один_два'; marker='□'},
    @{name='nbsp-negated-class'; pattern='один[^\s]два'; replacement='_'; body="один`u{00a0}два; одинXдва"; expected="один`u{00a0}два; _"; marker='□'},
    @{name='nbsp-class-repeat'; pattern='один\sдва'; replacement='один_два'; body="один`u{00a0}два; один`u{00a0}два"; expected='один_два; один_два'; marker='□'; mode='repeat'},
    @{name='nbsp-not-nonspace'; pattern='один\Sдва'; replacement='_'; body="один`u{00a0}два; одинXдва"; expected="один`u{00a0}два; _"; marker='□'},
    @{name='quoted-space-class'; pattern='\Q\s\E'; replacement='_'; body='до \s после'; expected='до _ после'; marker='□'},
    @{name='space-class'; pattern='один\sдва'; replacement='один_два'; body='один два'; expected='один_два'},
    @{name='quoted-nbsp-escape'; pattern='\Q\x{a0}\E'; replacement='_'; body='до \x{a0} после'; expected='до _ после'; marker='□'},
    @{name='emoji'; pattern='😀'; replacement='!'; body='до 😀 после'; expected='до ! после'},
    @{name='emphasis'; pattern='23'; replacement='X'; body='до <emphasis>23</emphasis> после'; expected='до X после'; tag='emphasis'},
    @{name='cross-inline'; pattern='AB'; replacement='xy'; body='A<emphasis>B</emphasis>C'; expected='xyC'},
    @{name='selection'; pattern='\d+'; replacement='Z'; body='X 12 Y 34'; expected='X Z Y 34'; mode='selection'},
    @{name='section'; pattern='\d+'; replacement='Z'; body='X 12 Y 34'; expected='X Z Y 34'; mode='section'},
    @{name='up'; pattern='\d+'; replacement='Z'; body='X 12 Y 34'; expected='X 12 Y Z'; mode='up'},
    @{name='cross-paragraph'; pattern='A\nB'; replacement='X'; body='A'; expected='A'; untouched='B'; mode='cross-paragraph'}
)
if ($Cases) { $matrix = @($matrix | Where-Object { $_.name -in $Cases }); if (-not $matrix.Count) { throw 'No matching test cases.' } }
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_MATRIX_PATTERN','FBE_NEXT_TEST_MATRIX_REPLACEMENT','FBE_NEXT_TEST_MATRIX_OUTPUT','FBE_NEXT_TEST_MATRIX_MODE')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name) }
New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
$failures = @()
try {
    foreach ($case in $matrix) {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name ('replace-one-' + $case.name)
        $passed = $false
        try {
            $settingsFile = New-IsolatedFbeSettingsSeed -Isolation $iso
            if ($case.ContainsKey('customXsl')) {
                $xslPath = Join-Path $iso.Runtime 'fb2.xsl'
                $xsl = [IO.File]::ReadAllText($xslPath)
                $before = '<p><xsl:call-template name="id"/><xsl:call-template name="style"/><xsl:apply-templates/></p>'
                $afterTemplate = '<p><xsl:call-template name="id"/><xsl:call-template name="style"/><xsl:if test="starts-with(., ''до'')"><xsl:choose><xsl:when test="contains(., ''  '') and normalize-space(.) = ''до после X'' and string-length(.) - string-length(translate(., '' '', '''')) = 3"><xsl:text>match </xsl:text></xsl:when><xsl:otherwise><xsl:text>wrong </xsl:text></xsl:otherwise></xsl:choose></xsl:if><xsl:apply-templates/></p>'
                if (-not $xsl.Contains($before)) { throw 'Paragraph XSL template not found.' }
                [IO.File]::WriteAllText($xslPath, $xsl.Replace($before, $afterTemplate), [Text.UTF8Encoding]::new($false))
            }
            if ($case.ContainsKey('marker')) {
                $settingsText = [IO.File]::ReadAllText($settingsFile).Replace('</Settings>', '<NBSPChar>' + $case.marker + '</NBSPChar></Settings>')
                [IO.File]::WriteAllText($settingsFile, $settingsText, [Text.UTF8Encoding]::new($false))
            }
            $fixture = Join-Path $iso.DataPath 'input.fb2'; $report = Join-Path $iso.DataPath 'report.txt'
            $untouchedText = if($case.ContainsKey('untouched')){$case.untouched}else{'Неприкосновенный абзац.'}
            $targetSection = '<section><p id="target">' + $case.body + '</p><p id="untouched">' + $untouchedText + '</p></section>'
            $otherSection = '<section><p id="outside">Вне раздела 99</p></section>'
            $bodySections = if($case.ContainsKey('mode') -and $case.mode -in @('selection','section')) { $otherSection + $targetSection } else { $targetSection + $otherSection }
            $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Test</book-title><lang>ru</lang></title-info><document-info><author><first-name>T</first-name><last-name>T</last-name></author><date>2026</date><id>test</id><version>1.0</version></document-info></description><body>' + $bodySections + '</body></FictionBook>'
            [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
            $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='replace-one-matrix'
            $env:FBE_NEXT_TEST_MATRIX_PATTERN=$case.pattern; $env:FBE_NEXT_TEST_MATRIX_REPLACEMENT=$case.replacement; $env:FBE_NEXT_TEST_MATRIX_OUTPUT=$iso.DataPath
            $env:FBE_NEXT_TEST_MATRIX_MODE=if($case.ContainsKey('mode')){$case.mode}else{''}
            $p = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',('"'+$report+'"'),('"'+$fixture+'"')) -PassThru
            $code = Wait-IsolatedFbeProcess -Process $p -Isolation $iso -Scenario 'replace-one-matrix' -Report $report -TimeoutSeconds 90
            $out = Join-Path $ArtifactDirectory $case.name; New-Item -ItemType Directory -Path $out -Force | Out-Null
            foreach ($file in @($fixture,$report,(Join-Path $iso.DataPath 'before.html'),(Join-Path $iso.DataPath 'after.fb2'),(Join-Path $iso.DataPath 'snapshot-before.xml'),(Join-Path $iso.DataPath 'snapshot-reopened.xml'))) { if(Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination $out -Force } }
            $text = if(Test-Path -LiteralPath $report){Get-Content -LiteralPath $report -Raw}else{'<missing>'}
            if ($code -ne 0) { throw "runtime=$code $text" }
            $after = [Xml.XmlDocument]::new(); $after.PreserveWhitespace = $true
            $after.Load((Join-Path $iso.DataPath 'after.fb2'))
            $ns = [Xml.XmlNamespaceManager]::new($after.NameTable); $ns.AddNamespace('f','http://www.gribuser.ru/xml/fictionbook/2.0')
            $target = $after.SelectSingleNode('//f:p[@id="target"]',$ns)
            $untouched = $after.SelectSingleNode('//f:p[@id="untouched"]',$ns)
            $outside = $after.SelectSingleNode('//f:p[@id="outside"]',$ns)
            if (!$target -or $target.InnerText -cne $case.expected -or !$untouched -or $untouched.InnerText -cne $untouchedText -or !$outside -or $outside.InnerText -cne 'Вне раздела 99') { throw "Unexpected output: $($target.InnerText)" }
            if ($case.ContainsKey('tag') -and !$target.SelectSingleNode(('f:'+$case.tag),$ns)) { throw 'Formatting was lost.' }
            $settings=[Xml.XmlReaderSettings]::new(); $settings.ValidationType=[Xml.ValidationType]::Schema; $settings.Schemas.XmlResolver=[Xml.XmlUrlResolver]::new()
            [void]$settings.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0',(Join-Path $root 'runtime\FictionBook.xsd'))
            $reader=[Xml.XmlReader]::Create((Join-Path $iso.DataPath 'after.fb2'),$settings); try { while($reader.Read()) {} } finally { $reader.Dispose() }
            $passed=$true; $history=if($case.name -eq 'cross-paragraph'){'rejected without change'}else{'Undo/Redo'}; Write-Host "PASS $($case.name): button, exact text, $history, Save/Reopen, XSD"
        } catch { $failures += "$($case.name): $_"; Write-Warning "FAIL $($case.name): $_" }
        finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
    }
} finally { foreach($name in $names) { [Environment]::SetEnvironmentVariable($name,$old[$name]) } }
if ($failures.Count) { throw "Single replacement failures:`n$($failures -join "`n")" }
