$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
foreach($feature in @('FEATURE_BROWSER_EMULATION','FEATURE_DOCUMENT_COMPATIBLE_MODE','FEATURE_LOCALMACHINE_LOCKDOWN','FEATURE_BLOCK_LMZ_SCRIPT','FEATURE_RESTRICT_ACTIVEXINSTALL','FEATURE_ZONE_ELEVATION')) { if($source.IndexOf($feature, [StringComparison]::Ordinal) -lt 0) { throw "FeatureControl setting is missing: $feature" } }
foreach($required in @('HKEY_CURRENT_USER','HKEY_LOCAL_MACHINE','KEY_WOW64_32KEY','KEY_WOW64_64KEY','KEY_QUERY_VALUE','RegOpenKeyEx','RegQueryValueEx','E022')) { if($source.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "FeatureControl trace contract is missing: $required" } }
Write-Host 'FeatureControl diagnostic contract passed.'
