<# Exercises NSIS' test-only deployment branches without installing FBE. #>
[CmdletBinding()]
param([string]$TestDirectory)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$installerDirectory = Join-Path $root 'packaging\nsis\Installer'
$testsDirectory = [IO.Path]::GetFullPath((Join-Path $root 'out\tests')) + [IO.Path]::DirectorySeparatorChar
$testRoot = if ($TestDirectory) { [IO.Path]::GetFullPath($TestDirectory) } else { Join-Path $testsDirectory 'nsis-install-scopes' }
if (-not $testRoot.StartsWith($testsDirectory, [StringComparison]::OrdinalIgnoreCase)) { throw 'NSIS scope artifacts must stay inside out/tests.' }
if ($TestDirectory -and (Test-Path -LiteralPath $testRoot)) { throw 'Use a fresh isolated TestDirectory to preserve previous scope evidence.' }
$inputDirectory = Join-Path $testRoot 'input'

$makensis = & (Join-Path $root 'tools\build\resolve-nsis.ps1')
if ($LASTEXITCODE -ne 0 -or -not $makensis) { throw 'Unable to resolve makensis for NSIS scope smoke.' }

function Get-RegistrySnapshot([string]$Key) {
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & reg.exe query $Key /s 2>&1
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
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
function Invoke-ScopeProbe([string]$Name, [string[]]$Defines, [string[]]$RunArguments, [switch]$LongPath) {
    $directory = Join-Path $testRoot $Name
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $setup = Join-Path $directory 'scope-probe.exe'
    $arguments = @(
        '/V4',
        '/X!addincludedir ..\NSIS',
        '/X!addplugindir /x86-unicode ..\NSIS',
        ('/DINPUTDIR=' + $inputDirectory),
        ('/DOUTPUTFILE=' + $setup),
        # Scope probes embed no shell payload; these valid-format placeholders are unused.
        ('/DFBE_SHELL_X86_SHA256=' + ('0' * 64)),
        ('/DFBE_SHELL_X64_SHA256=' + ('0' * 64)),
        ('/DFBE_SCHEMA_SHA256=' + ('0' * 64)),
        '/DFBE_DEPLOYMENT_TEST_SCOPE_PROBE=1',
        ('/DFBE_DEPLOYMENT_TEST_ROOT=' + $directory),
        ('/DFBE_INSTALLER_PAYLOAD_LOG_DIRECTORY=' + $directory)
    ) + $Defines + @('MakeInstaller.nsi')
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $makensis
    $start.Arguments = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $start.WorkingDirectory = $installerDirectory
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    if ($LongPath) {
        # cmd.exe drops an inherited PATH longer than 8191 characters. The host must
        # still start through !system, without changing the parent gate environment.
        $start.EnvironmentVariables['Path'] = ('C:\FbeScopePathPadding;' * 450) + $env:Path
    }
    $environment = @{}
    foreach ($key in $start.EnvironmentVariables.Keys) { $environment[$key] = $start.EnvironmentVariables[$key] }
    $environment | ConvertTo-Json | Set-Content (Join-Path $directory 'environment.json')
    $nestedHostBinary = if ([Environment]::Is64BitOperatingSystem) { 'SysWOW64' } else { 'System32' }
    $meta = @{
        exe=$makensis; commandLine=('"' + $makensis + '" ' + $start.Arguments)
        arguments=$arguments; cwd=$installerDirectory; pathLength=$start.EnvironmentVariables['Path'].Length
        makensisVersion=(& $makensis /VERSION | Out-String).Trim()
        driverPowerShell=$PSVersionTable.PSVersion.ToString()
        nestedHost=(Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe')
        nestedHostVersion=[Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $env:SystemRoot ($nestedHostBinary + '\WindowsPowerShell\v1.0\powershell.exe'))).FileVersion
    }
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    $stdout = [IO.File]::Create((Join-Path $directory 'makensis.stdout.log'))
    $stderr = [IO.File]::Create((Join-Path $directory 'makensis.stderr.log'))
    try {
        if (-not $process.Start()) { throw 'Could not start makensis.' }
        $outTask = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $errTask = $process.StandardError.BaseStream.CopyToAsync($stderr)
        $process.WaitForExit()
        $outTask.GetAwaiter().GetResult()
        $errTask.GetAwaiter().GetResult()
        $meta.exitCode = $process.ExitCode
    }
    finally {
        $stdout.Dispose(); $stderr.Dispose(); $process.Dispose()
        $meta | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $directory 'compile.json')
    }
    $compilerOutput = Get-Content -Raw -LiteralPath (Join-Path $directory 'makensis.stdout.log')
    $nestedCommand = [regex]::Match($compilerOutput, '(?m)^!system: "(.+)"\r?$')
    $nestedExit = [regex]::Match($compilerOutput, '(?m)^!system: returned (-?\d+)')
    $meta.nestedCommandLine = $nestedCommand.Groups[1].Value
    $meta.nestedExitCode = if ($nestedExit.Success) { [int]$nestedExit.Groups[1].Value } else { $null }
    $meta | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $directory 'compile.json')
    if ($meta.exitCode -ne 0) { throw "makensis failed for $Name scope probe; full diagnostics: $directory" }
    if (-not $nestedCommand.Success -or -not $nestedExit.Success -or $meta.nestedExitCode -ne 0) { throw "Missing successful mandatory !system verification: $directory" }
    $process = Start-Process -FilePath $setup -ArgumentList $RunArguments -Wait -PassThru -WindowStyle Hidden
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
$current = Invoke-ScopeProbe 'current' @() @('/S', '/CURRENTUSER', '/RECOVERSHELL')
if ($current.State.DeploymentMode -ne 'installed' -or $current.State.InstallScope -ne 'current' -or
    $current.State.UninstallRegistryRoot -ne 'HKCU' -or $current.State.ProductionPath -notmatch '\\Programs\\FictionBook Editor Next$') {
    throw "Current User scope probe returned an unexpected state: $($current.State | Out-String)"
}
if ($current.State.ProductionPath -match 'Program Files') { throw 'Current User scope selected Program Files semantics.' }

if ($current.State.ShellRecoveryRequested -ne '1') { throw 'Explicit shell recovery request was lost.' }
$installerSource=Get-Content -Raw -LiteralPath (Join-Path $installerDirectory 'MakeInstaller.nsi')
if (-not $installerSource.Contains('!insertmacro FBE_RestoreElevationVar $ShellRecoveryRequested')) { throw 'Shell recovery request is not transferred through existing elevation state restoration.' }

$allUsers = Invoke-ScopeProbe 'allusers' @() @('/S', '/ALLUSERS')
if ($allUsers.State.DeploymentMode -ne 'installed' -or $allUsers.State.InstallScope -ne 'allusers' -or
    $allUsers.State.UninstallRegistryRoot -ne 'HKLM' -or $allUsers.State.ProductionPath -notmatch 'Program Files') {
    throw "All Users scope probe returned an unexpected state: $($allUsers.State | Out-String)"
}

$portable = Invoke-ScopeProbe 'portable' @() @('/S', '/PORTABLE')
if ($portable.State.DeploymentMode -ne 'portable' -or $portable.State.InstallScope -ne 'current') {
    throw "Portable scope probe returned an unexpected state: $($portable.State | Out-String)"
}
foreach ($path in @('portable.ini', 'Data\Settings', 'Data\Scripts', 'Data\Dictionaries', 'Data\Themes', 'Data\Logs', 'Data\Diagnostics', 'Data\Recovery', 'Data\Cache', 'Data\Temp')) {
    if (-not (Test-Path -LiteralPath (Join-Path $portable.Directory $path))) { throw "Portable NSIS probe missed $path." }
}
if (Test-Path -LiteralPath (Join-Path $portable.Directory 'uninst.exe')) { throw 'Portable NSIS probe created an uninstaller.' }

$transitions = Invoke-ScopeProbe 'scope-transitions' @('/DFBE_DEPLOYMENT_TEST_SCOPE_TRANSITIONS=1') @('/S', '/CURRENTUSER')
if ($transitions.State.ScopeTransitions -ne 'passed') { throw "NSIS scope path transition regression failed: $($transitions.State | Out-String)" }

$longPath = Invoke-ScopeProbe 'long-path' @() @('/S', '/CURRENTUSER') -LongPath
if ($longPath.State.InstallScope -ne 'current') { throw 'Long PATH scope probe returned an unexpected scope.' }

foreach ($key in $registryKeys) {
    if ((Get-RegistrySnapshot $key) -cne $before[$key]) { throw "NSIS scope probe changed registry state: $key" }
}
Write-Host 'NSIS current-user, all-users and portable scope behavior passed.'
# reg.exe returns 1 for an absent key, which is an expected observation above.
# Do not leak that native status after every contract assertion has succeeded.
exit 0
