[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$names = @('FBE_NEXT_TEST_MODE', 'FBE_NEXT_TEST_SCENARIO', 'FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name) }
try {
    foreach ($mode in @('portable', 'installed')) {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name "history-$mode"
        $passed = $false
        try {
            $settings = Join-Path $iso.DataPath 'Settings'
            New-Item -ItemType Directory -Force -Path $settings | Out-Null
            $env:FBE_NEXT_TEST_MODE = '1'
            $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $settings
            foreach ($phase in @('write', 'read')) {
                $env:FBE_NEXT_TEST_SCENARIO = "search-history-$phase"
                $report = Join-Path $iso.Root "history-$phase.txt"
                $document = Join-Path $root 'runtime\blank.fb2'
                $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden `
                    -ArgumentList @("--$mode", '-b', ('"' + $report + '"'), ('"' + $document + '"')) -PassThru
                $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 120
                $text = if (Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing>' }
                if ($code -ne 0 -or $text -notmatch '(?m)^opened=1\r?$' -or $text -notmatch '(?m)^first=1\r?$' -or $text -notmatch '(?m)^ordered=1\r?$' -or $text -notmatch '(?m)^deduplicated=1\r?$') { throw "$mode/$phase failed: $text" }
            }
            $passed = $true
            Write-Host "Search/replace history: PASS ($mode; reopen and restart)."
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
    }
} finally { foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name]) } }
