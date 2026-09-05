[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$projects = @(
    'src\contracts\FBEContracts.vcxproj',
    'src\fbe\FBE.vcxproj',
    'src\fbshell\FBShell.vcxproj',
    'src\fbv\FBV.vcxproj',
    'src\export-html\ExportHTML.vcxproj',
    'src\export-docx\ExportDOCX.vcxproj',
    'src\export-docx\ExportDOCXBatch.vcxproj',
    'src\export-epub\ExportEPUB.vcxproj',
    'src\export-epub\ExportEPUBBatch.vcxproj',
    'src\import-epub\ImportEPUB.vcxproj',
    'src\import-epub\ImportEPUBBatch.vcxproj',
    'src\import-epub\ImportEPUBLunaSVG.vcxproj'
)

foreach ($project in $projects) {
    $path = Join-Path $repoRoot $project
    $text = Get-Content -Raw -LiteralPath $path
    if ($text -notmatch [regex]::Escape('tools\msbuild\FBE.Common.props')) {
        throw "First-party project does not import FBE.Common.props: $project"
    }
    if ($text -match '<PlatformToolset>v145</PlatformToolset>') {
        throw "First-party project retains obsolete default v145: $project"
    }
    if ($text -notmatch [regex]::Escape('<PlatformToolset>$(FbePlatformToolset)</PlatformToolset>')) {
        throw "First-party project does not use the centralized toolset property: $project"
    }
}

foreach ($vendorProject in @(
    'src\import-epub\thirdparty\lunasvg\lunasvg.vcxproj',
    'src\import-epub\thirdparty\lunasvg\plutovg.vcxproj'
)) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $vendorProject)
    if ($text -match [regex]::Escape('tools\msbuild\FBE.Common.props')) {
        throw "Vendored project must not import FBE.Common.props: $vendorProject"
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "vswhere.exe is required for evaluated MSBuild policy: $vswhere"
}
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -version '[17.0,18.0)' -find 'MSBuild\Current\Bin\MSBuild.exe' | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($msbuild)) {
    throw 'Visual Studio 2022 MSBuild was not found.'
}

foreach ($case in @(
    @{ Project = 'src\contracts\FBEContracts.vcxproj'; Configuration = 'Release'; Platform = 'Win32' },
    @{ Project = 'src\fbe\FBE.vcxproj'; Configuration = 'Release'; Platform = 'Win32' },
    @{ Project = 'src\fbshell\FBShell.vcxproj'; Configuration = 'Release'; Platform = 'x64' },
    @{ Project = 'src\export-epub\ExportEPUBBatch.vcxproj'; Configuration = 'Release'; Platform = 'x64' },
    @{ Project = 'src\import-epub\ImportEPUBLunaSVG.vcxproj'; Configuration = 'Release'; Platform = 'Win32' }
)) {
    $output = & $msbuild (Join-Path $repoRoot $case.Project) "/p:Configuration=$($case.Configuration)" "/p:Platform=$($case.Platform)" '/getProperty:PlatformToolset;VCToolsVersion;VCToolsInstallDir;FbeRepoRoot;SolutionDir' /nologo
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild property evaluation failed: $($case.Project)"
    }
    $properties = (($output -join "`n") | ConvertFrom-Json).Properties
    if ($properties.PlatformToolset -ne 'v143') {
        throw "Unexpected toolset for $($case.Project): $($properties.PlatformToolset)"
    }
    if ([string]::IsNullOrWhiteSpace($properties.VCToolsVersion) -or -not $properties.VCToolsVersion.StartsWith('14.44')) {
        throw "Universal Win7 build must evaluate VC Tools 14.44 for $($case.Project): $($properties.VCToolsVersion)"
    }
    if ([string]::IsNullOrWhiteSpace($properties.VCToolsInstallDir) -or -not (Test-Path -LiteralPath (Join-Path $properties.VCToolsInstallDir 'bin') -PathType Container)) {
        throw "MSBuild did not resolve an installed VC compiler directory for $($case.Project): $($properties.VCToolsInstallDir)"
    }
    if ($properties.FbeRepoRoot.TrimEnd('\') -ne $repoRoot.TrimEnd('\')) {
        throw "Unexpected repository root for $($case.Project): $($properties.FbeRepoRoot)"
    }
    if ($properties.SolutionDir.TrimEnd('\') -ne $repoRoot.TrimEnd('\')) {
        throw "Standalone SolutionDir is not repository-rooted for $($case.Project): $($properties.SolutionDir)"
    }
}

Write-Host 'First-party MSBuild policy passed.'
