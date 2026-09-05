<# Keeps the EditorBackgrounds catalogue as a settings service. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$project = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj')
$filters = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj.filters')
foreach ($name in @('EditorBackgrounds.cpp', 'EditorBackgrounds.h')) {
    $newPath = Join-Path $repoRoot "src\fbe\settings\$name"
    $oldPath = Join-Path $repoRoot "src\fbe\$name"
    if (-not (Test-Path -LiteralPath $newPath -PathType Leaf)) { throw "Settings service is missing: $newPath" }
    if (Test-Path -LiteralPath $oldPath -PathType Leaf) { throw "Settings service returned to the FBE root: $oldPath" }
    if ($filters -notmatch [regex]::Escape("settings\$name")) { throw "FBE filters do not include settings service: $name" }
    if ($name -like '*.cpp' -and $project -notmatch [regex]::Escape("settings\$name")) { throw "FBE project does not compile settings service: $name" }
}
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\settings\EditorBackgrounds.cpp')
if ($source -match 'mainfrm\.h|FBEview\.h|FBDoc\.h|SettingsDlg\.h') { throw 'EditorBackgrounds unexpectedly depends on an editor coordinator.' }
foreach ($caller in @('src\fbe\FBDoc.cpp', 'src\fbe\mainfrm.cpp', 'src\fbe\SettingsEditorPage.h')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $caller)
    if ($text -notmatch [regex]::Escape('settings\\EditorBackgrounds.h')) { throw "Settings service caller lost its explicit include: $caller" }
}
Write-Host 'FBE settings background boundary passed.'
