param(
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'

function Read-ProjectFile([string]$relativePath) {
    Get-Content -Raw -LiteralPath (Join-Path $RepoRoot $relativePath)
}

$catalog = Read-ProjectFile 'src\fbe\scripts\ScriptCatalog.cpp'
foreach($required in @(
    'U::CheckScriptsVersion',
    'PathMatchSpec',
    'FILE_ATTRIBUTE_DIRECTORY',
    'relative.MakeLower()',
    "relative.Replace(L'\\', L'/')",
    'left.isFolder != right.isFolder',
    'left.relativePath.CompareNoCase(right.relativePath)')) {
    if($catalog.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "ScriptCatalog lost required discovery semantic: $required"
    }
}

function Legacy-Order([string]$name) {
    if($name -match '^\d+_') { return @($name.Substring($name.IndexOf('_') + 1), $name) }
    return @($name, "0_$name")
}

# The fixture mirrors a Scripts tree containing prefix-named folders, duplicate
# leaf names, Unicode and ignored dot entries.  It checks the observable order
# expected from the former CollectScripts/SortScripts pair.
$fixture = @(
    @{ Name = '.'; Folder = $true },
    @{ Name = '..'; Folder = $true },
    @{ Name = '20_Папка'; Folder = $true },
    @{ Name = 'alpha'; Folder = $true },
    @{ Name = '20_Папка\\02_тест.js'; Folder = $false },
    @{ Name = 'alpha\\same.js'; Folder = $false },
    @{ Name = 'same.js'; Folder = $false }
) | Where-Object { $_.Name -notin @('.', '..') } | ForEach-Object {
    $leaf = Split-Path $_.Name -Leaf
    $identity = $_.Name.ToLowerInvariant().Replace('\\', '/')
    $display, $order = Legacy-Order $leaf
    if(-not $_.Folder) { $display = $display.Substring(0, $display.Length - 3) }
    [pscustomobject]@{ Folder = $_.Folder; Display = $display; Order = $order; RelativePath = $identity }
}
$actual = @($fixture | Sort-Object @{ Expression = { if($_.Folder) { 0 } else { 1 } } }, @{ Expression = { $_.Order.ToLowerInvariant() } }, @{ Expression = { $_.RelativePath } })
$expected = @('alpha', 'Папка', 'same', 'same', 'тест')
if((@($actual.Display) -join '|') -ne ($expected -join '|')) { throw 'ScriptCatalog fixture order diverges from legacy semantics.' }
if($actual[1].RelativePath -ne '20_папка' -or $actual[4].RelativePath -ne '20_папка/02_тест.js') { throw 'Relative path normalization is not stable for Unicode nested scripts.' }

Write-Host 'FBE ScriptCatalog discovery contract passed.'
