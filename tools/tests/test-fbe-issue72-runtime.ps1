<# Exercises the actual Source and Validate commands on invalid FB2 nesting.
   The supplied original is copied into a disposable portable runtime. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FbeExe,
    [string]$OriginalBook,
    [int]$TimeoutSeconds = 45,
    [switch]$KeepArtifacts
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$cases = [ordered]@{
    'subtitle-strong-p' = '<subtitle><strong><p>Nested</p></strong></subtitle>'
    'subtitle-emphasis-p' = '<subtitle><emphasis><p>Nested</p></emphasis></subtitle>'
    'paragraph-strong-p' = '<p><strong><p>Nested</p></strong></p>'
}
if ($OriginalBook) { $cases['user-book'] = $null }
foreach ($case in $cases.GetEnumerator()) {
    foreach ($operation in 'source', 'validate') {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'issue72'
        $passed = $false
        try {
            $null = New-IsolatedFbeSettingsSeed -Isolation $iso
            $fixture = Join-Path $iso.Root 'input.fb2'
            $report = Join-Path $iso.Root 'report.txt'
            if ($case.Key -eq 'user-book') {
                Copy-Item -LiteralPath $OriginalBook -Destination $fixture
            } else {
                $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Issue72</book-title><lang>en</lang></title-info><document-info><id>issue72-test</id><version>1.0</version></document-info></description><body><section>' + $case.Value + '<p>Neighbour</p></section></body></FictionBook>'
                [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
            }
            $before = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
            $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
            try {
                $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = "issue72-$operation"
                $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList @('--portable', '-b', $report, $fixture) -PassThru -WindowStyle Hidden
                $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario "issue72-$operation" -Report $report -TimeoutSeconds $TimeoutSeconds
            } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
            $text = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing>' }
            if ($exitCode -ne 0 -or $text -notmatch '(?m)^stage=command-returned\r?$' -or
                $text -notmatch '(?m)^same-document=1\r?$' -or $text -notmatch '(?m)^dirty-before=1\r?$' -or
                $text -notmatch '(?m)^dirty-after=1\r?$' -or $text -notmatch '(?m)^marker=1\r?$' -or
                $text -notmatch '(?m)^window-enabled=1\r?$' -or $text -notmatch '(?m)^subsequent-edit=1\r?$' -or
                $text -notmatch '(?m)^memory-available=1\r?$') {
                throw "Issue72 $($case.Key)/$operation failed: exit=$exitCode; report=$text"
            }
            if ($text -match '(?m)^source-active=0\r?$' -and $text -notmatch '(?m)^body-unchanged=1\r?$') {
                throw "Issue72 $($case.Key)/$operation changed the visual document during refusal: $text"
            }
            if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $before) { throw 'Original fixture changed on disk.' }
            $passed = $true
            Write-Host "Issue72 $($case.Key)/$operation returned and preserved the unsaved edit."
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed -KeepArtifacts:$KeepArtifacts }
    }
}
