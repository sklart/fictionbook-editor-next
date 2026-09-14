param([string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference='Stop'
function Text($p) { Get-Content -Raw -LiteralPath (Join-Path $RepoRoot $p) }
function Must($t,$p,$n) { if($t -notmatch $p){throw "Missing management contract: $n"} }
$frame=Text 'src\fbe\mainfrm.cpp'; $dialog=Text 'src\fbe\ScriptToolbarManagerDlg.cpp'; $manager=Text 'src\fbe\toolbars\ScriptToolbarManager.cpp'; $collection=Text 'src\fbe\toolbars\ScriptToolbarCollection.cpp'
Must $frame 'PortableToolbarStore::Load\(persistedToolbars\)' 'installed and portable definitions load through settings directory'
Must $frame 'if\(runtime\.window == NULL\) continue;' 'empty custom toolbar reaches layout adapter'
Must $frame 'ToolbarLayoutAdapter::Apply\(runtime\.window, items, catalog\)' 'UID layout is applied to every runtime control'
Must $dialog 'm_manager\.Create' 'create panel action'
Must $dialog 'm_manager\.Delete' 'delete panel action'
Must $dialog 'm_manager\.Rename' 'rename panel action'
Must $dialog 'm_manager\.SetVisible' 'visibility action'
Must $collection 'if\(id == L"scripts-main"\) return false' 'main panel cannot be deleted'
Must $manager 'definition->visible = visible' 'visibility persists in definition'
Write-Host 'Script toolbar management contract passed.'
