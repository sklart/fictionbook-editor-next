<# Ensures the shared FBE COM contract is generated outside product sources. #>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$contractProject = Join-Path $repoRoot 'src\contracts\FBEContracts.vcxproj'
$contractIdl = Join-Path $repoRoot 'src\contracts\fbe.idl'

foreach ($path in @($contractProject, $contractIdl)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "FBE contract input is missing: $path"
    }
}
foreach ($obsoletePath in @(
    'src\fbe\fbe.idl', 'src\fbe\FBE.h', 'src\fbe\FBE_i.c', 'src\fbe\FBE.tlb',
    'src\export-docx\fbe.h', 'src\export-docx\fbe_i.c',
    'src\export-epub\fbe.h', 'src\export-epub\fbe_i.c'
)) {
    if (Test-Path -LiteralPath (Join-Path $repoRoot $obsoletePath)) {
        throw "Generated FBE contract artifact remains in a product source directory: $obsoletePath"
    }
}

$projectText = Get-Content -Raw -LiteralPath $contractProject
foreach ($needle in @('Midl Include="fbe.idl"', '$(FbeApiOutputDirectory)FBE.h', '$(FbeApiOutputDirectory)FBE_i.c', '$(FbeApiOutputDirectory)FBE.tlb')) {
    if (-not $projectText.Contains($needle)) {
        throw "FBEContracts project does not declare expected generated output: $needle"
    }
}
foreach ($consumer in @('src\fbe\FBE.vcxproj', 'src\import-epub\ImportEPUB.vcxproj', 'src\import-epub\ImportEPUBBatch.vcxproj', 'src\export-html\ExportHTML.vcxproj', 'src\export-docx\ExportDOCX.vcxproj', 'src\export-epub\ExportEPUB.vcxproj')) {
    $consumerText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $consumer)
    if (-not $consumerText.Contains('{A6F27D46-6116-4A85-A1E5-8C68E79E5B4D}')) {
        throw "Contract consumer does not reference FBEContracts: $consumer"
    }
    if (-not $consumerText.Contains('$(FbeApiOutputDirectory)')) {
        throw "Contract consumer does not include the generated API directory: $consumer"
    }
}

foreach ($producer in @('src\export-html\ExportHTML.vcxproj', 'src\export-docx\ExportDOCX.vcxproj', 'src\export-epub\ExportEPUB.vcxproj')) {
    $producerText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $producer)
    if ($producerText.Contains('Midl Include="..\contracts\fbe.idl"')) {
        throw "Contract consumer independently generates fbe.idl: $producer"
    }
}

$apiDirectory = & (Join-Path $PSScriptRoot 'ensure-fbe-api.ps1') -Configuration $Configuration
foreach ($name in @('FBE.h', 'FBE_i.c', 'FBE.tlb')) {
    if (-not (Test-Path -LiteralPath (Join-Path $apiDirectory $name) -PathType Leaf)) {
        throw "FBE API output is missing after MIDL generation: $name"
    }
}

Write-Host 'FBE contract generation boundary passed.'
