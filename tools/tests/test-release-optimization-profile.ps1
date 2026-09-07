[CmdletBinding()]
param(
    [switch]$RequireEffectiveFlags
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sharedProps = Join-Path $repoRoot 'tools\msbuild\FBE.ReleaseOptimization.props'
if (-not (Test-Path -LiteralPath $sharedProps -PathType Leaf)) { throw "Shared release optimization policy is missing: $sharedProps" }
$commonProps = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\msbuild\FBE.Common.props')
if ($commonProps -notmatch 'ForceImportAfterCppProps' -or $commonProps -notmatch [regex]::Escape('FBE.ReleaseOptimization.props')) {
    throw 'Shared release optimization policy is not imported after Microsoft.Cpp.props.'
}

$speedProjects = @(
    @{ Path = 'src\fbe\FBE.vcxproj'; TlogRoot = 'build\obj\fbe\Release' },
    @{ Path = 'src\fbv\FBV.vcxproj'; TlogRoot = 'build\obj\fbv\Win32\Release' },
    @{ Path = 'src\export-html\ExportHTML.vcxproj'; TlogRoot = 'build\obj\export-html\Win32\Release' },
    @{ Path = 'src\export-docx\ExportDOCX.vcxproj'; TlogRoot = 'build\obj\export-docx\Win32\Release' },
    @{ Path = 'src\export-docx\ExportDOCXBatch.vcxproj'; TlogRoot = 'build\obj\ExportDOCXBatch\Win32\Release' },
    @{ Path = 'src\export-epub\ExportEPUB.vcxproj'; TlogRoot = 'build\obj\export-epub\Win32\Release' },
    @{ Path = 'src\export-epub\ExportEPUBBatch.vcxproj'; TlogRoot = 'build\obj\export-epub-batch\Win32\Release' },
    @{ Path = 'src\import-epub\ImportEPUB.vcxproj'; TlogRoot = 'build\obj\import-epub\Win32\Release' },
    @{ Path = 'src\import-epub\ImportEPUBBatch.vcxproj'; TlogRoot = 'build\obj\ImportEPUBBatch\Win32\Release' },
    @{ Path = 'src\import-epub\ImportEPUBLunaSVG.vcxproj'; TlogRoot = 'build\obj\ImportEPUBLunaSVG\Win32\Release' }
)
$shellProject = @{ Path = 'src\fbshell\FBShell.vcxproj'; TlogRoot = 'build\obj\fbshell\Release' }
$projectPaths = @($speedProjects.Path) + $shellProject.Path + 'src\contracts\FBEContracts.vcxproj'
foreach ($relativePath in $projectPaths) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativePath)
    if ($text -notmatch [regex]::Escape('tools\msbuild\FBE.Common.props')) { throw "Project does not inherit the shared optimization policy: $relativePath" }
}

$policy = Get-Content -Raw -LiteralPath $sharedProps
foreach ($requiredValue in @('MaxSpeed', 'AnySuitable', 'Speed', 'MinSpace', 'Size', 'UseLinkTimeCodeGeneration')) {
    if ($policy -notmatch [regex]::Escape(">$requiredValue<")) { throw "Shared release optimization policy is missing: $requiredValue" }
}
if ($policy -notmatch [regex]::Escape('/Gw')) { throw 'Shared release optimization policy is missing: /Gw' }
if ($policy -notmatch 'MSBuildProjectName.*FBShell') { throw 'FBShell size-profile exception is not explicit.' }

$fbeText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj')
if ($fbeText -match 'OnlyExplicitInline|<WholeProgramOptimization>false</WholeProgramOptimization>|AdditionalOptions>[^<]*\/LTCG|Release\|Win32.*>Full<') { throw 'FBE Release profile contains a legacy inline/WPO/LTCG override.' }
$shellText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $shellProject.Path)
if ($shellText -match "Condition=.*Release\|Win32[\s\S]*<Optimization>MaxSpeed</Optimization>") { throw 'FBShell must retain its size-oriented Release profile.' }

foreach ($relativePath in $projectPaths) {
    [xml]$projectXml = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativePath)
    $namespace = New-Object System.Xml.XmlNamespaceManager($projectXml.NameTable)
    $namespace.AddNamespace('msb', $projectXml.DocumentElement.NamespaceURI)
    $releaseGroups = $projectXml.SelectNodes('//msb:ItemDefinitionGroup[@Condition]', $namespace) | Where-Object { $_.GetAttribute('Condition') -match 'Release\|Win32' }
    foreach ($group in $releaseGroups) {
        $additionalOptions = $group.SelectSingleNode('msb:ClCompile/msb:AdditionalOptions', $namespace)
        if ($null -ne $additionalOptions -and $additionalOptions.InnerText -notmatch [regex]::Escape('%(AdditionalOptions)')) { throw "Release|Win32 AdditionalOptions must retain inherited options: $relativePath" }
    }
}

Write-Host 'Release optimization profile structural policy passed.'
if (-not $RequireEffectiveFlags) {
    Write-Host 'Effective flag verification requires a Release|Win32 build; rerun with -RequireEffectiveFlags.'
    exit 0
}

function Get-TlogText {
    param([hashtable]$Project, [string]$FileName)
    $root = Join-Path $repoRoot $Project.TlogRoot
    $tlog = Get-ChildItem -LiteralPath $root -Recurse -File -Filter $FileName -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $tlog) { throw "Missing $FileName for $($Project.Path); build Release|Win32 first." }
    Get-Content -Raw -LiteralPath $tlog.FullName -Encoding Unicode
}
function Assert-HasSwitches {
    param([string]$Text, [string]$ProjectPath, [string[]]$Switches, [string]$Kind)
    foreach ($switch in $Switches) {
        if ($Text -notmatch "(?<!\\S)$([regex]::Escape($switch))(?!\\S)") { throw "$ProjectPath $Kind tlog is missing $switch" }
    }
}
function Assert-LacksSwitches {
    param([string]$Text, [string]$ProjectPath, [string[]]$Switches, [string]$Kind)
    foreach ($switch in $Switches) {
        if ($Text -match "(?<!\\S)$([regex]::Escape($switch))(?!\\S)") { throw "$ProjectPath $Kind tlog has conflicting $switch" }
    }
}

foreach ($project in $speedProjects) {
    $compiler = Get-TlogText $project 'CL.command.1.tlog'
    $linker = Get-TlogText $project 'link.command.1.tlog'
    Assert-HasSwitches $compiler $project.Path @('/O2', '/Ob2', '/Oi', '/Ot', '/Gy', '/Gw', '/GL') 'compiler'
    Assert-LacksSwitches $compiler $project.Path @('/Od', '/O1', '/Os', '/Ob0', '/Ob1', '/GL-') 'compiler'
    Assert-HasSwitches $linker $project.Path @('/LTCG', '/OPT:REF', '/OPT:ICF') 'linker'
    Assert-LacksSwitches $linker $project.Path @('/LTCG:incremental') 'linker'
}
$shellCompiler = Get-TlogText $shellProject 'CL.command.1.tlog'
$shellLinker = Get-TlogText $shellProject 'link.command.1.tlog'
Assert-HasSwitches $shellCompiler $shellProject.Path @('/O1', '/Os', '/Oi', '/Gy', '/Gw') 'compiler'
Assert-LacksSwitches $shellCompiler $shellProject.Path @('/O2', '/Ot', '/GL', '/GL-') 'compiler'
Assert-HasSwitches $shellLinker $shellProject.Path @('/OPT:REF', '/OPT:ICF') 'linker'
Assert-LacksSwitches $shellLinker $shellProject.Path @('/LTCG', '/LTCG:incremental') 'linker'
Write-Host 'Release optimization profile effective flags passed.'
