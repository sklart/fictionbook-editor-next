[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$generator = Join-Path $repoRoot 'tools\build\generate-fb2-schema-metadata.ps1'
$outputs = @()
$originalCulture = [Threading.Thread]::CurrentThread.CurrentCulture
$originalUiCulture = [Threading.Thread]::CurrentThread.CurrentUICulture
try {
    foreach ($cultureName in @('en-US', 'ru-RU')) {
        [Threading.Thread]::CurrentThread.CurrentCulture = [Globalization.CultureInfo]::GetCultureInfo($cultureName)
        [Threading.Thread]::CurrentThread.CurrentUICulture = [Globalization.CultureInfo]::GetCultureInfo($cultureName)
        $path = Join-Path ([IO.Path]::GetTempPath()) ("Fb2SchemaMetadata.$cultureName." + [guid]::NewGuid().ToString('N') + '.h')
        & $generator -OutputPath $path
        $outputs += $path
    }
    if (-not [Linq.Enumerable]::SequenceEqual([IO.File]::ReadAllBytes($outputs[0]), [IO.File]::ReadAllBytes($outputs[1]))) {
        throw 'FB2 schema metadata generator depends on the current culture.'
    }
    Write-Host 'FB2 schema metadata culture determinism test passed.'
}
finally {
    [Threading.Thread]::CurrentThread.CurrentCulture = $originalCulture
    [Threading.Thread]::CurrentThread.CurrentUICulture = $originalUiCulture
    foreach ($path in $outputs) { Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue }
}
