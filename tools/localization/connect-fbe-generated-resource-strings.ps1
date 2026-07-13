<#
.SYNOPSIS
Подключает generated-строки FBE к русской и украинской resource DLL.

.DESCRIPTION
Скрипт механически удаляет из `src/locales/res_rus/FBE.rc` и
`src/locales/res_ukr/FBE.rc` ручные строки FBE, уже заведённые в
`localization/app-ui/catalog.json`, и добавляет `#include
"FBEStrings.generated.rc2"` перед концом основного языкового блока. Это нужно,
чтобы не получить дубли `STRINGTABLE` при подключении generated `.rc2`.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$catalogPath = Join-Path $repoRoot "localization\app-ui\catalog.json"

$catalog = Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json -Depth 30
$resourceIds = @(
    $catalog.seedStrings.PSObject.Properties |
        Where-Object {
            [string]$_.Value.component -eq "fbe.core" -and
                [string]$_.Value.resourceId -match '^IDS_[A-Z0-9_]+$'
        } |
        ForEach-Object { [string]$_.Value.resourceId } |
        Sort-Object -Unique
)
$resourceIdsByLength = @($resourceIds | Sort-Object { $_.Length } -Descending)
if ($resourceIdsByLength.Count -eq 0) {
    throw "В $catalogPath не найдены FBE-строки с IDS_* resourceId."
}

$targets = @(
    @{
        Path = Join-Path $repoRoot "src\locales\res_rus\FBE.rc"
        EndMarker = "#endif    // Russian (Russia) resources"
        Encoding = [Text.Encoding]::GetEncoding(1251)
    },
    @{
        Path = Join-Path $repoRoot "src\locales\res_ukr\FBE.rc"
        EndMarker = "#endif    // Ukrainian (Ukraine) resources"
        Encoding = [Text.Encoding]::GetEncoding(1251)
    }
)

foreach ($target in $targets) {
    $path = $target["Path"]
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Не найден ресурсный файл FBE: $path"
    }

    $text = [IO.File]::ReadAllText($path, $target["Encoding"])
    $includeInserted = $false
    $lines = [Collections.Generic.List[string]]::new()
    $removed = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $skipRemovedContinuation = $false
    foreach ($line in ($text.Replace("`r`n", "`n").Replace("`r", "`n").Split("`n"))) {
        $trimmedLine = $line.TrimStart()
        if ($skipRemovedContinuation -and $trimmedLine.StartsWith('"', [StringComparison]::Ordinal)) {
            continue
        }
        $skipRemovedContinuation = $false

        $resourceId = $resourceIdsByLength | Where-Object { $trimmedLine.StartsWith($_, [StringComparison]::Ordinal) } | Select-Object -First 1
        if ($resourceId) {
            [void]$removed.Add($resourceId)
            $skipRemovedContinuation = $true
            continue
        }

        if ($line -eq '#include "FBEStrings.generated.rc2"' -or
            $line -eq "// Generated FBE strings from localization/app-ui/catalog.json.") {
            continue
        }

        if ($line -eq $target["EndMarker"]) {
            $lines.Add("")
            $lines.Add("// Generated FBE strings from localization/app-ui/catalog.json.")
            $lines.Add('#include "FBEStrings.generated.rc2"')
            $lines.Add("")
            $includeInserted = $true
        }

        $lines.Add($line)
    }

    if (-not $includeInserted) {
        throw "В $path не найдено место для подключения FBEStrings.generated.rc2: $($target["EndMarker"])"
    }

    $output = ($lines -join "`r`n")
    $output = [regex]::Replace($output, "(?m)^\s*STRINGTABLE\r?\n\s*BEGIN\r?\n\s*END\r?\n", "")
    [IO.File]::WriteAllText($path, $output, $target["Encoding"])
    Write-Host "Generated FBE строки подключены к $path"
    Write-Host "  Удалено ручных строк: $($removed.Count)"
}
