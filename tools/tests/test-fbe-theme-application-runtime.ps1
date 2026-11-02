<# Checks repeated theme application, new controls, rebar visibility and GUI resource balance in an isolated FBE process. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 120)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'theme-application'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'theme.fb2'
    $report = Join-Path $isolation.Root 'theme.tsv'
    [IO.File]::WriteAllText($fixture, '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>theme</book-title><lang>en</lang></title-info><document-info><id>theme-application-test</id><version>1.0</version></document-info></description><body><section><p>theme</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'theme-application-runtime'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'theme-application-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0) { throw "FBE theme-application scenario exited with $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario
    }
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $report) {
        $parts = $line -split "`t"
        if ($parts.Count -eq 2) { $values[$parts[0]] = $parts[1] }
    }
    $palettes = @('latte', 'sepia', 'solarized', 'custom')
    if($values.dark_supported -eq '1') { $palettes += 'dark' }
    if($env:FBE_NEXT_TEST_APPEARANCE_ARTIFACT_DIR) {
        foreach($name in @('classic') + $palettes) {
            $path = Join-Path $env:FBE_NEXT_TEST_APPEARANCE_ARTIFACT_DIR "main-$name.bmp"
            if($values["screenshot_$name"] -ne '1' -or -not (Test-Path -LiteralPath $path -PathType Leaf)) {
                throw "Missing appearance capture for ${name}: $path"
            }
        }
    }
    foreach ($palette in $palettes) {
        $tableField = $values["table_combo_disabled_$palette"] -split '/'
        if($tableField.Count -ne 2 -or $tableField[0] -ne $tableField[1] -or $values["table_combo_disabled_text_$palette"] -ne '1') {
            throw "Disabled table ComboBox palette or text mismatch ($palette): $($values["table_combo_disabled_$palette"])"
        }
        Write-Host "Disabled table ComboBox ${palette}: color=$($tableField[0]), text visible"
        Write-Host "Status pixel ${palette}: actual=$($values["pixel_${palette}_actual"]) expected=$($values["pixel_${palette}_expected"])"
        Write-Host "Toolbar ${palette}: matching pixels=$($values["toolbar_${palette}_matches"])/$($values["toolbar_${palette}_observed"])"
        Write-Host "Rebar ${palette}: border pixels=$($values["rebar_${palette}_matches"])/$($values["rebar_${palette}_observed"])"
        $borderMatches = [int]$values["rebar_${palette}_matches"]
        $borderObserved = [int]$values["rebar_${palette}_observed"]
        if([int]$values["rebar_${palette}_matches"] -ne [int]$values["rebar_${palette}_observed"]) {
            foreach($key in @($values.Keys | Where-Object { $_ -like "rebar_${palette}_band*" } | Sort-Object)) { Write-Host "  ${key}: $($values[$key])" }
        }
        if($borderObserved -lt 3 -or $borderMatches -ne $borderObserved) { throw "Rebar border mismatch ($palette): $borderMatches/$borderObserved" }
        foreach($surface in @('scripts', 'context', 'context_table', 'context_table2')) {
            $surfaceMatches = [int]$values["${surface}_${palette}_matches"]
            $surfaceObserved = [int]$values["${surface}_${palette}_observed"]
            Write-Host "${surface} ${palette}: matching pixels=$surfaceMatches/$surfaceObserved"
            if($surfaceObserved -lt 10 -or $surfaceMatches -lt 0.95 * $surfaceObserved) { throw "Surface pixel mismatch ($surface/$palette): $surfaceMatches/$surfaceObserved" }
        }
        $matchesCount = [int]$values["toolbar_${palette}_matches"]
        $observedCount = [int]$values["toolbar_${palette}_observed"]
        if($observedCount -lt 10 -or $matchesCount -lt 0.95 * $observedCount) { throw "Toolbar surface pixel mismatch ($palette): $matchesCount/$observedCount" }
    }
    Write-Host "Toolbar enabled icons on Latte/Sepia: $($values.icon_buttons_passed)/$($values.icon_buttons_checked)"
    Write-Host "Toolbar disabled icons on Latte/Sepia: $($values.disabled_icons_passed)/$($values.disabled_icons_checked)"
    if ([long]$values.initial_applied -le 0 -or $values.repeated_delta -ne '0' -or
        $values.new_child_auto -ne '1' -or $values.hidden_preserved -ne '1' -or
        $values.order_preserved -ne '1' -or $values.deleted_band_cleared -ne '1' -or
        $values.reused_band_fresh -ne '1' -or
        $values.refresh_noop -ne '1' -or $values.surface_pixels -ne '1' -or $values.classic_restored -ne '1') {
        throw "Theme traversal/state failed: $($values | ConvertTo-Json -Compress)"
    }
    if ([math]::Abs([int]$values.gdi_delta) -gt 30 -or [int]$values.user_delta_second20 -gt 10) {
        throw "Repeated theme switches grew GUI resources without stabilizing: $($values | ConvertTo-Json -Compress)"
    }
    $passed = $true
    Write-Host "Theme traversal, new-control application, 40 switches and rebar visibility passed. GDI delta=$($values.gdi_delta), USER first20=$($values.user_delta_first20), second20=$($values.user_delta_second20)."
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
