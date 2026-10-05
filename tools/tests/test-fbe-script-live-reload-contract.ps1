<# Verifies the script reload mechanism and the explicit catalogue refresh command. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([string]$text, [string]$pattern, [string]$message) {
    if($text -notmatch $pattern) { throw $message }
}

$runtime = Get-Content -LiteralPath (Join-Path $root 'runtime\main.js') -Raw
$frame = Get-Content -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp') -Raw
$scripts = Get-Content -LiteralPath (Join-Path $root 'src\fbe\scripts\ScriptUiController.cpp') -Raw
$registry = Get-Content -LiteralPath (Join-Path $root 'src\fbe\scripts\ScriptRegistry.cpp') -Raw
$frameHeader = Get-Content -LiteralPath (Join-Path $root 'src\fbe\mainfrm.h') -Raw
$resource = Get-Content -LiteralPath (Join-Path $root 'src\fbe\FBE.rc') -Raw
$ids = Get-Content -LiteralPath (Join-Path $root 'src\fbe\resource.h') -Raw
$locale = Get-Content -LiteralPath (Join-Path $root 'localization\app-ui\fbe-idr-mainframe-menu.json') -Raw
$scenario = Get-Content -LiteralPath (Join-Path $root 'src\fbe\testing\RuntimeTestPortableState.inl') -Raw
$runner = Get-Content -LiteralPath (Join-Path $root 'tools\tests\test-fbe-navigation-scripts-runtime.ps1') -Raw
$runtimeRunner = Get-Content -LiteralPath (Join-Path $root 'tools\tests\test-fbe-script-live-reload-runtime.ps1') -Raw

Require $runtime 'function LoadUserCommandScript\(path\)' 'Runtime no longer has the fresh user-script loader.'
Require $runtime 'replaceChild\(script,previous\)' 'Runtime must replace the previous userCmd node before each execution.'
Require $runtime 'script\.id="userCmd"' 'Fresh script node must preserve the legacy userCmd id.'
Require $runtime 'if\(!LoadUserCommandScript\(path\)\)\s*return;\s*Run\(\);' 'Run must use the fresh user-script loader.'
Require $ids 'ID_TOOLS_REFRESH_SCRIPTS' 'Refresh Scripts command id is missing.'
Require $resource 'MENUITEM "Refresh &scripts",\s+ID_TOOLS_REFRESH_SCRIPTS' 'Refresh Scripts menu item is missing.'
Require $frameHeader 'COMMAND_ID_HANDLER\(ID_TOOLS_REFRESH_SCRIPTS, OnToolsRefreshScripts\)' 'Refresh Scripts command is not routed.'
Require $frame 'LRESULT CMainFrame::OnToolsRefreshScripts[\s\S]*?InitializeScripts\(\)' 'Refresh Scripts must rebuild the script-owned UI.'
Require $frame 'RefreshNavigationScriptTree\(\)' 'Script initialization must refresh the navigation tree.'
Require $scripts 'm_registryLoaded' 'Catalogue refresh must retain the in-process script identity registry.'
Require $registry 'std::vector<bool> claimed' 'Script registry must not merge equal-content scripts during discovery.'
Require $registry '!claimed\[i\] && m_identities\[i\]\.relativePath == script\.relativePath' 'Script registry must prefer an unclaimed matching path.'
Require $locale 'fbe\.menu\.idr_mainframe\.tools\.refresh_scripts' 'Refresh Scripts localization entry is missing.'
Require $scenario 'script-live-reload-runtime' 'Runtime reload scenario is missing.'
Require $scenario 'script-catalog-refresh-runtime' 'Runtime catalogue-refresh scenario is missing.'
Require $scenario 'version-1[\s\S]*?version-2' 'Runtime scenario must overwrite and execute one script path twice.'
Require $runner 'script-live-reload-runtime' 'Runtime runner does not execute the live-reload scenario.'
Require $runner 'script-catalog-refresh-runtime' 'Runtime runner does not execute the catalogue-refresh scenario.'
Require $runtimeRunner 'script-live-reload-runtime' 'Isolated runtime runner does not execute the live-reload scenario.'
Require $runtimeRunner 'script-catalog-refresh-runtime' 'Isolated runtime runner does not execute the catalogue-refresh scenario.'
Write-Host 'Script reload and catalogue refresh contract passed.'
