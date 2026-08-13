<#
.SYNOPSIS
Проверяет, что отчёты тестового режима не выдают фиктивный HRESULT.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')

if ($source -match 'save(?:-1)?-failed;hr=0x80004005;operation=Save') {
    throw 'Тестовый отчёт Save всё ещё подставляет фиктивный 0x80004005.'
}

foreach ($scenario in @('save-1-failed', 'save-failed')) {
    $pattern = [regex]::Escape($scenario) + '.*actual_hresult=unavailable;symbolic_hresult=unavailable'
    if ($source -notmatch $pattern) {
        throw "Для $scenario отсутствует честная диагностика недоступного HRESULT."
    }
}

Write-Host 'Диагностика HRESULT в отчётах тестового режима прошла проверку.'
