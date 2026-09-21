[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$traceHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\StartupTrace.h')
$traceSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\StartupTrace.cpp')
$document = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBDoc.cpp')
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceDocumentTransfer.cpp')
$search = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\DocumentSearchCoordinator.cpp')
$external = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\ExternalHelper.h')

$expected = @{
    XsltTemplateBuild = 'J410[\s\S]{0,160}CountXsltTemplateBuild'
    XsdSchemaLoad = 'schemas->add[\s\S]{0,160}CountXsdSchemaLoad'
    SourceSerialization = 'serializedCache\.xml[\s\S]{0,200}CountSourceSerialization'
    SnapshotBuild = 'BuildSnapshot\(document, documentGeneration\)[\s\S]{0,160}\+\+m_snapshotBuildCount'
    SearchQueryRun = '\+\+m_searchQueryRunCount[\s\S]{0,160}m_session\.SetQuery'
}
foreach ($entry in $expected.GetEnumerator()) {
    $counterOwner = if ($entry.Key -eq 'SnapshotBuild' -or $entry.Key -eq 'SearchQueryRun') { $search } else { $traceHeader + $traceSource }
    if ($counterOwner -notmatch "$($entry.Key)Count" -or
        ($document + $source + $search + $external) -notmatch $entry.Value) {
        throw "Не закреплён trace-счётчик Phase B: $($entry.Key)"
    }
}

Write-Host 'Trace-счётчики Phase B закреплены контрактом.'
