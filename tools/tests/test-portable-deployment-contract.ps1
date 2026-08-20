<# Verifies the source-level portable isolation contract without requiring a GUI session. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Text([string]$path) { Get-Content -LiteralPath (Join-Path $root $path) -Raw }
function Has([string]$text, [string]$needle, [string]$what) { if ($text.IndexOf($needle, [StringComparison]::Ordinal) -lt 0) { throw "${what}: '$needle' не найдено." } }
function Lacks([string]$text, [string]$needle, [string]$what) { if ($text.IndexOf($needle, [StringComparison]::Ordinal) -ge 0) { throw "${what}: найдено запрещённое '$needle'." } }

$context = Text 'src\common\DeploymentContext.h'
foreach ($item in @('enum class Mode', 'portable.ini', '--portable', '--installed', 'HasInvalidModeOverride', 'CommandLineToArgvW', 'DataPath', 'find_first_of', 'SettingsDirectory', 'SHGetFolderPathW', 'DiagnosticsDirectory', 'RecoveryDirectory', 'RegistryPersistenceAllowed')) { Has $context $item 'DeploymentContext' }
$settings = Text 'src\fbe\Settings.cpp'; Has $settings 'RegistryPersistenceAllowed()' 'Portable settings'; Has $settings 'm_key.Create(HKEY_CURRENT_USER' 'Installed settings compatibility'
$locale = Text 'src\fbe\RuntimeLocalization.cpp'; Has $locale 'DeploymentContext::SettingsDirectory()' 'Portable locale'; Has $locale 'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH' 'Atomic portable locale'
$trace = Text 'src\fbe\StartupTrace.cpp'; Has $trace 'DeploymentContext::DiagnosticsDirectory()' 'Portable diagnostics'; Has $trace 'RegistryPersistenceAllowed()' 'Portable trace preference'
$frame = Text 'src\fbe\mainfrm.cpp'; Has $frame 'ReadPortableMru' 'Portable MRU read'; Has $frame 'WritePortableMru' 'Portable MRU write'; Has $frame 'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH' 'Atomic portable MRU'; Has $frame 'RegistryPersistenceAllowed()' 'Registry-free portable MRU'; Has $frame 'm_CmdToolbar.RestoreState' 'Toolbar layout restore'; Has $frame 'm_CmdToolbar.SaveState' 'Toolbar layout save'; Has $frame 'm_ScriptsToolbar.RestoreState' 'Scripts toolbar layout restore'; Has $frame 'm_ScriptsToolbar.SaveState' 'Scripts toolbar layout save'
Has $frame 'let a stale/foreign CLSID registration intercept' 'Bundled plugin local-first activation'
Has $frame 'BundledPluginCatalog' 'Plugin manifest catalog'; Has $frame 'Plugins\\plugins.json' 'Plugin manifest location'; Lacks $frame 'static const Entry entries[]' 'Hardcoded plugin catalog'
$entry = Text 'src\fbe\FBE.cpp'; Has $entry '--print-runtime-paths' 'Runtime paths CLI'; Has $entry 'HasInvalidModeOverride()' 'Conflicting mode override'; Has $entry 'cannot be used together' 'Conflicting mode diagnostic'; Has $entry 'EscapeJson' 'Runtime paths JSON escaping'
$portable = Text 'tools\build\package-portable.ps1'; Has $portable 'portable.ini' 'Portable package marker'; Has $portable 'Data\\$name' 'Portable data layout'; Lacks $portable 'FBShell' 'Portable packager'
Write-Host 'Portable deployment contract passed.'
