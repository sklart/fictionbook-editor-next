<# Exercises bounded production spellcheck work after real local BODY mutations. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [ValidateSet('direct','formatting','script','large-paragraph')][string]$Mutation = 'direct',
    [ValidateRange(100, 10000)][int]$ParagraphCount = 2000,
    [switch]$VerifyCountIndependence,
    [int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }
function Invoke-SpellcheckCase([int]$Count) {
    $directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-spell-local-edit-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $directory | Out-Null
    try {
        $fixture = Join-Path $directory 'long-section.fb2'; $report = Join-Path $directory 'spell.tsv'
        $paragraphs = 1..$Count | ForEach-Object { if ($Mutation -eq 'large-paragraph' -and $_ -eq $Count) { '<p>' + ('largeword ' * 20000) + '</p>' } else { "<p>ordinary paragraph $_</p>" } }
        @('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>spell</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>spell-local-edit</id><version>1.0</version></document-info></description><body><section>', ($paragraphs -join ''), '</section></body></FictionBook>') | Set-Content -LiteralPath $fixture -Encoding utf8
        $oldMode, $oldScenario, $oldMutation = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_SPELLCHECK_MUTATION
        try {
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'spellcheck-local-edit'; $env:FBE_NEXT_TEST_SPELLCHECK_MUTATION = $Mutation
            $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
            if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE did not complete the $Mutation spellcheck scenario." }
            if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE $Mutation spellcheck scenario failed: exit $($process.ExitCode)." }
        } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario; $env:FBE_NEXT_TEST_SPELLCHECK_MUTATION=$oldMutation }
        $result = @{}; Get-Content -LiteralPath $report | ForEach-Object { $parts = $_ -split "`t", 2; if ($parts.Count -eq 2) { $result[$parts[0]] = $parts[1] } }
        foreach ($field in 'paragraph_count','check_element_calls','visited_paragraphs','dom_changed') { if (-not $result.ContainsKey($field)) { throw "Missing $field in $Mutation report." } }
        if ([int]$result.paragraph_count -lt $Count -or [int]$result.dom_changed -ne 1) { throw "$Mutation did not apply the expected local DOM mutation." }
        if ([int]$result.check_element_calls -lt 1 -or [int]$result.check_element_calls -gt 2) { throw "$Mutation invoked CheckElement $($result.check_element_calls) times; expected bounded local work." }
        if ([int]$result.visited_paragraphs -lt 1 -or [int]$result.visited_paragraphs -gt 2) { throw "$Mutation visited $($result.visited_paragraphs) paragraphs; expected no global document walk." }
        return [pscustomobject]@{ Count=$Count; Checks=[int]$result.check_element_calls; Visited=[int]$result.visited_paragraphs }
    } finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
}
$first = Invoke-SpellcheckCase $ParagraphCount
if ($VerifyCountIndependence) {
    $second = Invoke-SpellcheckCase ($ParagraphCount * 2)
    if ($first.Checks -ne $second.Checks -or $first.Visited -ne $second.Visited) { throw "$Mutation spellcheck work changed with total paragraph count: $($first.Count)=$($first.Checks)/$($first.Visited), $($second.Count)=$($second.Checks)/$($second.Visited)." }
}
Write-Host "Production $Mutation spellcheck bounded-work regression passed: paragraphs=$($first.Count), CheckElement calls=$($first.Checks), visited paragraphs=$($first.Visited)."