$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$hotkeyDefaults = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\settings\hotkeys\HotkeyDefaults.cpp')
$hotkeyStore = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\settings\hotkeys\HotkeyStore.cpp')
$resource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\resource.h')
$catalog = Get-Content -Raw -LiteralPath (Join-Path $root 'localization\app-ui\catalog.json')
$utils = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\utils\Utils.cpp')
if ($hotkeyDefaults -notmatch 'CHotkey ToolsOptions\(L"Options", IDS_HOTKEY_TOOLS_OPTIONS, FCONTROL, ID_VIEW_OPTIONS, VK_OEM_COMMA\)') { throw 'Settings is not registered as Ctrl+, hotkey.' }
if ($resource -notmatch 'IDS_HOTKEY_TOOLS_OPTIONS') { throw 'Settings hotkey resource identifier is missing.' }
if ($catalog -notmatch '"fbe.hotkey.tools.options"') { throw 'Settings hotkey localization entry is missing.' }
if ($utils -notmatch 'keycodes\.Add\(L",", VK_OEM_COMMA\)') { throw 'Ctrl+, is not rendered by the hotkey UI.' }
if ($hotkeyStore -notmatch 'CHotkey\* foundHotkey = FindHotkey\(\*foundGroup, group\.m_hotkeys\[j\]\.m_reg_name\);') { throw 'Hotkeys.xml compatibility merge no longer ignores unknown commands.' }
if ($hotkeyStore -notmatch 'foundHotkey->m_accel\.fVirt = group\.m_hotkeys\[j\]\.m_accel\.fVirt;') { throw 'Existing Hotkeys.xml entries are not applied as overrides.' }
Write-Host 'Settings hotkey contract passed.'
