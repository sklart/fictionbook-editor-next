<# Real New/Open/Close operations must retain a dirty document and recovery when the save prompt receives WM_QUIT. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$template = '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Decision</book-title><lang>en</lang></title-info><document-info><id>save-decision-test</id><version>1.0</version></document-info></description><body><section><p>SAVE_DECISION_ORIGINAL</p></section></body></FictionBook>'
$savedMode, $savedScenario, $savedTarget = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_DECISION_OPEN_PATH
try {
    foreach($operation in @('new', 'open', 'close')) {
        # A preserved recovery snapshot must not become the next case's
        # startup-recovery prompt. Give each operation its own portable profile.
        $isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name "save-decision-$operation"
        $passed = $false
        try {
            $fixture = Join-Path $isolation.Root 'current.fb2'
            $target = Join-Path $isolation.Root 'other.fb2'
            [IO.File]::WriteAllText($fixture, $template, [Text.UTF8Encoding]::new($false))
            [IO.File]::WriteAllText($target, $template.Replace('SAVE_DECISION_ORIGINAL', 'OTHER_DOCUMENT'), [Text.UTF8Encoding]::new($false))
            $env:FBE_NEXT_TEST_MODE = '1'
            $env:FBE_NEXT_TEST_DECISION_OPEN_PATH = $target
            $scenario = "save-decision-quit-$operation"
            $report = Join-Path $isolation.Root "$scenario.tsv"
            $env:FBE_NEXT_TEST_SCENARIO = $scenario
            $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
            $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario $scenario -Report $report -TimeoutSeconds $TimeoutSeconds
            if($exitCode -ne 73) { throw "$scenario did not preserve WM_QUIT code 73: exit $exitCode." }
            $results = @{}
            foreach($line in Get-Content -LiteralPath $report) {
                $parts = $line -split "`t"
                if($parts.Count -eq 2) { $results[$parts[0]] = $parts[1] }
            }
            foreach($field in @('dirty_before', 'recovery_before', 'dialog_found', 'quit_posted', 'operation_cancelled', 'document_retained', 'recovery_retained')) {
                if($results[$field] -ne '1') { throw "$scenario failed $field`: $($results | ConvertTo-Json -Compress)" }
            }
            $passed = $true
        } finally {
            Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
        }
    }
    Write-Host 'New/Open/Close retained the dirty document and recovery after an unanswered save prompt; WM_QUIT code was preserved.'
} finally {
    $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_DECISION_OPEN_PATH = $savedMode, $savedScenario, $savedTarget
}
