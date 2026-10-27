[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
 [ValidateSet('minimal','full','multiple','binary','missing-optional','unicode')]
 [string[]]$Fixtures=@('minimal','full','multiple','binary','missing-optional','unicode'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
function Get-SemanticXml([Xml.XmlNode]$Node) {
 $text=[Text.StringBuilder]::new()
 function Append-Node([Xml.XmlNode]$Current,[Text.StringBuilder]$Target) {
  if($null -eq $Current){return}
  if($Current.NodeType -in @([Xml.XmlNodeType]::Whitespace,[Xml.XmlNodeType]::SignificantWhitespace)){return}
  if($Current.NodeType -in @([Xml.XmlNodeType]::Text,[Xml.XmlNodeType]::CDATA)){$null=$Target.Append('T'+$Current.Value.Length+':'+$Current.Value);return}
  if($Current.NodeType -ne [Xml.XmlNodeType]::Element){return}
  $null=$Target.Append('E{'+$Current.NamespaceURI+'}'+$Current.LocalName+'[')
  foreach($attribute in @($Current.Attributes | Where-Object NamespaceURI -ne 'http://www.w3.org/2000/xmlns/' | Sort-Object NamespaceURI,LocalName)) {
   $null=$Target.Append('A{'+$attribute.NamespaceURI+'}'+$attribute.LocalName+'='+$attribute.Value.Length+':'+$attribute.Value)
  }
  foreach($child in $Current.ChildNodes){Append-Node $child $Target}
  $null=$Target.Append(']')
 }
 Append-Node $Node $text
 return $text.ToString()
}
$oldTestMode=[Environment]::GetEnvironmentVariable('FBE_NEXT_TEST_MODE','Process')
$oldScenario=[Environment]::GetEnvironmentVariable('FBE_NEXT_TEST_SCENARIO','Process')
try {
$env:FBE_NEXT_TEST_MODE='1'
$env:FBE_NEXT_TEST_SCENARIO='binary-roundtrip'
foreach($layout in @('modern','classic')) {
 $iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name "description-roundtrip-$layout";$passed=$false
 try {
  $settings=New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'ProbeScripts')
  [xml]$seed=Get-Content $settings -Raw;$node=$seed.CreateElement('DescriptionLayout');$node.InnerText=$layout;$null=$seed.FBE.Settings.AppendChild($node);$seed.Save($settings)
  $books=Join-Path $iso.Root 'books';New-Item -ItemType Directory -Path $books|Out-Null
  foreach($fixture in $Fixtures) {
   # A normal book whose name contains blank.fb2 must not become a New template.
   $fileName=if($fixture -eq 'minimal'){'blank.fb2'}else{"$fixture.fb2"}
   $path=Join-Path $books $fileName;Copy-Item (Join-Path $PSScriptRoot "fixtures/description/$fixture.fb2") $path
   [xml]$before=Get-Content $path -Raw
   $expected=Get-SemanticXml $before.DocumentElement.description
   foreach($iteration in @(1,2)) {
    $report=Join-Path $iso.Root "$fixture-$iteration.txt"
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-s','-b',$report,$path) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario description-roundtrip -Report $report -TimeoutSeconds 45
    if($code -ne 0 -or -not(Test-Path $report) -or -not (Select-String -LiteralPath $report -Pattern '^save-complete\s' -Quiet)){throw "Production Save failed: $layout / $fixture"}
    [xml]$after=Get-Content $path -Raw
    if((Get-SemanticXml $after.DocumentElement.description) -cne $expected){throw "Description metadata changed: $layout / $fixture / save $iteration"}
    if((Get-SemanticXml $after.DocumentElement.SelectSingleNode('*[local-name()="stylesheet"]')) -cne (Get-SemanticXml $before.DocumentElement.SelectSingleNode('*[local-name()="stylesheet"]'))){throw "Stylesheet changed: $fixture"}
    $beforeBinaries=@($before.DocumentElement.ChildNodes | Where-Object LocalName -eq 'binary');$afterBinaries=@($after.DocumentElement.ChildNodes | Where-Object LocalName -eq 'binary')
    if($beforeBinaries.Count -ne $afterBinaries.Count){throw 'Binary count changed.'}
    for($i=0;$i -lt $beforeBinaries.Count;$i++) {
     if($beforeBinaries[$i].id -cne $afterBinaries[$i].id -or $beforeBinaries[$i].GetAttribute('content-type') -cne $afterBinaries[$i].GetAttribute('content-type') -or
      [Convert]::ToBase64String([Convert]::FromBase64String($beforeBinaries[$i].InnerText)) -cne [Convert]::ToBase64String([Convert]::FromBase64String($afterBinaries[$i].InnerText))){throw "Binary payload/metadata changed: $fixture"}
    }
   }
  }
  $passed=$true;Write-Host "Description $layout production Save -> reopen -> Save PASS ($($Fixtures.Count) fixtures)."
 }finally{Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
}
} finally {
 [Environment]::SetEnvironmentVariable('FBE_NEXT_TEST_MODE',$oldTestMode,'Process')
 [Environment]::SetEnvironmentVariable('FBE_NEXT_TEST_SCENARIO',$oldScenario,'Process')
}
