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
$xml.validateOnParse = $false
$schema = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
$schema.add('http://www.gribuser.ru/xml/fictionbook/2.0',(Join-Path $root 'runtime/FictionBook.xsd'))
foreach ($fixture in Get-ChildItem (Join-Path $PSScriptRoot 'fixtures/description') -Filter '*.fb2') {
    if (-not $xml.load($fixture.FullName)) { throw $xml.parseError.reason }
    $xml.schemas=$schema
    $validation=$xml.validate()
    if ($fixture.BaseName -eq 'invalid-ish') {
        if ($validation.errorCode -eq 0) { throw 'Invalid fixture must retain invalid metadata.' }
    } elseif ($validation.errorCode -ne 0) { throw "Invalid fixture: $($fixture.Name): $($validation.reason)" }
    $previous=$null
    foreach ($layout in @('modern','classic','unknown')) {
        $processor=$template.createProcessor(); $processor.input=$xml
        $processor.setStartMode('description',''); $processor.addParameter('descriptionLayout',$layout,'')
        if (-not $processor.transform()) { throw "Fixture transform failed: $($fixture.Name) / $layout" }
        $html=[string]$processor.output
        if ($previous -and $html -cne $previous) { throw "Legacy presentation changed with layout preference: $($fixture.Name)" }
        foreach ($id in @('tiTitle','tiAuthor','tiGenre','tiLang','tiCover','tiSeq','stiAuthor','diAuthor','diID','diVersion','piSeq','binobj','stylesheetId')) {
            if ($html -notmatch ('id="' + $id + '"')) { throw "Missing $id in $($fixture.Name)" }
        }
        if ($html -match 'description-(section|navigation|binary|person)' -or $html -match 'data-layout=') { throw 'Experimental layout appeared in the XSL output.' }
        if ($html.IndexOf('id="tiGenre"') -ge $html.IndexOf('id="tiAuthor"') -or $html.IndexOf('id="tiAuthor"') -ge $html.IndexOf('id="tiTitle"')) { throw 'The legacy genre, author and title order changed.' }
        foreach($id in @('tiDate','stiDate')) { if ($html -notmatch ('id="'+$id+'"[^>]*data-fbe-date-present="[01]"')) { throw "Missing date-presence marker: $id" } }
        $previous=$html
    }
}
Write-Host 'Legacy Description XSL: all fixtures and saved layout preferences PASS.'
