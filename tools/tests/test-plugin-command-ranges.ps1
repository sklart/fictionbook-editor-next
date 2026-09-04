[CmdletBinding()] param([string]$ResourcePath = (Join-Path $PSScriptRoot '..\..\src\fbe\resource.h'))
$text = Get-Content -LiteralPath $ResourcePath -Raw
foreach ($name in 'ID_PLUGIN_IMPORT_FIRST','ID_PLUGIN_IMPORT_LAST','ID_PLUGIN_EXPORT_FIRST','ID_PLUGIN_EXPORT_LAST') { if ($text -notmatch "#define\s+$name\s+") { throw "Missing $name" } }
if ($text -notmatch 'ID_PLUGIN_EXPORT_LAST\s+30327' -or $text -notmatch 'ID_PLUGIN_IMPORT_LAST\s+30127') { throw 'Plugin ranges must reserve at least 128 commands.' }
Write-Host 'Plugin command ranges passed.'
