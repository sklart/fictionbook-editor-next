[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('Update','Remove','Inspect','Recover')][string]$Operation,
    [string]$Directory='',
    [Parameter(Mandatory=$true)][string]$InstallationDirectory,
    [string]$PayloadDirectory='',
    [string]$LegacyDirectories=($env:ProgramData+'\FictionBook Editor Next\Shell|'+$env:ProgramData+'\FictionBook Editor\Shell'),
    [string]$ExpectedX86Sha256='',
    [string]$ExpectedX64Sha256='',
    [string]$ExpectedSchemaSha256='',
    [int]$PreviousProperty=0,
    [int]$PreviousSystem=0,
    [string]$PreviousInstallLocation='',
    [string]$StatusFilePath=''
)
$ErrorActionPreference='Stop'
$toolsDirectory=Split-Path -Parent $MyInvocation.MyCommand.Path
if(-not $StatusFilePath){$StatusFilePath=Join-Path $toolsDirectory 'Status.ini'}
if(-not $PayloadDirectory){$PayloadDirectory=Join-Path $toolsDirectory 'Payload'}
if(-not $Directory){
    $parent=Split-Path -Parent $toolsDirectory
    if([IO.Path]::GetFileName($parent) -ine 'Maintenance'){throw 'An explicit shell Directory is required outside the packaged maintenance folder.'}
    $Directory=Join-Path (Split-Path -Parent $parent) 'Shell'
}
$previousPath=Join-Path $toolsDirectory 'Previous.ini'
if(-not $PreviousInstallLocation -and [IO.File]::Exists($previousPath)){
    foreach($line in [IO.File]::ReadAllLines($previousPath)){if($line.StartsWith('InstallLocation=')){$PreviousInstallLocation=$line.Substring(16)}}
}
. (Join-Path $toolsDirectory 'shell-integration-lifecycle.ps1')
$result=New-ShellResult 'error' 'INITIALIZATION_FAILED' ($PreviousProperty -eq 1) $false $true 'Integration maintenance did not start.'
try {
    [IO.File]::WriteAllLines($StatusFilePath,[string[]]@('[Shell]','Result=pending','Code=STARTING'),[Text.Encoding]::Unicode)
    $principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Administrator rights are required for system integration.'}
    $platform='Win32';$nativeName='FBShell.dll';$index=0
    if([IntPtr]::Size -eq 8){$platform='x64';$nativeName='FBShell64.dll';$index=1}
    elseif(-not [string]::IsNullOrEmpty($env:PROCESSOR_ARCHITEW6432)){throw 'Run system integration maintenance in native 64-bit Windows PowerShell.'}
    $hashes=@($ExpectedX86Sha256,$ExpectedX64Sha256,$ExpectedSchemaSha256)
    if($Operation -eq 'Update'){foreach($hash in $hashes){if($hash -notmatch '^[0-9a-fA-F]{64}$'){throw 'Missing package SHA-256.'}}}
    $context=@{Directory=[IO.Path]::GetFullPath($Directory);InstallationDirectory=[IO.Path]::GetFullPath($InstallationDirectory).TrimEnd('\');PayloadDirectory=$PayloadDirectory;ToolsDirectory=$toolsDirectory;Hashes=$hashes;Platform=$platform;NativeName=$nativeName;NativeIndex=$index;LegacyDirectories=@($LegacyDirectories.Split('|') | Where-Object {$_});PreviousProperty=$PreviousProperty;PreviousSystem=$PreviousSystem;PreviousInstallLocation=$PreviousInstallLocation}
    $result=Invoke-ShellLifecycle $context $Operation
}
catch {$result.Message=$_.Exception.Message}
$lines=@('[Shell]')
foreach($name in @('Result','Code','EffectiveInstalled','CleanupPending','RecoveryPending','SystemIntegrationInstalled','Message')){$lines+=($name+'='+$result[$name])}
[IO.File]::WriteAllLines($StatusFilePath,[string[]]$lines,[Text.Encoding]::Unicode)
Write-Host ('Shell stage='+$Operation+'; result='+$result.Result+'; code='+$result.Code+'; '+$result.Message)
if($result.Result -eq 'error'){exit 1}
