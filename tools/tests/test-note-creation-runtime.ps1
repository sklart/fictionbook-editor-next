[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [switch]$RepresentativeOnly, [string[]]$Cases=@(), [int]$TimeoutSeconds=60,
    [string]$ArtifactDirectory=(Join-Path $PSScriptRoot '..\..\out\tests\note-creation-runtime'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'note-creation'
$savedMode=$env:FBE_NEXT_TEST_MODE
$savedScenario=$env:FBE_NEXT_TEST_SCENARIO
$savedProbe=$env:FBE_NEXT_TEST_NOTES_PATH
$savedStage=$env:FBE_NEXT_TEST_NOTES_STAGE_PATH
$savedBreadcrumb=$env:FBE_NEXT_TEST_STARTUP_BREADCRUMB
$passed=$false
try {
    if($RepresentativeOnly -and $Cases.Count) { throw '-Cases and -RepresentativeOnly cannot be combined.' }
    if(@($Cases | Select-Object -Unique).Count -ne $Cases.Count) { throw '-Cases contains duplicate scenario names.' }
    foreach($case in $Cases) {
        if($case -cnotmatch '^(notes|comments)-0[1-6]-[a-z-]+$') { throw "Invalid note creation scenario: $case" }
    }
    New-IsolatedFbeSettingsSeed -Isolation $isolation | Out-Null
    $selectedCasesJson=ConvertTo-Json -InputObject @($Cases) -Compress
    $wrapper="function Run(){`r`nvar candidates=[];`r`nvar selectedCases=$selectedCasesJson;`r`n"
    $expected=@()
    foreach($kind in @('notes','comments')) {
        $folder=if($kind -eq 'notes') {'02_Примечания'} else {'03_Комментарии'}
        $variants=if($Cases.Count) {
            @($Cases | Where-Object { $_ -like "$kind-*" } | ForEach-Object { [int]($_ -split '-')[1] } | Select-Object -Unique)
        } elseif($RepresentativeOnly) {@(2)} else {@(1..6)}
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
    if($Cases.Count) {
        foreach($case in $Cases) { if($expected -cnotcontains $case) { throw "Unsupported note creation scenario: $case" } }
        $expected=@($Cases)
    }
    $wrapper+=(Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'note-creation-runtime-probe.js'))+"`r`n}`r`n"
    $probe=Join-Path $isolation.Root 'notes-probe.js'
    [IO.File]::WriteAllText($probe,$wrapper,[Text.UTF8Encoding]::new($true))
    $document=Join-Path $isolation.DataPath 'notes.fb2'
    [IO.File]::WriteAllText($document,'<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Notes Undo probe</book-title><lang>ru</lang></title-info></description><body><section><p>Test</p></section></body></FictionBook>',[Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='note-creation-runtime'; $env:FBE_NEXT_TEST_NOTES_PATH=$probe
    $env:FBE_NEXT_TEST_NOTES_STAGE_PATH=$isolation.StagePath
    $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB=Join-Path $isolation.Root 'startup-breadcrumb.txt'
    Set-IsolatedRuntimeStage -Isolation $isolation -Stage 'launch'
    $report=Join-Path $isolation.DataPath 'Diagnostics/note-creation-report.txt'
    $process=Start-Process -FilePath $isolation.Exe -WindowStyle Hidden -WorkingDirectory $isolation.Root -ArgumentList @('--portable',$document) -PassThru
    try {
        $exitCode=Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'note-creation-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
    } catch {
        $stage=if(Test-Path -LiteralPath $isolation.StagePath) { (Get-Content -Raw -LiteralPath $isolation.StagePath).Trim() } else { '<missing>' }
        $startup=if(Test-Path -LiteralPath $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB) { Get-Content -LiteralPath $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB -Tail 1 } else { '<missing>' }
        throw "Note creation runtime stopped: $($_.Exception.Message); last stage: $stage; startup breadcrumb: $startup"
    }
    New-Item -ItemType Directory -Force $ArtifactDirectory | Out-Null
    Copy-Item -LiteralPath $report -Destination (Join-Path $ArtifactDirectory 'note-creation-report.txt')
    $entries=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    foreach($line in (Get-Content -LiteralPath $report)) {
        if(!$line) {continue}
        $equals=$line.IndexOf('='); if($equals -lt 1) {throw "Malformed report: $line"}
        $key=$line.Substring(0,$equals); if($entries.ContainsKey($key)) {throw "Duplicate: $key"}
        $entries.Add($key,$line.Substring($equals+1))
    }
    if($entries.ContainsKey('failed-scenario')) {
        throw "Note creation scenario $($entries['failed-scenario']) failed at $($entries['failed-stage']): native error $($entries['native-error'])"
    }
    foreach($key in $expected) {
        if(!$entries.ContainsKey($key) -or $entries[$key] -cne '1') {
            $parts=@('behavior','closed','undo','redo') | ForEach-Object {
                $partKey="$key-$_"
                $partValue=if($entries.ContainsKey($partKey)) { $entries[$partKey] } else { '<missing>' }
                "$_=$partValue"
            }
            $detail=if($entries.ContainsKey("$key-detail")) { [uri]::UnescapeDataString($entries["$key-detail"]) } else { '<missing>' }
            throw "Note creation scenario $key failed: $($parts -join ', '); detail=$detail"
        }
    }
    if($exitCode -ne 0 -or $entries['result'] -cne 'pass' -or $entries['fixtures'] -ne [string]$expected.Count) {throw 'Native runtime result/count is not PASS.'}
    $passed=$true
    Write-Host "Note creation runtime: PASS ($($expected.Count) scenarios; HTML Undo/Redo and closed group)."
} finally {
    $env:FBE_NEXT_TEST_MODE=$savedMode; $env:FBE_NEXT_TEST_SCENARIO=$savedScenario
    $env:FBE_NEXT_TEST_NOTES_PATH=$savedProbe; $env:FBE_NEXT_TEST_NOTES_STAGE_PATH=$savedStage
    $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB=$savedBreadcrumb
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
