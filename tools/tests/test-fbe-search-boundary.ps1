<# Keeps the editor's regex backend grouped with its PCRE2 implementation. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$project = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj')
$filters = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj.filters')
foreach ($name in @('RegexBackend.cpp', 'RegexBackend.h', 'RegexBackendPcre2.cpp', 'RegexPcre2CodeCache.h', 'RegexPcre2MatchLoop.h')) {
    $newPath = Join-Path $repoRoot "src\fbe\search\$name"
    $oldPath = Join-Path $repoRoot "src\fbe\$name"
    if (-not (Test-Path -LiteralPath $newPath -PathType Leaf)) { throw "Search backend file is missing: $newPath" }
    if (Test-Path -LiteralPath $oldPath -PathType Leaf) { throw "Search backend file returned to the FBE root: $oldPath" }
    if ($filters -notmatch [regex]::Escape("search\$name")) { throw "FBE filters do not include search backend file: $name" }
    if ($name -like '*.cpp' -and $project -notmatch [regex]::Escape("search\$name")) { throw "FBE project does not compile search backend source: $name" }
}
foreach ($source in @('RegexBackend.cpp', 'RegexBackendPcre2.cpp')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\search\$source")
    if ($text -match 'mainfrm\.h|FBEview\.h|FBDoc\.h|SettingsDlg\.h') { throw "Search backend unexpectedly depends on an editor coordinator: $source" }
}
Write-Host 'FBE search backend boundary passed.'
