[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ((Get-FileHash (Join-Path $root 'src\contracts\fbe.idl') -Algorithm SHA256).Hash -ne 'FB69F8A6C18DACED5C5E435C5E23286A30F89274586F569C9A730322CE3AAEEE') { throw 'Legacy COM ABI source changed.' }
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'scripting-api'
$passed = $false
try {
    $null = New-IsolatedFbeSettingsSeed -Isolation $isolation
    $scripts = Join-Path $isolation.Root 'api-scripts'
    New-Item -ItemType Directory -Path $scripts | Out-Null
    $settings = Join-Path $isolation.DataPath 'Settings'
    '<Scripts version="1"><Script uid="11111111-1111-4111-8111-111111111111" path="outer.js" fingerprint="test"/><Script uid="22222222-2222-4222-8222-222222222222" path="child.js" fingerprint="test2"/></Scripts>' | Set-Content (Join-Path $settings 'ScriptRegistry.xml') -Encoding Unicode
    $outer = @'
var stage1Failure = "not executed";
function stage1Assert(value, label) { if(!value) throw new Error(label); }
function Run() {
 var e=window.external, self=e.GetCurrentScriptPath();
 try {
  stage1Assert(e.GetScriptingApiVersion()=="1.0.0","version");
  stage1Assert(e.HasScriptingMethod("BeginUndoUnit") && e.HasScriptingMethod("ApplySourceText"),"legacy lookup");
  stage1Assert(!e.HasScriptingMethod("Nonexistent45") && !e.HasScriptingMethod("ReadTextFile"),"unknown/deferred lookup");
  stage1Assert(e.GetScriptingCapabilities().indexOf('"scriptContext":true')>=0,"capabilities");
  stage1Assert(self.toLowerCase()==@@OUTER@@.toLowerCase(),"outer path");
  stage1Assert(e.GetCurrentScriptName()=="outer.js","name");
  stage1Assert(e.GetCurrentScriptDirectory().toLowerCase()==(@@ROOT@@+"\\").toLowerCase(),"directory");
  stage1Assert(e.GetCurrentScriptId()=="11111111-1111-4111-8111-111111111111","registered uid");
  stage1Assert(e.GetScriptsRootDirectory().toLowerCase().replace(/[\\\/]$/, "")==@@ROOT@@.toLowerCase(),"scripts root");
  stage1Assert(e.GetInterfaceLanguage().indexOf("-")>0,"locale");
  stage1Assert(e.GetCurrentViewMode()=="design" && typeof(e.IsDocumentModified())=="boolean","environment");
  stage1Assert(e.GetUUID().length>0 && e.GetNBSP().length>0 && e.GetProgramVersion().length>0,"legacy invocation");
  apiRunCmd(@@CHILD@@);
  stage1Assert(window.stage1Child===true && e.GetCurrentScriptPath()==self,"nested restore");
  try { apiRunCmd(@@ERROR@@); stage1Assert(false,"missing nested exception"); } catch(error) { stage1Assert(e.GetCurrentScriptPath()==self,"error restore"); }
  stage1Failure="PASS";
 } catch(error) { stage1Failure=String(error.message || error); }
}
function apiStage1Results() {
 var e=window.external;
 if(e.GetCurrentScriptPath()!="" || e.GetCurrentScriptDirectory()!="" || e.GetCurrentScriptName()!="" || e.GetCurrentScriptId()!="") return "leaked context";
 return stage1Failure;
}
'@
    $outer = $outer.Replace('@@OUTER@@', ((Join-Path $scripts 'outer.js') | ConvertTo-Json -Compress)).Replace('@@ROOT@@', ($scripts | ConvertTo-Json -Compress)).Replace('@@CHILD@@', ((Join-Path $scripts 'child.js') | ConvertTo-Json -Compress)).Replace('@@ERROR@@', ((Join-Path $scripts 'error.js') | ConvertTo-Json -Compress))
    $outer | Set-Content (Join-Path $scripts 'outer.js') -Encoding utf8
    'function Run(){var e=window.external;window.stage1Child=e.GetCurrentScriptName()=="child.js" && e.GetCurrentScriptId()=="22222222-2222-4222-8222-222222222222";}' | Set-Content (Join-Path $scripts 'child.js') -Encoding utf8
    'function Run(){if(window.external.GetCurrentScriptId()!="")throw new Error("unexpected UID");throw new Error("intentional");}' | Set-Content (Join-Path $scripts 'error.js') -Encoding utf8
    $fixture = Join-Path $isolation.Root 'api.fb2'; $report = Join-Path $isolation.Root 'api.tsv'
    '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>API</book-title><lang>en</lang></title-info><document-info><id>scripting-api</id><version>1</version></document-info></description><body><section><p>ordinary paragraph</p></section></body></FictionBook>' | Set-Content $fixture -Encoding utf8
    $oldMode,$oldScenario,$oldRoot=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_SCRIPT_API_ROOT
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='scripting-stage1'; $env:FBE_NEXT_SCRIPT_API_ROOT=$scripts
        $process=Start-Process -FilePath $isolation.Exe -ArgumentList @('--portable','-b',$report,$fixture) -WorkingDirectory $isolation.Root -WindowStyle Hidden -PassThru
        $code=Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'scripting-stage1' -Report $report -TimeoutSeconds $TimeoutSeconds
        $details=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
        if($code -ne 0 -or $details -notmatch 'result=PASS'){throw "Scripting API runtime failed: $code; $details"}
    } finally {$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_SCRIPT_API_ROOT=$oldMode,$oldScenario,$oldRoot}
    $passed=$true
    Write-Host 'Scripting stage 1 COM/JScript runtime: PASS.'
} finally {
    if(-not ([IO.Path]::GetFullPath($isolation.Root).StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase))){throw 'Unexpected runtime directory'}
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
