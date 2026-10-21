[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode='portable',
    [ValidateSet('fb2','fbd')][string]$Kind='fb2',
    [ValidateSet('light','dark')][string]$Theme='light',
    [ValidateSet('normal','zip','rar')][string]$Container='normal')
$ErrorActionPreference='Stop'
$catalog=Get-Content (Join-Path $PSScriptRoot '../../localization/app-ui/fbe-idr-mainframe-menu.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$close=$catalog.strings.'fbe.menu.idr_mainframe.file.close'
foreach($language in $catalog.targetLanguages) {
    $caption=$close.translations.$language
    if($caption -notmatch '&([^&])'){throw "Close mnemonic missing: $language"}
    $mnemonic=$Matches[1]
    foreach($item in $catalog.strings.PSObject.Properties) {
        if($item.Name -eq 'fbe.menu.idr_mainframe.file.close'){continue}
        if($item.Name -notlike 'fbe.menu.idr_mainframe.file.*' -and $item.Name -notin @('fbe.menu.idr_mainframe.popup.import','fbe.menu.idr_mainframe.popup.export','fbe.menu.idr_mainframe.popup.recent_documents')){continue}
        if($item.Value.translations.$language -match '&([^&])' -and $Matches[1] -ieq $mnemonic){throw "Close mnemonic conflict: $language / $($item.Name)"}
    }
}
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'file-close'
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_CLOSE_SECOND','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TEST_CLOSE_EPUB')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name)}
try {
    $fixture=Join-Path $PSScriptRoot '../../out/tests/export-epub-cyrillic/fb2-metadata-cyrillic-smoke.epub'
    if(!(Test-Path -LiteralPath $fixture)) {
        & (Join-Path $PSScriptRoot 'test-export-epub-cyrillic.ps1') -OutputPath $fixture
        if($LASTEXITCODE -ne 0){throw 'EPUB fixture generation failed'}
    }
    $epubCopy=Join-Path $iso.Root 'import-source.epub'
    Copy-Item -LiteralPath $fixture -Destination $epubCopy
    $epubHash=(Get-FileHash $epubCopy -Algorithm SHA256).Hash
    $env:FBE_NEXT_TEST_CLOSE_EPUB=$epubCopy
    $seed=New-IsolatedFbeSettingsSeed $iso
    $scripts=Join-Path $iso.Root 'CloseScripts'
    New-Item -ItemType Directory -Path $scripts | Out-Null
    [IO.File]::WriteAllText((Join-Path $scripts 'CloseProbe.js'),'function Run() { throw new Error("closed script executed"); }',[Text.UTF8Encoding]::new($false))
    $text=[IO.File]::ReadAllText($seed).Replace('<InterfaceTheme>0</InterfaceTheme>',"<InterfaceTheme>$(if($Theme -eq 'dark'){2}else{1})</InterfaceTheme>")
    $text=$text.Replace('</Settings>',"<ScriptsFolder>$scripts</ScriptsFolder></Settings>")
    [IO.File]::WriteAllText($seed,$text,[Text.UTF8Encoding]::new($false))
    $body=if($Kind -eq 'fb2'){'<body><section><p id="close-probe">CLOSE_FIRST_UNIQUE</p><p><image l:href="#cover"/></p></section></body>'}else{''}
    $xml='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>CLOSE_FIRST_UNIQUE</book-title><coverpage><image l:href="#cover"/></coverpage><lang>en</lang></title-info><document-info><id>close-test</id><version>1</version></document-info></description>'+$body+'<binary id="cover" content-type="image/png">iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+a3ioAAAAASUVORK5CYII=</binary></FictionBook>'
    $first=Join-Path $iso.Root "first.$Kind";$second=Join-Path $iso.Root "second.$Kind";$report=Join-Path $iso.Root 'file-close.txt'
    [IO.File]::WriteAllText($first,$xml,[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $iso.Root 'malformed.fb2'),'<invalid>',[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($second,$xml.Replace('CLOSE_FIRST_UNIQUE','CLOSE_SECOND_UNIQUE'),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $iso.Root 'import-source.fb2'),$xml.Replace('CLOSE_FIRST_UNIQUE','EXISTING_FB2_MUST_NOT_CHANGE'),[Text.UTF8Encoding]::new($false))
    $inputPath=$first
    if($Container -eq 'zip') {
        Add-Type -AssemblyName System.IO.Compression
        $inputPath=Join-Path $iso.Root 'first.zip';$stream=[IO.File]::Create($inputPath)
        $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
        try{$entry=$zip.CreateEntry("first.$Kind");$writer=[IO.StreamWriter]::new($entry.Open(),[Text.UTF8Encoding]::new($false));try{$writer.Write($xml)}finally{$writer.Dispose()}}finally{$zip.Dispose();$stream.Dispose()}
    } elseif($Container -eq 'rar') {
        if($Kind -ne 'fb2'){throw 'RAR fixture contains FB2'}
        $inputPath=Join-Path $iso.Root 'first.rar'
        [IO.File]::WriteAllBytes($inputPath,[Convert]::FromBase64String((Get-Content (Join-Path $PSScriptRoot 'fixtures/archive-runtime-rar5.b64') -Raw).Trim()))
    }
    $before=(Get-FileHash $inputPath -Algorithm SHA256).Hash
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='file-close-runtime'
    $env:FBE_NEXT_TEST_CLOSE_SECOND=$second;$env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @("--$Mode",'-b',$report,$inputPath) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'file-close' -Report $report -TimeoutSeconds 120
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "File Close failed (exit=$code): $text"}
    if((Get-FileHash $inputPath -Algorithm SHA256).Hash -ne $before){throw 'Opening/closing changed the initial file/container'}
    if((Get-FileHash $epubCopy -Algorithm SHA256).Hash -ne $epubHash){throw 'Import/save changed the source EPUB'}
    if($Container -eq 'normal' -and (Get-Content $second -Raw) -notmatch 'CLOSE_UNSAVED'){throw 'Confirmed Save did not write Source changes'}
    Write-Host $text;$passed=$true;Write-Host "File Close $Mode/$Kind/$Theme/$Container PASS"
}finally{
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name])}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
