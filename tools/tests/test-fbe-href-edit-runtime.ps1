[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name community-selection
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO');$saved=@{};foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
try {
 $null=New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root empty-scripts)
 $fixture=Join-Path $iso.Root selection.fb2;$report=Join-Path $iso.Root selection.txt
 $xml=@'
<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>COMMUNITY_SMOKE_3010</book-title><annotation><p id="position-annotation">Book annotation repeated phrase</p></annotation><lang>ru</lang></title-info><document-info><id>positions</id><version>1.0</version></document-info></description><body><section id="outer-section"><section id="first-section"><title><p id="position-title">Section title</p></title><p id="position-first">Repeated identical phrase</p><p id="position-second">Repeated identical phrase</p><p id="position-inline">Prefix <strong>bold <emphasis>inside</emphasis></strong> suffix</p><p id="position-unicode">Кириллица Ω &amp; &lt; Юникод текст</p></section><section id="nested-section"><p id="position-nested">INDEPENDENT_SOURCE_TARGET nested text</p></section></section></body><body name="notes"><section id="note"><p id="position-note">Repeated identical phrase</p></section></body><body name="comments"><section id="comment"><p id="position-comment">Repeated identical phrase</p></section></body></FictionBook>
'@
 [IO.File]::WriteAllText($fixture,$xml,[Text.UTF8Encoding]::new($false))
 $xml=$xml.Replace('<p id="position-first">', '<p id="href-tests"><a id="href-internal" l:href="#note">Internal</a> <a id="href-http" l:href="http://example.org/path">HTTP</a> <a id="href-https" l:href="https://example.org/path">HTTPS</a> <a id="href-note" type="note" l:href="#note">1</a></p><p id="position-first">')
 [IO.File]::WriteAllText($fixture,$xml,[Text.UTF8Encoding]::new($false))
 $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='community-href-edit'
 $p=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
 $exitCode=Wait-IsolatedFbeProcess -Process $p -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 240
 $text=if(Test-Path -LiteralPath $report){Get-Content -LiteralPath $report -Raw}else{'missing'}
 $evidence=Join-Path (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path ('build/community-href-'+[guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $evidence | Out-Null
 if(Test-Path -LiteralPath $report){Copy-Item -LiteralPath $report -Destination $evidence}
 Get-FileHash -LiteralPath $FbeExe | Format-List | Out-File (Join-Path $evidence executable-sha256.txt)
 if($exitCode -ne 0 -or $text -notmatch '(?m)^result=PASS\r?$'){throw "Native selection runtime failed: exit=$exitCode`n$text"}
 $passed=$true;Write-Host "PASS: native href edit, caret, attribute, Undo/Redo, Save/Reopen. $evidence"
} finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name])};Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
