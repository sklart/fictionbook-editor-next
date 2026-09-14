param([string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference = 'Stop'
function Text($path) { Get-Content -Raw -LiteralPath (Join-Path $RepoRoot $path) }
function Must($text, $pattern, $name) { if($text -notmatch $pattern) { throw "Missing toolbar v2 contract: $name" } }

$layout = Text 'src\fbe\toolbars\PortableToolbarLayout.h'
$collection = Text 'src\fbe\toolbars\ScriptToolbarCollection.cpp'
$codec = Text 'src\fbe\toolbars\ToolbarsV2Codec.cpp'
$store = Text 'src\fbe\toolbars\PortableToolbarStore.cpp'
$frame = Text 'src\fbe\mainfrm.cpp'

Must $layout 'std::vector<ScriptToolbarDefinition> scriptToolbars' 'unbounded persistent script-toolbar collection'
Must $layout 'CString scriptUid' 'UID toolbar item identity'
Must $collection 'scripts-main' 'stable primary toolbar ID'
Must $collection 'toolbar-%u' 'stable generated toolbar IDs'
Must $collection 'if\(id == L"scripts-main"\) return false' 'primary toolbar is protected from deletion'
Must $codec '<Toolbars version=\\"2\\">' 'v2 root format'
Must $codec 'kind=\\"scripts\\"' 'scripts toolbar kind'
Must $codec 'Script uid=' 'UID script persistence'
Must $codec 'visible=' 'persisted panel visibility'
Must $codec 'scriptToolbars\.push_back' 'multiple panels parser'
Must $store 'ToolbarsV2Codec::Parse' 'v2 read integration'
Must $store 'ToolbarsV2Codec::Serialize' 'v2 atomic write integration'
Must $frame 'item\.scriptUid' 'runtime restore keyed by UID'
Write-Host 'Script toolbars v2 contract passed.'
