<# Guards intended dependencies of the toolbar subsystem. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Text([string]$path) { Get-Content -Raw -LiteralPath (Join-Path $root $path) }
function MustNot([string]$text, [string]$pattern, [string]$what) { if ($text -match $pattern) { throw "${what}: forbidden dependency '$pattern'." } }
MustNot (Text 'src\fbe\toolbars\PortableToolbarStore.cpp') '#include.*(mainfrm\.h|Settings\.h|scripts)' 'PortableToolbarStore'
MustNot (Text 'src\fbe\toolbars\ToolbarFactory.cpp') 'mainfrm\.h|CMainFrame' 'ToolbarFactory'
MustNot (Text 'src\fbe\toolbars\TableToolbarCommands.cpp') 'FBDoc|FBEView|MSHTML|CMainFrame' 'TableToolbarCommands'
Write-Host 'Toolbar architecture boundaries passed.'
