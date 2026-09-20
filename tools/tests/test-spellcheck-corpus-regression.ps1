<# Fixed quality and relative-performance guard for shipped Hunspell dictionaries. #>
[CmdletBinding()]
param([string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = $OutputEncoding
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$corpus = Join-Path $PSScriptRoot 'spellcheck-corpus'
$baseline = Get-Content -Raw -LiteralPath (Join-Path $corpus 'fixtures\fb2\baseline.json') | ConvertFrom-Json
& (Join-Path $corpus 'build-hunspell-probe.ps1') -Configuration $Configuration
if ($LASTEXITCODE) { throw 'Cannot build native Hunspell regression probe.' }
$probe = Join-Path $root 'out\tools\hunspell-probe.exe'
function Get-Result([string]$Dictionary, [string[]]$Words) {
    $input = [IO.Path]::GetTempFileName()
    try { [IO.File]::WriteAllLines($input, $Words, [Text.UTF8Encoding]::new($false)); return (& $probe -d $Dictionary --benchmark $input | ConvertFrom-StringData) }
    finally { Remove-Item -LiteralPath $input -Force -ErrorAction SilentlyContinue }
}
function Get-Suggestions([string]$Dictionary, [string[]]$Words) {
    $input = [IO.Path]::GetTempFileName()
    try {
        [IO.File]::WriteAllLines($input, $Words, [Text.UTF8Encoding]::new($false))
        $result = @{}
        foreach($line in @(& $probe -d $Dictionary --suggest-file $input)) {
            $parts = $line -split "`t", 2
            $result[$parts[0]] = if($parts.Count -gt 1 -and $parts[1]) { @($parts[1] -split '\|') } else { @() }
        }
        return $result
    } finally { Remove-Item -LiteralPath $input -Force -ErrorAction SilentlyContinue }
}
foreach($property in $baseline.languages.psobject.Properties) {
    $case = $property.Value; [xml]$fb2 = Get-Content -Raw -LiteralPath (Join-Path $corpus "fixtures\fb2\$($case.file)")
    $words = @([regex]::Matches($fb2.FictionBook.body.InnerText, "[\p{L}\p{N}]+(?:['’ʼ-][\p{L}\p{N}]+)*") | ForEach-Object Value)
    $result = Get-Result (Join-Path $root "out\$Configuration\dict\$($case.dictionary)") $words
    $suggestions = Get-Suggestions (Join-Path $root "out\$Configuration\dict\$($case.dictionary)") @($case.knownTypos.psobject.Properties.Name)
    if([int]$result.rejected -ne [int]$case.rejected) { throw "$($property.Name): rejected baseline changed: $($result.rejected)" }
    foreach($typo in $case.knownTypos.psobject.Properties) {
        if(-not $suggestions.ContainsKey($typo.Name)) { throw "$($property.Name): typo was silently accepted: $($typo.Name)" }
        foreach($expected in @($typo.Value)) { if($suggestions[$typo.Name] -notcontains $expected) { throw "$($property.Name): missing suggestion $expected for $($typo.Name)" } }
    }
    foreach($ordered in $case.firstSuggestion.psobject.Properties) {
        $orderedSuggestions = @($suggestions[$ordered.Name])
        if($orderedSuggestions.Count -eq 0 -or $orderedSuggestions[0] -ne [string]$ordered.Value) { throw "$($property.Name): first suggestion order changed for $($ordered.Name)" }
    }
    $benchmarkWords = @(); foreach($repeat in 1..[int]$baseline.benchmarkRepeats) { $benchmarkWords += $words }
    $benchmark = Get-Result (Join-Path $root "out\$Configuration\dict\$($case.dictionary)") $benchmarkWords
    foreach($metric in 'load','check','suggest') { if([int]$benchmark."${metric}_ms" -gt [int]([double]$case.referenceMs.$metric * [double]$baseline.allowedRelativeSlowdown + 50)) { throw "$($property.Name): $metric performance regression" } }
}
Write-Host 'Fixed Hunspell corpus quality and relative performance baseline passed.'
