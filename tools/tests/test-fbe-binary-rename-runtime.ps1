<# Exercises binary rename through the production MSHTML document and validates Save/Reopen persistence. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'binary-rename'
New-IsolatedFbeSettingsSeed -Isolation $isolation | Out-Null
$FbeExe = $isolation.Exe
$directory = $isolation.Root
$passed = $false
try {
    $fixture = Join-Path $directory 'rename.fb2'; $report = Join-Path $directory 'rename.txt'; $reopen = Join-Path $directory 'reopen.txt'
    $bytes = 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAF/gL+MZ7M0QAAAABJRU5ErkJggg=='
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>rename</book-title><lang>en</lang><coverpage><image l:href="#cover-old.png"/></coverpage></title-info><src-title-info><genre>prose</genre><author><first-name>S</first-name><last-name>S</last-name></author><book-title>source rename</book-title><lang>en</lang><coverpage><image l:href="#cover-old.png"/></coverpage></src-title-info><document-info><id>rename-test</id><version>1.0</version></document-info></description><body><section><p><image l:href="#cover-old.png"/></p><p>Inline <image l:href="#cover-old.png"/> image</p></section></body><binary id="cover-old.png" content-type="image/png">$bytes</binary><binary id="other.png" content-type="image/png">$bytes</binary></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode,$oldScenario=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='binary-rename-runtime'
        $process=Start-Process -FilePath $FbeExe -WorkingDirectory $isolation.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',$report,$fixture) -PassThru
        $deadline=[Diagnostics.Stopwatch]::StartNew()
        while(((-not (Test-Path -LiteralPath $report)) -or (Get-Item -LiteralPath $report).Length -eq 0) -and -not $process.HasExited -and $deadline.Elapsed.TotalSeconds -lt $TimeoutSeconds){Start-Sleep -Milliseconds 100}
        if((-not (Test-Path -LiteralPath $report)) -or (Get-Item -LiteralPath $report).Length -eq 0){if(-not $process.HasExited){Stop-Process -Id $process.Id -Force};throw 'FBE binary rename runtime did not write its report.'}
        # The native scenario posts WM_CLOSE after writing the report. Await its
        # real shutdown; killing it here hides teardown defects from ASan.
        if(-not $process.WaitForExit($TimeoutSeconds*1000)){Stop-Process -Id $process.Id -Force;throw 'FBE binary rename shutdown timed out.'}
        if($process.ExitCode -ne 0){throw "FBE binary rename failed: exit $($process.ExitCode)."}
    } finally {$env:FBE_NEXT_TEST_MODE=$oldMode;$env:FBE_NEXT_TEST_SCENARIO=$oldScenario}
    $rows=@{}; Get-Content -LiteralPath $report | ForEach-Object {$pair=$_ -split '=',2;if($pair.Count -eq 2){$rows[$pair[0]]=$pair[1]}}
    if($rows.dom_rename -ne '1' -or $rows.body_source_roundtrip -ne '1' -or $rows.saved -ne '1'){throw "Binary rename runtime report failed: $(Get-Content $report -Raw)"}
    [xml]$xml=Get-Content -LiteralPath $fixture -Raw; $ns=[Xml.XmlNamespaceManager]::new($xml.NameTable);$ns.AddNamespace('fb','http://www.gribuser.ru/xml/fictionbook/2.0')
    foreach($path in @('//fb:title-info/fb:coverpage/fb:image','//fb:src-title-info/fb:coverpage/fb:image','//fb:body//fb:image')){foreach($image in @($xml.SelectNodes($path,$ns))){if($image.GetAttribute('href','http://www.w3.org/1999/xlink') -ne '#cover-renamed.png'){throw "Saved reference was not renamed: $path"}}}
    if($null -eq $xml.SelectSingleNode('/fb:FictionBook/fb:binary[@id="cover-renamed.png"]',$ns)){throw 'Saved binary with new ID is missing.'}
    if($null -ne $xml.SelectSingleNode('/fb:FictionBook/fb:binary[@id="other.png"]',$ns)){throw 'Unused binary survived deletion and Save.'}
    if($xml.OuterXml -match 'cover-old\.png'){throw 'Old binary ID survived the save.'}
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='binary-rename-reopen-runtime'
        $process=Start-Process -FilePath $FbeExe -WorkingDirectory $isolation.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',$reopen,$fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds*1000)){Stop-Process -Id $process.Id -Force;throw 'FBE binary rename reopen timed out.'}
    } finally {$env:FBE_NEXT_TEST_MODE=$oldMode;$env:FBE_NEXT_TEST_SCENARIO=$oldScenario}
    if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $reopen)){
        $detail=if(Test-Path -LiteralPath $reopen){Get-Content -LiteralPath $reopen -Raw}else{'<report missing>'}
        throw "FBE binary rename reopen failed: exit $($process.ExitCode). $detail"
    }
    $reopenRows=@{}; Get-Content -LiteralPath $reopen | ForEach-Object {$pair=$_ -split '=',2;if($pair.Count -eq 2){$reopenRows[$pair[0]]=$pair[1]}}
    if($reopenRows.dom_reopen -ne '1'){throw "FBE binary rename reopen DOM validation failed: $(Get-Content $reopen -Raw)"}
    $passed = $true
    Write-Host 'FBE production binary rename -> Save -> reopen passed.'
} finally { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed }
