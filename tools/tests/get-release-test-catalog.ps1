<# Emits a machine-readable catalog derived from verify-release.ps1. #>
[CmdletBinding()]
param(
    [switch]$AsJson,
    [switch]$Validate,
    [ValidateSet('FAST', 'FULL', 'TABLE')][string[]]$Contour = @(),
    [string[]]$Id = @()
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$verifyPath = Join-Path $repoRoot 'tools\build\verify-release.ps1'
$lines = Get-Content -LiteralPath $verifyPath
$sourceLocations = @(for ($sourceIndex=0; $sourceIndex -lt $lines.Count; ++$sourceIndex) { @{path='tools/build/verify-release.ps1'; line=$sourceIndex+1} })
# FULL also consumes the workflow's canonical lifecycle/API matrices. Include
# their real commands rather than claiming the local verifier covers them by name.
$lines += 'if ($FullValidation) {'
$sourceLocations += $null
foreach ($suite in (& (Join-Path $PSScriptRoot 'get-required-ci-runtime-suites.ps1'))) {
    $suiteLines = @($suite.Command.Replace('tools/tests/', 'tools\tests\') -split '\r?\n')
    $lines += $suiteLines
    $sourceLocations += @(for ($suiteLine=0; $suiteLine -lt $suiteLines.Count; ++$suiteLine) { @{path='.github/workflows/build.yml'; line=$suite.SourceLine+$suiteLine} })
}
$lines += '}'
$sourceLocations += $null
$entries = @{}
$tableDepth = 0
$fullDepth = 0

function Get-Component([string]$FileName) {
    if ($FileName -match '^test-fb2') { return 'fb2' }
    if ($FileName -match '^test-(plugin|export|import)') { return 'plugins' }
    if ($FileName -match '^test-(nsis|portable|runtime|language|localization)') { return 'packaging-localization' }
    if ($FileName -match '^test-(scintilla|pcre2)') { return 'native-dependencies' }
    return 'editor'
}

for ($index = 0; $index -lt $lines.Count; $index++) {
    $line = $lines[$index]
    $commandLocation = $sourceLocations[$index]
    # A release invocation may deliberately span physical lines for readability.
    # Catalog its complete PowerShell command, not merely its first line.
    while ($line.TrimEnd().EndsWith('`') -and $index + 1 -lt $lines.Count) {
        $line = $line.TrimEnd().Substring(0, $line.TrimEnd().Length - 1) + ' ' + $lines[++$index].Trim()
    }
    if ($line -match '^\s*if \(\$runTables\)') { $tableDepth = 1; continue }
    if ($line -match '^\s*if \(\$FullValidation\)') { $fullDepth = 1; continue }
    $currentContour = if ($tableDepth -gt 0) { 'TABLE' } elseif ($fullDepth -gt 0) { 'FULL' } else { 'FAST' }
    $match = [regex]::Match($line, 'tools\\tests\\(?<name>test-[A-Za-z0-9-]+\.ps1)')
    $isCommandRouteMatrix = $line -match '\$commandRouteOperation'
    if ($match.Success -and -not $isCommandRouteMatrix) {
        $name = $match.Groups['name'].Value
        $entryId = 'release.' + [IO.Path]::GetFileNameWithoutExtension($name).Substring(5)
        # IDs describe a runnable scenario, rather than the source line that
        # happens to contain it.  These variants are part of the public FULL
        # contour and must not disappear into one generic script entry.
        if ($line -match '\s-Huge(?:\s|$)') { $entryId += '.huge' }
        if ($line -match '-Fault\s+change-colspan-after-normalize') { $entryId += '.fault-change-colspan-after-normalize' }
        elseif ($line -match '-Fault\s+drop-row-after-normalize') { $entryId += '.fault-drop-row-after-normalize' }
        if ($name -eq 'test-fbe-table-structural-production.ps1' -and $line -match '-FixtureId\s+(?<fixture>[A-Za-z0-9-]+)') {
            $entryId += '.' + $Matches.fixture
            if ($line -match '-Target\s+"(?<target>[^"]+)"') {
                $targetId = $Matches.target -replace '[^A-Za-z0-9]+', '-'
                $entryId += '.target-' + $targetId.Trim('-')
            }
            if ($line -match '-Operation\s+(?<operation>[A-Za-z0-9-]+)') { $entryId += '.operation-' + $Matches.operation }
            if ($line -match '-SecondOperation\s+(?<second>[A-Za-z0-9-]+)') { $entryId += '.then-' + $Matches.second }
        }
        if (-not $entries.ContainsKey($entryId)) {
            # Most legacy scripts own fixtures, isolation and timeouts internally.
            # Keep that fact explicit: an empty array would falsely claim that the
            # catalog has inspected and found no such metadata.
            $entries[$entryId] = [pscustomobject][ordered]@{ id = $entryId; path = ('tools/tests/' + $name); component = Get-Component $name; contours = @(); invocations = @(); fixtures = $null; requirements = $null; required = $true; timeoutSeconds = 'declared-by-test'; isolation = 'declared-by-test' }
        }
        $entry = $entries[$entryId]
        if ($entry.contours -notcontains $currentContour) { $entry.contours += $currentContour }
        if ($line -match 'FbeExe|FbeExecutable') { $entry.requirements = @('gui') }
        if ($name -match 'pcre2|scintilla|image-import-native|plugin-v2') {
            $entry.requirements = @($entry.requirements + 'native-toolchain' | Where-Object { $_ } | Select-Object -Unique)
        }
        $entry.invocations += [pscustomobject][ordered]@{ source = $commandLocation.path; line = $commandLocation.line; command = $line.Trim() }
    }
    if ($line -match '^\s*foreach \(\$commandRouteOperation in @\((?<operations>[^)]*)\)\)') {
        $operations = [regex]::Matches($Matches.operations, "'(?<operation>[^']+)'") | ForEach-Object { $_.Groups['operation'].Value }
        foreach ($operation in $operations) {
            $entryId = "release.fbe-table-structural-production.command-route.$operation"
            $entries[$entryId] = [pscustomobject][ordered]@{
                id = $entryId; path = 'tools/tests/test-fbe-table-structural-production.ps1'; component = 'editor';
                contours = @('TABLE'); invocations = @([pscustomobject][ordered]@{ line = $index + 1; command = "-FixtureId plain -Operation $operation -RouteThroughFrame" });
                fixtures = @('plain'); requirements = @('gui'); required = $true; timeoutSeconds = $null; isolation = 'declared-by-test'
            }
        }
    }
    if ($tableDepth -gt 0) { $tableDepth += ([regex]::Matches($line, '\{').Count - [regex]::Matches($line, '\}').Count) }
    if ($fullDepth -gt 0) { $fullDepth += ([regex]::Matches($line, '\{').Count - [regex]::Matches($line, '\}').Count) }
}

# Materialised portable state is a required FULL package producer, not a test
# silently supplied by the core verifier. Keep that execution boundary explicit.
$packageLines=Get-Content (Join-Path $repoRoot 'tools/build/create-release.ps1')
$packageCalls=@(for($packageIndex=0;$packageIndex -lt $packageLines.Count;++$packageIndex){if($packageLines[$packageIndex] -match 'tools\\tests\\test-portable-gui-state\.ps1'){@{line=$packageIndex+1;command=$packageLines[$packageIndex].Trim()}}})
if($packageCalls.Count -ne 1){throw 'Expected one mandatory FULL portable package state producer.'}
$entries['release.portable-gui-state']=[pscustomobject][ordered]@{
    id='release.portable-gui-state';path='tools/tests/test-portable-gui-state.ps1';component='packaging-localization';
    contours=@('FULL');invocations=@([pscustomobject]@{source='tools/build/create-release.ps1';line=$packageCalls[0].line;command=$packageCalls[0].command});
    fixtures=$null;requirements=@('gui','materialized-portable-package','external-ci-package');required=$true;
    timeoutSeconds='declared-by-test';isolation='declared-by-test';execution='package-stage'
}
$catalog = [ordered]@{ schemaVersion = 2; generatedFrom = 'tools/build/verify-release.ps1'; tests = @($entries.Values | Sort-Object id) }
foreach ($entry in $catalog.tests) {
    if ($entry.id -eq 'release.fbe-scripting-clipboard-ole-ci') { $entry.requirements = @('github-hosted', 'disposable-user-logon', 'private-window-station') }
}
if ($Validate) {
    if ($catalog.tests.Count -eq 0) { throw 'Release test catalog is empty.' }
    $ids = @{}
    foreach ($entry in $catalog.tests) {
        if ($ids.ContainsKey($entry.id)) { throw "Duplicate release test ID: $($entry.id)" }
        $ids[$entry.id] = $true
        if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $entry.path) -PathType Leaf)) { throw "Catalog refers to a missing test script: $($entry.path)" }
        if ($entry.contours.Count -eq 0 -or $entry.invocations.Count -eq 0) { throw "Catalog entry is incomplete: $($entry.id)" }
    }
}
$selectedTests = @($catalog.tests)
if ($Contour.Count -gt 0) {
    # These are executable profiles, not merely labels on source blocks.
    # FULL runs the normal FAST suite plus tables and FULL-only cases;
    # RunTableTests adds the table contour to the normal suite.
    $effectiveContours = if ('FULL' -in $Contour) { @('FAST', 'TABLE', 'FULL') }
    elseif ('TABLE' -in $Contour) { @('FAST', 'TABLE') }
    else { @('FAST') }
    $selectedTests = @($selectedTests | Where-Object { @($_.contours | Where-Object { $_ -in $effectiveContours }).Count -gt 0 })
}
if ($Id.Count -gt 0) {
    $knownIds = @($catalog.tests | ForEach-Object id)
    foreach ($requestedId in $Id) { if ($requestedId -notin $knownIds) { throw "Unknown release test ID: $requestedId" } }
    $selectedTests = @($selectedTests | Where-Object { $_.id -in $Id })
}
$selectedCatalog = [ordered]@{ schemaVersion = $catalog.schemaVersion; generatedFrom = $catalog.generatedFrom; selection = [ordered]@{ contours = @($Contour); ids = @($Id) }; tests = $selectedTests }
$selectedCatalog.additionalSources = @('.github/workflows/build.yml: required lifecycle/API/OLE runtime matrices','tools/build/create-release.ps1: FULL package state producer')
if ($AsJson) { $selectedCatalog | ConvertTo-Json -Depth 7 }
else { $selectedTests | Select-Object id, path, component, contours, required, requirements }
