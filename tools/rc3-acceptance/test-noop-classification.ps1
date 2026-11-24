$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'NoopClassification.ps1')
function item([string]$name, $dirty, [string]$edit = 'PASS') {
    return [pscustomobject]@{case=$name;noOpDirty=$dirty;realEditSaveUndoRedoReopen=$edit}
}
$cases = @('body','description','binary')
$good = @(item 'body' $false; item 'description' $false; item 'binary' $false)
$known = @(item 'body' $true; item 'description' $true; item 'binary' $true)
$checks = @(
    @{name='clean';results=$good;accept=$false;expected='PASS'},
    @{name='strict-known';results=$known;accept=$false;expected='FAIL'},
    @{name='explicit-known';results=$known;accept=$true;expected='KNOWN FAILURE'},
    @{name='edit-failure';results=@((item 'body' $true 'FAIL'),$known[1],$known[2]);accept=$true;expected='FAIL'},
    @{name='missing';results=@($known[0],$known[1]);accept=$true;expected='INCOMPLETE'},
    @{name='duplicate';results=@($known[0],$known[0],$known[2]);accept=$true;expected='INCOMPLETE'},
    @{name='unknown-dirty';results=@((item 'body' $null),$known[1],$known[2]);accept=$true;expected='INCOMPLETE'}
)
foreach ($check in $checks) {
    $actual = Get-Rc3NoopStatus -Results $check.results -ExpectedCases $cases -AcceptKnownNoop $check.accept
    if ($actual -cne $check.expected) { throw "Wrong no-op classification for $($check.name): $actual" }
}
Write-Host 'No-op classification: PASS (known exception and incomplete/failing edits distinguished).'
