[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$DllPath,
    [string]$InstallationDirectory = "",
    [string]$ExpectedDllSha256 = "",
    [string]$ExpectedSchemaSha256 = "",
    [switch]$UnregisterSchema,

    [ValidateSet("Win32", "x64")]
    [string]$Platform = "x64",

    [string]$StatusFilePath = ""
)

$ErrorActionPreference = "Stop"
$runtimeScript = $MyInvocation.MyCommand.Path

function Test-Blank { param([string]$Value) return (-not $Value -or $Value.Trim().Length -eq 0) }


function Write-Status {
    param(
        [Parameter(Mandatory)]
        [string]$Result,
        [Parameter(Mandatory)]
        [string]$Step,
        [Parameter(Mandatory)]
        [string]$Code,
        [Parameter(Mandatory)]
        [string]$Message
    )

    if ((Test-Blank $StatusFilePath)) {
        return
    }

    $dir = Split-Path -Parent $StatusFilePath
    if ($dir) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }

    $lines = @(
        "[Shell]"
        "Result=$Result"
        "Step=$Step"
        "Code=$Code"
        "Message=$Message"
    )
    [IO.File]::WriteAllLines($StatusFilePath, $lines, [Text.Encoding]::Unicode)
}

function Write-EmergencyStatus {
    param(
        [Parameter(Mandatory)]
        [string]$Step,
        [Parameter(Mandatory)]
        [string]$Code,
        [Parameter(Mandatory)]
        [string]$Message
    )

    if ((Test-Blank $StatusFilePath)) {
        return
    }

    try {
        $dir = Split-Path -Parent $StatusFilePath
        if ($dir) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }

        $content = "[Shell]`r`nResult=error`r`nStep=$Step`r`nCode=$Code`r`nMessage=$Message`r`n"
        [System.IO.File]::WriteAllText($StatusFilePath, $content, [System.Text.Encoding]::Unicode)
    }
    catch {
    }
}

function Test-PackageOwner {
    $directory = Split-Path -Parent $DllPath
    $ownerPath = Join-Path $directory 'Owner.ini'
    if (-not [IO.File]::Exists($ownerPath)) { return (Test-Blank $InstallationDirectory) }
    if (Test-Blank $InstallationDirectory) { return $false }
    $owner = @{}
    foreach ($line in [IO.File]::ReadAllLines($ownerPath)) {
        $separator = $line.IndexOf('=')
        if ($separator -ge 0) { $owner[$line.Substring(0,$separator)] = $line.Substring($separator+1) }
    }
    if (-not [string]::Equals($owner['InstallDirectory'], $InstallationDirectory, [StringComparison]::OrdinalIgnoreCase)) { return $false }
    $dllKey = if ($Platform -eq 'x64') { 'X64Sha256' } else { 'X86Sha256' }
    foreach ($pair in @(@($DllPath,$ExpectedDllSha256,$dllKey), @((Join-Path $directory 'FBE.Sequence.propdesc'),$ExpectedSchemaSha256,'SchemaSha256'))) {
        if ($pair[1] -notmatch '^[0-9a-fA-F]{64}$' -or -not [string]::Equals($owner[$pair[2]],$pair[1],[StringComparison]::OrdinalIgnoreCase)) { return $false }
        if (([IO.File]::GetAttributes($pair[0]) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { return $false }
        $stream = [IO.File]::Open($pair[0], [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        try {
            $sha = [Security.Cryptography.SHA256]::Create()
            try { $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') }
            finally { $sha.Clear() }
        }
        finally { $stream.Close() }
        if (-not [string]::Equals($hash,$pair[1],[StringComparison]::OrdinalIgnoreCase)) { return $false }
    }
    return $true
}

function Remove-MachineKey {
    param($BaseKey, [string]$Path)
    $key = $BaseKey.OpenSubKey($Path, $false)
    if ($null -ne $key) { $key.Close(); $BaseKey.DeleteSubKeyTree($Path) }
}

function Remove-OwnedSchema {
    if (-not $UnregisterSchema) { return }
    if (-not ('FbePropertySchemaUnregisterNative' -as [type])) {
        Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;
public static class FbePropertySchemaUnregisterNative {
    [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
    public static extern int PSUnregisterPropertySchema(string path);
    [DllImport("propsys.dll")]
    public static extern int PSRefreshPropertySchema();
}
"@
    }
    $path = Join-Path (Split-Path -Parent $DllPath) 'FBE.Sequence.propdesc'
    $hr = [FbePropertySchemaUnregisterNative]::PSUnregisterPropertySchema($path)
    if ($hr -lt 0) { throw "PSUnregisterPropertySchema failed: Path=$path; HRESULT=$hr" }
    $hr = [FbePropertySchemaUnregisterNative]::PSRefreshPropertySchema()
    if ($hr -lt 0) { throw "PSRefreshPropertySchema failed: HRESULT=$hr" }
}

function Test-IsAdministrator {
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Open-MachineRegistry {
    $os64 = [IntPtr]::Size -eq 8 -or -not [string]::IsNullOrEmpty($env:PROCESSOR_ARCHITEW6432)
    if (($Platform -eq 'x64' -and [IntPtr]::Size -ne 8) -or ($Platform -eq 'Win32' -and [IntPtr]::Size -eq 8)) {
        throw "Registry architecture mismatch: Platform=$Platform; ProcessBits=$([IntPtr]::Size * 8). Run the matching Windows PowerShell executable."
    }
    if ($Platform -eq 'x64' -and -not $os64) { throw 'Cannot register x64 COM components on 32-bit Windows.' }
    return [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey('', $true)
}

function Get-ComClassRegistryPath {
    param(
        [Parameter(Mandatory)]
        [string]$Clsid
    )

    return "SOFTWARE\Classes\CLSID\$Clsid"
}

function Remove-ComClassRegistration {
    param(
        [Parameter(Mandatory)]
        [string]$Clsid
    )

    $baseKey = Open-MachineRegistry

    try {
        Remove-MachineKey $baseKey (Get-ComClassRegistryPath -Clsid $Clsid)
    }
    finally {
        $baseKey.Close()
    }
}

function Test-ComClassRegistrationExists {
    param(
        [Parameter(Mandatory)]
        [string]$Clsid
    )

    $baseKey = Open-MachineRegistry

    try {
        $clsidKey = $baseKey.OpenSubKey((Get-ComClassRegistryPath -Clsid $Clsid), $false)
        if ($null -eq $clsidKey) {
            return $false
        }

        $clsidKey.Close()
        return $true
    }
    finally {
        $baseKey.Close()
    }
}

function Test-RegistryDefaultValue {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$ExpectedValue
    )

    $baseKey = Open-MachineRegistry
    try {
        $key = $baseKey.OpenSubKey($Path, $false)
        if ($null -eq $key) { return $false }
        try {
            $actual = $key.GetValue('', $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            return [string]::Equals([string]$actual, $ExpectedValue, [System.StringComparison]::OrdinalIgnoreCase)
        }
        finally { $key.Close() }
    }
    finally { $baseKey.Close() }
}

function Test-ComClassRegistrationOwned {
    param(
        [Parameter(Mandatory)][string]$Clsid,
        [Parameter(Mandatory)][string]$ExpectedDllPath
    )

    $baseKey = Open-MachineRegistry
    try {
        $key = $baseKey.OpenSubKey("$(Get-ComClassRegistryPath -Clsid $Clsid)\InprocServer32", $false)
        if ($null -eq $key) { return $false }
        try {
            $actual = $key.GetValue('', $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            return [string]::Equals([string]$actual, $ExpectedDllPath, [System.StringComparison]::OrdinalIgnoreCase)
        }
        finally { $key.Close() }
    }
    finally { $baseKey.Close() }
}

function Test-ModernRegistrationOwned {
    param(
        [Parameter(Mandatory)][string]$PropertyHandlerClsid,
        [Parameter(Mandatory)][string]$ThumbnailProviderClsid,
        [Parameter(Mandatory)][string]$ExpectedDllPath
    )

    return (Test-RegistryDefaultValue -Path 'SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2' -ExpectedValue $PropertyHandlerClsid) -and
        (Test-RegistryDefaultValue -Path 'SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}' -ExpectedValue $ThumbnailProviderClsid) -and
        (Test-RegistryDefaultValue -Path 'SOFTWARE\Classes\FictionBook.2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}' -ExpectedValue $ThumbnailProviderClsid) -and
        (Test-ComClassRegistrationOwned -Clsid $PropertyHandlerClsid -ExpectedDllPath $ExpectedDllPath) -and
        (Test-ComClassRegistrationOwned -Clsid $ThumbnailProviderClsid -ExpectedDllPath $ExpectedDllPath)
}

function Remove-PropertyHandlerRegistration {
    $baseKey = Open-MachineRegistry

    try {
        Remove-MachineKey $baseKey "SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2"
    }
    finally {
        $baseKey.Close()
    }
}

function Remove-ThumbnailProviderRegistration {
    $baseKey = Open-MachineRegistry

    try {
        Remove-MachineKey $baseKey "SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
        Remove-MachineKey $baseKey "SOFTWARE\Classes\FictionBook.2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
    }
    finally {
        $baseKey.Close()
    }
}

function Test-PropertyHandlerRegistrationExists {
    $baseKey = Open-MachineRegistry

    try {
        $propertyHandlersKey = $baseKey.OpenSubKey(
            "SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2",
            $false
        )
        if ($null -eq $propertyHandlersKey) {
            return $false
        }

        $propertyHandlersKey.Close()
        return $true
    }
    finally {
        $baseKey.Close()
    }
}

function Test-ThumbnailProviderRegistrationExists {
    $baseKey = Open-MachineRegistry

    try {
        $thumbnailHandlerKey = $baseKey.OpenSubKey(
            "SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}",
            $false
        )
        if ($null -ne $thumbnailHandlerKey) {
            $thumbnailHandlerKey.Close()
            return $true
        }

        $thumbnailHandlerProgIdKey = $baseKey.OpenSubKey(
            "SOFTWARE\Classes\FictionBook.2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}",
            $false
        )
        if ($null -ne $thumbnailHandlerProgIdKey) {
            $thumbnailHandlerProgIdKey.Close()
            return $true
        }

        return $false
    }
    finally {
        $baseKey.Close()
    }
}

try {
    $os64 = [IntPtr]::Size -eq 8 -or -not [string]::IsNullOrEmpty($env:PROCESSOR_ARCHITEW6432)
    $runtime = ''
    if ($Platform -eq 'Win32' -and [IntPtr]::Size -eq 8) { $runtime = "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" }
    if ($Platform -eq 'x64' -and [IntPtr]::Size -eq 4) {
        if (-not $os64) { throw 'Cannot use x64 COM components on 32-bit Windows.' }
        $runtime = "$env:WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
    }
    if ($runtime) {
        if (-not (Test-Path -LiteralPath $runtime -PathType Leaf)) { throw "Matching Windows PowerShell runtime missing: $runtime" }
        $relay = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$runtimeScript)
        foreach ($parameter in $PSBoundParameters.GetEnumerator()) {
            if ($parameter.Value -is [Management.Automation.SwitchParameter]) {
                if ($parameter.Value.IsPresent) { $relay += '-' + $parameter.Key }
            }
            else { $relay += '-' + $parameter.Key; $relay += [string]$parameter.Value }
        }
        & $runtime @relay
        if ($LASTEXITCODE -ne 0) { throw "COM helper failed: Platform=$Platform; Runtime=$runtime; ExitCode=$LASTEXITCODE" }
        return
    }
    if (-not (Test-IsAdministrator)) {
        Write-Status -Result "error" -Step "Test-IsAdministrator" -Code "ACCESS_DENIED" -Message "Administrator rights are required."
        throw "Administrator rights are required to unregister the modern property handler."
    }

    $propertyHandlerClsid = "{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}"
    $thumbnailProviderClsid = "{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}"

    # A shared CLSID or extension may legitimately be replaced by a newer
    # instance.  Never let an older uninstaller erase that instance's shell
    # registration merely because it knows the same identifiers.
    if (-not (Test-PackageOwner)) {
        Write-Status -Result "skipped" -Step "ownership" -Code "FOREIGN_PACKAGE" -Message "Shared payload belongs to another installation or package; retained."
        return
    }
    if (-not (Test-ModernRegistrationOwned -PropertyHandlerClsid $propertyHandlerClsid -ThumbnailProviderClsid $thumbnailProviderClsid -ExpectedDllPath $DllPath)) {
        Write-Status -Result "skipped" -Step "ownership" -Code "FOREIGN_REGISTRATION" -Message "Modern shell registration no longer points to this installation; it was left untouched."
        Write-Host "Modern shell registration belongs to another installation; skipping cleanup."
        return
    }

    # Registry ownership has been checked above. Removing those owned keys
    # directly avoids loading a DLL from an older, possibly writable path.
    Remove-OwnedSchema
    Remove-PropertyHandlerRegistration
    Remove-ThumbnailProviderRegistration
    Remove-ComClassRegistration -Clsid $propertyHandlerClsid
    Remove-ComClassRegistration -Clsid $thumbnailProviderClsid

    if ((Test-PropertyHandlerRegistrationExists) -or
        (Test-ThumbnailProviderRegistrationExists) -or
        (Test-ComClassRegistrationExists -Clsid $propertyHandlerClsid) -or
        (Test-ComClassRegistrationExists -Clsid $thumbnailProviderClsid)) {
        Write-Status -Result "error" -Step "cleanup" -Code "REGISTRY_NOT_CLEANED" -Message "Modern shell registry entries are still present after cleanup."
        throw "Modern shell registry entries are still present after cleanup."
    }

    Write-Status -Result "ok" -Step "done" -Code "0x00000000" -Message "Modern property handler and thumbnail provider have been unregistered."
    Write-Host "Modern property handler and thumbnail provider have been unregistered."
}
catch {
    if (-not (Test-Blank $StatusFilePath)) {
        Write-EmergencyStatus -Step "exception" -Code "EXIT_1" -Message $_.Exception.Message
    }
    throw
}
