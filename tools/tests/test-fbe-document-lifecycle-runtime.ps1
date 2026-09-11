[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$FbeExe,
    [ValidateRange(30, 300)][int]$TimeoutSeconds = 180,
    [ValidateRange(1, 5)][int]$Iterations = 2,
    [ValidateSet('all','open','new','reload-success','reload-failure')][string]$Scenario = 'all'
)

$ErrorActionPreference = 'Stop'
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe not found: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-document-lifecycle-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
try {
    $first = Join-Path $root 'first.fb2'; $second = Join-Path $root 'second.fb2'
    $book = { param($title) '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Runtime</first-name><last-name>Test</last-name></author><book-title>' + $title + '</book-title><lang>en</lang></title-info><document-info><author><nickname>FBE Next</nickname></author><program-used>FBE Next</program-used><date value="2026-09-11">11 September 2026</date><id>11111111-1111-1111-1111-111111111111</id><version>1.0</version></document-info></description><body><section><p>' + $title + '</p></section></body></FictionBook>' }
    [IO.File]::WriteAllText($first, (& $book 'First'), [Text.UTF8Encoding]::new($false)); [IO.File]::WriteAllText($second, (& $book 'Second'), [Text.UTF8Encoding]::new($false))
    function Invoke-LifecycleScenario([string]$scenario, [string[]]$required, [bool]$enableTrace = $false) {
        $report = Join-Path $root ($scenario + '-' + [guid]::NewGuid().ToString('N') + '.txt')
        $oldMode,$oldScenario,$oldOpen,$oldTrace=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_SUCCESSFUL_OPEN_PATH,$env:FBE_NEXT_TRACE
        try {
            $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO=$scenario; $env:FBE_NEXT_TEST_SUCCESSFUL_OPEN_PATH=$second
            if($enableTrace) { $env:FBE_NEXT_TRACE='1' }
            $process=Start-Process -FilePath $FbeExe -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$first) -WorkingDirectory (Split-Path $FbeExe) -PassThru
            if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "$scenario timed out" }
            $state=Get-Content -LiteralPath $report -Raw
            if($process.ExitCode -ne 0) { throw "$scenario exited $($process.ExitCode): $state" }
            foreach($line in $required) { if($state -notmatch [regex]::Escape($line)) { throw "$scenario regression: $line`n$state" } }
        } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_SUCCESSFUL_OPEN_PATH,$env:FBE_NEXT_TRACE=$oldMode,$oldScenario,$oldOpen,$oldTrace }
    }
    foreach($iteration in 1..$Iterations) {
        if($Scenario -in @('all','open')) { Invoke-LifecycleScenario 'successful-open-runtime' @('opened=1','identity_changed=1','active=1','session=1','filename=1','valid=1') }
        if($Scenario -in @('all','new')) { Invoke-LifecycleScenario 'new-document-runtime' @('created=1','identity_changed=1','active=1','session_new=1','valid=1') }
        if($Scenario -in @('all','reload-success')) { Invoke-LifecycleScenario 'reload-success-runtime' @('reloaded=1','identity=1','active=1','session=1','valid=1') }
        if($Scenario -in @('all','reload-failure')) { Invoke-LifecycleScenario 'reload-failure-runtime' @('reloaded=0','identity=1','active=1','session=1','valid=1') $true }
    }
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
Write-Host 'FBE document lifecycle runtime regressions passed.'
