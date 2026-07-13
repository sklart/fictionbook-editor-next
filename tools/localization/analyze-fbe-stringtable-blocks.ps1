<#
.SYNOPSIS
Анализирует FBE STRINGTABLE по 16-ID блокам перед переносом строк в generated .rc2.

.DESCRIPTION
Win32 resource compiler группирует строковые ресурсы по блокам из 16 числовых ID.
Если часть блока остаётся в ручном FBE.rc, а часть переносится в generated .rc2,
сборка может упасть с RC2151 "cannot reuse string constants". Скрипт читает
`src/fbe/resource.h`, `localization/app-ui/catalog.json` и локализованные
`FBE.rc`, затем показывает, какие блоки уже частично/полностью перенесены и
какие ручные строки остаются кандидатами для следующего безопасного среза.
#>
[CmdletBinding()]
param(
    [ValidateSet("ru-RU", "uk-UA")]
    [string]$Language = "ru-RU",

    [int]$Top = 40
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$resourceHeaderPath = Join-Path $repoRoot "src\fbe\resource.h"
$catalogPath = Join-Path $repoRoot "localization\app-ui\catalog.json"
$resourceFilePath = switch ($Language) {
    "ru-RU" { Join-Path $repoRoot "src\locales\res_rus\FBE.rc" }
    "uk-UA" { Join-Path $repoRoot "src\locales\res_ukr\FBE.rc" }
}

$definitions = @{}
Select-String -Path $resourceHeaderPath -Pattern '^#define\s+(\S+)\s+(\d+)' |
    ForEach-Object {
        $definitions[$_.Matches[0].Groups[1].Value] = [int]$_.Matches[0].Groups[2].Value
    }

$catalog = Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json -Depth 50
$generatedIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$catalog.seedStrings.PSObject.Properties |
    Where-Object {
        [string]$_.Value.component -eq "fbe.core" -and
            [string]$_.Value.resourceId -match '^IDS_[A-Z0-9_]+$'
    } |
    ForEach-Object {
        [void]$generatedIds.Add([string]$_.Value.resourceId)
    }

$manualIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$manualSymbols = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
Select-String -Path $resourceFilePath -Pattern '^\s+(IDS_[A-Z0-9_]+)\b' |
    ForEach-Object {
        [void]$manualIds.Add($_.Matches[0].Groups[1].Value)
    }
Select-String -Path $resourceFilePath -Pattern '^\s+([A-Z][A-Z0-9_]+)\b' |
    ForEach-Object {
        [void]$manualSymbols.Add($_.Matches[0].Groups[1].Value)
    }

$blockMap = @{}
foreach ($pair in $definitions.GetEnumerator()) {
    $id = [string]$pair.Key
    if ($id -notmatch '^IDS_[A-Z0-9_]+$') {
        continue
    }

    $value = [int]$pair.Value
    $block = [int][math]::Floor($value / 16)
    if (-not $blockMap.Contains($block)) {
        $blockMap[$block] = [ordered]@{
            Block = $block
            Range = ("{0}-{1}" -f ($block * 16), ($block * 16 + 15))
            Manual = [Collections.Generic.List[string]]::new()
            ManualAll = [Collections.Generic.List[string]]::new()
            Generated = [Collections.Generic.List[string]]::new()
            Known = [Collections.Generic.List[string]]::new()
        }
    }

    $blockMap[$block].Known.Add($id)
    if ($generatedIds.Contains($id)) {
        $blockMap[$block].Generated.Add($id)
    }
    if ($manualIds.Contains($id)) {
        $blockMap[$block].Manual.Add($id)
    }
    if ($manualSymbols.Contains($id)) {
        $blockMap[$block].ManualAll.Add($id)
    }
}

$rows = foreach ($block in $blockMap.Values) {
    if ($block.ManualAll.Count -eq 0 -and $block.Generated.Count -eq 0) {
        continue
    }

    $status = if ($block.ManualAll.Count -gt 0 -and $block.Generated.Count -gt 0) {
        "Смешанный"
    }
    elseif ($block.Generated.Count -gt 0) {
        "Generated"
    }
    else {
        "Ручной"
    }

    [pscustomobject]@{
        Block = $block.Block
        Range = $block.Range
        Status = $status
        ManualCount = $block.Manual.Count
        ManualAllCount = $block.ManualAll.Count
        GeneratedCount = $block.Generated.Count
        ManualIds = ($block.Manual | Sort-Object) -join ", "
        ManualAllIds = ($block.ManualAll | Sort-Object) -join ", "
        GeneratedIds = ($block.Generated | Sort-Object) -join ", "
    }
}

$rows |
    Sort-Object @{ Expression = { if ($_.Status -eq "Смешанный") { 0 } elseif ($_.Status -eq "Ручной") { 1 } else { 2 } } }, Block |
    Select-Object -First $Top

