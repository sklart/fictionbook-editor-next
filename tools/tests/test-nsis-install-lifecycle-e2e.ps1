<#
.SYNOPSIS
Runs the real NSIS lifecycle on a clean Windows test machine.

.DESCRIPTION
Unlike test-nsis-install-scopes.ps1 this test never uses the probe build.
It deliberately refuses to overwrite an existing installation.  Run it from
an elevated PowerShell session: that is required to exercise the HKLM and
Program Files (x86) branch.
#>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$InstallerPath)

$ErrorActionPreference = 'Stop'
$installer = (Resolve-Path -LiteralPath $InstallerPath).Path
$product = 'FictionBook Editor Next'
$currentDir = Join-Path $env:LOCALAPPDATA "Programs\$product"
$machineDir = Join-Path ${env:ProgramFiles(x86)} $product
$currentKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$product"
$machineKey = "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$product"
$portableDir = Join-Path $PSScriptRoot "..\..\out\tests\nsis-lifecycle-portable"
$shellKeys = @('HKCU:\Software\Classes\FictionBook.2', 'HKCU:\Software\Classes\.fb2', 'HKLM:\Software\Classes\FictionBook.2', 'HKLM:\Software\Classes\.fb2', 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2', 'HKLM:\Software\Classes\CLSID\{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}', 'HKLM:\Software\Classes\CLSID\{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}\InprocServer32', 'HKLM:\Software\Classes\CLSID\{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}', 'HKLM:\Software\Classes\CLSID\{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}\InprocServer32', 'HKLM:\Software\Classes\.fb2\ShellEx', 'HKLM:\Software\Classes\FictionBook.2\ShellEx')
$shellFiles = @((Join-Path $env:ProgramData 'FictionBook Editor Next\Shell\FBShell.dll'), (Join-Path $env:ProgramData 'FictionBook Editor Next\Shell\FBShell64.dll'), (Join-Path $env:ProgramData 'FictionBook Editor Next\Shell\FBE.Sequence.propdesc'))

function Assert([bool]$Value, [string]$Message) { if (-not $Value) { throw $Message } }
function Invoke-Setup([string[]]$Arguments, [int[]]$Allowed = @(0)) {
    $p = Start-Process -FilePath $installer -ArgumentList $Arguments -Wait -PassThru -WindowStyle Hidden
    Assert ($Allowed -contains $p.ExitCode) "setup $Arguments returned $($p.ExitCode)."
    return $p.ExitCode
}
function Invoke-Uninstall([string]$Directory) {
    $uninstaller = Join-Path $Directory 'uninst.exe'
    Assert (Test-Path -LiteralPath $uninstaller -PathType Leaf) "Missing uninstaller: $uninstaller"
    $p = Start-Process -FilePath $uninstaller -ArgumentList '/S' -Wait -PassThru -WindowStyle Hidden
    Assert ($p.ExitCode -eq 0) "uninstall $Directory returned $($p.ExitCode)."
}
function Get-RegistryState([string]$Key) {
    if (-not (Test-Path -LiteralPath $Key)) { return '<absent>' }
    return (Get-ItemProperty -LiteralPath $Key | Out-String)
}
function Get-FileState([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return '<absent>' }
    return ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash + ':' + (Get-Item -LiteralPath $Path).Length)
}

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
Assert ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) 'Run the lifecycle E2E from an elevated PowerShell session.'
foreach ($path in @($currentDir, $machineDir, $portableDir)) { Assert (-not (Test-Path -LiteralPath $path)) "Refusing to overwrite existing installation: $path" }
foreach ($key in @($currentKey, $machineKey)) { Assert (-not (Test-Path -LiteralPath $key)) "Refusing to overwrite existing uninstall record: $key" }

try {
    # Current User and upgrade stay in LocalAppData/HKCU and require no UAC.
    Invoke-Setup @('/S', '/CURRENTUSER')
    Assert (Test-Path (Join-Path $currentDir 'FBE.exe')) 'Current User installation missing FBE.exe.'
    Assert ((Get-ItemProperty -LiteralPath $currentKey).InstallScope -eq 'current') 'Current User installation did not write HKCU scope.'
    Assert (-not (Test-Path -LiteralPath $machineKey)) 'Current User installation wrote HKLM.'
    Invoke-Setup @('/S', '/CURRENTUSER') # same-scope upgrade

    # The other installed scope must fail before it creates any HKLM state.
    Invoke-Setup @('/S', '/ALLUSERS') @(183)
    Assert (-not (Test-Path -LiteralPath $machineDir)) 'Cross-scope conflict created Program Files installation.'
    Assert (-not (Test-Path -LiteralPath $machineKey)) 'Cross-scope conflict created HKLM uninstall state.'
    Invoke-Uninstall $currentDir
    Assert (-not (Test-Path -LiteralPath $currentDir)) 'Current User uninstall left program files.'
    Assert (-not (Test-Path -LiteralPath $currentKey)) 'Current User uninstall left HKCU record.'

    # All Users uses Program Files (x86)/HKLM and supports a same-scope upgrade.
    Invoke-Setup @('/S', '/ALLUSERS')
    Assert (Test-Path (Join-Path $machineDir 'FBE.exe')) 'All Users installation missing FBE.exe.'
    Assert ((Get-ItemProperty -LiteralPath $machineKey).InstallScope -eq 'allusers') 'All Users installation did not write HKLM scope.'
    Assert (-not (Test-Path -LiteralPath $currentKey)) 'All Users installation wrote HKCU.'
    Invoke-Setup @('/S', '/ALLUSERS')
    Invoke-Setup @('/S', '/CURRENTUSER') @(183)
    Assert (-not (Test-Path -LiteralPath $currentDir)) 'Reverse cross-scope conflict created LocalAppData installation.'
    Assert (-not (Test-Path -LiteralPath $currentKey)) 'Reverse cross-scope conflict created HKCU uninstall state.'
    Invoke-Uninstall $machineDir
    Assert (-not (Test-Path -LiteralPath $machineDir)) 'All Users uninstall left Program Files installation.'
    Assert (-not (Test-Path -LiteralPath $machineKey)) 'All Users uninstall left HKLM record.'

    # /D must be final for NSIS. Portable must not create uninstall or shell state.
    $shellBefore = @{}; foreach ($key in $shellKeys) { $shellBefore[$key] = Get-RegistryState $key }
    $shellFilesBefore = @{}; foreach ($path in $shellFiles) { $shellFilesBefore[$path] = Get-FileState $path }
    Invoke-Setup @('/S', '/PORTABLE', "/D=$portableDir")
    Assert (Test-Path (Join-Path $portableDir 'portable.ini')) 'Portable installation missing portable.ini.'
    Assert (-not (Test-Path (Join-Path $portableDir 'uninst.exe'))) 'Portable installation created uninst.exe.'
    Assert (-not (Test-Path -LiteralPath $currentKey)) 'Portable installation created HKCU uninstall record.'
    Assert (-not (Test-Path -LiteralPath $machineKey)) 'Portable installation created HKLM uninstall record.'
    foreach ($key in $shellKeys) { Assert ((Get-RegistryState $key) -ceq $shellBefore[$key]) "Portable installation changed shell registration: $key" }
    foreach ($path in $shellFiles) { Assert ((Get-FileState $path) -ceq $shellFilesBefore[$path]) "Portable installation changed shell file: $path" }

    foreach ($invalid in @(@('/S','/CURRENTUSER','/ALLUSERS'), @('/S','/CURRENTUSER','/PORTABLE'), @('/S','/ALLUSERS','/PORTABLE'))) {
        Invoke-Setup $invalid @(87)
    }
    Write-Host 'NSIS Current User, All Users and Portable lifecycle E2E passed.'
}
finally {
    foreach ($dir in @($currentDir, $machineDir)) {
        $uninstaller = Join-Path $dir 'uninst.exe'
        if (Test-Path -LiteralPath $uninstaller) { Start-Process -FilePath $uninstaller -ArgumentList '/S' -Wait -WindowStyle Hidden -ErrorAction SilentlyContinue | Out-Null }
    }
    if (Test-Path -LiteralPath $portableDir) { Remove-Item -LiteralPath $portableDir -Recurse -Force -ErrorAction SilentlyContinue }
}
