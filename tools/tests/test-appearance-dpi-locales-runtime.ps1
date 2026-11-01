<# Exercises Settings DPI geometry under explicitly selected UI languages. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'appearance-dpi-locales'
$passed = $false
$oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
try {
    $seed = New-IsolatedFbeSettingsSeed -Isolation $isolation
    $template = [IO.File]::ReadAllText($seed)
    $fixture = Join-Path $isolation.Root 'appearance-dpi.fb2'
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fb2-validate-valid-smoke.fb2') -Destination $fixture
    $report = Join-Path $isolation.Root 'appearance-dpi.tsv'
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = 'settings-dialog-runtime'
    foreach($entry in @(@{Locale='ru-RU'; Id=0xFBE002}, @{Locale='en-US'; Id=0xFBE001}, @{Locale='de-DE'; Id=0xFBE004})) {
        $xml = $template -replace '<IntefaceLangID>[^<]*</IntefaceLangID>', ''
        $xml = $xml.Replace('</Settings>', "<IntefaceLangID>$($entry.Id)</IntefaceLangID></Settings>")
        [IO.File]::WriteAllText($seed, $xml, [Text.UTF8Encoding]::new($false))
        Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', '-b', $report, $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario "appearance-dpi-$($entry.Locale)" -Report $report -TimeoutSeconds $TimeoutSeconds
        if($exitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "Settings DPI run failed for $($entry.Locale), exit $exitCode." }
        $result = Get-Content -LiteralPath $report -Raw
        foreach($line in @('layout=1', 'followup=1', "interface-language=$($entry.Id)")) {
            if($result -notmatch ('(?m)^' + [regex]::Escape($line) + '\r?$')) { throw "Missing $line for $($entry.Locale): $result" }
        }
        if($result -notmatch '(?m)^appearance_screen_width=(\d+)\r?$') { throw 'Missing 1366px screen probe width.' }
        $screenWidth = [int]$Matches[1]
        if($result -notmatch '(?m)^appearance_screen_height=(\d+)\r?$') { throw 'Missing 1366px screen probe height.' }
        $screenHeight = [int]$Matches[1]
        if($screenWidth -le 0 -or $screenWidth -gt 1366 -or $screenHeight -le 0 -or $screenHeight -gt 728) {
            throw "Settings preferred size does not fit the test work area: ${screenWidth}x${screenHeight}."
        }
        Write-Host "Preferred window at 96 DPI: ${screenWidth}x${screenHeight} ($($entry.Locale))."
        Write-Host "Settings geometry at 96/120/144/192 DPI: PASS ($($entry.Locale))."
    }
    $passed = $true
} finally {
    $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
