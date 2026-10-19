<# Exercises bounded production spellcheck work after real local BODY mutations. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [ValidateSet('direct','formatting','script','large-paragraph','dense-misspellings')][string]$Mutation = 'direct',
    [ValidateRange(100, 10000)][int]$ParagraphCount = 2000,
    [switch]$VerifyCountIndependence,
    [int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
function Invoke-SpellcheckCase([int]$Count) {
    $isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'spell-local-edit'
    $directory = $isolation.Root
    $passed = $false
    try {
        $null = New-IsolatedFbeSettingsSeed -Isolation $isolation
        $fixture = Join-Path $directory 'long-section.fb2'; $report = Join-Path $directory 'spell.tsv'
        $paragraphs = 1..$Count | ForEach-Object {
            if ($Mutation -eq 'dense-misspellings' -and $_ -eq $Count) {
                '<p><em>largeword</em>, <strong>largeword</strong> ordinary <em><strong>largeword</strong></em>&#160;' + ('largeword ' * 19997) + '</p>'
            } elseif ($Mutation -eq 'large-paragraph' -and $_ -eq $Count) { '<p>' + ('ordinary ' * 20000) + '</p>' }
            else { "<p>ordinary paragraph $_</p>" }
        }
        @('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>spell</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>spell-local-edit</id><version>1.0</version></document-info></description><body><section>', ($paragraphs -join ''), '</section></body></FictionBook>') | Set-Content -LiteralPath $fixture -Encoding utf8
        $oldMode, $oldScenario, $oldMutation = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_SPELLCHECK_MUTATION
        try {
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'spellcheck-local-edit'; $env:FBE_NEXT_TEST_SPELLCHECK_MUTATION = $Mutation
            $process = Start-Process -FilePath $isolation.Exe -ArgumentList @('--portable', '-b', $report, $fixture) -WorkingDirectory $directory -WindowStyle Hidden -PassThru
            if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; $process.WaitForExit(); throw "FBE did not complete the $Mutation spellcheck scenario." }
            if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE $Mutation spellcheck scenario failed: exit $($process.ExitCode)." }
        } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario; $env:FBE_NEXT_TEST_SPELLCHECK_MUTATION=$oldMutation }
        $result = @{}; Get-Content -LiteralPath $report | ForEach-Object { $parts = $_ -split "`t", 2; if ($parts.Count -eq 2) { $result[$parts[0]] = $parts[1] } }
        foreach ($field in 'paragraph_count','check_element_calls','visited_paragraphs','dom_changed') { if (-not $result.ContainsKey($field)) { throw "Missing $field in $Mutation report." } }
        if ([int]$result.paragraph_count -lt $Count -or [int]$result.dom_changed -ne 1) { throw "$Mutation did not apply the expected local DOM mutation." }
        if ([int]$result.check_element_calls -lt 1 -or [int]$result.check_element_calls -gt 2) { throw "$Mutation invoked CheckElement $($result.check_element_calls) times; expected bounded local work." }
        if ([int]$result.visited_paragraphs -lt 1 -or [int]$result.visited_paragraphs -gt 2) { throw "$Mutation visited $($result.visited_paragraphs) paragraphs; expected no global document walk." }
        if ($Mutation -eq 'dense-misspellings') {
            foreach ($field in 'highlights','refreshed_highlights','cleared_highlights','spell_elapsed_ms','refresh_elapsed_ms','segments') {
                if (-not $result.ContainsKey($field)) { throw "Missing $field in dense highlight report." }
            }
            if ([int]$result.highlights -ne 20000 -or [int]$result.refreshed_highlights -ne 20000 -or [int]$result.cleared_highlights -ne 0 -or [int]$result.segments -ne 3) {
                throw "Dense highlighting lost, duplicated or retained stale segments: $($result | Out-String)"
            }
            if ([long]$result.spell_elapsed_ms -gt 2000 -or [long]$result.refresh_elapsed_ms -gt 2000) { throw "Dense highlight pass exceeded 2 seconds: $($result | Out-String)" }
            Write-Host "20,000 misspellings: edit=$($result.spell_elapsed_ms) ms, refresh=$($result.refresh_elapsed_ms) ms; all misspelled words covered by $($result.segments) segments, stale highlights cleared."
        }
        $passed = $true
        return [pscustomobject]@{ Count=$Count; Checks=[int]$result.check_element_calls; Visited=[int]$result.visited_paragraphs }
    } finally {
        $target = [IO.Path]::GetFullPath($directory)
        $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if ($target.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($target).StartsWith('fr-', [StringComparison]::Ordinal)) {
            Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
        }
    }
}
$first = Invoke-SpellcheckCase $ParagraphCount
if ($VerifyCountIndependence) {
    $second = Invoke-SpellcheckCase ($ParagraphCount * 2)
    if ($first.Checks -ne $second.Checks -or $first.Visited -ne $second.Visited) { throw "$Mutation spellcheck work changed with total paragraph count: $($first.Count)=$($first.Checks)/$($first.Visited), $($second.Count)=$($second.Checks)/$($second.Visited)." }
}
Write-Host "Production $Mutation spellcheck bounded-work regression passed: paragraphs=$($first.Count), CheckElement calls=$($first.Checks), visited paragraphs=$($first.Visited)."
