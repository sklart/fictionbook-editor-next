[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe,[int]$TimeoutSeconds=120)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'image-unicode-url'; $passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY'); $old=@{}
foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
 $null=New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts')
 Add-Type -AssemblyName System.Drawing
 $unicodeId=([string]::Concat([char[]](0x043a,0x0438,0x0440,0x0438,0x043b,0x043b,0x0438,0x0446,0x0430))+'.png')
 $items=@(@{id=$unicodeId;width=103;color='Red'},@{id='ascii.png';width=109;color='Blue'})
 $images='';$binaries=''
 foreach($item in $items){
  $bitmap=[Drawing.Bitmap]::new($item.width,7);$graphics=[Drawing.Graphics]::FromImage($bitmap);$stream=[IO.MemoryStream]::new()
  try {$graphics.Clear([Drawing.Color]::FromName($item.color));$bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png);$data=[Convert]::ToBase64String($stream.ToArray())} finally {$graphics.Dispose();$bitmap.Dispose();$stream.Dispose()}
  $images+='<p><image l:href="#'+$item.id+'"/></p>'; $binaries+='<binary id="'+$item.id+'" content-type="image/png">'+$data+'</binary>'
 }
 $book='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Image URL regression</book-title><lang>en</lang></title-info><document-info><id>image-url</id><version>1</version></document-info></description><body><section><title><p>Images</p></title>'+$images+'</section></body>'+$binaries+'</FictionBook>'
 [IO.File]::WriteAllText((Join-Path $iso.Root 'fixture.fb2'),$book,[Text.UTF8Encoding]::new($false))
 $js=@'
var result="NOT RUN";
function Run(){var root=document.getElementById("fbw_body"), a=root?root.getElementsByTagName("IMG"):[], expected=[103,109]; if(a.length!==expected.length){result="FAIL count="+a.length;return;} for(var i=0;i<a.length;i++){if(a[i].width!==expected[i]){result="FAIL image="+i+" width="+a[i].width+" src="+a[i].src;return;}} result="PASS";}
function CompatResult(){return result;}
'@
 [IO.File]::WriteAllText((Join-Path $iso.Root 'probe.js'),$js,[Text.UTF8Encoding]::new($false))
 [IO.File]::WriteAllText((Join-Path $iso.Root 'suite.xml'),'<suite><step name="before" script="probe.js" save="saved.fb2"/><step name="reopen" command="open" open="saved.fb2" script="probe.js"/></suite>',[Text.UTF8Encoding]::new($false))
 $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='binary-scripting-compatibility';$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root;$env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
 $report=Join-Path $iso.Root 'report.txt';$fixture=Join-Path $iso.Root 'fixture.fb2'
 $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
 $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'image-unicode-url' -Report $report -TimeoutSeconds $TimeoutSeconds
 $state=Get-Content -LiteralPath $report -Raw; if($code -ne 0 -or $state -notmatch 'result=PASS'){throw "Scenario failed: $state"}
 $before=[xml](Get-Content -LiteralPath $fixture -Raw -Encoding UTF8);$after=[xml](Get-Content -LiteralPath (Join-Path $iso.Root 'saved.fb2') -Raw -Encoding UTF8)
 $b=@($before.FictionBook.binary | ForEach-Object { "$($_.id)|$($_.'content-type')|$([Convert]::ToBase64String([Convert]::FromBase64String($_.'#text')))" })
 $a=@($after.FictionBook.binary | ForEach-Object { "$($_.id)|$($_.'content-type')|$([Convert]::ToBase64String([Convert]::FromBase64String($_.'#text')))" })
 if($a.Count -ne $b.Count -or (Compare-Object $a $b)){throw 'Binary bytes or metadata changed after Save'}
 $passed=$true;Write-Output "PASS: $($a.Count) binary images displayed before and after Save/Reopen; bytes and metadata unchanged"
} finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')};Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
