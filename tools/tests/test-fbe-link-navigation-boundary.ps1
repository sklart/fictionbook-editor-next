<#
.SYNOPSIS
Verifies the pure URL, editor DOM and editor state link-navigation boundaries.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$pure = Get-Content -Raw (Join-Path $root 'src\fbe\LinkNavigation.h')
$dom = Get-Content -Raw (Join-Path $root 'src\fbe\navigation\LinkDomNavigation.cpp')
$view = Get-Content -Raw (Join-Path $root 'src\fbe\FBEview.cpp')

foreach($forbidden in @('#include <mshtml', 'IHTML', 'CFBEView', 'CMainFrame', 'mainfrm.h', 'FBEview.h')) {
    if($pure -match [regex]::Escape($forbidden)) { throw "Pure LinkNavigation зависит от $forbidden." }
}
foreach($forbidden in @('CFBEView', 'CMainFrame', 'mainfrm.h', 'FBEview.h', 'ShellExecute', 'Settings')) {
    if($dom -match [regex]::Escape($forbidden)) { throw "Link navigation module зависит от $forbidden." }
}
foreach($legacy in @('static MSHTML::IHTMLElementPtr FindNearestLinkElement', 'static MSHTML::IHTMLElementPtr GetEditableBody', 'static CString GetInternalLinkTargetId', 'static long GetLinkTargetOrdinal', 'm_link_navigation_target_id', 'm_link_navigation_origin_ordinal')) {
    if($view -match [regex]::Escape($legacy)) { throw "FBEview retains extracted link navigation: $legacy" }
}
foreach($required in @('FBELinkNavigation::FindNearestLinkElement', 'FBELinkNavigation::FindTargetElement', 'DecideLinkActivation', 'ShellExecuteW')) {
    if(-not $view.Contains($required)) { throw "FBEview no longer coordinates link navigation: $required" }
}
foreach($required in @('FindTargetElement')) {
    if(-not $dom.Contains($required)) { throw "LinkDomNavigation omits $required." }
}
foreach($required in @('ReturnToLinkNavigationOrigin', 'ClearLinkNavigationHistory', 'FindOriginLink', 'GetLinkUniqueNumber', 'm_link_navigation_state', 'originUniqueNumber')) {
    if($view -notmatch [regex]::Escape($required) -and $dom -notmatch [regex]::Escape($required)) { throw "Link-navigation return history is missing: $required" }
}
Write-Host 'Link navigation boundary passed.'
