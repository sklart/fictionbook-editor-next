# Проверяет, что runtime JSON-overlay FBE синхронизирован с каталогом Lang и ресурсными ID.
# Тест страхует связку `src/fbe/RuntimeLocalization.cpp` ↔ `localization/app-ui/catalog.json`
# и гарантирует, что внешний слой Lang/<язык>/fbe.json содержит все подключённые ключи.
[CmdletBinding()]
param(
    [string] $RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
else {
    $RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
}

$runtimeCppPath = Join-Path $RepositoryRoot 'src\fbe\RuntimeLocalization.cpp'
$resourceHeaderPath = Join-Path $RepositoryRoot 'src\fbe\resource.h'
$catalogPath = Join-Path $RepositoryRoot 'localization\app-ui\catalog.json'

$runtimeCpp = Get-Content -Raw -LiteralPath $runtimeCppPath -Encoding UTF8
$resourceHeader = Get-Content -Raw -LiteralPath $resourceHeaderPath
$catalog = Get-Content -Raw -LiteralPath $catalogPath -Encoding UTF8 | ConvertFrom-Json -AsHashtable

$resourceNames = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($match in [regex]::Matches($resourceHeader, '(?m)^#define\s+(IDS_[A-Z0-9_]+)\s+\d+')) {
    [void] $resourceNames.Add($match.Groups[1].Value)
}

$bindings = [ordered] @{}
foreach ($match in [regex]::Matches($runtimeCpp, '\{\s*(IDS_[A-Z0-9_]+)\s*,\s*L"([^"]+)"\s*\}')) {
    $resourceId = $match.Groups[1].Value
    $key = $match.Groups[2].Value
    if ($bindings.Contains($resourceId)) {
        throw "Дублирующий FBE runtime binding для $resourceId."
    }
    if (-not $key.StartsWith('fbe.', [StringComparison]::Ordinal)) {
        throw "FBE runtime binding $resourceId ссылается на не-FBE ключ: $key"
    }
    if (-not $resourceNames.Contains($resourceId)) {
        throw "Resource ID $resourceId из runtime binding не найден в resource.h."
    }
    if (-not $catalog.seedStrings.ContainsKey($key)) {
        throw "Ключ $key из runtime binding отсутствует в localization/app-ui/catalog.json."
    }
    if ([string] $catalog.seedStrings[$key].resourceId -ne $resourceId) {
        throw "Ключ $key в catalog.json указывает на $($catalog.seedStrings[$key].resourceId), а runtime binding использует $resourceId."
    }
    $bindings[$resourceId] = $key
}

if ($bindings.Count -lt 20) {
    throw "В RuntimeLocalization.cpp найдено слишком мало FBE runtime binding-строк: $($bindings.Count)."
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("fbe-runtime-overlay-" + $PID)
$langRoot = Join-Path $tempRoot 'Lang'
try {
    & (Join-Path $RepositoryRoot 'tools\localization\export-runtime-lang.ps1') -RepositoryRoot $RepositoryRoot -OutputDirectory $langRoot -Clean | Out-Host

    foreach ($language in @($catalog.targetLanguages)) {
        $jsonPath = Join-Path (Join-Path $langRoot $language) 'fbe.json'
        if (-not (Test-Path -LiteralPath $jsonPath)) {
            throw "Не создан runtime JSON для FBE: $jsonPath"
        }

        $payload = Get-Content -Raw -LiteralPath $jsonPath -Encoding UTF8 | ConvertFrom-Json -AsHashtable
        foreach ($key in $bindings.Values) {
            if (-not $payload.strings.ContainsKey($key)) {
                throw "В $jsonPath отсутствует ключ runtime binding: $key"
            }
            if ([string]::IsNullOrWhiteSpace([string] $payload.strings[$key])) {
                throw "В $jsonPath пустой runtime binding: $key"
            }
        }
    }

    Write-Host 'FBE runtime JSON-overlay прошёл проверку.'
    Write-Host "  Binding-строк: $($bindings.Count)"
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
