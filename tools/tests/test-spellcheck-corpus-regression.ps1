<# Fixed quality and relative-performance guard for shipped Hunspell dictionaries. #>
[CmdletBinding()]
param([string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
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
foreach($property in $baseline.languages.psobject.Properties) {
    $case = $property.Value; [xml]$fb2 = Get-Content -Raw -LiteralPath (Join-Path $corpus "fixtures\fb2\$($case.file)")
    $words = @([regex]::Matches($fb2.FictionBook.body.InnerText, "[\p{L}\p{N}]+(?:['’ʼ-][\p{L}\p{N}]+)*") | ForEach-Object Value)
    $result = Get-Result (Join-Path $root "out\$Configuration\dict\$($case.dictionary)") $words
    if([int]$result.rejected -ne [int]$case.rejected) { throw "$($property.Name): rejected baseline changed: $($result.rejected)" }
    foreach($typo in $case.knownTypos.psobject.Properties) {
        $line = @($typo.Name | & $probe -d (Join-Path $root "out\$Configuration\dict\$($case.dictionary)") -a | Select-Object -Last 1)[0]
        if($line -notlike '& *') { throw "$($property.Name): typo was silently accepted: $($typo.Name)" }
        foreach($expected in @($typo.Value)) { if($line -notmatch [regex]::Escape($expected)) { throw "$($property.Name): missing suggestion $expected for $($typo.Name)" } }
    }
    foreach($ordered in $case.firstSuggestion.psobject.Properties) {
        $line = @($ordered.Name | & $probe -d (Join-Path $root "out\$Configuration\dict\$($case.dictionary)") -a | Select-Object -Last 1)[0]
        if($line -notmatch (':\s*' + [regex]::Escape([string]$ordered.Value) + '(?:,|$)')) { throw "$($property.Name): first suggestion order changed for $($ordered.Name)" }
    }
    foreach($metric in 'load','suggest') { if([int]$result."${metric}_ms" -gt [int]([double]$case.referenceMs.$metric * [double]$baseline.allowedRelativeSlowdown + 50)) { throw "$($property.Name): $metric performance regression" } }
}
Write-Host 'Fixed Hunspell corpus quality and relative performance baseline passed.'
