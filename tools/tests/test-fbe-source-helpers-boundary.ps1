<# Keeps standalone XML source helpers separate from UI and document ownership. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$project = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj')
$filters = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj.filters')
foreach ($name in @('Fb2SourceAutocomplete.cpp', 'Fb2SourceAutocomplete.h', 'Fb2SourceStructuralContext.cpp', 'Fb2SourceStructuralContext.h')) {
    $newPath = Join-Path $repoRoot "src\fbe\source\$name"
    $oldPath = Join-Path $repoRoot "src\fbe\$name"
    if (-not (Test-Path -LiteralPath $newPath -PathType Leaf)) { throw "XML source helper is missing: $newPath" }
    if (Test-Path -LiteralPath $oldPath -PathType Leaf) { throw "XML source helper returned to the FBE root: $oldPath" }
    if ($filters -notmatch [regex]::Escape("source\$name")) { throw "FBE filters do not include XML source helper: $name" }
    if ($name -like '*.cpp' -and $project -notmatch [regex]::Escape("source\$name")) { throw "FBE project does not compile XML source helper: $name" }
}
foreach ($source in @('Fb2SourceAutocomplete.cpp', 'Fb2SourceStructuralContext.cpp')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\source\$source")
    if ($text -match 'mainfrm\.h|FBEview\.h|FBDoc\.h|SettingsDlg\.h') { throw "XML source helper unexpectedly depends on an editor coordinator: $source" }
}
Write-Host 'FBE XML source helpers boundary passed.'
