[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'new-document-safety'
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TRACE','FBE_NEXT_FAULT_INJECT','FBE_NEXT_TEST_NEW_EPUB','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TEST_IMPORT_EPUB_PATH')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name)}
try {
    $seed=New-IsolatedFbeSettingsSeed $iso
    $fixture=Join-Path $PSScriptRoot '../../out/tests/export-epub-cyrillic/fb2-metadata-cyrillic-smoke.epub'
    if(!(Test-Path -LiteralPath $fixture)) {
        & (Join-Path $PSScriptRoot 'test-export-epub-cyrillic.ps1') -OutputPath $fixture
        if($LASTEXITCODE -ne 0){throw 'EPUB fixture generation failed'}
    }
    $epub=Join-Path $iso.Root 'new-document.epub';Copy-Item -LiteralPath $fixture -Destination $epub
    $epubHash=(Get-FileHash $epub -Algorithm SHA256).Hash
    $first=Join-Path $iso.Root 'original.fb2';$report=Join-Path $iso.Root 'new-document.txt'
    $xml='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>NEW_OLD_UNIQUE</book-title><lang>en</lang></title-info><document-info><id>new-safety</id><version>1</version></document-info></description><body><section><title><p>NEW_OLD_UNIQUE</p></title><p>NEW_OLD_UNIQUE</p></section></body></FictionBook>'
    [IO.File]::WriteAllText($first,$xml,[Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='new-document-safety-runtime';$env:FBE_NEXT_TRACE='1'
    $env:FBE_NEXT_FAULT_INJECT=$null;$env:FBE_NEXT_TEST_NEW_EPUB=$epub
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$first) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'new-document-safety' -Report $report -TimeoutSeconds 90
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "New Document failed (exit=$code): $text"}
    if((Get-FileHash $epub -Algorithm SHA256).Hash -ne $epubHash){throw 'Source EPUB changed'}
    if((Get-Content $first -Raw) -notmatch 'NEW_SOURCE_SAVED'){throw 'Save before New did not persist the original Source changes'}
    Write-Host $text;$passed=$true;Write-Host 'New Document safety runtime PASS'
} finally {
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name])}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
