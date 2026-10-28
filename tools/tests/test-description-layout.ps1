[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$cssBlocks=@()
foreach($cssName in @('main.css','main_fast.css')) {
    $css=Get-Content (Join-Path $root "runtime/$cssName") -Raw
    $match=[regex]::Match($css,'(?s)/\* DESCRIPTION BINARY TABLE BEGIN:.*?/\* DESCRIPTION BINARY TABLE END \*/')
    if(-not $match.Success){throw "Missing Description binary CSS block: $cssName"}
    $cssBlocks+=($match.Value -replace "`r`n","`n")
}
if($cssBlocks[0] -cne $cssBlocks[1]){throw 'Description binary CSS differs between normal and Fast mode.'}
$navBlocks=@()
foreach($cssName in @('main.css','main_fast.css')) {
    $css=Get-Content (Join-Path $root "runtime/$cssName") -Raw
    $match=[regex]::Match($css,'(?s)/\* DESCRIPTION MODERN NAVIGATION BEGIN:.*?/\* DESCRIPTION MODERN NAVIGATION END \*/')
    if(-not $match.Success){throw "Missing Description navigation CSS block: $cssName"}
    $navBlocks+=($match.Value -replace "`r`n","`n")
}
if($navBlocks[0] -cne $navBlocks[1]){throw 'Description navigation CSS differs between normal and Fast mode.'}
$xsl = New-Object -ComObject Msxml2.FreeThreadedDOMDocument.6.0
$xsl.async = $false
$xsl.setProperty('ResolveExternals', $true)
if (-not $xsl.load((Join-Path $root 'runtime\fb2.xsl'))) { throw $xsl.parseError.reason }
$template = New-Object -ComObject Msxml2.XSLTemplate.6.0
$template.stylesheet = $xsl
$xml = New-Object -ComObject Msxml2.DOMDocument.6.0
$xml.async = $false
$xml.validateOnParse = $false
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
$schema = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
$schema.add('http://www.gribuser.ru/xml/fictionbook/2.0',(Join-Path $root 'runtime/FictionBook.xsd'))
foreach ($fixture in Get-ChildItem (Join-Path $PSScriptRoot 'fixtures/description') -Filter '*.fb2') {
    if (-not $xml.load($fixture.FullName)) { throw $xml.parseError.reason }
    $xml.schemas=$schema
    $validation=$xml.validate()
    if ($fixture.BaseName -eq 'invalid-ish') {
        if ($validation.errorCode -eq 0) { throw 'Invalid-ish fixture must retain its invalid metadata.' }
    } elseif ($validation.errorCode -ne 0) { throw "Invalid correct fixture: $($fixture.Name): $($validation.reason)" }
    $previous = $null
    foreach ($layout in @('modern','classic')) {
        $processor=$template.createProcessor(); $processor.input=$xml
        $processor.setStartMode('description',''); $processor.addParameter('descriptionLayout',$layout,'')
        if (-not $processor.transform()) { throw "Fixture transform failed: $($fixture.Name) / $layout" }
        $html=([string]$processor.output) -replace 'data-layout="(modern|classic)"','data-layout="shared"'
        if ($previous -and $html -cne $previous) { throw "Fixture field DOM differs: $($fixture.Name)" }
        $previous=$html
    }
}
Write-Host 'Description fixtures A-G transform in both layouts PASS.'
