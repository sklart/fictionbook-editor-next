# Проверяет, что runtime JSON-overlay ExportDOCX синхронизирован с каталогом Lang и ресурсными ID.
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

$runtimeCppPath = Join-Path $RepositoryRoot 'src\export-docx\RuntimeLocalization.cpp'
$dllMainPath = Join-Path $RepositoryRoot 'src\export-docx\dllmain.cpp'
$resourcePath = Join-Path $RepositoryRoot 'src\export-docx\resource.h'
$runtimeCpp = Get-Content -Raw -LiteralPath $runtimeCppPath -Encoding UTF8
$dllMain = Get-Content -Raw -LiteralPath $dllMainPath -Encoding UTF8
$resource = Get-Content -Raw -LiteralPath $resourcePath -Encoding UTF8

if ($dllMain -notmatch 'InitExportDocxRuntimeStrings\(\);') {
    throw 'ExportDOCX не вызывает InitExportDocxRuntimeStrings() при загрузке DLL.'
}

$bindingMatches = [regex]::Matches($runtimeCpp, '\{\s*(IDS_[A-Z0-9_]+|IDR_EXPORTDOCX)\s*,\s*L"([^"]+)"\s*\}')
if ($bindingMatches.Count -eq 0) {
    throw 'В RuntimeLocalization.cpp не найдены runtime JSON binding-строки ExportDOCX.'
}

$bindings = @{}
foreach ($match in $bindingMatches) {
    $resourceId = $match.Groups[1].Value
    $key = $match.Groups[2].Value
    if ($bindings.ContainsKey($resourceId)) {
        throw "Дублирующий ExportDOCX runtime binding для $resourceId."
    }
    if ($key -notlike 'export_docx.*') {
        throw "ExportDOCX runtime binding $resourceId ссылается на не-ExportDOCX ключ: $key"
    }
    if ($resource -notmatch ('#define\s+' + [regex]::Escape($resourceId) + '\s+\d+')) {
        throw "Resource ID $resourceId из runtime binding не найден в resource.h."
    }
    $bindings[$resourceId] = $key
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("fbe-export-docx-runtime-overlay-" + $PID)
$langRoot = Join-Path $tempRoot 'Lang'
try {
    & (Join-Path $RepositoryRoot 'tools\localization\export-runtime-lang.ps1') -RepositoryRoot $RepositoryRoot -OutputDirectory $langRoot -Clean

    $languages = Get-ChildItem -LiteralPath $langRoot -Directory | Select-Object -ExpandProperty Name
    if ($languages.Count -eq 0) {
        throw 'Экспорт Lang не создал языковых каталогов.'
    }

    foreach ($language in $languages) {
        $jsonPath = Join-Path (Join-Path $langRoot $language) 'export-docx.json'
        if (-not (Test-Path -LiteralPath $jsonPath)) {
            throw "Не найден $jsonPath"
        }
        $json = Get-Content -Raw -LiteralPath $jsonPath -Encoding UTF8 | ConvertFrom-Json -AsHashtable
        $strings = $json['strings']
        foreach ($key in $bindings.Values) {
            if (-not $strings.ContainsKey($key)) {
                throw "В $jsonPath отсутствует ключ runtime binding: $key"
            }
            if ([string]::IsNullOrWhiteSpace([string] $strings[$key])) {
                throw "В $jsonPath пустой runtime binding: $key"
            }
        }
    }

    Write-Host 'ExportDOCX runtime JSON-overlay прошёл проверку.'
    Write-Host "  Binding-строк: $($bindings.Count)"
    Write-Host "  Языков: $($languages.Count)"
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
