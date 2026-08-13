<#
.SYNOPSIS
Выполняет Base64-валидатор из поставляемой HTA в MSHTML/JScript.
#>

[CmdletBinding()]
param([switch]$Include25MiB)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$htaPath = Join-Path $repoRoot 'runtime\Utilities\FB2CheckContentTypes\FB2CheckContentTypes.hta'
$runner = Join-Path $PSScriptRoot 'fb2-check-content-types-base64.js'
& cscript.exe //nologo $runner $htaPath $(if($Include25MiB) { '--include-25mib' })
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$runnerSource = Get-Content -Raw -LiteralPath $runner
foreach ($contract in @('validateManyBinaries(100, 64 * 1024)', 'validateManyBinaries(1000, 4 * 1024)', 'accepted invalid binary near end')) {
    if (-not $runnerSource.Contains($contract)) { throw "Не найден many-binary regression contract: $contract" }
}

Write-Host 'FB2CheckContentTypes full Base64 validation passed.'
