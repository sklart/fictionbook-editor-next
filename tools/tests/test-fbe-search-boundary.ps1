<# Keeps the editor's regex backend grouped with its PCRE2 implementation. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$project = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj')
$filters = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj.filters')
foreach ($name in @('LiteralSearch.cpp', 'LiteralSearch.h', 'RegexBackend.cpp', 'RegexBackend.h', 'RegexBackendPcre2.cpp', 'RegexPcre2CodeCache.h', 'RegexPcre2MatchLoop.h', 'SearchResults.cpp', 'SearchResults.h', 'SearchViewportResults.h', 'SearchSession.cpp', 'SearchSession.h', 'SearchTextSnapshot.cpp', 'SearchTextSnapshot.h', 'SearchDocumentGeneration.h', 'SearchTypes.h')) {
    $newPath = Join-Path $repoRoot "src\fbe\search\$name"
    $oldPath = Join-Path $repoRoot "src\fbe\$name"
    if (-not (Test-Path -LiteralPath $newPath -PathType Leaf)) { throw "Search backend file is missing: $newPath" }
    if (Test-Path -LiteralPath $oldPath -PathType Leaf) { throw "Search backend file returned to the FBE root: $oldPath" }
    if ($filters -notmatch [regex]::Escape("search\$name")) { throw "FBE filters do not include search backend file: $name" }
    if ($name -like '*.cpp' -and $project -notmatch [regex]::Escape("search\$name")) { throw "FBE project does not compile search backend source: $name" }
}
foreach ($source in @('LiteralSearch.cpp', 'LiteralSearch.h', 'RegexBackend.cpp', 'RegexBackendPcre2.cpp', 'SearchResults.cpp', 'SearchResults.h', 'SearchViewportResults.h', 'SearchSession.cpp', 'SearchSession.h', 'SearchTextSnapshot.cpp', 'SearchTextSnapshot.h', 'SearchDocumentGeneration.h', 'SearchTypes.h')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\search\$source")
    if ($text -match 'mainfrm\.h|FBEview\.h|FBDoc\.h|SettingsDlg\.h') { throw "Search backend unexpectedly depends on an editor coordinator: $source" }
	if ($text -match 'MSHTML::|IHTMLTxtRange|IHTMLDocument') { throw "Search Core unexpectedly depends on MSHTML: $source" }
}
foreach ($adapter in @('SearchDocumentAdapter.cpp', 'SearchDocumentAdapter.h')) {
    $path = Join-Path $repoRoot "src\fbe\$adapter"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Search document adapter is missing: $path" }
}
$designSearch = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
if ($designSearch -match '\.findText\s*\(') {
    throw 'Design-mode search or replace returned to IHTMLTxtRange::findText.'
}
Write-Host 'FBE search backend boundary passed.'
