[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'), [int]$TimeoutSeconds=45)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$repoRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name description-layout
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE','FBE_NEXT_TRACE_VERBOSE')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
 $null=New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'ProbeScripts')
 # Exercise current source assets with the compiled native bridge.
 foreach($name in @('main.js','main.css','main_fast.css','fb2.xsl')) { Copy-Item (Join-Path $repoRoot "runtime/$name") (Join-Path $iso.Root $name) -Force }
 Copy-Item (Join-Path $repoRoot 'runtime/DescriptionIcons') (Join-Path $iso.Root 'DescriptionIcons') -Recurse -Force
 Copy-Item (Join-Path $repoRoot 'runtime/blank.fb2') (Join-Path $iso.Root 'fixture.fb2')
 Copy-Item (Join-Path $PSScriptRoot 'fixtures/description') (Join-Path $iso.Root 'description-fixtures') -Recurse
 foreach($count in @(100,500)) {
  [xml]$stress=Get-Content (Join-Path $PSScriptRoot 'fixtures/description/minimal.fb2') -Raw
  $ns='http://www.gribuser.ru/xml/fictionbook/2.0'
  for($i=0;$i -lt $count;$i++) {
   $binary=$stress.CreateElement('binary',$ns);$binary.SetAttribute('id',"stress-$i");$binary.SetAttribute('content-type','application/octet-stream');$binary.InnerText='AAECA/8='
   $null=$stress.DocumentElement.AppendChild($binary)
  }
  $titleInfo=$stress.GetElementsByTagName('title-info').Item(0)
  $firstAuthor=$titleInfo.GetElementsByTagName('author').Item(0)
  $bookTitle=$titleInfo.GetElementsByTagName('book-title').Item(0)
  for($i=0;$i -lt 29;$i++){$null=$titleInfo.InsertBefore($firstAuthor.CloneNode($true),$bookTitle)}
  $stress.Save((Join-Path $iso.Root "description-fixtures/stress-$count.fb2"))
 }
 $probe=Get-Content (Join-Path $PSScriptRoot 'description-layout-behavior.js') -Raw
 $literal=$probe|ConvertTo-Json -Compress
 $timingsPath=(Join-Path $iso.Root 'description-timings.txt')|ConvertTo-Json -Compress
 $script='var compatResult="NOT RUN";function CompatResult(){return compatResult;}function Run(){try{document.parentWindow.execScript('+$literal+',"JScript");compatResult=String(document.documentElement.getAttribute("data-description-regression"));if(!window.external.WriteTextFile('+$timingsPath+',String(document.documentElement.getAttribute("data-description-timings")),"utf-8"))throw new Error("Write timing evidence");}catch(error){compatResult="FAIL: "+String(error.message||error);}}'
 [IO.File]::WriteAllText((Join-Path $iso.Root 'description.js'),$script,[Text.UTF8Encoding]::new($false))
 $navigationProbe='var result="NOT RUN";function CompatResult(){return result;}function Run(){try{apiSetDescriptionLayout("modern");var root=document.getElementById("fbw_desc"),items=root._descriptionNavigationItems;for(var i=0;i<items.length;i++){items[i].click();}apiSetDescriptionLayout("classic");apiSetDescriptionLayout("modern");result="PASS";}catch(e){result="FAIL: "+String(e.message||e);}}'
 [IO.File]::WriteAllText((Join-Path $iso.Root 'navigation-dirty.js'),$navigationProbe,[Text.UTF8Encoding]::new($false))
 [IO.File]::WriteAllText((Join-Path $iso.Root 'suite.xml'),'<suite><step name="navigation-dirty" script="navigation-dirty.js" rawDirty="unchanged"/><step name="description-light" descriptionTheme="light" script="description.js"/><step name="description-dark" descriptionTheme="dark" script="description.js"/><step name="description-light-again" descriptionTheme="light" script="description.js"/></suite>',[Text.UTF8Encoding]::new($false))
 $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='binary-scripting-compatibility';$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
 $env:FBE_NEXT_TRACE='1'
 $env:FBE_NEXT_TRACE_VERBOSE='1'
 $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
 $report=Join-Path $iso.Root 'report.txt'
 $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,(Join-Path $iso.Root 'fixture.fb2')) -WindowStyle Hidden -PassThru
 $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario binary-scripting-compatibility -Report $report -TimeoutSeconds $TimeoutSeconds
 if($code -ne 0 -or -not(Test-Path $report) -or (Get-Content $report -Raw) -notmatch 'result=PASS'){ if(Test-Path $report){Get-Content $report}; throw "Description runtime regression failed: $report" }
 $evidence=Join-Path $repoRoot 'out/tests/description-layout';New-Item -ItemType Directory -Force $evidence|Out-Null
 Copy-Item $report (Join-Path $evidence 'report.txt') -Force
 Copy-Item (Join-Path $iso.Root 'description-timings.txt') $evidence -Force
 @{status='PASS';fixtures=9;themes=@('light','dark','light');layouts=@('modern','classic');binaryCounts=@(100,500);stressAuthors=30;exeSha256=(Get-FileHash $iso.Exe).Hash;mainJsSha256=(Get-FileHash (Join-Path $iso.Root 'main.js')).Hash;xslSha256=(Get-FileHash (Join-Path $iso.Root 'fb2.xsl')).Hash}|ConvertTo-Json|Set-Content (Join-Path $evidence 'summary.json') -Encoding utf8
 Get-Content (Join-Path $evidence 'description-timings.txt')
 $passed=$true;Write-Host 'Description Modern/Classic production MSHTML behavior PASS.'
} finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')};Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
