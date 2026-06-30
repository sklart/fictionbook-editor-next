[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$DllPath,

    [ValidateSet("Win32", "x64")]
    [string]$Platform = "x64",

    [string]$PropertyHandlerClsid = "{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}",

    [string]$ThumbnailProviderClsid = "{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}",

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

function Get-ComClassRegistrationState {
    param(
        [Parameter(Mandatory)]
        [string]$Clsid,
        [Parameter(Mandatory)]
        [string]$ExpectedDllPath
    )

    $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine,
        (Get-RegistryView)
    )

    try {
        $inprocPath = "{0}\InprocServer32" -f (Get-ComClassRegistryPath -Clsid $Clsid)
        $inprocKey = $baseKey.OpenSubKey($inprocPath, $false)
        if ($null -eq $inprocKey) {
            return $false
        }

        try {
            $registeredDllPath = $inprocKey.GetValue("", "", [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            if ([string]::IsNullOrWhiteSpace($registeredDllPath)) {
                return $false
            }

            return [string]::Equals($registeredDllPath, $ExpectedDllPath, [System.StringComparison]::OrdinalIgnoreCase)
        }
        finally {
            $inprocKey.Dispose()
        }
    }
    finally {
        $baseKey.Dispose()
    }
}

function Set-ComClassRegistrationFallback {
    param(
        [Parameter(Mandatory)]
        [string]$Clsid,
        [Parameter(Mandatory)]
        [string]$DisplayName,
        [Parameter(Mandatory)]
        [string]$ThreadingModel,
        [Parameter(Mandatory)]
        [string]$ModulePath
    )

    $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine,
        (Get-RegistryView)
    )

    try {
        $clsidRegistryPath = Get-ComClassRegistryPath -Clsid $Clsid
        $clsidKey = $baseKey.CreateSubKey($clsidRegistryPath)
        try {
            $clsidKey.SetValue("", $DisplayName, [Microsoft.Win32.RegistryValueKind]::String)

            $programmableKey = $clsidKey.CreateSubKey("Programmable")
            if ($null -ne $programmableKey) {
                $programmableKey.Dispose()
            }

            $inprocKey = $clsidKey.CreateSubKey("InprocServer32")
            try {
                $inprocKey.SetValue("", $ModulePath, [Microsoft.Win32.RegistryValueKind]::String)
                $inprocKey.SetValue("ThreadingModel", $ThreadingModel, [Microsoft.Win32.RegistryValueKind]::String)
            }
            finally {
                $inprocKey.Dispose()
            }
        }
        finally {
            $clsidKey.Dispose()
        }
    }
    finally {
        $baseKey.Dispose()
    }
}

function Set-PropertyHandlerRegistration {
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
        $propertyHandlersKey = $baseKey.CreateSubKey(
            "SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2"
        )
        try {
            $propertyHandlersKey.SetValue("", $PropertyHandlerClsid, [Microsoft.Win32.RegistryValueKind]::String)
        }
        finally {
            $propertyHandlersKey.Dispose()
        }
    }
    finally {
        $baseKey.Dispose()
    }
}

function Set-ThumbnailProviderRegistration {
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
        $thumbnailHandlerPath = "SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
        $thumbnailHandlerKey = $baseKey.CreateSubKey($thumbnailHandlerPath)
        try {
            $thumbnailHandlerKey.SetValue("", $ThumbnailProviderClsid, [Microsoft.Win32.RegistryValueKind]::String)
        }
        finally {
            $thumbnailHandlerKey.Dispose()
        }

        $thumbnailHandlerProgIdPath = "SOFTWARE\Classes\FictionBook.2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
        $thumbnailHandlerProgIdKey = $baseKey.CreateSubKey($thumbnailHandlerProgIdPath)
        try {
            $thumbnailHandlerProgIdKey.SetValue("", $ThumbnailProviderClsid, [Microsoft.Win32.RegistryValueKind]::String)
        }
        finally {
            $thumbnailHandlerProgIdKey.Dispose()
        }
    }
    finally {
        $baseKey.Dispose()
    }
}

function Ensure-ThumbnailProviderComRegistration {
    if (Get-ComClassRegistrationState -Clsid $ThumbnailProviderClsid -ExpectedDllPath $DllPath) {
        return
    }

    Write-Host "COM-класс thumbnail provider не зарегистрирован в ожидаемый DLL; достраиваем регистрацию вручную."
    Set-ComClassRegistrationFallback `
        -Clsid $ThumbnailProviderClsid `
        -DisplayName "FictionBook Modern Thumbnail Provider (Experimental)" `
        -ThreadingModel "Apartment" `
        -ModulePath $DllPath

    if (-not (Get-ComClassRegistrationState -Clsid $ThumbnailProviderClsid -ExpectedDllPath $DllPath)) {
        Write-Status -Result "error" -Step "thumbnail-provider-com" -Code "CLASS_NOT_REGISTERED" -Message "Thumbnail provider COM class was not registered."
        throw "Thumbnail provider COM class was not registered."
    }
}

try {
    if (-not (Test-IsAdministrator)) {
        Write-Status -Result "error" -Step "Test-IsAdministrator" -Code "ACCESS_DENIED" -Message "Administrator rights are required."
        throw "Administrator rights are required to register the modern property handler."
    }

    if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
        Write-Status -Result "error" -Step "Test-Path" -Code "FILE_NOT_FOUND" -Message "Property handler DLL was not found."
        throw "Property handler DLL was not found: $DllPath"
    }

    $regsvr32 = Get-RegSvr32Path
    $workingDirectory = Split-Path -Parent $DllPath
    Write-Host "Registering modern property handler via regsvr32:"
    Write-Host "  $regsvr32"
    Write-Host "  $DllPath"
    $regsvr32ExitCode = Invoke-RegSvr32 -RegSvr32Path $regsvr32 -Arguments @("/s", ('"{0}"' -f $DllPath)) -WorkingDirectory $workingDirectory
    if ($regsvr32ExitCode -ne 0) {
        $message = "regsvr32 failed. FilePath=$regsvr32; DllPath=$DllPath; ExitCode=$regsvr32ExitCode; Is64BitProcess=$([Environment]::Is64BitProcess); WorkingDirectory=$workingDirectory"
        if (Get-ComClassRegistrationState -Clsid $PropertyHandlerClsid -ExpectedDllPath $DllPath) {
            Write-Host "regsvr32 returned a non-zero exit code, but the COM class is already registered to the target DLL."
        }
        else {
            Write-Host "regsvr32 failed; applying fallback COM registration for the property handler class."
            Set-ComClassRegistrationFallback -Clsid $PropertyHandlerClsid -DisplayName "FictionBook Modern Property Handler (Experimental)" -ThreadingModel "Both" -ModulePath $DllPath

            if (-not (Get-ComClassRegistrationState -Clsid $PropertyHandlerClsid -ExpectedDllPath $DllPath)) {
                Write-Status -Result "error" -Step "regsvr32" -Code ("EXIT_{0}" -f $regsvr32ExitCode) -Message $message
                throw "regsvr32 failed with exit code $regsvr32ExitCode."
            }
        }
    }

    Set-PropertyHandlerRegistration
    Ensure-ThumbnailProviderComRegistration
    Set-ThumbnailProviderRegistration

    Write-Status -Result "ok" -Step "done" -Code "0x00000000" -Message "Modern property handler and thumbnail provider have been registered."
    Write-Host "Modern property handler and thumbnail provider have been registered."
}
catch {
    if (-not [string]::IsNullOrWhiteSpace($StatusFilePath) -and -not (Test-Path -LiteralPath $StatusFilePath)) {
        Write-EmergencyStatus -Step "exception" -Code "EXIT_1" -Message $_.Exception.Message
    }
    throw
}
