[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Before,
    [Parameter(Mandatory)][string]$AfterSplit,
    [Parameter(Mandatory)][string]$AfterUndo,
    [string]$AfterRedo,
    [string]$SchemaPath,
    [Parameter(Mandatory)][int]$UndoSteps,
    [switch]$UndoHistoryExhausted
)
$ErrorActionPreference = 'Stop'
if ($UndoSteps -lt 1) { throw 'UndoSteps must be positive.' }
function ReadSnapshot([string]$Path) {
    $xml = New-Object System.Xml.XmlDocument
    $xml.PreserveWhitespace = $false
    $xml.XmlResolver = $null
    $xml.Load((Resolve-Path -LiteralPath $Path).Path)
    $nsm = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
    $nsm.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $bodies = @($xml.SelectNodes('/fb:FictionBook/fb:body', $nsm))
    if ($bodies.Count -eq 0) { throw "No FictionBook body: $Path" }
    $ids = @{}; $duplicates = New-Object System.Collections.Generic.List[string]
    foreach ($node in $xml.SelectNodes('//*[@id]')) {
        $id = $node.GetAttribute('id'); if (!$id) { continue }
        if ($ids.ContainsKey($id)) { $duplicates.Add($id) } else { $ids[$id] = $true }
    }
    $broken = New-Object System.Collections.Generic.List[string]
    foreach ($node in $xml.SelectNodes('//*[@*[local-name()="href"]]')) {
        foreach ($attr in $node.Attributes) {
            if ($attr.LocalName -eq 'href' -and $attr.NamespaceURI -eq 'http://www.w3.org/1999/xlink' -and $attr.Value.StartsWith('#') -and !$ids.ContainsKey($attr.Value.Substring(1))) { $broken.Add($attr.Value) }
        }
    }
    $xsd = 'NOT_RUN'
    if ($SchemaPath) {
        $schema = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
        $schema.add('http://www.gribuser.ru/xml/fictionbook/2.0', (Resolve-Path -LiteralPath $SchemaPath).Path)
        $doc = New-Object -ComObject Msxml2.DOMDocument.6.0; $doc.async = $false
        if (!$doc.load((Resolve-Path -LiteralPath $Path).Path)) { throw "MSXML cannot load: $Path" }
        $doc.schemas = $schema; $failure = $doc.validate(); $xsd = if ($failure.errorCode -eq 0) { 'PASS' } else { 'FAIL: ' + $failure.reason }
    }
    [pscustomobject]@{ path=(Resolve-Path -LiteralPath $Path).Path; sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash; body=($bodies | ForEach-Object OuterXml) -join "`n"; sections=@($xml.SelectNodes('//fb:body//fb:section', $nsm)).Count; duplicateIds=@($duplicates); brokenLocalHrefs=@($broken); xsd=$xsd }
}
$beforeDoc = ReadSnapshot $Before; $splitDoc = ReadSnapshot $AfterSplit; $undoDoc = ReadSnapshot $AfterUndo
$redoDoc = if ($AfterRedo) { ReadSnapshot $AfterRedo } else { $null }
$matchesBefore = $beforeDoc.body -ceq $undoDoc.body
$structuralDamage = $undoDoc.duplicateIds.Count -gt 0 -or $undoDoc.brokenLocalHrefs.Count -gt 0 -or !$matchesBefore
$classification = if (!$structuralDamage) { if ($UndoSteps -eq 1) { 'RESTORED_ONE_UNDO' } else { 'MULTISTEP_UNDO' } } elseif ($UndoHistoryExhausted) { 'STRUCTURAL_UNDO_CORRUPTION' } else { 'TEST_INCONCLUSIVE' }
$redoStatus = if (!$redoDoc) { 'NOT_RUN' } elseif ($redoDoc.body -ceq $splitDoc.body -and $redoDoc.duplicateIds.Count -eq 0 -and $redoDoc.brokenLocalHrefs.Count -eq 0) { 'PASS' } else { 'REDO_FAILURE' }
[pscustomobject]@{ classification=$classification; undoSteps=$UndoSteps; exhaustedClaim=[bool]$UndoHistoryExhausted; matchesBefore=$matchesBefore; redo=$redoStatus; before=$beforeDoc; afterSplit=$splitDoc; afterUndo=$undoDoc; afterRedo=$redoDoc; warning='Offline saved-XML comparison; exhaustedClaim must be independently observed. Exact body XML, not semantic canonicalization. Save may change history; use independent runs.' } | ConvertTo-Json -Depth 8
