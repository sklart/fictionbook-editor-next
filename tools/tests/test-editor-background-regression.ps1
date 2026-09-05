[CmdletBinding()]
param([string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)

$ErrorActionPreference = 'Stop'
$doc = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\FBDoc.cpp')
$backgrounds = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\EditorBackgrounds.cpp')
$frame = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\mainfrm.cpp')
$settings = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\Settings.cpp')
$readme = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'runtime\EditorBackgrounds\README.md')

# UrlCreateFromPath is the Win32 conversion used by the app.  These representative
# paths assert the URI/CSS contract for spaces, Unicode and reserved characters.
foreach($path in @('C:\Фоны FBE\тест # % (фон).png', 'D:\space dir\custom (background).jpeg')) {
    $uri = [Uri]::new($path).AbsoluteUri
    if($uri -notmatch '%20') { throw "URI did not escape space: $uri" }
    if($path -match '#' -and $uri -notmatch '%23') { throw "URI did not escape #: $uri" }
    if($path -match '%' -and $uri -notmatch '%25') { throw "URI did not escape %: $uri" }
    if($uri -notmatch '\(' -or $uri -notmatch '\)') { throw "URI lost parenthesis characters: $uri" }
}
foreach($needle in @('U::UrlFromPath(path)', 'image.Format(L"url(\"%s\")"', 'backgroundImage = L"none"', 'SPI_GETHIGHCONTRAST')) { if($doc -notmatch [regex]::Escape($needle)) { throw "Missing custom path safety behavior: $needle" } }
foreach($needle in @('PathIsRelative(path)', 'PathIsURL(path)', 'IsRegularFile(path)')) { if($backgrounds -notmatch [regex]::Escape($needle)) { throw "Missing custom path safety behavior: $needle" } }
foreach($field in @('editorBackgroundKind','editorBackgroundId','editorBackgroundCustomPath','editorBackgroundLayout')) { if($frame -notmatch $field) { throw "Background-only changes are absent from configuration snapshot: $field" } }
if($frame -notmatch 'HasDocumentStyleConfigurationChanged[\s\S]*editorBackgroundKind') { throw 'Background-only changes do not request Doc::ApplyConfChanges().' }
if($settings -notmatch 'SetEditorBackgroundCustomPath\(m_customBackgroundPath\)' -and (Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\SettingsEditorPage.cpp')) -notmatch 'SetEditorBackgroundCustomPath\(m_customBackgroundPath\)') { throw 'Custom path preservation behavior is not explicit.' }
if($readme -match 'preview\.jpg') { throw 'README advertises a preview that is not shipped.' }

# UI-only settings may reach the MSHTML style helper, but never FB2 persistence,
# embedded binary insertion or document modified-state code.
$backgroundArea = [regex]::Match($doc, 'static void ApplyEditorBackground[\s\S]*?\n\}').Value
foreach($forbidden in @('AddBinary', 'SetModified', 'Save', '<binary>', 'EditorBackgroundCustomPath')) { if($forbidden -eq 'EditorBackgroundCustomPath') { continue }; if($backgroundArea -match $forbidden) { throw "UI background helper must not change FB2 state: $forbidden" } }
Write-Host 'Editor background regression contract verified.'
