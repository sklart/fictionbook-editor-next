<# Проверяет embedded JScript поставляемых HTA на синтаксис, несовместимый с MSHTML. #>
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$paths = @(
    'runtime\Utilities\ArchHandler\ConfigRarHandler.hta',
    'runtime\Utilities\ArchHandler\ConfigZipHandler.hta',
    'runtime\Utilities\FB2CheckContentTypes\FB2CheckContentTypes.hta',
    'runtime\Utilities\fb2recode\fb2recode.hta',
    'runtime\Utilities\Save Sections As Separate Documents\SaveSectionsAsSeparateDocuments.hta'
)
foreach($relativePath in $paths) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativePath)
    if($text -notmatch '(?is)<script\b') { throw "В HTA нет embedded script: $relativePath" }
    foreach($forbidden in @('\blet\b', '\bconst\b', '=>', '\bPromise\b', '\basync\s+function\b', '\bawait\s+', '\bclass\s+')) {
        if($text -match $forbidden) { throw "В legacy HTA найден неподдерживаемый JScript token [$forbidden]: $relativePath" }
    }
}
Write-Host 'Legacy HTA JavaScript compatibility gate passed.'
