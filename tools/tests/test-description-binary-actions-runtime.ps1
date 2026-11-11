[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'), [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'description-binary-actions'
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old = @{}; foreach($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$passed = $false
try {
    $null = New-IsolatedFbeSettingsSeed $iso
    $fixture = Join-Path $iso.Root 'binary.fb2'
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fixtures/description/binary.fb2') -Destination $fixture
    $script = @'
var compatResult='NOT RUN';
function CompatResult(){return compatResult;}
function Run(){
 try {
  function assert(ok,message){if(!ok)throw new Error(message);}
  function serialize(){var x=new ActiveXObject('Msxml2.DOMDocument.6.0');x.async=false;x.loadXML('<FictionBook xmlns="'+fbNS+'" xmlns:l="'+xlNS+'"/>');GetDesc(x,null,null);return String(x.xml);}
  var xml=new ActiveXObject('Msxml2.DOMDocument.6.0');xml.async=false;
  assert(xml.load(window.external.GetStylePath()+'\\binary.fb2') && LoadFromDOM(xml,'english'),'Load fixture');
  var rows=document.getElementById('binobj').getElementsByTagName('DIV');
  assert(rows.length>=2,'Binary fixture rows exist');
  var coverRow=rows[0], coverId=GetBinaryInput(coverRow,'id').value, buttons=coverRow.getElementsByTagName('BUTTON'), view=null, save=null;
  for(var b=0;b<buttons.length;b++){if(buttons[b].id=='show')view=buttons[b];if(buttons[b].id=='save')save=buttons[b];}
  assert(view && save && String(coverRow.className).indexOf('description-binary')<0,'Legacy view/save buttons do not depend on removed icons');
  // The native writer is real; the Save As prompt is replaced by a disposable path.
  var oldSave=SaveImage, savedSource='', savedFile=false;
  SaveImage=function(source){savedSource=source;savedFile=window.external.SaveBinary(window.external.GetStylePath()+'\\description-export.bin',coverRow.base64data,false);};
  try{save.click();}finally{SaveImage=oldSave;}
  assert(savedFile && savedSource=='fbw-internal:#'+coverId,'Save button and native binary writer');
  view.click();
  assert(document.getElementById('fullImgPanel').style.visibility=='visible' && String(document.getElementById('fullImg').src).indexOf(coverId)>=0,'View binary through button');
  HideFullImage();
  var cover=document.getElementById('tiCover').getElementsByTagName('SELECT')[0], original=cover.value, alternate=-1;
  for(var c=0;c<cover.options.length;c++)if(String(cover.options[c].value).charAt(0)=='#' && cover.options[c].value!=original){alternate=c;break;}
  assert(alternate>=0,'Alternative cover choice populated');
  cover.selectedIndex=alternate;cover.fireEvent('onchange');assert(cover.value!=original && serialize().indexOf(cover.value)>=0,'Cover selection serialized');
  cover.value=original;cover.fireEvent('onchange');
  var count=rows.length, added=apiAddBinary('','description-ui-test.png','image/png',coverRow.base64data), extra=null;
  for(var r=0;r<rows.length;r++)if(GetBinaryInput(rows[r],'id').value==added)extra=rows[r];
  assert(extra && rows.length==count+1,'Add binary for delete check');
  var remove=extra.getElementsByTagName('BUTTON')[0];assert(remove && remove.id=='del','Delete button present');
  remove.click();assert(rows.length==count,'Delete unreferenced binary through button');
  compatResult='PASS';
 }catch(error){compatResult='FAIL: '+String(error.message||error);}
}
'@
    [IO.File]::WriteAllText((Join-Path $iso.Root 'actions.js'), $script, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $iso.Root 'suite.xml'), '<suite><step name="description-binary-actions" script="actions.js"/></suite>', [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='binary-scripting-compatibility'; $env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
    $report = Join-Path $iso.Root 'report.txt'
    $process = Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'description-binary-actions' -Report $report -TimeoutSeconds $TimeoutSeconds
    if($code -ne 0 -or -not (Test-Path -LiteralPath $report) -or (Get-Content -LiteralPath $report -Raw) -notmatch 'result=PASS') {
        if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report }; throw 'Description binary actions failed.'
    }
    [xml]$source = Get-Content -LiteralPath $fixture -Raw
    $node = $source.SelectSingleNode("//*[local-name()='binary' and @id='cover.png']")
    if(-not $node) { throw 'Fixture cover binary missing.' }
    $expected = [Convert]::FromBase64String($node.InnerText)
    $actual = [IO.File]::ReadAllBytes((Join-Path $iso.Root 'description-export.bin'))
    if([Convert]::ToBase64String($expected) -cne [Convert]::ToBase64String($actual)) { throw 'Exported cover bytes differ from fixture.' }
    $passed=$true
    Write-Host 'Description binary view, export bytes, cover selection and delete: PASS. Save As dialog: NOT RUN.'
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name,$old[$name],'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
