param([string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)

$ErrorActionPreference = 'Stop'
function Text([string]$relative) { Get-Content -Raw -LiteralPath (Join-Path $RepoRoot $relative) }
function Must([string]$text, [string]$pattern, [string]$description) { if($text -notmatch $pattern) { throw "Script identity contract missing: $description" } }

$registry = Text 'src\fbe\scripts\ScriptRegistry.cpp'
$catalog = Text 'src\fbe\scripts\ScriptCatalog.cpp'
$commands = Text 'src\fbe\scripts\ScriptCommandRegistry.cpp'
$menu = Text 'src\fbe\scripts\ScriptMenuBuilder.cpp'
$hotkeys = Text 'src\fbe\mainfrm.cpp'

Must $registry 'ScriptRegistry\.xml' 'persistent registry in SettingsDirectory'
Must $registry 'DeploymentContext::SettingsDirectory' 'portable-safe settings location'
Must $registry 'UuidCreate' 'new scripts receive GUID UIDs'
Must $registry 'relativePath == script\.relativePath' 'path match is preferred'
Must $registry 'fingerprint == fingerprint' 'fingerprint rename/move fallback'
Must $registry 'candidate != NULL' 'ambiguous duplicate fingerprints are not merged'
Must $registry 'MOVEFILE_REPLACE_EXISTING \| MOVEFILE_WRITE_THROUGH' 'atomic registry persistence'
Must $catalog 'registry->Resolve\(m_items\)' 'discovery resolves identities'
Must $commands 'Assign\(const CString& uid\)' 'runtime IDs are keyed by UID'
Must $menu 'MigrateLegacyPath\(script\.relativePath, script\.uid\)' 'legacy command IDs migrate without reset'
Must $hotkeys 'L"script:" \+ script\.uid' 'hotkeys use script UID identity'

Write-Host 'Script identity contract passed.'
