$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$document = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBDoc.cpp')
$xsl = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\fb2.xsl')
$installer = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\nsis\Installer\MakeInstaller.nsi')
$fixtures = Join-Path $root 'tools\tests\fixtures\fbd'
$checklist = Join-Path $root 'tools\tests\fbd-manual-checklist.md'
$fixtureNames = 'description_only.fbd', 'empty_body.fbd', 'with_cover.fbd', 'unicode_metadata.fbd'
foreach($fixture in $fixtureNames) {
  $path = Join-Path $fixtures $fixture
  if(-not (Test-Path -LiteralPath $path)) { throw "Missing FBD fixture: $fixture" }
  [xml]$xml = Get-Content -Raw -LiteralPath $path
  if($xml.DocumentElement.LocalName -ne 'FictionBook') { throw "Invalid FBD root: $fixture" }
  if($xml.DocumentElement.NamespaceURI -ne 'http://www.gribuser.ru/xml/fictionbook/2.0') { throw "Invalid FBD namespace: $fixture" }
  if($null -eq $xml.DocumentElement.description) { throw "Missing FBD description: $fixture" }
}
if((Get-Content -Raw -LiteralPath (Join-Path $fixtures 'description_only.fbd')) -match '<body') { throw 'description_only.fbd must remain body-less.' }
if((Get-Content -Raw -LiteralPath (Join-Path $fixtures 'empty_body.fbd')) -notmatch '<body\s*/>') { throw 'empty_body.fbd must contain an empty body.' }
if((Get-Content -Raw -LiteralPath (Join-Path $fixtures 'with_cover.fbd')) -notmatch 'cover\.jpg') { throw 'with_cover.fbd must retain a cover binary reference.' }
if((Get-Content -Raw -LiteralPath (Join-Path $fixtures 'unicode_metadata.fbd')) -notmatch 'Zoë|déjà vu') { throw 'unicode_metadata.fbd must retain Unicode metadata.' }
$invalidFixture = Join-Path $fixtures 'invalid_xml.fbd'
if(-not (Test-Path -LiteralPath $invalidFixture)) { throw 'Missing malformed FBD fixture.' }
$invalidParsed = $true
try { [xml](Get-Content -Raw -LiteralPath $invalidFixture) | Out-Null } catch { $invalidParsed = $false }
if($invalidParsed) { throw 'invalid_xml.fbd must not parse.' }
if($mainFrame -notmatch '\*\.fb2;\*\.fbd') { throw 'Open dialog does not expose both FictionBook extensions.' }
if($mainFrame -notmatch 'FictionBook Description \(\*\.fbd\)') { throw 'Save As does not expose the separate FBD type.' }
if($mainFrame -notmatch 'dlg\.m_ofn\.nFilterIndex') { throw 'Save As filter selection does not control the target type.' }
if($xsl -notmatch 'class="body" fbdsynthetic="1"') { throw 'Body-less FBD visual placeholder is not marked synthetic.' }
if($mainFrame -notmatch 'if \(IsSourceActive\(\)\)\s*fv=m_doc->SetXMLAndValidate') { throw 'F8 source mode must validate current Scintilla text for FBD and FB2.' }
if($mainFrame -match 'IsFbdFile\(m_doc->m_filename\)\s*\)\s*fv=m_doc->Validate') { throw 'F8 source mode must not validate serialized DOM for FBD.' }
if($document -notmatch 'ConfigureFictionBookSaxReader\(rdr, targetType, scol\)' -or
	$document -notmatch 'ConfigureFictionBookSaxReader\(rdr, fileType, scol\)') { throw 'XML validation policy is not shared by SaveToFile, source validation and TextToXML.' }
if($document -notmatch 'ShouldUseFb2SchemaValidation' -or $document -notmatch 'type != FictionBookFileType::Fbd') { throw 'FBD must disable only FB2 schema validation.' }
if($document -notmatch 'HasMeaningfulFb2BodyContent' -or $document -notmatch 'child->nodeType == NODE_ELEMENT') { throw 'FB2 body normalization must ignore whitespace-only nodes.' }
if($document -notmatch 'descendants->length != 4') { throw 'Synthetic FBD body must be classified structurally.' }
if($document -notmatch 'fbdsynthetic", 0') { throw 'Modified synthetic body must be promoted before serialization.' }
if($installer -notmatch 'PreviousProgId' -or $installer -notmatch 'fbd_uninstall_remove_owned') { throw 'Installer does not preserve and restore a prior FBD association.' }
if($installer -notmatch 'FictionBook\.Description') { throw 'Installer does not define a distinct FBD ProgID.' }
if($installer -match 'FictionBook\.Description\\shell\\Validate') { throw 'FBD must not receive the FB2 Validate shell verb.' }
if(-not (Test-Path -LiteralPath $checklist)) { throw 'Missing FBD manual integration checklist.' }
$manual = Get-Content -Raw -LiteralPath $checklist
foreach($scenario in 'body-less FBD', 'FBD to FB2', 'FB2 to FBD', 'F8', 'association', 'Source', 'inline image', 'empty_body.fbd') { if($manual -notmatch [regex]::Escape($scenario)) { throw "Manual checklist misses scenario: $scenario" } }
Write-Host 'FBD support contract passed.'
