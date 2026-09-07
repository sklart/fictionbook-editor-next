[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sharedProps = Join-Path $repoRoot 'tools\msbuild\FBE.ReleaseOptimization.props'
if (-not (Test-Path -LiteralPath $sharedProps -PathType Leaf)) {
    throw "Shared release optimization policy is missing: $sharedProps"
}
$commonProps = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\msbuild\FBE.Common.props')
if ($commonProps -notmatch 'ForceImportAfterCppProps' -or $commonProps -notmatch [regex]::Escape('FBE.ReleaseOptimization.props')) {
    throw 'Shared release optimization policy is not imported after Microsoft.Cpp.props.'
}

$speedProjects = @(
    'src\fbe\FBE.vcxproj', 'src\fbv\FBV.vcxproj',
    'src\export-html\ExportHTML.vcxproj', 'src\export-docx\ExportDOCX.vcxproj',
    'src\export-docx\ExportDOCXBatch.vcxproj', 'src\export-epub\ExportEPUB.vcxproj',
    'src\export-epub\ExportEPUBBatch.vcxproj', 'src\import-epub\ImportEPUB.vcxproj',
    'src\import-epub\ImportEPUBBatch.vcxproj', 'src\import-epub\ImportEPUBLunaSVG.vcxproj'
)
$allProjects = $speedProjects + 'src\fbshell\FBShell.vcxproj'
foreach ($relativePath in $allProjects) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativePath)
    if ($text -notmatch [regex]::Escape('tools\msbuild\FBE.Common.props')) {
        throw "Project does not inherit the shared optimization policy: $relativePath"
    }
}

$policy = Get-Content -Raw -LiteralPath $sharedProps
foreach ($requiredValue in @('MaxSpeed', 'AnySuitable', 'Speed', 'MinSpace', 'Size', 'UseLinkTimeCodeGeneration')) {
    if ($policy -notmatch [regex]::Escape(">$requiredValue<")) {
        throw "Shared optimization policy is missing: $requiredValue"
    }
}
if ($policy -notmatch [regex]::Escape('/Gw')) {
    throw 'Shared optimization policy is missing: /Gw'
}
if ($policy -notmatch "MSBuildProjectName.*FBShell") {
    throw 'FBShell size-profile exception is not explicit.'
}

$fbeText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj')
if ($fbeText -match 'OnlyExplicitInline|<WholeProgramOptimization>false</WholeProgramOptimization>|AdditionalOptions>[^<]*\/LTCG|Release\|Win32.*>Full<') {
    throw 'FBE Release profile contains a legacy inline/WPO/LTCG override.'
}

$shellText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbshell\FBShell.vcxproj')
if ($shellText -match "Condition=.*Release\|Win32[\s\S]*<Optimization>MaxSpeed</Optimization>") {
    throw 'FBShell must retain its size-oriented Release profile.'
}

Write-Host 'Release optimization profile policy passed.'
