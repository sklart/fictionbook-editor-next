<#
.SYNOPSIS
Exercises FBELib per-user registration repair without retaining any registry changes.
#>
[CmdletBinding()]
param([string]$Configuration = 'Release')

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$typeLibKey = 'HKCU:\Software\Classes\TypeLib\{37B16C7D-4400-4D7D-AA35-14C74E265EA4}'
$testRoot = Join-Path $repoRoot 'out\tests\fbe-typelib-registration'
$backup = Join-Path $testRoot 'fbelib-typelib-before.reg'
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
$hadRegistration = Test-Path -LiteralPath $typeLibKey

function Restore-Registration {
    Remove-Item -LiteralPath $typeLibKey -Recurse -Force -ErrorAction SilentlyContinue
    if($hadRegistration) {
        & reg.exe import $backup | Out-Null
        if($LASTEXITCODE -ne 0) { throw "Could not restore the original FBELib registration (reg.exe exit $LASTEXITCODE)." }
    }
}

function Invoke-FbeRegistrationStartup([string]$Scenario, [switch]$AllowTraceErrors) {
	$started = (Get-Date).ToUniversalTime().AddSeconds(-5)
	& (Join-Path $PSScriptRoot 'test-fbe-startup.ps1') -Configuration $Configuration -Trace -TimeoutSeconds 90 -AllowTraceErrors:$AllowTraceErrors
    if($LASTEXITCODE -ne 0) { throw "FBE startup failed for registration scenario '$Scenario'." }
	$directories = @((Join-Path $env:LOCALAPPDATA 'FBE Next\Diagnostics'), (Join-Path $env:TEMP 'FBE Next Diagnostics'))
	$candidates = @(foreach($directory in $directories) { if(Test-Path -LiteralPath $directory) { Get-ChildItem -LiteralPath $directory -Filter 'fbe-trace-*.log' -File | Where-Object { $_.LastWriteTimeUtc -ge $started } } })
	$trace = $candidates | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
	if($null -eq $trace) { throw "No trace was created for registration scenario '$Scenario'." }
	return Get-Content -Raw -LiteralPath $trace.FullName
}

try {
    if($hadRegistration) {
        & reg.exe export 'HKCU\Software\Classes\TypeLib\{37B16C7D-4400-4D7D-AA35-14C74E265EA4}' $backup /y | Out-Null
        if($LASTEXITCODE -ne 0) { throw "Could not back up the original FBELib registration (reg.exe exit $LASTEXITCODE)." }
    }

    # A: no per-user registration. Startup must use its embedded ITypeInfo and repair only afterwards.
    Remove-Item -LiteralPath $typeLibKey -Recurse -Force -ErrorAction SilentlyContinue
	$missingRegistrationTrace = Invoke-FbeRegistrationStartup 'missing-per-user-registration'
	if($missingRegistrationTrace -notmatch 'external-typeinfo=') { throw 'Embedded ExternalHelper was not observed without per-user FBELib registration.' }
	if($missingRegistrationTrace -notmatch 'core-compatible=1; diagnostic-compatible=0') { throw 'Legacy registered FBELib did not remain core-compatible with diagnostic methods absent.' }
	if($missingRegistrationTrace -notmatch 'internal diagnostic bridge uses embedded typelib') { throw 'Legacy registered FBELib was not isolated from the embedded diagnostic bridge.' }

    # B: a stale registered path must not affect the embedded window.external contract.
	if($hadRegistration) { & reg.exe import $backup | Out-Null; if($LASTEXITCODE -ne 0) { throw 'Could not restore the test registration before the stale-path scenario.' } }
	$stalePathKey = 'HKCU\Software\Classes\TypeLib\{37B16C7D-4400-4D7D-AA35-14C74E265EA4}\1.0\0\win32'
	& reg.exe add $stalePathKey /ve /t REG_SZ /d 'Z:\missing\legacy-FBELib.tlb' /f | Out-Null
	if($LASTEXITCODE -ne 0) { throw 'Could not create the stale FBELib registered path.' }
	$staleRegistrationTrace = Invoke-FbeRegistrationStartup 'stale-registered-path' -AllowTraceErrors
	if($staleRegistrationTrace -notmatch 'code=TL159') { throw 'Stale registered FBELib was not identified for repair.' }
	if($staleRegistrationTrace -notmatch 'code=TL180') { throw 'Stale registered FBELib did not reach per-user repair.' }

	Write-Host 'FBELib missing, legacy-compatible, and stale-registration scenarios passed and will be restored.'
}
finally {
    Restore-Registration
}
