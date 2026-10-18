[CmdletBinding()]
param([string]$FbeExe)
$ErrorActionPreference='Stop'
if(-not $FbeExe){$FbeExe=Join-Path $PSScriptRoot '../../out/Release/FBE.exe'}
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$isolation=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'prerelease-serialization'
$passed=$false
$oldMode,$oldScenario,$oldNbsp,$oldEntry=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_NBSP,$env:FBE_NEXT_TEST_ARCHIVE_ENTRY
function Invoke-SerializationFbe([string]$source,[string]$report,[string]$scenario='prerelease-serialization') {
    $env:FBE_NEXT_TEST_SCENARIO=$scenario
    $process=Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$source) -WindowStyle Hidden -PassThru
    # Match the established archive contour: MSHTML owns a 120-second first-load timeout.
    $exitCode=Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario $scenario -Report $report -TimeoutSeconds 150
    if($exitCode -ne 0){throw "Serialization failed: $report"}
}
try {
    $settings=New-IsolatedFbeSettingsSeed -Isolation $isolation
    $fixture=Join-Path $isolation.Root 'tables.fb2'
    $xml=@'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Tables</book-title><annotation><table id="annotation"><tr><td>A&#160;B</td></tr></table></annotation><lang>en</lang></title-info><src-title-info><genre>prose</genre><author><nickname>Source</nickname></author><book-title>Hidden</book-title><annotation><table id="hidden"><tr><td>hidden source</td></tr></table></annotation><lang>en</lang></src-title-info><document-info><id>prerelease</id><version>1.0</version><history><table id="history"><tr><td>history</td></tr></table></history></document-info></description><body><section><table id="body"><tr><td>body</td></tr></table></section></body></FictionBook>
'@
    [IO.File]::WriteAllText($fixture,$xml,[Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='prerelease-serialization'
    foreach($code in 0x25a1,0x25ab,0x25e6) {
        $env:FBE_NEXT_TEST_NBSP=[string][char]$code
        [xml]$preferences=[IO.File]::ReadAllText($settings)
        $node=$preferences.SelectSingleNode('/FBE/Settings/NBSPChar')
        if(-not $node){$node=$preferences.CreateElement('NBSPChar');$null=$preferences.SelectSingleNode('/FBE/Settings').AppendChild($node)}
        $node.InnerText=[string][char]$code;$preferences.Save($settings)
        $report=Join-Path $isolation.Root ("serialization-$code.txt")
        Invoke-SerializationFbe $fixture $report
        if([IO.File]::ReadAllText($report) -notmatch 'result=pass'){throw "Serialization failed for U+$('{0:X4}' -f $code): $report"}
        foreach($suffix in '.fb2','.recovery.fb2') {
            [xml]$saved=[IO.File]::ReadAllText($report+$suffix)
            $ns=[Xml.XmlNamespaceManager]::new($saved.NameTable);$ns.AddNamespace('fb','http://www.gribuser.ru/xml/fictionbook/2.0')
            if($saved.SelectNodes('//fb:table',$ns).Count -ne 3 -or $saved.DocumentElement.InnerText -notmatch "A$([char]0xa0)B"){throw 'Saved XML lost table/NBSP content'}
        }
        foreach($suffix in '.fb2','.recovery.fb2') {
            $reopened=Join-Path $isolation.Root ("reopen-$code-$($suffix.Trim('.')).txt")
            Invoke-SerializationFbe ($report+$suffix) $reopened
            if([IO.File]::ReadAllText($reopened) -notmatch 'result=pass'){throw "FBE reopening failed: $reopened"}
        }
        $archive=Join-Path $isolation.Root ("nbsp-$code.zip")
        $zip=[IO.Compression.ZipFile]::Open($archive,'Create')
        try {$entry=$zip.CreateEntry('book.fb2');$writer=[IO.StreamWriter]::new($entry.Open(),[Text.UTF8Encoding]::new($false));try{$writer.Write($xml.Replace('A&#160;B','ARCHIVE_RUNTIME_BEFORE A&#160;B'))}finally{$writer.Dispose()}}finally{$zip.Dispose()}
        $env:FBE_NEXT_TEST_ARCHIVE_ENTRY='book.fb2'
        Invoke-SerializationFbe $archive (Join-Path $isolation.Root "zip-save-$code.txt") 'archive-runtime'
        $zip=[IO.Compression.ZipFile]::OpenRead($archive)
        try {$reader=[IO.StreamReader]::new($zip.GetEntry('book.fb2').Open(),[Text.Encoding]::UTF8);try{$archivedXml=$reader.ReadToEnd()}finally{$reader.Dispose()}}finally{$zip.Dispose()}
        if(-not $archivedXml.Contains("A$([char]0xa0)B") -or $archivedXml.Contains([string][char]$code) -or -not $archivedXml.Contains('ARCHIVE_RUNTIME_AFTER')){throw 'Production ZIP save corrupted NBSP or lost the edit'}
        Invoke-SerializationFbe $archive (Join-Path $isolation.Root "zip-reopen-$code.txt") 'archive-open-runtime'
        $env:FBE_NEXT_TEST_ARCHIVE_ENTRY=$null
    }
    $passed=$true
    Write-Host 'PASS: annotation/history/body and hidden source tables; three NBSP markers; FB2/recovery editor reopen; production ZIP save and reopen; visual DOM unchanged.'
} finally {
    $env:FBE_NEXT_TEST_MODE=$oldMode;$env:FBE_NEXT_TEST_SCENARIO=$oldScenario;$env:FBE_NEXT_TEST_NBSP=$oldNbsp;$env:FBE_NEXT_TEST_ARCHIVE_ENTRY=$oldEntry
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
