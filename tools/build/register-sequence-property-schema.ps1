[CmdletBinding()]
param(
    [string]$SchemaPath,
    [string]$ExpectedSchemaSha256 = "",
    [string]$StatusFilePath,
    [switch]$NoRestartExplorer
)

$ErrorActionPreference = "Stop"
$sessionDirectory=Split-Path -Parent $MyInvocation.MyCommand.Path
$sessionLease=$null
if([IO.File]::Exists((Join-Path $sessionDirectory 'Session.ini'))){
    . (Join-Path $sessionDirectory 'shell-integration-lifecycle.ps1')
    $sessionLease=Open-ShellMaintenanceLease $sessionDirectory
}


function Test-Blank { param([string]$Value) return (-not $Value -or $Value.Trim().Length -eq 0) }


function Assert-PackageHash {
    param([string]$Path, [string]$Expected)
    if ($Expected -notmatch '^[0-9a-fA-F]{64}$') { throw 'Missing package SHA-256 for property schema.' }
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
        finally { $sha.Clear() }
    }
    finally { $stream.Close() }
    if (-not [string]::Equals($actual, $Expected, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Property schema does not match the trusted installer package.'
    }
}

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDirectory '..\..')).Path
if ((Test-Blank $SchemaPath)) {
    $schemaPath = Join-Path $repoRoot "packaging\property-schema\FBE.Sequence.propdesc"
}
else {
    $schemaPath = $SchemaPath
}

function Write-StatusIni {
    param(
        [string]$Result,
        [string]$Step,
        [string]$Code = "",
        [string]$Message = ""
    )

    if ((Test-Blank $StatusFilePath)) {
        return
    }

    $content = @(
        "[Schema]"
        "Result=$Result"
        "Step=$Step"
        "Code=$Code"
        "Message=$Message"
    )

    $directory = Split-Path -Parent $StatusFilePath
    if (-not (Test-Blank $directory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }

    [IO.File]::WriteAllLines($StatusFilePath, [string[]]$content, [Text.Encoding]::Unicode)
}

function Test-IsAdministrator {
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-Path -LiteralPath $schemaPath -PathType Leaf)) {
    Write-StatusIni -Result "error" -Step "Test-Path" -Code "FILE_NOT_FOUND" -Message "schema-file-not-found"
    throw "Property schema file was not found: $schemaPath"
}

if ($PSBoundParameters.ContainsKey('ExpectedSchemaSha256')) {
    Assert-PackageHash -Path $schemaPath -Expected $ExpectedSchemaSha256
}

if (-not (Test-IsAdministrator)) {
    Write-StatusIni -Result "error" -Step "Test-IsAdministrator" -Code "ACCESS_DENIED" -Message "administrator-rights-required"
    throw "Administrator rights are required to register the property schema."
}

if (-not ("FbePropertySchemaRegisterNative" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class FbePropertySchemaRegisterNative {
    [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
    public static extern int PSRegisterPropertySchema(string path);

    [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
    public static extern int PSRefreshPropertySchema();
}
"@
}

function Test-HResultFailed {
    param([int]$HResult)

    return $HResult -lt 0
}

Write-Host "Registering property schema:"
Write-Host "  $schemaPath"

try {
    $hr = [FbePropertySchemaRegisterNative]::PSRegisterPropertySchema($schemaPath)
    if (Test-HResultFailed $hr) {
        $code = ("0x{0:X8}" -f ($hr -band 0xffffffff))
        Write-StatusIni -Result "error" -Step "PSRegisterPropertySchema" -Code $code -Message "register-failed"
        throw ("PSRegisterPropertySchema failed: {0}" -f $code)
    }
    if ($hr -ne 0) {
        Write-Host ("PSRegisterPropertySchema returned non-fatal status: 0x{0:X8}" -f ($hr -band 0xffffffff))
    }

    $refreshHr = [FbePropertySchemaRegisterNative]::PSRefreshPropertySchema()
    if (Test-HResultFailed $refreshHr) {
        $code = ("0x{0:X8}" -f ($refreshHr -band 0xffffffff))
        Write-StatusIni -Result "error" -Step "PSRefreshPropertySchema" -Code $code -Message "refresh-failed"
        throw ("PSRefreshPropertySchema failed: {0}" -f $code)
    }
    if ($refreshHr -ne 0) {
        Write-Host ("PSRefreshPropertySchema returned non-fatal status: 0x{0:X8}" -f ($refreshHr -band 0xffffffff))
    }

    if (-not $NoRestartExplorer) {
        Write-Host "Restarting explorer.exe so the changes take effect..."
        Get-Process explorer -ErrorAction SilentlyContinue | Stop-Process -Force
        Start-Sleep -Seconds 2
        Start-Process explorer.exe
    }

    Write-StatusIni -Result "ok" -Step "done" -Code "0x00000000" -Message "success"
    Write-Host "FBE-specific property schema has been registered."
}
catch {
    if (-not (Test-Blank $StatusFilePath)) {
        $message = $_.Exception.Message
        if ((Test-Blank $message)) {
            $message = "unexpected-error"
        }
        Write-StatusIni -Result "error" -Step "exception" -Code "EXIT_1" -Message $message
    }
    throw
}
