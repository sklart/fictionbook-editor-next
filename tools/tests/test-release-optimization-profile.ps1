[CmdletBinding()]
param([switch]$RequireEffectiveFlags)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sharedProps = Join-Path $repoRoot 'tools\msbuild\FBE.ReleaseOptimization.props'
$commonPropsPath = Join-Path $repoRoot 'tools\msbuild\FBE.Common.props'
if (-not (Test-Path -LiteralPath $sharedProps -PathType Leaf)) { throw "Shared release optimization policy is missing: $sharedProps" }
$commonProps = Get-Content -Raw -LiteralPath $commonPropsPath
if ($commonProps -notmatch 'ForceImportAfterCppProps' -or $commonProps -notmatch [regex]::Escape('FBE.ReleaseOptimization.props')) { throw 'Shared release optimization policy is not imported after Microsoft.Cpp.props.' }

$speedProjects = @(
    @{ Path = 'src\fbe\FBE.vcxproj'; TlogRoot = 'build\obj\fbe\Release'; Target = 'FBE.exe' },
    @{ Path = 'src\fbv\FBV.vcxproj'; TlogRoot = 'build\obj\fbv\Win32\Release'; Target = 'FBV.exe' },
    @{ Path = 'src\export-html\ExportHTML.vcxproj'; TlogRoot = 'build\obj\export-html\Win32\Release'; Target = 'ExportHTML.dll' },
    @{ Path = 'src\export-docx\ExportDOCX.vcxproj'; TlogRoot = 'build\obj\export-docx\Win32\Release'; Target = 'ExportDOCX.dll' },
    @{ Path = 'src\export-docx\ExportDOCXBatch.vcxproj'; TlogRoot = 'build\obj\ExportDOCXBatch\Win32\Release'; Target = 'ExportDOCXBatch.exe' },
    @{ Path = 'src\export-epub\ExportEPUB.vcxproj'; TlogRoot = 'build\obj\export-epub\Win32\Release'; Target = 'ExportEPUB.dll' },
    @{ Path = 'src\export-epub\ExportEPUBBatch.vcxproj'; TlogRoot = 'build\obj\export-epub-batch\Win32\Release'; Target = 'ExportEPUBBatch.exe' },
    @{ Path = 'src\import-epub\ImportEPUB.vcxproj'; TlogRoot = 'build\obj\import-epub\Win32\Release'; Target = 'ImportEPUB.dll' },
    @{ Path = 'src\import-epub\ImportEPUBBatch.vcxproj'; TlogRoot = 'build\obj\ImportEPUBBatch\Win32\Release'; Target = 'ImportEPUBBatch.exe' },
    @{ Path = 'src\import-epub\ImportEPUBLunaSVG.vcxproj'; TlogRoot = 'build\obj\ImportEPUBLunaSVG\Win32\Release'; Target = 'ImportEPUBLunaSVG.dll' }
)
$shellProject = @{ Path = 'src\fbshell\FBShell.vcxproj'; TlogRoot = 'build\obj\fbshell\Release'; Target = 'FBShell.dll' }
$projectPaths = @($speedProjects.Path) + $shellProject.Path + 'src\contracts\FBEContracts.vcxproj'
foreach ($relativePath in $projectPaths) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativePath)
    if ($text -notmatch [regex]::Escape('tools\msbuild\FBE.Common.props')) { throw "Project does not inherit the shared optimization policy: $relativePath" }
}
$policy = Get-Content -Raw -LiteralPath $sharedProps
foreach ($value in @('MaxSpeed', 'AnySuitable', 'Speed', 'MinSpace', 'Size', 'UseLinkTimeCodeGeneration')) { if ($policy -notmatch [regex]::Escape(">$value<")) { throw "Shared optimization policy is missing: $value" } }
if ($policy -notmatch [regex]::Escape('/Gw')) { throw 'Shared release optimization policy is missing: /Gw' }
if ($policy -notmatch 'MSBuildProjectName.*FBShell') { throw 'FBShell size-profile exception is not explicit.' }
$fbeText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj')
if ($fbeText -match 'OnlyExplicitInline|<WholeProgramOptimization>false</WholeProgramOptimization>|AdditionalOptions>[^<]*\/LTCG|Release\|Win32.*>Full<') { throw 'FBE Release profile contains a legacy inline/WPO/LTCG override.' }
$shellText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $shellProject.Path)
if ($shellText -match "Condition=.*Release\|Win32[\s\S]*<Optimization>MaxSpeed</Optimization>") { throw 'FBShell must retain its size-oriented Release profile.' }
foreach ($relativePath in $projectPaths) {
    [xml]$projectXml = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativePath)
    $namespace = New-Object System.Xml.XmlNamespaceManager($projectXml.NameTable); $namespace.AddNamespace('msb', $projectXml.DocumentElement.NamespaceURI)
    $projectXml.SelectNodes('//msb:ItemDefinitionGroup[@Condition]', $namespace) | Where-Object { $_.GetAttribute('Condition') -match 'Release\|Win32' } | ForEach-Object {
        $options = $_.SelectSingleNode('msb:ClCompile/msb:AdditionalOptions', $namespace)
        if ($null -ne $options -and $options.InnerText -notmatch [regex]::Escape('%(AdditionalOptions)')) { throw "Release|Win32 AdditionalOptions must retain inherited options: $relativePath" }
    }
}
Write-Host 'Release optimization profile structural policy passed.'

function Get-TlogCommandEntries {
    param([string[]]$Lines, [ValidateSet('Compiler', 'Linker')][string]$Kind)
    $entries = @(); $marker = $null
    foreach ($line in $Lines) {
        if ($line.StartsWith('^')) { $marker = $line.Substring(1); continue }
        if ($null -ne $marker -and $line -match '^\s*/') {
            $entries += [pscustomobject]@{ Marker = $marker; Command = $line.Trim() }
            $marker = $null
        }
    }
    if ($entries.Count -eq 0) { throw "No $Kind command entries found in tlog." }
    return $entries
}

function Get-CommandSwitches {
    param([string]$Command)
    # A switch is an argument beginning with '/', bounded by whitespace. This avoids /O1 matching /Ob1 or /O2.
    return @([regex]::Matches($Command, '(?<!\S)/[^\s"]+') | ForEach-Object { $_.Value.ToUpperInvariant() })
}

function Get-EffectiveState {
    param([string[]]$Switches)
    $state = @{ Optimization = $null; Inline = $null; Favor = $null; WholeProgram = $null }
    foreach ($option in $Switches) {
        switch ($option) {
            '/OD' { $state.Optimization = $option; continue }
            '/O1' { $state.Optimization = $option; continue }
            '/O2' { $state.Optimization = $option; continue }
            '/OB0' { $state.Inline = $option; continue }
            '/OB1' { $state.Inline = $option; continue }
            '/OB2' { $state.Inline = $option; continue }
            '/OB3' { $state.Inline = $option; continue }
            '/OX' { $state.Optimization = $option; continue }
            '/OS' { $state.Favor = $option; continue }
            '/OT' { $state.Favor = $option; continue }
            '/GL' { $state.WholeProgram = $option; continue }
            '/GL-' { $state.WholeProgram = $option; continue }
        }
    }
    return $state
}

function Format-State { param([hashtable]$State) "{0} {1} {2} {3}" -f $State.Optimization, $State.Inline, $State.Favor, $State.WholeProgram }
function Throw-Mismatch {
    param([hashtable]$Project, [pscustomobject]$Entry, [string]$Expected, [string]$Problem, [hashtable]$State)
    throw "Release optimization mismatch:`nProject: $($Project.Path)`nCommand: $($Entry.Marker)`nExpected: $Expected`nActual: $(Format-State $State)`nConflicting/missing option: $Problem"
}
function Assert-SpeedCommand {
    param([hashtable]$Project, [pscustomobject]$Entry)
    $switches = Get-CommandSwitches $Entry.Command; $state = Get-EffectiveState $switches
    foreach ($required in @('/OI', '/GY', '/GW')) { if ($switches -notcontains $required) { Throw-Mismatch $Project $Entry '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL' $required $state } }
    foreach ($forbidden in @('/OB3', '/OX')) { if ($switches -contains $forbidden) { Throw-Mismatch $Project $Entry '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL' $forbidden $state } }
    if ($state.Optimization -ne '/O2') { Throw-Mismatch $Project $Entry '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL' 'effective optimization' $state }
    if ($state.Inline -ne '/OB2') { Throw-Mismatch $Project $Entry '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL' 'effective inline expansion' $state }
    if ($state.Favor -ne '/OT') { Throw-Mismatch $Project $Entry '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL' 'effective favor speed' $state }
    if ($state.WholeProgram -ne '/GL') { Throw-Mismatch $Project $Entry '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL' 'effective whole-program optimization' $state }
}
function Assert-ShellCommand {
    param([hashtable]$Project, [pscustomobject]$Entry)
    $switches = Get-CommandSwitches $Entry.Command; $state = Get-EffectiveState $switches
    foreach ($required in @('/OI', '/GY', '/GW')) { if ($switches -notcontains $required) { Throw-Mismatch $Project $Entry '/O1 /Os /Oi /Gy /Gw' $required $state } }
    if ($state.Optimization -ne '/O1') { Throw-Mismatch $Project $Entry '/O1 /Os /Oi /Gy /Gw' 'effective optimization' $state }
    if ($state.Favor -ne '/OS') { Throw-Mismatch $Project $Entry '/O1 /Os /Oi /Gy /Gw' 'effective favor size' $state }
    if ($state.WholeProgram) { Throw-Mismatch $Project $Entry '/O1 /Os /Oi /Gy /Gw' 'whole-program optimization must be disabled' $state }
}
function Assert-LinkCommand {
    param([hashtable]$Project, [pscustomobject]$Entry, [switch]$Shell)
    $switches = Get-CommandSwitches $Entry.Command
    foreach ($required in @('/OPT:REF', '/OPT:ICF')) { if ($switches -notcontains $required) { throw "Release link mismatch: $($Project.Path), target $($Project.Target), missing $required" } }
    $ltcg = @($switches | Where-Object { $_ -eq '/LTCG' -or $_ -eq '/LTCG:INCREMENTAL' })
    if ($ltcg -contains '/LTCG:INCREMENTAL') { throw "Release link mismatch: $($Project.Path), target $($Project.Target), conflicting /LTCG:incremental" }
    if ($Shell) { if ($ltcg -contains '/LTCG') { throw "Release link mismatch: $($Project.Path), target $($Project.Target), FBShell must not use /LTCG" } }
    elseif ($ltcg -notcontains '/LTCG') { throw "Release link mismatch: $($Project.Path), target $($Project.Target), missing /LTCG" }
}
function Assert-ParserRegressionTests {
    $speed = @{ Path = 'synthetic-speed'; Target = 'synthetic' }; $shell = @{ Path = 'synthetic-shell'; Target = 'synthetic' }
    $valid = [pscustomobject]@{ Marker = 'valid.cpp'; Command = '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL' }; Assert-SpeedCommand $speed $valid
    foreach ($sample in @('/O2 /Ob2 /Oi /Ot /Gy /GL', '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL /Ob1', '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL /Ob3', '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL /Od', '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL /Ox', '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL /GL-')) {
        try { Assert-SpeedCommand $speed ([pscustomobject]@{ Marker = 'broken.cpp'; Command = $sample }); throw "Synthetic parser test unexpectedly passed: $sample" } catch { if ($_.Exception.Message -like 'Synthetic parser test unexpectedly passed*') { throw } }
    }
    Assert-ShellCommand $shell ([pscustomobject]@{ Marker = 'shell.cpp'; Command = '/O1 /Os /Oi /Gy /Gw' })
    try { Assert-ShellCommand $shell ([pscustomobject]@{ Marker = 'shell.cpp'; Command = '/O1 /Os /Oi /Gy /Gw /O2' }); throw 'Synthetic FBShell /O2 test unexpectedly passed.' } catch { if ($_.Exception.Message -like 'Synthetic FBShell*') { throw } }
    $entries = Get-TlogCommandEntries @('^one.cpp', '/O2 /Ob2 /Oi /Ot /Gy /Gw /GL', '^two.cpp', '/O2 /Ob2 /Oi /Ot /Gy /GL') 'Compiler'
    if ($entries.Count -ne 2) { throw 'Synthetic tlog parser did not preserve separate compiler commands.' }
    try { $entries | ForEach-Object { Assert-SpeedCommand $speed $_ }; throw 'Synthetic missing-/Gw multi-command test unexpectedly passed.' } catch { if ($_.Exception.Message -like 'Synthetic missing-*') { throw } }
}
Assert-ParserRegressionTests
if (-not $RequireEffectiveFlags) { Write-Host 'Effective flag verification requires a Release|Win32 build; rerun with -RequireEffectiveFlags.'; exit 0 }

function Get-ProjectTlogEntries {
    param([hashtable]$Project, [ValidateSet('Compiler', 'Linker')][string]$Kind)
    $root = Join-Path $repoRoot $Project.TlogRoot; $fileName = if ($Kind -eq 'Compiler') { 'CL.command.1.tlog' } else { 'link.command.1.tlog' }
    # Search only direct .tlog directories in the project's deterministic IntDir.
    # Nested directories (for example Modern\) belong to other output variants.
    $matches = @(Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like '*.tlog' } |
        ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -File -Filter $fileName -ErrorAction SilentlyContinue })
    if ($matches.Count -ne 1) { throw "Expected one $fileName below $($Project.TlogRoot) for $($Project.Path), found $($matches.Count)." }
    $tlog = $matches[0]
    $buildStates = @(Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like '*.tlog' } |
        ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -File -Filter '*.lastbuildstate' -ErrorAction SilentlyContinue })
    if ($buildStates.Count -ne 1) { throw "Expected one Release build state below $($Project.TlogRoot) for $($Project.Path), found $($buildStates.Count)." }
    $latestInput = @($sharedProps, $commonPropsPath, (Join-Path $repoRoot $Project.Path)) | ForEach-Object { (Get-Item -LiteralPath $_).LastWriteTimeUtc } | Sort-Object -Descending | Select-Object -First 1
    if ($buildStates[0].LastWriteTimeUtc -lt $latestInput) { throw "Stale Release build state for $($Project.Path); rebuild Release|Win32." }
    Get-TlogCommandEntries (Get-Content -LiteralPath $tlog.FullName -Encoding Unicode) $Kind
}
foreach ($project in $speedProjects) {
    Get-ProjectTlogEntries $project 'Compiler' | ForEach-Object { Assert-SpeedCommand $project $_ }
    $links = @(Get-ProjectTlogEntries $project 'Linker' | Where-Object { $_.Command -match [regex]::Escape($project.Target) })
    if ($links.Count -ne 1) { throw "Expected one final link command for $($project.Path), found $($links.Count)." }; Assert-LinkCommand $project $links[0]
}
Get-ProjectTlogEntries $shellProject 'Compiler' | ForEach-Object { Assert-ShellCommand $shellProject $_ }
$shellLinks = @(Get-ProjectTlogEntries $shellProject 'Linker' | Where-Object { $_.Command -match [regex]::Escape($shellProject.Target) })
if ($shellLinks.Count -ne 1) { throw "Expected one final link command for $($shellProject.Path), found $($shellLinks.Count)." }; Assert-LinkCommand $shellProject $shellLinks[0] -Shell
Write-Host 'Release optimization profile effective flags passed.'
