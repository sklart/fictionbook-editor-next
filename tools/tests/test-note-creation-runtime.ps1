[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [switch]$RepresentativeOnly, [int]$TimeoutSeconds=60,
    [string]$ArtifactDirectory=(Join-Path $PSScriptRoot '..\..\out\tests\note-creation-runtime'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'note-creation'
$savedMode=$env:FBE_NEXT_TEST_MODE
$savedScenario=$env:FBE_NEXT_TEST_SCENARIO
$savedProbe=$env:FBE_NEXT_TEST_NOTES_PATH
$passed=$false
try {
    New-IsolatedFbeSettingsSeed -Isolation $isolation | Out-Null
    $wrapper="function Run(){`r`nvar candidates=[];`r`n"
    $expected=@()
    foreach($kind in @('notes','comments')) {
        $folder=if($kind -eq 'notes') {'02_Примечания'} else {'03_Комментарии'}
        $variants=if($RepresentativeOnly) {@(2)} else {@(1..6)}
        foreach($variant in $variants) {
            $script=Get-ChildItem -LiteralPath (Join-Path $root "runtime/Scripts/$folder") -Filter ('{0:00}_*.js' -f $variant)
            $source=[IO.File]::ReadAllText($script.FullName, [Text.Encoding]::UTF8).TrimStart([char]0xFEFF)
            $name='{0}-{1:00}' -f $kind,$variant
            $wrapper+="candidates.push({name:'$name',kind:'$kind',variant:$variant,run:(function(){`r`n$source`r`nreturn Run;})()});`r`n"
            $modes=@('ordinary','existing','missing-body','bad-link','nested')
            $modes+=if($variant -le 3) {'middle'} else {'last-refusal'}
            if($variant -in @(2,5)) {$modes+=@('cancel','retry-cancel')}
            if($variant -eq 2) {$modes+=@('fault','fault-before-body','empty-body','id-collision','fault-middle','selected','selected-fault')}
            $expected+=@($modes | ForEach-Object {"$name-$_"})
        }
    }
    $wrapper+=(Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'note-creation-runtime-probe.js'))+"`r`n}`r`n"
    $probe=Join-Path $isolation.Root 'notes-probe.js'
    [IO.File]::WriteAllText($probe,$wrapper,[Text.UTF8Encoding]::new($true))
    $document=Join-Path $isolation.DataPath 'notes.fb2'
    [IO.File]::WriteAllText($document,'<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Notes Undo probe</book-title><lang>ru</lang></title-info></description><body><section><p>Test</p></section></body></FictionBook>',[Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='note-creation-runtime'; $env:FBE_NEXT_TEST_NOTES_PATH=$probe
    $report=Join-Path $isolation.DataPath 'Diagnostics/note-creation-report.txt'
    $process=Start-Process -FilePath $isolation.Exe -WindowStyle Hidden -WorkingDirectory $isolation.Root -ArgumentList @('--portable',$document) -PassThru
    $exitCode=Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'note-creation-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
    New-Item -ItemType Directory -Force $ArtifactDirectory | Out-Null
    Copy-Item -LiteralPath $report -Destination (Join-Path $ArtifactDirectory 'note-creation-report.txt')
    $entries=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    foreach($line in (Get-Content -LiteralPath $report)) {
        if(!$line) {continue}
        $equals=$line.IndexOf('='); if($equals -lt 1) {throw "Malformed report: $line"}
        $key=$line.Substring(0,$equals); if($entries.ContainsKey($key)) {throw "Duplicate: $key"}
        $entries.Add($key,$line.Substring($equals+1))
    }
    foreach($key in $expected) {
        if(!$entries.ContainsKey($key) -or $entries[$key] -cne '1') {
            Get-Content -LiteralPath $report | Write-Host
            throw "Failed/missing scenario: $key"
        }
    }
    if($exitCode -ne 0 -or $entries['result'] -cne 'pass' -or $entries['fixtures'] -ne [string]$expected.Count) {throw 'Native runtime result/count is not PASS.'}
    $passed=$true
    Write-Host "Note creation runtime: PASS ($($expected.Count) scenarios; isolated history, HTML Undo/Redo, refusal state, closed group)."
} finally {
    $env:FBE_NEXT_TEST_MODE=$savedMode; $env:FBE_NEXT_TEST_SCENARIO=$savedScenario; $env:FBE_NEXT_TEST_NOTES_PATH=$savedProbe
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
