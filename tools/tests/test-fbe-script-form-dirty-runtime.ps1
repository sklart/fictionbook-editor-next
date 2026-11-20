<# A script must not hide a programmatic change to a clean Description form. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [int]$TimeoutSeconds = 90, [switch]$SaveFirst, [switch]$SaveOnly)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'script-form-dirty'
$passed = $false
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    $null = New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'ProbeScripts')
    $fixture='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Tester</nickname></author><book-title>Before</book-title><lang>en</lang></title-info><document-info><author><nickname>Tester</nickname></author><date value="2026-01-01">2026-01-01</date><id>script-form-test</id><version>1.0</version></document-info></description><body><section><p>Body</p></section></body></FictionBook>'
    [IO.File]::WriteAllText((Join-Path $iso.Root 'fixture.fb2'),$fixture,[Text.UTF8Encoding]::new($false))
    $script = @'
var result="NOT RUN";
function Run(){try{var e=window.external, field=document.getElementById("tiTitle");if(!field){result="FAIL missing title";return;}if(@@SAVE@@){field.value="SAVED TITLE";if(!e.SaveCurrentDocument() || e.IsDocumentModified()){result="FAIL save or clean state";return;}if(@@ONLY@@){result="PASS";return;}}field.value="SCRIPT CHANGED TITLE";result=e.IsDocumentModified()?"PASS":"FAIL edited title reported clean";}catch(error){result="FAIL exception "+String(error.message||error.description||error);}}
function CompatResult(){if(result!=="PASS")return result;var dirty=window.external.IsDocumentModified(),value=document.getElementById("tiTitle").value;return (@@ONLY@@ ? !dirty && value==="SAVED TITLE" : dirty && value==="SCRIPT CHANGED TITLE")?"PASS":"FAIL title state after script";}
'@
    $script=$script.Replace('@@SAVE@@',$(if($SaveFirst -or $SaveOnly){'true'}else{'false'})).Replace('@@ONLY@@',$(if($SaveOnly){'true'}else{'false'}))
    [IO.File]::WriteAllText((Join-Path $iso.Root 'edit.js'), $script, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $iso.Root 'suite.xml'), '<suite><step name="script-form-dirty" script="edit.js"/></suite>', [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='binary-scripting-compatibility'; $env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
    $report=Join-Path $iso.Root 'report.txt'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,(Join-Path $iso.Root 'fixture.fb2')) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'script-form-dirty' -Report $report -TimeoutSeconds $TimeoutSeconds
    $text=if(Test-Path -LiteralPath $report){Get-Content -Raw -LiteralPath $report}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "Script form dirty failed: $text"}
    if ($SaveFirst -or $SaveOnly) {
        $disk=New-Object -ComObject Msxml2.DOMDocument.6.0; $disk.async=$false; $disk.resolveExternals=$false
        if (-not $disk.load((Join-Path $iso.Root 'fixture.fb2'))) { throw 'Saved fixture is invalid XML.' }
        $title=$disk.selectSingleNode('//*[local-name()="title-info"]/*[local-name()="book-title"]')
        if (-not $title -or $title.text -ne 'SAVED TITLE') { throw 'Saved file did not retain the title before the later edit.' }
    }
    $passed=$true; if ($SaveOnly) { Write-Host 'Saved Description remains clean during and after script execution.' } else { Write-Host 'Programmatic Description edit is dirty during and after script execution.' }
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name,$old[$name],'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
