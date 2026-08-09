[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$generator = Join-Path $repoRoot 'tools\build\generate-fb2-schema-metadata.ps1'
$committed = Join-Path $repoRoot 'src\fbe\generated\Fb2SchemaMetadata.h'
$temporary = Join-Path ([System.IO.Path]::GetTempPath()) ('Fb2SchemaMetadata.' + [guid]::NewGuid().ToString('N') + '.h')
try {
    & $generator -OutputPath $temporary
    if (-not [System.Linq.Enumerable]::SequenceEqual([System.IO.File]::ReadAllBytes($committed), [System.IO.File]::ReadAllBytes($temporary))) {
        throw 'FB2 schema metadata is stale; regenerate it.'
    }
    Write-Host 'FB2 schema metadata freshness test passed.'
}
finally {
    Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
}
