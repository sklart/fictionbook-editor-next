[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$generator = Join-Path $repoRoot 'tools\build\generate-fb2-schema-metadata.ps1'
$committed = Join-Path $repoRoot 'src\fbe\generated\Fb2SchemaMetadata.h'
$temporary = Join-Path ([System.IO.Path]::GetTempPath()) ('Fb2SchemaMetadata.' + [guid]::NewGuid().ToString('N') + '.h')
function Get-NormalizedText([string]$path) {
    return [System.IO.File]::ReadAllText($path).Replace("`r`n", "`n")
}
try {
    & $generator -OutputPath $temporary
    # actions/checkout honours core.autocrlf on Windows, while the generator
    # intentionally writes portable LF output.  Compare generated content,
    # not the checkout-specific line-ending representation.
    if ((Get-NormalizedText $committed) -cne (Get-NormalizedText $temporary)) {
        throw 'FB2 schema metadata is stale; regenerate it.'
    }
    Write-Host 'FB2 schema metadata freshness test passed.'
}
finally {
    Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
}
