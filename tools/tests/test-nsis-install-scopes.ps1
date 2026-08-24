<# Exercises NSIS' test-only deployment branches without installing FBE. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$installerDirectory = Join-Path $root 'packaging\nsis\Installer'
$testRoot = Join-Path $root 'out\tests\nsis-install-scopes'
$inputDirectory = Join-Path $testRoot 'input'

$makensis = & (Join-Path $root 'tools\build\resolve-nsis.ps1')
if ($LASTEXITCODE -ne 0 -or -not $makensis) { throw 'Unable to resolve makensis for NSIS scope smoke.' }

function Get-RegistrySnapshot([string]$Key) {
    $output = & reg.exe query $Key /s 2>&1
    if ($LASTEXITCODE -eq 1) { return '<absent>' }
    if ($LASTEXITCODE -ne 0) { throw "reg.exe query failed for ${Key}: $output" }
    return (($output | ForEach-Object { [string]$_ }) -join "`n").Trim()
}
function Read-Probe([string]$Path) {
    $result = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        $pair = $line -split '=', 2
        if ($pair.Count -eq 2) { $result[$pair[0]] = $pair[1] }
    }
    return $result
}
function Invoke-ScopeProbe([string]$Name, [string[]]$Defines) {
    $directory = Join-Path $testRoot $Name
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $setup = Join-Path $directory 'scope-probe.exe'
    $arguments = @(
        '/X!addincludedir ..\NSIS',
        '/X!addplugindir /x86-unicode ..\NSIS',
        ('/DINPUTDIR=' + $inputDirectory),
        ('/DOUTPUTFILE=' + $setup),
        '/DFBE_DEPLOYMENT_TEST_SCOPE_PROBE=1',
        ('/DFBE_DEPLOYMENT_TEST_ROOT=' + $directory)
    ) + $Defines + @('MakeInstaller.nsi')
    Push-Location $installerDirectory
    try { & $makensis @arguments } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "makensis failed for $Name scope probe." }
    $process = Start-Process -FilePath $setup -ArgumentList '/S' -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "NSIS $Name scope probe exited with $($process.ExitCode)." }
    $probe = Join-Path $directory 'deployment-scope.txt'
    if (-not (Test-Path -LiteralPath $probe -PathType Leaf)) { throw "NSIS $Name scope probe did not write its result." }
    return @{ Directory = $directory; State = Read-Probe $probe }
}

$registryKeys = @(
    'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\FictionBook Editor Next',
    'HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\FictionBook Editor Next',
    'HKCU\Software\Classes\FictionBook.2', 'HKLM\Software\Classes\FictionBook.2'
)
$before = @{}; foreach ($key in $registryKeys) { $before[$key] = Get-RegistrySnapshot $key }

Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $inputDirectory -Force | Out-Null
foreach ($license in @('LICENSE', 'gpl-3.0.ru.txt', 'gpl-3.0.ua.txt')) {
    Set-Content -LiteralPath (Join-Path $inputDirectory $license) -Value 'NSIS deployment scope test fixture.' -Encoding ASCII
}
$current = Invoke-ScopeProbe 'current' @()
if ($current.State.DeploymentMode -ne 'installed' -or $current.State.InstallScope -ne 'current' -or
    $current.State.UninstallRegistryRoot -ne 'HKCU' -or $current.State.ProductionPath -notmatch '\\Programs\\FictionBook Editor Next$') {
    throw "Current User scope probe returned an unexpected state: $($current.State | Out-String)"
}
if ($current.State.ProductionPath -match 'Program Files') { throw 'Current User scope selected Program Files semantics.' }

$allUsers = Invoke-ScopeProbe 'allusers' @('/DFBE_DEPLOYMENT_TEST_ALLUSERS=1')
if ($allUsers.State.DeploymentMode -ne 'installed' -or $allUsers.State.InstallScope -ne 'allusers' -or
    $allUsers.State.UninstallRegistryRoot -ne 'HKLM' -or $allUsers.State.ProductionPath -notmatch 'Program Files') {
    throw "All Users scope probe returned an unexpected state: $($allUsers.State | Out-String)"
}

$portable = Invoke-ScopeProbe 'portable' @('/DFBE_DEPLOYMENT_TEST_PORTABLE=1')
if ($portable.State.DeploymentMode -ne 'portable' -or $portable.State.InstallScope -ne 'current') {
    throw "Portable scope probe returned an unexpected state: $($portable.State | Out-String)"
}
foreach ($path in @('portable.ini', 'Data\Settings', 'Data\Scripts', 'Data\Dictionaries', 'Data\Themes', 'Data\Logs', 'Data\Diagnostics', 'Data\Recovery', 'Data\Cache', 'Data\Temp')) {
    if (-not (Test-Path -LiteralPath (Join-Path $portable.Directory $path))) { throw "Portable NSIS probe missed $path." }
}
if (Test-Path -LiteralPath (Join-Path $portable.Directory 'uninst.exe')) { throw 'Portable NSIS probe created an uninstaller.' }

foreach ($key in $registryKeys) {
    if ((Get-RegistrySnapshot $key) -cne $before[$key]) { throw "NSIS scope probe changed registry state: $key" }
}
Write-Host 'NSIS current-user, all-users and portable scope behavior passed.'
