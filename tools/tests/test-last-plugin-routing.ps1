[CmdletBinding()] param([string]$SourcePath = (Join-Path $PSScriptRoot '..\..\src\fbe\mainfrm.cpp'))
$source = Get-Content -LiteralPath $SourcePath -Raw
if ($source -notmatch 'm_last_plugin\s*=\s*wID\s*\+\s*ID_IMPORT_BASE') { throw 'Import must save an import command ID.' }
if ($source -notmatch 'm_last_plugin\s*=\s*wID\s*\+\s*ID_EXPORT_BASE') { throw 'Export must save an export command ID.' }
Write-Host 'Last Plugin routing passed.'
