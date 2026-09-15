param(
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'

function Text([string]$relativePath) {
    Get-Content -Raw -LiteralPath (Join-Path $RepoRoot $relativePath)
}

$builder = Text 'src\fbe\scripts\ScriptMenuBuilder.cpp'
$controller = Text 'src\fbe\scripts\ScriptUiController.cpp'
$controllerHeader = Text 'src\fbe\scripts\ScriptUiController.h'
$frameHeader = Text 'src\fbe\mainfrm.h'

if($builder -notmatch 'BuildSubMenu\(parentMenu, CString\(\), initializeHotkey, addVisual\);') {
    throw 'Script menu root must use the empty ScriptDescriptor::parentId.'
}
if($builder -match 'BuildSubMenu\(parentMenu, L"0",') {
    throw 'Script menu builder regressed to the obsolete "0" root ID.'
}
if($controllerHeader -notmatch 'ScriptCommandCount\s*=\s*999' -or
   $controller -notmatch 'AssignCommandIds\(ScriptCommandCount,' -or
   $frameHeader -notmatch 'COMMAND_RANGE_HANDLER\(ID_SCRIPT_BASE, ID_SCRIPT_BASE \+ FbeScripts::ScriptCommandCount, OnToolsScript\)') {
    throw 'Script command assignment and command routing no longer share the 999-command range.'
}

# This fixture mirrors the descriptor tree supplied by ScriptCatalog.  A root
# script and a root folder both have parentId == L""; the child belongs to the
# folder's stable generated id.  It covers the observable menu topology and
# ID allocation performed by MenuBuilder::Build/BuildSubMenu.
$items = @(
    [pscustomobject]@{ Name = 'Root script'; Id = '_1'; ParentId = ''; IsFolder = $false; CommandId = 7 },
    [pscustomobject]@{ Name = 'Root folder'; Id = '_2'; ParentId = ''; IsFolder = $true; CommandId = -1 },
    [pscustomobject]@{ Name = 'Child script'; Id = '_2_1'; ParentId = '_2'; IsFolder = $false; CommandId = 8 }
)

function Build-MenuFixture([string]$parentId, [ref]$nextFolderId) {
    $result = @()
    foreach($item in $items | Where-Object { $_.ParentId -ceq $parentId }) {
        if($item.IsFolder) {
            $id = 10101 + $nextFolderId.Value
            $nextFolderId.Value++
            $result += [pscustomobject]@{ Name = $item.Name; Id = $id; Children = @(Build-MenuFixture $item.Id ([ref]$nextFolderId.Value)) }
        } else {
            $result += [pscustomobject]@{ Name = $item.Name; Id = 9000 + $item.CommandId; Children = @() }
        }
    }
    return $result
}

$nextFolderId = 0
$root = @(Build-MenuFixture '' ([ref]$nextFolderId))
if($root.Count -ne 2) { throw 'Root ScriptDescriptor entries were not added to the root HMENU.' }
if($root[0].Name -ne 'Root script' -or $root[0].Id -ne 9007) { throw 'Root script received an incorrect command ID.' }
if($root[1].Name -ne 'Root folder' -or $root[1].Id -ne 10101) { throw 'Root folder received an incorrect temporary menu ID.' }
if($root[1].Children.Count -ne 1 -or $root[1].Children[0].Name -ne 'Child script' -or $root[1].Children[0].Id -ne 9008) {
    throw 'Nested root-folder child script was not built with the expected command ID.'
}

Write-Host 'Script menu builder regression contract passed.'
