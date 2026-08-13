$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$document = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBDoc.cpp')
$catalog = Get-Content -Raw -LiteralPath (Join-Path $root 'localization\app-ui\catalog.json') | ConvertFrom-Json
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
function Assert-StructurallyInvalidFbd([string]$Fixture, [string]$Reason) {
  $path = Join-Path $fixtures $Fixture
  if(-not (Test-Path -LiteralPath $path)) { throw "Missing structural-invalid FBD fixture: $Fixture" }
  [xml]$xml = Get-Content -Raw -LiteralPath $path
  $root = $xml.DocumentElement
  $descriptionCount = @($root.ChildNodes | Where-Object {
    $_.NodeType -eq [System.Xml.XmlNodeType]::Element -and $_.LocalName -eq 'description' -and $_.NamespaceURI -eq 'http://www.gribuser.ru/xml/fictionbook/2.0'
  }).Count
  $valid = $root -and $root.LocalName -eq 'FictionBook' -and
    $root.NamespaceURI -eq 'http://www.gribuser.ru/xml/fictionbook/2.0' -and $descriptionCount -eq 1
  if($valid) { throw "Structural-invalid FBD fixture accepted: $Fixture ($Reason)" }
}
Assert-StructurallyInvalidFbd 'wrong_root.fbd' 'wrong root'
Assert-StructurallyInvalidFbd 'wrong_namespace.fbd' 'wrong namespace'
Assert-StructurallyInvalidFbd 'missing_description.fbd' 'missing description'
Assert-StructurallyInvalidFbd 'duplicate_description.fbd' 'duplicate description'
if($mainFrame -notmatch '\*\.fb2;\*\.fbd') { throw 'Open dialog does not expose both FictionBook extensions.' }
if($mainFrame -notmatch 'FictionBook Description \(\*\.fbd\)') { throw 'Save As does not expose the separate FBD type.' }
if($mainFrame -notmatch 'dlg\.m_ofn\.nFilterIndex') { throw 'Save As filter selection does not control the target type.' }
if($xsl -notmatch 'class="body" fbdsynthetic="1"') { throw 'Body-less FBD visual placeholder is not marked synthetic.' }
if($mainFrame -notmatch 'if \(IsSourceActive\(\)\)\s*fv=m_doc->SetXMLAndValidate') { throw 'F8 source mode must validate current Scintilla text for FBD and FB2.' }
if($mainFrame -match 'IsFbdFile\(m_doc->m_filename\)\s*\)\s*fv=m_doc->Validate') { throw 'F8 source mode must not validate serialized DOM for FBD.' }
if($mainFrame -notmatch 'TextToXML[\s\S]{0,800}IsFbdFile\(m_doc->m_filename\)') { throw 'Source to Body must not fall back to XmlFromText after FBD structural validation fails.' }
if($document -notmatch 'ConfigureFictionBookSaxReader\(rdr, targetType, scol\)' -or
	$document -notmatch 'ConfigureFictionBookSaxReader\(rdr, fileType, scol\)') { throw 'XML validation policy is not shared by SaveToFile, source validation and TextToXML.' }
if($document -notmatch 'ShouldUseFb2SchemaValidation' -or $document -notmatch 'type != FictionBookFileType::Fbd') { throw 'FBD must disable only FB2 schema validation.' }
if($document -notmatch 'ValidateFbdDocumentStructure') { throw 'FBD structural validator is missing.' }
if($document -notmatch 'rootName\.Compare\(L"FictionBook"\)' -or $document -notmatch 'root->namespaceURI' -or $document -notmatch 'descriptions != 1') { throw 'FBD structural validator must check root, namespace and exactly one description.' }
if(@([regex]::Matches($document, 'ValidateFbdDocumentStructure\(')).Count -lt 4) { throw 'FBD structural validator must be used by SaveToFile, SetXMLAndValidate and TextToXML.' }
if($document -notmatch '!fValidateOnly \|\| fileType == FictionBookFileType::Fbd') { throw 'FBD F8 validation must construct a DOM for structural validation.' }
if(@([regex]::Matches($document, 'StartupTrace::\w+\(L"document", L"D222", L"book save completed"\)')).Count -ne 1) { throw 'D222 must remain reserved for the completed book save event.' }
if($document -match 'D222", L"FBD structural validation failed') { throw 'FBD structural validation must not reuse D222.' }
if(@([regex]::Matches($document, 'StartupTrace::Warning\(L"document", L"D227", L"FBD structural validation failed"\)')).Count -ne 1) { throw 'D227 must uniquely identify FBD structural validation failure.' }
foreach($key in 'fbe.validation.fbd.missing_root', 'fbe.validation.fbd.wrong_root', 'fbe.validation.fbd.wrong_namespace', 'fbe.validation.fbd.missing_description', 'fbe.validation.fbd.duplicate_description') {
  $entry = $catalog.seedStrings.PSObject.Properties[$key].Value
  if($null -eq $entry -or [string]::IsNullOrWhiteSpace([string]$entry.translations.'en-US')) { throw "Missing runtime localization for FBD structural error: $key" }
}
if($catalog.seedStrings.'fbe.status.fbd.validation_not_applicable'.translations.'en-US' -notmatch 'structure is valid') { throw 'Successful FBD status must report structural validation.' }
if($document -notmatch 'HasMeaningfulFb2BodyContent' -or $document -notmatch 'child->nodeType == NODE_ELEMENT') { throw 'FB2 body normalization must ignore whitespace-only nodes.' }
if($document -notmatch 'descendants->length != 4') { throw 'Synthetic FBD body must be classified structurally.' }
if($document -notmatch 'fbdsynthetic", 0') { throw 'Modified synthetic body must be promoted before serialization.' }
if($installer -notmatch 'PreviousProgId' -or $installer -notmatch 'fbd_uninstall_remove_owned') { throw 'Installer does not preserve and restore a prior FBD association.' }
if($installer -notmatch 'FictionBook\.Description') { throw 'Installer does not define a distinct FBD ProgID.' }
if($installer -match 'FictionBook\.Description\\shell\\Validate') { throw 'FBD must not receive the FB2 Validate shell verb.' }
if(-not (Test-Path -LiteralPath $checklist)) { throw 'Missing FBD manual integration checklist.' }
$manual = Get-Content -Raw -LiteralPath $checklist
foreach($scenario in 'body-less FBD', 'FBD to FB2', 'FB2 to FBD', 'F8', 'association', 'Source', 'inline image', 'empty_body.fbd', 'wrong_root.fbd', 'wrong_namespace.fbd', 'missing_description.fbd', 'duplicate_description.fbd', 'Batch `-b`') { if($manual -notmatch [regex]::Escape($scenario)) { throw "Manual checklist misses scenario: $scenario" } }
Write-Host 'FBD support contract passed.'
