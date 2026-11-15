[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$saved = @{}
foreach($name in @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
try {
    foreach($mode in @('portable','installed')) {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name "delete-table-$mode"
        $passed = $false
        try {
            $empty = Join-Path $iso.Root 'empty-scripts'; New-Item -ItemType Directory -Force $empty | Out-Null
            $seed = New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory $empty
            $fixture = Join-Path $iso.Root 'fixture.fb2'
            [IO.File]::WriteAllText($fixture, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>T</nickname></author><book-title>Test</book-title><lang>en</lang></title-info><document-info><author><nickname>T</nickname></author><id>test</id><version>1</version></document-info></description><body><section><p>test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
            $env:FBE_NEXT_TEST_MODE = '1'
            if($mode -eq 'installed') {
                $settings = Join-Path $iso.Root 'installed-settings'; New-Item -ItemType Directory -Force $settings | Out-Null
                Copy-Item -LiteralPath $seed -Destination (Join-Path $settings 'Settings.xml')
                $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $settings
            } else { Remove-Item Env:FBE_NEXT_TEST_SETTINGS_DIRECTORY -ErrorAction SilentlyContinue }
            foreach($step in @('write','read','remove','removed')) {
                $scenario = "delete-table-toolbar-$step"; $env:FBE_NEXT_TEST_SCENARIO = $scenario
                $report = Join-Path $iso.Root "$scenario.txt"
                $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @("--$mode", '-b', $report, $fixture) -PassThru
                $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $scenario -Report $report -TimeoutSeconds $TimeoutSeconds
                if($exitCode -ne 0 -or -not (Test-Path -LiteralPath $report) -or (Get-Content -LiteralPath $report -Raw) -notmatch '(?m)^result=pass\r?$') { throw "$mode $step failed: exit $exitCode" }
            }
            $passed = $true; Write-Host "${mode}: table toolbar add, restart, remove, restart passed."
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
    }
} finally { foreach($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) } }
