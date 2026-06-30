[CmdletBinding()]
param(
    [string]$SchemaPath,
    [string]$StatusFilePath,
    [switch]$NoRestartExplorer
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($SchemaPath)) {
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

    if ([string]::IsNullOrWhiteSpace($StatusFilePath)) {
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
    if (-not [string]::IsNullOrWhiteSpace($directory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }

    Set-Content -LiteralPath $StatusFilePath -Value $content -Encoding Ascii
}

function Test-IsAdministrator {
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-IsAdministrator)) {
    Write-StatusIni -Result "error" -Step "Test-IsAdministrator" -Code "ACCESS_DENIED" -Message "administrator-rights-required"
    throw "Administrator rights are required to register the property schema."
}

if (-not (Test-Path -LiteralPath $schemaPath -PathType Leaf)) {
    Write-StatusIni -Result "error" -Step "Test-Path" -Code "FILE_NOT_FOUND" -Message "schema-file-not-found"
    throw "Property schema file was not found: $schemaPath"
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
    if ([string]::IsNullOrWhiteSpace($StatusFilePath) -eq $false -and -not (Test-Path -LiteralPath $StatusFilePath)) {
        $message = $_.Exception.Message
        if ([string]::IsNullOrWhiteSpace($message)) {
            $message = "unexpected-error"
        }
        Write-StatusIni -Result "error" -Step "exception" -Code "EXIT_1" -Message $message
    }
    throw
}
