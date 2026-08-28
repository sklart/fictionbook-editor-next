[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$main = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\resource.h')
if($main -notmatch 'SCRIPT_FOLDER_MENU_ID_BASE\s*=\s*10000' -or $main -notmatch 'SCRIPT_FOLDER_MENU_ID_COUNT\s*=\s*999') { throw 'Folders need a dedicated temporary menu-ID range.' }
if($main -match 'mi\.wID\s*=\s*0') { throw 'Folder icons must not share menu image key 0.' }
if($main -notmatch 'scripts\[i\]\.isFolder[\s\S]*?scripts\[i\]\.wID\s*=\s*-1') { throw 'Folders must remain outside persistent script command IDs.' }
if($main -notmatch 'AddIcon\(\(HICON\)scripts\[i\]\.picture, mi\.wID\)' -or $main -notmatch 'AddBitmap\(\(HBITMAP\)scripts\[i\]\.picture, mi\.wID\)') { throw 'Folder and script pictures must use their individual menu ID.' }
if($header -notmatch 'ID_SCRIPT_BASE\s+9000' -or $header -notmatch 'ID_EDIT_INS_SYMBOL\s+10000') { throw 'Unexpected dynamic ID range boundary.' }
Write-Host 'Script folder icon ID contract passed.'
