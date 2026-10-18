[CmdletBinding()]
param([string]$FbeExe)
$ErrorActionPreference='Stop'
if(-not $FbeExe){$FbeExe=Join-Path $PSScriptRoot '../../out/Release/FBE.exe'}
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'prerelease-serialization'
$passed=$false
$oldMode,$oldScenario,$oldNbsp=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_NBSP
try {
    $null=New-IsolatedFbeSettingsSeed -Isolation $isolation
    $fixture=Join-Path $isolation.Root 'tables.fb2'
    $xml=@'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Tables</book-title><annotation><table id="annotation"><tr><td>A&#160;B</td></tr></table></annotation><lang>en</lang></title-info><src-title-info><genre>prose</genre><author><nickname>Source</nickname></author><book-title>Hidden</book-title><annotation><table id="hidden"><tr><td>hidden source</td></tr></table></annotation><lang>en</lang></src-title-info><document-info><id>prerelease</id><version>1.0</version><history><table id="history"><tr><td>history</td></tr></table></history></document-info></description><body><section><table id="body"><tr><td>body</td></tr></table></section></body></FictionBook>
'@
    [IO.File]::WriteAllText($fixture,$xml,[Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='prerelease-serialization'
    foreach($code in 0x25a1,0x25ab,0x25e6) {
        $env:FBE_NEXT_TEST_NBSP=[string][char]$code
        $report=Join-Path $isolation.Root ("serialization-$code.txt")
        $process=Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$fixture) -WindowStyle Hidden -PassThru
        $exitCode=Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'prerelease-serialization' -Report $report -TimeoutSeconds 45
        if($exitCode -ne 0 -or [IO.File]::ReadAllText($report) -notmatch 'result=pass'){throw "Serialization failed for U+$('{0:X4}' -f $code): $report"}
        foreach($suffix in '.fb2','.recovery.fb2') {
            [xml]$saved=[IO.File]::ReadAllText($report+$suffix)
            $ns=[Xml.XmlNamespaceManager]::new($saved.NameTable);$ns.AddNamespace('fb','http://www.gribuser.ru/xml/fictionbook/2.0')
            if($saved.SelectNodes('//fb:table',$ns).Count -ne 3 -or $saved.DocumentElement.InnerText -notmatch "A$([char]0xa0)B"){throw 'Saved XML lost table/NBSP content'}
        }
    }
    $passed=$true
    Write-Host 'PASS: annotation/history/body and hidden source tables; three NBSP markers; FB2, archive serialization and recovery XML; visual DOM unchanged.'
} finally {
    $env:FBE_NEXT_TEST_MODE=$oldMode;$env:FBE_NEXT_TEST_SCENARIO=$oldScenario;$env:FBE_NEXT_TEST_NBSP=$oldNbsp
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
