[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$DllPath,

    [ValidateSet("Win32", "x64")]
    [string]$Platform = "x64",

    [string]$StatusFilePath = ""
)

$ErrorActionPreference = "Stop"

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

    if ([string]::IsNullOrWhiteSpace($StatusFilePath)) {
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
    [IO.File]::WriteAllLines($StatusFilePath, $lines, [Text.UTF8Encoding]::new($false))
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

    if ([string]::IsNullOrWhiteSpace($StatusFilePath)) {
        return
    }

    try {
        $dir = Split-Path -Parent $StatusFilePath
        if ($dir) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }

        $content = "[Shell]`r`nResult=error`r`nStep=$Step`r`nCode=$Code`r`nMessage=$Message`r`n"
        [System.IO.File]::WriteAllText($StatusFilePath, $content, [System.Text.Encoding]::UTF8)
    }
    catch {
    }
}

function Test-IsAdministrator {
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-RegSvr32Path {
    if ($Platform -eq "x64") {
        if ([Environment]::Is64BitProcess) {
            return "$env:WINDIR\System32\regsvr32.exe"
        }

        return "$env:WINDIR\sysnative\regsvr32.exe"
    }

    return "$env:WINDIR\SysWOW64\regsvr32.exe"
}

function Get-RegistryView {
    if ($Platform -eq "x64") {
        return [Microsoft.Win32.RegistryView]::Registry64
    }

    return [Microsoft.Win32.RegistryView]::Registry32
}

function Invoke-RegSvr32 {
    param(
        [Parameter(Mandatory)]
        [string]$RegSvr32Path,
        [Parameter(Mandatory)]
        [string[]]$Arguments,
        [Parameter(Mandatory)]
        [string]$WorkingDirectory
    )

    $process = Start-Process -FilePath $RegSvr32Path -ArgumentList $Arguments -WorkingDirectory $workingDirectory -Wait -PassThru
    if ($null -eq $process) {
        return -1
    }

    return [int]$process.ExitCode
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

    $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine,
        (Get-RegistryView)
    )

    try {
        $baseKey.DeleteSubKeyTree((Get-ComClassRegistryPath -Clsid $Clsid), $false)
    }
    finally {
        $baseKey.Dispose()
    }
}

function Test-ComClassRegistrationExists {
    param(
        [Parameter(Mandatory)]
        [string]$Clsid
    )

    $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine,
        (Get-RegistryView)
    )

    try {
        $clsidKey = $baseKey.OpenSubKey((Get-ComClassRegistryPath -Clsid $Clsid), $false)
        if ($null -eq $clsidKey) {
            return $false
        }

        $clsidKey.Dispose()
        return $true
    }
    finally {
        $baseKey.Dispose()
    }
}

function Remove-PropertyHandlerRegistration {
    $registryView = if ($Platform -eq "x64") {
        [Microsoft.Win32.RegistryView]::Registry64
    } else {
        [Microsoft.Win32.RegistryView]::Registry32
    }

    $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine,
        $registryView
    )

    try {
        $baseKey.DeleteSubKeyTree(
            "SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2",
            $false
        )
    }
    finally {
        $baseKey.Dispose()
    }
}

function Remove-ThumbnailProviderRegistration {
    $registryView = if ($Platform -eq "x64") {
        [Microsoft.Win32.RegistryView]::Registry64
    } else {
        [Microsoft.Win32.RegistryView]::Registry32
    }

    $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine,
        $registryView
    )

    try {
        $baseKey.DeleteSubKeyTree(
            "SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}",
            $false
        )
        $baseKey.DeleteSubKeyTree(
            "SOFTWARE\Classes\FictionBook.2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}",
            $false
        )
    }
    finally {
        $baseKey.Dispose()
    }
}

function Test-PropertyHandlerRegistrationExists {
    $registryView = if ($Platform -eq "x64") {
        [Microsoft.Win32.RegistryView]::Registry64
    } else {
        [Microsoft.Win32.RegistryView]::Registry32
    }

    $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine,
        $registryView
    )

    try {
        $propertyHandlersKey = $baseKey.OpenSubKey(
            "SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2",
            $false
        )
        if ($null -eq $propertyHandlersKey) {
            return $false
        }

        $propertyHandlersKey.Dispose()
        return $true
    }
    finally {
        $baseKey.Dispose()
    }
}

function Test-ThumbnailProviderRegistrationExists {
    $registryView = if ($Platform -eq "x64") {
        [Microsoft.Win32.RegistryView]::Registry64
    } else {
        [Microsoft.Win32.RegistryView]::Registry32
    }

    $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine,
        $registryView
    )

    try {
        $thumbnailHandlerKey = $baseKey.OpenSubKey(
            "SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}",
            $false
        )
        if ($null -ne $thumbnailHandlerKey) {
            $thumbnailHandlerKey.Dispose()
            return $true
        }

        $thumbnailHandlerProgIdKey = $baseKey.OpenSubKey(
            "SOFTWARE\Classes\FictionBook.2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}",
            $false
        )
        if ($null -ne $thumbnailHandlerProgIdKey) {
            $thumbnailHandlerProgIdKey.Dispose()
            return $true
        }

        return $false
    }
    finally {
        $baseKey.Dispose()
    }
}

try {
    if (-not (Test-IsAdministrator)) {
        Write-Status -Result "error" -Step "Test-IsAdministrator" -Code "ACCESS_DENIED" -Message "Administrator rights are required."
        throw "Administrator rights are required to unregister the modern property handler."
    }

    $propertyHandlerClsid = "{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}"
    $thumbnailProviderClsid = "{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}"
    $regsvr32ExitCode = 0
    $regsvr32FailureMessage = ""

    if (Test-Path -LiteralPath $DllPath -PathType Leaf) {
        $regsvr32 = Get-RegSvr32Path
        $workingDirectory = Split-Path -Parent $DllPath
        Write-Host "Unregistering modern property handler via regsvr32:"
        Write-Host "  $regsvr32"
        Write-Host "  $DllPath"
        $regsvr32ExitCode = Invoke-RegSvr32 -RegSvr32Path $regsvr32 -Arguments @("/u", "/s", ('"{0}"' -f $DllPath)) -WorkingDirectory $workingDirectory
        if ($regsvr32ExitCode -ne 0) {
            $regsvr32FailureMessage = "regsvr32 /u failed. FilePath=$regsvr32; DllPath=$DllPath; ExitCode=$regsvr32ExitCode; Is64BitProcess=$([Environment]::Is64BitProcess); WorkingDirectory=$workingDirectory"
            Write-Host "regsvr32 /u returned a non-zero exit code; finishing cleanup via direct registry removal."
        }
    }

    Remove-PropertyHandlerRegistration
    Remove-ThumbnailProviderRegistration
    Remove-ComClassRegistration -Clsid $propertyHandlerClsid
    Remove-ComClassRegistration -Clsid $thumbnailProviderClsid

    if ((Test-PropertyHandlerRegistrationExists) -or
        (Test-ThumbnailProviderRegistrationExists) -or
        (Test-ComClassRegistrationExists -Clsid $propertyHandlerClsid) -or
        (Test-ComClassRegistrationExists -Clsid $thumbnailProviderClsid)) {
        if ($regsvr32ExitCode -ne 0 -and -not [string]::IsNullOrWhiteSpace($regsvr32FailureMessage)) {
            Write-Status -Result "error" -Step "regsvr32" -Code ("EXIT_{0}" -f $regsvr32ExitCode) -Message $regsvr32FailureMessage
            throw "regsvr32 /u failed with exit code $regsvr32ExitCode, and registry cleanup did not complete."
        }

        Write-Status -Result "error" -Step "cleanup" -Code "REGISTRY_NOT_CLEANED" -Message "Modern shell registry entries are still present after cleanup."
        throw "Modern shell registry entries are still present after cleanup."
    }

    Write-Status -Result "ok" -Step "done" -Code "0x00000000" -Message "Modern property handler and thumbnail provider have been unregistered."
    Write-Host "Modern property handler and thumbnail provider have been unregistered."
}
catch {
    if (-not [string]::IsNullOrWhiteSpace($StatusFilePath) -and -not (Test-Path -LiteralPath $StatusFilePath)) {
        Write-EmergencyStatus -Step "exception" -Code "EXIT_1" -Message $_.Exception.Message
    }
    throw
}
