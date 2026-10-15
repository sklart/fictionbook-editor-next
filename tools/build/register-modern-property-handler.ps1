[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$DllPath,

    [string]$ExpectedDllSha256 = "",

    [ValidateSet("Win32", "x64")]
    [string]$Platform = "x64",

    [string]$PropertyHandlerClsid = "{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}",

    [string]$ThumbnailProviderClsid = "{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}",

    [string]$StatusFilePath = ""
)

$ErrorActionPreference = "Stop"
$runtimeScript = $MyInvocation.MyCommand.Path

function Test-Blank { param([string]$Value) return (-not $Value -or $Value.Trim().Length -eq 0) }


function Assert-PackageHash {
    param([string]$Path, [string]$Expected)
    if ($Expected -notmatch '^[0-9a-fA-F]{64}$') { throw 'Missing package SHA-256 for shell DLL.' }
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
        finally { $sha.Clear() }
    }
    finally { $stream.Close() }
    if (-not [string]::Equals($actual, $Expected, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Shell DLL does not match the trusted installer package.'
    }
}

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

function Test-IsAdministrator {
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-RegSvr32Path {
    param(
        [bool]$Is64BitOS = ([IntPtr]::Size -eq 8 -or -not [string]::IsNullOrEmpty($env:PROCESSOR_ARCHITEW6432)),
        [bool]$Is64BitProcess = ([IntPtr]::Size -eq 8),
        [string]$WindowsDirectory = $env:WINDIR
    )
    if ($Platform -eq 'x64' -and -not $Is64BitOS) { throw 'Cannot register x64 DLL on 32-bit Windows.' }
    $directory = 'System32'
    if ($Is64BitOS -and $Platform -eq 'Win32') { $directory = 'SysWOW64' }
    elseif ($Is64BitOS -and -not $Is64BitProcess) { $directory = 'Sysnative' }
    $path = Join-Path $WindowsDirectory ($directory + '\regsvr32.exe')
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "COM registration executable missing: Path=$path; Platform=$Platform; OS64=$Is64BitOS; Process64=$Is64BitProcess" }
    return $path
}

function Open-MachineRegistry {
    $os64 = [IntPtr]::Size -eq 8 -or -not [string]::IsNullOrEmpty($env:PROCESSOR_ARCHITEW6432)
    if (($Platform -eq 'x64' -and [IntPtr]::Size -ne 8) -or ($Platform -eq 'Win32' -and [IntPtr]::Size -eq 8)) {
        throw "Registry architecture mismatch: Platform=$Platform; ProcessBits=$([IntPtr]::Size * 8). Run the matching Windows PowerShell executable."
    }
    if ($Platform -eq 'x64' -and -not $os64) { throw 'Cannot register x64 COM components on 32-bit Windows.' }
    return [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey('', $true)
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

    $process = Start-Process -FilePath $RegSvr32Path -ArgumentList $Arguments -WorkingDirectory $workingDirectory -Wait -PassThru -WindowStyle Hidden
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

    $baseKey = Open-MachineRegistry

    try {
        $inprocPath = "{0}\InprocServer32" -f (Get-ComClassRegistryPath -Clsid $Clsid)
        $inprocKey = $baseKey.OpenSubKey($inprocPath, $false)
        if ($null -eq $inprocKey) {
            return $false
        }

        try {
            $registeredDllPath = $inprocKey.GetValue("", "", [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            if ((Test-Blank $registeredDllPath)) {
                return $false
            }

            return [string]::Equals($registeredDllPath, $ExpectedDllPath, [System.StringComparison]::OrdinalIgnoreCase)
        }
        finally {
            $inprocKey.Close()
        }
    }
    finally {
        $baseKey.Close()
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

    $baseKey = Open-MachineRegistry

    try {
        $clsidRegistryPath = Get-ComClassRegistryPath -Clsid $Clsid
        $clsidKey = $baseKey.CreateSubKey($clsidRegistryPath)
        try {
            $clsidKey.SetValue("", $DisplayName, [Microsoft.Win32.RegistryValueKind]::String)

            $programmableKey = $clsidKey.CreateSubKey("Programmable")
            if ($null -ne $programmableKey) {
                $programmableKey.Close()
            }

            $inprocKey = $clsidKey.CreateSubKey("InprocServer32")
            try {
                $inprocKey.SetValue("", $ModulePath, [Microsoft.Win32.RegistryValueKind]::String)
                $inprocKey.SetValue("ThreadingModel", $ThreadingModel, [Microsoft.Win32.RegistryValueKind]::String)
            }
            finally {
                $inprocKey.Close()
            }
        }
        finally {
            $clsidKey.Close()
        }
    }
    finally {
        $baseKey.Close()
    }
}

function Set-PropertyHandlerRegistration {
    $baseKey = Open-MachineRegistry

    try {
        $propertyHandlersKey = $baseKey.CreateSubKey(
            "SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2"
        )
        try {
            $propertyHandlersKey.SetValue("", $PropertyHandlerClsid, [Microsoft.Win32.RegistryValueKind]::String)
        }
        finally {
            $propertyHandlersKey.Close()
        }
    }
    finally {
        $baseKey.Close()
    }
}

function Set-ThumbnailProviderRegistration {
    $baseKey = Open-MachineRegistry

    try {
        $thumbnailHandlerPath = "SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
        $thumbnailHandlerKey = $baseKey.CreateSubKey($thumbnailHandlerPath)
        try {
            $thumbnailHandlerKey.SetValue("", $ThumbnailProviderClsid, [Microsoft.Win32.RegistryValueKind]::String)
        }
        finally {
            $thumbnailHandlerKey.Close()
        }

        $thumbnailHandlerProgIdPath = "SOFTWARE\Classes\FictionBook.2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
        $thumbnailHandlerProgIdKey = $baseKey.CreateSubKey($thumbnailHandlerProgIdPath)
        try {
            $thumbnailHandlerProgIdKey.SetValue("", $ThumbnailProviderClsid, [Microsoft.Win32.RegistryValueKind]::String)
        }
        finally {
            $thumbnailHandlerProgIdKey.Close()
        }
    }
    finally {
        $baseKey.Close()
    }
}

function Ensure-ThumbnailProviderComRegistration {
    if (Get-ComClassRegistrationState -Clsid $ThumbnailProviderClsid -ExpectedDllPath $DllPath) {
        return
    }

    Write-Host "COM-класс thumbnail provider не зарегистрирован в ожидаемый DLL; достраиваем регистрацию вручную."
    Set-ComClassRegistrationFallback `
        -Clsid $ThumbnailProviderClsid `
        -DisplayName "FictionBook Thumbnail Provider" `
        -ThreadingModel "Both" `
        -ModulePath $DllPath

    if (-not (Get-ComClassRegistrationState -Clsid $ThumbnailProviderClsid -ExpectedDllPath $DllPath)) {
        Write-Status -Result "error" -Step "thumbnail-provider-com" -Code "CLASS_NOT_REGISTERED" -Message "Thumbnail provider COM class was not registered."
        throw "Thumbnail provider COM class was not registered."
    }
}

$registrationStep = 'validate-payload'
try {
    $os64 = [IntPtr]::Size -eq 8 -or -not [string]::IsNullOrEmpty($env:PROCESSOR_ARCHITEW6432)
    $runtime = ''
    if ($Platform -eq 'Win32' -and [IntPtr]::Size -eq 8) { $runtime = "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" }
    if ($Platform -eq 'x64' -and [IntPtr]::Size -eq 4) {
        if (-not $os64) { throw 'Cannot use x64 COM components on 32-bit Windows.' }
        $runtime = "$env:WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
    }
    if ($runtime) {
        $registrationStep = 'select-runtime'
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
    if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
        Write-Status -Result "error" -Step "Test-Path" -Code "FILE_NOT_FOUND" -Message "Property handler DLL was not found."
        throw "Property handler DLL was not found: $DllPath"
    }

    $registrationStep = 'validate-package-hash'
    if ($PSBoundParameters.ContainsKey('ExpectedDllSha256')) {
        Assert-PackageHash -Path $DllPath -Expected $ExpectedDllSha256
    }

    $registrationStep = 'administrator'
    if (-not (Test-IsAdministrator)) {
        Write-Status -Result "error" -Step "Test-IsAdministrator" -Code "ACCESS_DENIED" -Message "Administrator rights are required."
        throw "Administrator rights are required to register the modern property handler."
    }

    $registrationStep = 'select-regsvr32'
    $regsvr32 = Get-RegSvr32Path
    $registrationStep = 'regsvr32'
    $workingDirectory = Split-Path -Parent $DllPath
    Write-Host "Registering modern property handler via regsvr32:"
    Write-Host "  $regsvr32"
    Write-Host "  $DllPath"
    $regsvr32ExitCode = Invoke-RegSvr32 -RegSvr32Path $regsvr32 -Arguments @("/s", ('"{0}"' -f $DllPath)) -WorkingDirectory $workingDirectory
    if ($regsvr32ExitCode -ne 0) {
        $message = "regsvr32 failed. FilePath=$regsvr32; DllPath=$DllPath; ExitCode=$regsvr32ExitCode; Platform=$Platform; Is64BitProcess=$(([IntPtr]::Size -eq 8)); WorkingDirectory=$workingDirectory"
        Write-Host $message
        if (Get-ComClassRegistrationState -Clsid $PropertyHandlerClsid -ExpectedDllPath $DllPath) {
            Write-Host "regsvr32 returned a non-zero exit code, but the COM class is already registered to the target DLL."
        }
        else {
            Write-Host "regsvr32 failed; applying fallback COM registration for the property handler class."
            Set-ComClassRegistrationFallback -Clsid $PropertyHandlerClsid -DisplayName "FictionBook Property Handler" -ThreadingModel "Both" -ModulePath $DllPath

            if (-not (Get-ComClassRegistrationState -Clsid $PropertyHandlerClsid -ExpectedDllPath $DllPath)) {
                Write-Status -Result "error" -Step "regsvr32" -Code ("EXIT_{0}" -f $regsvr32ExitCode) -Message $message
                throw "regsvr32 failed with exit code $regsvr32ExitCode."
            }
        }
    }

    $registrationStep = 'registry'
    Set-PropertyHandlerRegistration
    Ensure-ThumbnailProviderComRegistration
    Set-ThumbnailProviderRegistration

    Write-Status -Result "ok" -Step "done" -Code "0x00000000" -Message "Modern property handler and thumbnail provider have been registered."
    Write-Host "Modern property handler and thumbnail provider have been registered."
}
catch {
    if (-not (Test-Blank $StatusFilePath)) {
        Write-EmergencyStatus -Step $registrationStep -Code "EXIT_1" -Message ("Platform=$Platform; DllPath=$DllPath; " + $_.Exception.Message)
    }
    throw
}
