[CmdletBinding()] param([string]$ResourcePath = (Join-Path $PSScriptRoot '..\..\src\fbe\resource.h'))
$text = Get-Content -LiteralPath $ResourcePath -Raw; $values = @{}
foreach ($match in [regex]::Matches($text, '(?m)^#define\s+(ID_[A-Z0-9_]+)\s+([0-9]+)\s*$')) { $values[$match.Groups[1].Value] = [int]$match.Groups[2].Value }
function Id([string]$name) { if (-not $values.ContainsKey($name)) { throw "Missing numeric $name" }; return $values[$name] }
$importFirst = Id 'ID_PLUGIN_IMPORT_FIRST'; $importLast = Id 'ID_PLUGIN_IMPORT_LAST'; $exportFirst = Id 'ID_PLUGIN_EXPORT_FIRST'; $exportLast = Id 'ID_PLUGIN_EXPORT_LAST'
foreach ($range in @(@($importFirst,$importLast,'Import'), @($exportFirst,$exportLast,'Export'))) { if ($range[0] -gt $range[1]) { throw "$($range[2]) FIRST exceeds LAST" }; if (($range[1] - $range[0] + 1) -lt 128) { throw "$($range[2]) range is smaller than 128" }; if ($range[0] -lt 0 -or $range[1] -ge 65536) { throw "$($range[2]) range is outside WM_COMMAND" } }
if ($importLast -ge $exportFirst) { throw 'Import and Export ranges overlap.' }
$scriptFirst = Id 'ID_SCRIPT_BASE'; $scriptLast = $scriptFirst + 999; $spellFirst = Id 'ID_SPELL_REPLACE_FIRST'; $spellLast = Id 'ID_SPELL_REPLACE_LAST'
foreach ($range in @(@($scriptFirst,$scriptLast,'script'), @($spellFirst,$spellLast,'spell'))) { if (-not ($exportLast -lt $range[0] -or $importFirst -gt $range[1])) { throw "Plugin ranges overlap $($range[2]) range" } }
foreach ($entry in $values.GetEnumerator()) { if ($entry.Key -match '^ID_PLUGIN_' -or $entry.Key -in 'ID_SPELL_REPLACE_FIRST','ID_SPELL_REPLACE_LAST','ID_SCRIPT_BASE') { continue }; if (($entry.Value -ge $importFirst -and $entry.Value -le $exportLast)) { throw "Regular command $($entry.Key) overlaps plugin ranges" } }
Write-Host 'Plugin command ranges passed.'
