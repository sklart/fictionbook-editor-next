<# Exercises real .fbetheme v1 import/export parsing in an isolated FBE process. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'xml-source-theme-v1'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'theme-runtime.fb2'
    $report = Join-Path $isolation.Root 'xml-source-theme-v1.tsv'
    [IO.File]::WriteAllText($fixture, '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>theme</book-title><lang>en</lang></title-info><document-info><id>xml-source-theme-v1-test</id><version>1.0</version></document-info></description><body><section><p>theme</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'xml-source-theme-v1-runtime'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'xml-source-theme-v1-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0) { throw "FBE XML source theme v1 scenario exited with $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario
    }
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $report) {
        $parts = $line -split '=', 2
        if ($parts.Count -eq 2) { $values[$parts[0]] = $parts[1] }
    }
    foreach ($key in @('import', 'fallbacks', 'optional_roles', 'export', 'utf8_export', 'reimport', 'palette', 'metadata', 'missing_required', 'invalid_values', 'no_self_reference', 'theme_switch', 'high_contrast_system_colors')) {
        if ($values[$key] -ne '1') { throw "XML source theme v1 runtime check failed: $key (value '$($values[$key])')." }
    }
    if ($values['result'] -ne 'pass') { throw "XML source theme v1 report did not pass: $($values | ConvertTo-Json -Compress)" }
    $passed = $true
    Write-Host 'FBE XML source theme v1 import/export runtime passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
