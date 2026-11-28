<# Verify actual NSIS embedded core assets against the staged input, without installing. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Installer,
    [Parameter(Mandatory)][string]$InputDirectory,
    [string]$SevenZip = (Join-Path $env:ProgramFiles '7-Zip/7z.exe')
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$inputPath = (Resolve-Path -LiteralPath $InputDirectory).Path
if (-not (Test-Path -LiteralPath $SevenZip -PathType Leaf)) { throw "7-Zip is required to inspect the installer: $SevenZip" }
$destination = Join-Path $root ('out/tests/installer-core-payload-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $destination | Out-Null
& $SevenZip x $installerPath "-o$destination" -y | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Installer extraction failed: $destination" }
$files = @('languages.txt', 'root_genres.xml', 'genres.librusec.txt', 'genres.rus.librusec.txt', 'Dict/sources.json',
    'Plugins/ImportFB3.dll', 'Plugins/ExportFB3.dll', 'ImportFB3Batch.exe', 'ExportFB3Batch.exe',
    'FB3Schemas/fb3_body.xsd', 'FB3Schemas/fb3_descr.xsd', 'FB3Schemas/fb3_descr_classes.xsd',
    'FB3Schemas/fb3_general.xsd', 'FB3Schemas/fb3_links.xsd', 'FB3Schemas/fb3_relations.xsd', 'FB3Schemas/license.txt')
if (Test-Path -LiteralPath (Join-Path $destination 'DescriptionIcons')) { throw 'Installer contains unused DescriptionIcons.' }
foreach ($relative in $files) {
    $expected = Join-Path $inputPath $relative; $actual = Join-Path $destination $relative
    if (-not (Test-Path -LiteralPath $expected -PathType Leaf)) { throw "Missing input $relative" }
    if (-not (Test-Path -LiteralPath $actual -PathType Leaf)) { throw "Missing installer asset $relative; extracted: $destination" }
    if ((Get-FileHash -LiteralPath $expected).Hash -ne (Get-FileHash -LiteralPath $actual).Hash) { throw "Installer asset differs: $relative" }
}
Write-Host "Installer embedded core assets match input: $($files.Count); extracted: $destination"
