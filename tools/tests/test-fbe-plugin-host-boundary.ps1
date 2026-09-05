<# Keeps the editor-only plug-in host grouped under src/fbe/plugins. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$project = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj')
$filters = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj.filters')
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach ($name in @('PluginManager.cpp', 'PluginManager.h', 'PluginApiV2.cpp', 'PluginApiV2.h')) {
    $newPath = Join-Path $repoRoot "src\fbe\plugins\$name"
    $oldPath = Join-Path $repoRoot "src\fbe\$name"
    if (-not (Test-Path -LiteralPath $newPath -PathType Leaf)) { throw "Plug-in host file is missing: $newPath" }
    if (Test-Path -LiteralPath $oldPath -PathType Leaf) { throw "Plug-in host file returned to the FBE root: $oldPath" }
    if ($name -like '*.cpp' -and $project -notmatch [regex]::Escape("plugins\$name")) { throw "FBE project does not include plug-in host source: $name" }
    if ($filters -notmatch [regex]::Escape("plugins\$name")) { throw "FBE filters do not include plug-in host file: $name" }
}
foreach ($include in @('plugins\\PluginManager.h', 'plugins\\PluginApiV2.h')) {
    if ($mainFrame -notmatch [regex]::Escape($include)) { throw "mainfrm does not use the explicit plug-in host include: $include" }
}
foreach ($source in @('PluginManager.cpp', 'PluginApiV2.cpp')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\plugins\$source")
    if ($text -match 'mainfrm\.h|FBEview\.h|SettingsDlg\.h') { throw "Editor plug-in host unexpectedly depends on a private UI coordinator: $source" }
}
Write-Host 'FBE plug-in host boundary passed.'
