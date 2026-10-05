<# Ensures a settings-requested restart follows a successful normal close. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$frame = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.h')
$resource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.rc')
$ids = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\resource.h')
$locale = Get-Content -Raw -LiteralPath (Join-Path $root 'localization\app-ui\fbe-idr-mainframe-menu.json')

function Require([string]$pattern, [string]$description)
{
    if($frame -notmatch $pattern) { throw "Restart contract missing: $description" }
}

Require 'void\s+CMainFrame::RestartProgram\s*\(\)\s*\{[\s\S]*?const HWND frame = m_hWnd;' 'restart retains the old HWND for close-result detection'
if($header -notmatch 'COMMAND_ID_HANDLER\(ID_FILE_RESTART, OnFileRestart\)' -or $header -notmatch 'OnFileRestart[^\r\n]*RestartProgram\(\)') { throw 'Restart command is not routed through the standard file menu.' }
if($resource -notmatch 'MENUITEM "&Restart FBE",\s+ID_FILE_RESTART' -or $ids -notmatch 'ID_FILE_RESTART') { throw 'Restart command resource is missing.' }
if($locale -notmatch 'fbe\.menu\.idr_mainframe\.file\.restart') { throw 'Restart command localization is missing.' }
Require 'const CString filename = U::GetModulePath\(_Module.GetModuleInstance\(\)\);[\s\S]*?arguments\.Format' 'restart captures executable and document arguments before close destroys state'
Require 'OnClose\(0, 0, 0, handled\);[\s\S]*?if\(!::IsWindow\(frame\)\)[\s\S]*?ShellExecute' 'restart launches only after the normal close actually destroys the frame'
if($frame -match 'if\s*\(\s*OnClose\(0, 0, 0,') { throw 'Restart must not interpret OnClose LRESULT as a boolean success value.' }

Write-Host 'FBE restart-after-settings contract passed.'
