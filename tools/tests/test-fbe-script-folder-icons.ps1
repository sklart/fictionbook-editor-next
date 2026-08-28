[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$main = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\resource.h')
if($main -notmatch 'SCRIPT_FOLDER_MENU_ID_BASE\s*=\s*ID_EDIT_INS_SYMBOL\s*\+\s*101' -or $main -notmatch 'SCRIPT_FOLDER_MENU_ID_COUNT\s*=\s*999') { throw 'Folders need a dedicated temporary menu-ID range after symbol commands.' }
foreach($guard in @('ID_SCRIPT_BASE \+ SCRIPT_COMMAND_COUNT < SCRIPT_FOLDER_MENU_ID_BASE', 'SCRIPT_FOLDER_MENU_ID_BASE > ID_EDIT_INS_SYMBOL \+ 100', 'SCRIPT_FOLDER_MENU_ID_BASE \+ SCRIPT_FOLDER_MENU_ID_COUNT < ID_NEXT_ITEM')) { if($main -notmatch $guard) { throw "Missing folder ID range guard: $guard" } }
if($main -notmatch 'int\s+nextFolderMenuId\s*=\s*0' -or $main -notmatch 'nextFolderMenuId < SCRIPT_FOLDER_MENU_ID_COUNT.*?SCRIPT_FOLDER_MENU_ID_BASE \+ nextFolderMenuId\+\+') { throw 'Folder ID base must be the first issued ID and the final slot must be usable.' }
if($main -notmatch 'nextFolderMenuId < SCRIPT_FOLDER_MENU_ID_COUNT.*?: 0' -or $main -notmatch '!scripts\[i\]\.isFolder \|\| mi\.wID != 0') { throw 'Folder menu exhaustion must keep folders accessible while skipping their icon registration.' }
if($main -notmatch 'scripts\[i\]\.isFolder[\s\S]*?scripts\[i\]\.wID\s*=\s*-1') { throw 'Folders must remain outside persistent script command IDs.' }
if($main -notmatch 'AddIcon\(\(HICON\)scripts\[i\]\.picture, mi\.wID\)' -or $main -notmatch 'AddBitmap\(\(HBITMAP\)scripts\[i\]\.picture, mi\.wID\)') { throw 'Folder and script pictures must use their individual menu ID.' }
if($header -notmatch 'ID_SCRIPT_BASE\s+9000' -or $header -notmatch 'ID_EDIT_INS_SYMBOL\s+10000') { throw 'Unexpected dynamic ID range boundary.' }
$symbolMenuId = 10000
$symbolMenuIdLast = $symbolMenuId + 100
$folderMenuIdBase = $symbolMenuId + 101
$folderMenuIdCount = 999
$firstFolderMenuId = $folderMenuIdBase
$lastFolderMenuId = $folderMenuIdBase + $folderMenuIdCount - 1
if($firstFolderMenuId -ne 10101 -or $lastFolderMenuId -ne 11099 -or $firstFolderMenuId -le $symbolMenuIdLast) { throw 'Folder menu ID range must issue 10101 through 11099 without overlapping symbol commands.' }
Write-Host 'Script folder icon ID contract passed.'
