$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$xsl = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\fb2.xsl')
$installer = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\nsis\Installer\MakeInstaller.nsi')
$fixtures = Join-Path $root 'tools\tests\fixtures\fbd'
foreach($fixture in 'description_only.fbd', 'empty_body.fbd', 'with_cover.fbd', 'unicode_metadata.fbd') { if(-not (Test-Path -LiteralPath (Join-Path $fixtures $fixture))) { throw "Missing FBD fixture: $fixture" } }
if($mainFrame -notmatch '\*\.fb2;\*\.fbd') { throw 'Open dialog does not expose both FictionBook extensions.' }
if($mainFrame -notmatch 'FictionBook Description \(\*\.fbd\)') { throw 'Save As does not expose the separate FBD type.' }
if($xsl -notmatch 'class="body" fbdsynthetic="1"') { throw 'Body-less FBD visual placeholder is not marked synthetic.' }
if($installer -notmatch 'FictionBook\.Description') { throw 'Installer does not define a distinct FBD ProgID.' }
if($installer -match 'FictionBook\.Description\\shell\\Validate') { throw 'FBD must not receive the FB2 Validate shell verb.' }
Write-Host 'FBD support contract passed.'
