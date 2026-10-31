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
            if ($mode -eq 'portable') {
                $moved = Join-Path $iso.Root 'moved-profile'
                Move-Item -LiteralPath $iso.DataPath -Destination $moved
                $settings = Join-Path $moved 'Settings'
                $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $settings
                [IO.File]::WriteAllText((Join-Path $iso.Root 'portable.ini'), "[Portable]`r`nDataPath=moved-profile`r`n", [Text.UTF8Encoding]::new($false))
                $env:FBE_NEXT_TEST_SCENARIO = 'search-history-read'
                $report = Join-Path $iso.Root 'history-moved.txt'
                $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden `
                    -ArgumentList @('--portable', '-b', ('"' + $report + '"'), ('"' + $document + '"')) -PassThru
                $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 120
                $text = if (Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing>' }
                if ($code -ne 0 -or $text -notmatch '(?m)^first=1\r?$' -or $text -notmatch '(?m)^ordered=1\r?$' -or $text -notmatch '(?m)^deduplicated=1\r?$') { throw "portable/moved failed: $text" }
            }
            [IO.File]::WriteAllBytes((Join-Path $settings 'SearchHistory.dat'), [byte[]](1,2,3,4,5,6,7,8))
            $env:FBE_NEXT_TEST_SCENARIO = 'search-history-corrupt'
            $report = Join-Path $iso.Root 'history-corrupt.txt'
            $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden `
                -ArgumentList @("--$mode", '-b', ('"' + $report + '"'), ('"' + $document + '"')) -PassThru
            $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 120
            $text = if (Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing>' }
            if ($code -ne 0 -or $text -notmatch '(?m)^opened=1\r?$' -or $text -notmatch '(?m)^empty=1\r?$' -or $text -notmatch '(?m)^preserved=1\r?$') { throw "$mode/corrupt failed: $text" }
            $passed = $true
            Write-Host "Search/replace history: PASS ($mode; reopen, restart, corrupt file preserved$(if ($mode -eq 'portable') { ', moved profile' }))."
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
    }
} finally { foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name]) } }
