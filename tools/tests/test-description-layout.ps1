[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$xsl = New-Object -ComObject Msxml2.FreeThreadedDOMDocument.6.0
$xsl.async = $false
$xsl.setProperty('ResolveExternals', $true)
if (-not $xsl.load((Join-Path $root 'runtime\fb2.xsl'))) { throw $xsl.parseError.reason }
$template = New-Object -ComObject Msxml2.XSLTemplate.6.0
$template.stylesheet = $xsl
$xml = New-Object -ComObject Msxml2.DOMDocument.6.0
$xml.async = $false
$null = $xml.loadXML('<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre match="90">prose</genre><author><first-name>Анна</first-name><last-name>李</last-name></author><book-title>Größe</book-title><lang>ru</lang></title-info><document-info><author><nickname>Test</nickname></author><id>description-test</id><version>1</version></document-info></description><body><section><p>Text</p></section></body></FictionBook>')
$outputs = @{}
foreach ($layout in @('modern', 'classic')) {
    $processor = $template.createProcessor()
    $processor.input = $xml
    $processor.setStartMode('description', '')
    $processor.addParameter('descriptionLayout', $layout, '')
    if (-not $processor.transform()) { throw "Transform failed: $layout" }
    $html = [string]$processor.output
    foreach ($id in @('tiTitle','tiAuthor','tiGenre','tiLang','tiCover','tiSeq','stiAuthor','diAuthor','diID','diVersion','piSeq','binobj','stylesheetId')) {
        if ($html -notmatch ('id="' + $id + '"')) { throw "Missing $id in $layout" }
    }
    foreach ($value in @('Анна','李','Größe','description-test')) {
        if (-not $html.Contains($value)) { throw "Lost value $value in $layout" }
    }
    if (-not $html.Contains('data-layout="' + $layout + '"')) { throw "Layout parameter ignored: $layout" }
    $outputs[$layout] = $html -replace 'data-layout="(modern|classic)"', 'data-layout="shared"'
}
if ($outputs.modern -cne $outputs.classic) { throw 'The layouts must share the field DOM and serialization contract.' }
Write-Host 'Description XSL / shared DOM contract PASS (MSXML 6).'
