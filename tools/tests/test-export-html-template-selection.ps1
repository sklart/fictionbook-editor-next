<# Exercises the path-migration decision matrix used by TemplateResolver.h. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$resolver = Get-Content -Raw -LiteralPath (Join-Path $root 'src\export-html\TemplateResolver.h')
foreach ($contract in @('UseCustomTemplate', 'ExportHTML.dll', 'FBE.exe', 'DeleteValue(L"Template")')) {
    if ($resolver.IndexOf($contract, [StringComparison]::Ordinal) -lt 0) { throw "Template resolver lacks migration contract: $contract" }
}
$temp = Join-Path $root 'out\tests\export-html-template-selection'
Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $temp | Out-Null
$bundled = Join-Path $temp 'Current\html.xsl'; New-Item -ItemType Directory -Path (Split-Path $bundled) -Force | Out-Null; Set-Content -LiteralPath $bundled -Value '<xsl:stylesheet/>'
function Resolve-Model([string]$Stored, [bool]$HasFlag, [bool]$CustomFlag) {
    $exists = -not [string]::IsNullOrWhiteSpace($Stored) -and (Test-Path -LiteralPath $Stored)
    $legacy = $Stored -ieq $bundled -or (($Stored -match '(?i)html\.xsl$') -and ((Test-Path (Join-Path (Split-Path $Stored) 'ExportHTML.dll')) -or (Test-Path (Join-Path (Split-Path $Stored) 'FBE.exe'))))
    if (($HasFlag -and $CustomFlag -and $exists) -or (-not $HasFlag -and $exists -and -not $legacy)) { return @{ Path=$Stored; Custom=$true } }
    return @{ Path=$bundled; Custom=$false }
}
if ((Resolve-Model '' $false $false).Custom) { throw 'Clean configuration must use bundled XSL.' }
if ((Resolve-Model $bundled $false $false).Custom) { throw 'Current bundled XSL must not be custom.' }
if ((Resolve-Model (Join-Path $temp 'missing.xsl') $false $false).Custom) { throw 'Missing template must fall back.' }
$old = Join-Path $temp 'OldFBE\html.xsl'; New-Item -ItemType Directory -Path (Split-Path $old) -Force | Out-Null; Set-Content -LiteralPath $old -Value ''; Set-Content -LiteralPath (Join-Path (Split-Path $old) 'ExportHTML.dll') -Value ''
if ((Resolve-Model $old $false $false).Custom) { throw 'Old bundled XSL must migrate.' }
$custom = Join-Path $temp 'User\custom.xsl'; New-Item -ItemType Directory -Path (Split-Path $custom) -Force | Out-Null; Set-Content -LiteralPath $custom -Value ''
if (-not (Resolve-Model $custom $false $false).Custom) { throw 'User XSL must be retained.' }
$plugin = Get-Content -Raw -LiteralPath (Join-Path $root 'src\export-html\ExportHTMLPlugin.cpp')
if ($plugin.IndexOf("namespace-uri()='http://www.w3.org/1999/XSL/Transform'", [StringComparison]::Ordinal) -lt 0) { throw 'Custom XSL embedimages compatibility check is missing.' }
$guard = $plugin.IndexOf('fEmbeddedImages && dlg.m_usingCustomTemplate && !SupportsEmbeddedImages', [StringComparison]::Ordinal)
$createFile = $plugin.IndexOf('CreateFile(dlg.m_szFileName', [StringComparison]::Ordinal)
if ($guard -lt 0 -or $guard -gt $createFile) { throw 'Incompatible custom XSL must be rejected before the output file is created.' }
Write-Host 'ExportHTML template selection regression passed.'
