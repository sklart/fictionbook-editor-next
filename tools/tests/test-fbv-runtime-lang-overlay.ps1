# Проверяет, что runtime JSON-overlay FBV синхронизирован с каталогом Lang и ресурсными ID.
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

$fbvCppPath = Join-Path $RepositoryRoot 'src\fbv\FBV.cpp'
$resourcePath = Join-Path $RepositoryRoot 'src\fbv\resource.h'
$fbvCpp = Get-Content -Raw -LiteralPath $fbvCppPath -Encoding UTF8
$resource = Get-Content -Raw -LiteralPath $resourcePath -Encoding UTF8

if ($fbvCpp -notmatch 'LoadRuntimeFbStrings\(\);') {
    throw 'FBV не вызывает LoadRuntimeFbStrings() при запуске.'
}

$bindingMatches = [regex]::Matches($fbvCpp, '\{\s*(IDS_[A-Z0-9_]+)\s*,\s*L"([^"]+)"\s*\}')
if ($bindingMatches.Count -eq 0) {
    throw 'В FBV.cpp не найдены runtime JSON binding-строки.'
}

$bindings = @{}
foreach ($match in $bindingMatches) {
    $resourceId = $match.Groups[1].Value
    $key = $match.Groups[2].Value
    if ($bindings.ContainsKey($resourceId)) {
        throw "Дублирующий FBV runtime binding для $resourceId."
    }
    if ($key -notlike 'fbv.*') {
        throw "FBV runtime binding $resourceId ссылается на не-FBV ключ: $key"
    }
    if ($resource -notmatch ('#define\s+' + [regex]::Escape($resourceId) + '\s+\d+')) {
        throw "Resource ID $resourceId из runtime binding не найден в resource.h."
    }
    $bindings[$resourceId] = $key
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("fbe-fbv-runtime-overlay-" + $PID)
$langRoot = Join-Path $tempRoot 'Lang'
try {
    & (Join-Path $RepositoryRoot 'tools\localization\export-runtime-lang.ps1') -RepositoryRoot $RepositoryRoot -OutputDirectory $langRoot -Clean

    $contract = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'localization\runtime\contract.json') -Encoding UTF8 | ConvertFrom-Json -AsHashtable
    $languages = Get-ChildItem -LiteralPath $langRoot -Directory | Select-Object -ExpandProperty Name
    if ($languages.Count -eq 0) {
        throw 'Экспорт Lang не создал языковых каталогов.'
    }

    foreach ($language in $languages) {
        $fbvJsonPath = Join-Path (Join-Path $langRoot $language) 'fbv.json'
        if (-not (Test-Path -LiteralPath $fbvJsonPath)) {
            throw "Не найден $fbvJsonPath"
        }
        $json = Get-Content -Raw -LiteralPath $fbvJsonPath -Encoding UTF8 | ConvertFrom-Json -AsHashtable
        $strings = $json['strings']
        foreach ($key in $bindings.Values) {
            if (-not $strings.ContainsKey($key)) {
                throw "В $fbvJsonPath отсутствует ключ runtime binding: $key"
            }
            if ([string]::IsNullOrWhiteSpace([string] $strings[$key])) {
                throw "В $fbvJsonPath пустой runtime binding: $key"
            }
        }
    }

    Write-Host 'FBV runtime JSON-overlay прошёл проверку.'
    Write-Host "  Binding-строк: $($bindings.Count)"
    Write-Host "  Языков: $($languages.Count)"
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
