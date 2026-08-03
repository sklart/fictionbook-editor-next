<#
.SYNOPSIS
Validates that the embedded FBELib and its registration repair use the diagnostic API contract.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$idl = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\fbe.idl')
$fbe = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.cpp')
$external = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ExternalHelper.cpp')

$checks = @(
    @{ Pattern = 'id\(29\).*IsDiagnosticTraceEnabled'; Source = $idl; Name = 'diagnostic enabled DISPID' },
    @{ Pattern = 'id\(30\).*TraceScript'; Source = $idl; Name = 'TraceScript DISPID' },
    @{ Pattern = 'LoadRegTypeLib'; Source = $fbe; Name = 'registered type library load' },
    @{ Pattern = 'LoadTypeLibEx\(current FBE\.exe\)'; Source = $fbe; Name = 'embedded type library load' },
    @{ Pattern = 'RegisterTypeLibForUser'; Source = $fbe; Name = 'per-user repair' },
    @{ Pattern = 'GetTypeInfoOfGuid\(IID_IExternalHelper\)'; Source = $fbe; Name = 'IExternalHelper lookup' },
    @{ Pattern = 'GetIDsOfNames'; Source = $fbe; Name = 'required method validation' },
    @{ Pattern = 'IsDiagnosticTraceEnabled'; Source = $fbe; Name = 'diagnostic enabled method validation' },
    @{ Pattern = 'TraceScript'; Source = $fbe; Name = 'TraceScript method validation' },
    @{ Pattern = 'REGKIND_NONE'; Source = $external; Name = 'embedded ExternalHelper typelib load' },
    @{ Pattern = 'typeInfo->GetIDsOfNames'; Source = $external; Name = 'embedded name resolution' },
    @{ Pattern = 'typeInfo->Invoke'; Source = $external; Name = 'embedded dispatch invoke' }
)

foreach ($check in $checks) {
    if ($check.Source -notmatch $check.Pattern) {
        throw "Missing type library diagnostic contract: $($check.Name)."
    }
}

Write-Host 'FBELib diagnostic type library contract passed.'