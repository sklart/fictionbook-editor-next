$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'NoopClassification.ps1')
function item([string]$name, $dirty, [string]$edit = 'PASS') {
    return [pscustomobject]@{case=$name;noOpDirty=$dirty;realEditSaveUndoRedoReopen=$edit}
}
$cases = @('body','description','binary')
$good = @(item 'body' $false; item 'description' $false; item 'binary' $false)
$known = @(item 'body' $true; item 'description' $true; item 'binary' $true)
$checks = @(
    @{name='clean';results=$good;expected='PASS'},
    @{name='strict-known';results=$known;expected='FAIL'},
    @{name='edit-failure';results=@((item 'body' $true 'FAIL'),$known[1],$known[2]);expected='FAIL'},
    @{name='missing';results=@($known[0],$known[1]);expected='INCOMPLETE'},
    @{name='duplicate';results=@($known[0],$known[0],$known[2]);expected='INCOMPLETE'},
    @{name='unknown-dirty';results=@((item 'body' $null),$known[1],$known[2]);expected='INCOMPLETE'}
)
foreach ($check in $checks) {
    $actual = Get-Rc3NoopStatus -Results $check.results -ExpectedCases $cases
    if ($actual -cne $check.expected) { throw "Wrong no-op classification for $($check.name): $actual" }
}
Write-Host 'No-op classification: PASS (incomplete results and dirty regressions rejected).'
