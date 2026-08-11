$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
foreach($name in @('ConfigZipHandler.hta','ConfigRarHandler.hta')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot ('runtime\Utilities\ArchHandler\' + $name))
    foreach($fragment in @('deleteValue(o,classes+archiveExtension+"\\OpenWithProgids\\"+progId)', 'deleteValue(o,capabilities+"FileAssociations\\"+archiveExtension)', 'if (!hasHandler(o,otherType))')) {
        if($text -notlike "*$fragment*") { throw "$name не реализует частичный безопасный Reset: $fragment" }
    }
    if($text -like '*deleteTree(o,classes+archiveExtension+"\\OpenWithProgids\\"+progId)*') { throw "$name удаляет OpenWithProgids value как fictitious tree." }
}
Write-Host 'ArchHandler reset registration contract passed.'
