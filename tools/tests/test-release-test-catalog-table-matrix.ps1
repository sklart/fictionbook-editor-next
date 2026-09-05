<# Compares the executable table matrix in verify-release.ps1 with catalog
   entries.  It deliberately derives both sides from the release plan. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$planPath = Join-Path $repoRoot 'tools\build\verify-release.ps1'
$catalogTool = Join-Path $PSScriptRoot 'get-release-test-catalog.ps1'
$tableScripts = @('test-fbe-table-structural-production.ps1', 'test-fbe-table-production-roundtrip.ps1', 'test-fbe-table-failure-safety.ps1')

function Get-JoinedPlanLines {
    $lines = Get-Content -LiteralPath $planPath
    $result = @()
    for ($index = 0; $index -lt $lines.Count; ++$index) {
        $line = $lines[$index]
        while ($line.TrimEnd().EndsWith('`') -and $index + 1 -lt $lines.Count) {
            $trimmed = $line.TrimEnd(); $line = $trimmed.Substring(0, $trimmed.Length - 1) + ' ' + $lines[++$index].Trim()
        }
        $result += $line
    }
    return $result
}

function Get-Value([string]$Command, [string]$Name) {
    $pattern = '-' + [regex]::Escape($Name) + '\s+(?:"(?<quoted>[^"]*)"|(?<plain>\S+))'
    $match = [regex]::Match($Command, $pattern)
    if (-not $match.Success) { return '' }
    if ($match.Groups['quoted'].Success) { return $match.Groups['quoted'].Value }
    return $match.Groups['plain'].Value
}

function Get-Signature([string]$Script, [string]$Command) {
    $route = if ($Command -match '-RouteThroughFrame(?:\\s|$)') { '1' } else { '0' }
    $huge = if ($Command -match '-Huge(?:\\s|$)') { '1' } else { '0' }
    return @(
        $Script,
        (Get-Value $Command 'FixtureId'),
        (Get-Value $Command 'Target'),
        (Get-Value $Command 'Operation'),
        (Get-Value $Command 'SecondOperation'),
        $route,
        (Get-Value $Command 'RuntimeStyle'),
        $huge,
        (Get-Value $Command 'Fault')
    ) -join '|'
}

$actual = @()
foreach ($line in (Get-JoinedPlanLines)) {
    if ($line -match '^\s*foreach \(\$commandRouteOperation in @\((?<operations>[^)]*)\)\)') {
        foreach ($operation in [regex]::Matches($Matches.operations, "'(?<operation>[^']+)'") | ForEach-Object { $_.Groups['operation'].Value }) {
            $actual += Get-Signature 'test-fbe-table-structural-production.ps1' "-FixtureId plain -Operation $operation -RouteThroughFrame"
        }
        continue
    }
    $match = [regex]::Match($line, 'tools\\tests\\(?<script>test-fbe-table-(?:structural-production|production-roundtrip|failure-safety)\.ps1)')
    if ($match.Success -and $line -notmatch '\$commandRouteOperation') {
        $actual += Get-Signature $match.Groups['script'].Value $line
    }
}

$catalog = & $catalogTool -AsJson | ConvertFrom-Json
$catalogSignatures = @()
foreach ($entry in $catalog.tests | Where-Object { [IO.Path]::GetFileName($_.path) -in $tableScripts }) {
    $script = [IO.Path]::GetFileName($entry.path)
    foreach ($invocation in $entry.invocations) { $catalogSignatures += Get-Signature $script $invocation.command }
}

function Get-Counts([string[]]$Signatures) {
    $counts = @{}
    foreach ($signature in $Signatures) { if ($counts.ContainsKey($signature)) { ++$counts[$signature] } else { $counts[$signature] = 1 } }
    return $counts
}

$actualCounts = Get-Counts $actual; $catalogCounts = Get-Counts $catalogSignatures
if ($actual.Count -ne $catalogSignatures.Count) { throw "Table matrix cardinality mismatch: release=$($actual.Count), catalog=$($catalogSignatures.Count)." }
foreach ($signature in @($actualCounts.Keys + $catalogCounts.Keys | Select-Object -Unique)) {
    if ($actualCounts[$signature] -ne $catalogCounts[$signature]) { throw "Table matrix mismatch for '$signature': release=$($actualCounts[$signature]), catalog=$($catalogCounts[$signature])." }
}
Write-Host "Release table matrix catalog one-to-one check passed: $($actual.Count) invocations."
