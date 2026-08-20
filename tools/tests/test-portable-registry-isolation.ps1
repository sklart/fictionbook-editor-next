<#
Exercises both the portable command-line and GUI startup paths while comparing
all FBE-owned HKCU locations before and after them. Neither path may initialise
the registry profile, register the embedded typelib or bundled plugins, or touch
file-association state.
#>
[CmdletBinding()]
param([string]$FbeExecutable)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $FbeExecutable) { $FbeExecutable = Join-Path $root 'out\Release\FBE.exe' }
$FbeExecutable = (Resolve-Path -LiteralPath $FbeExecutable).Path
$testRoot = Join-Path $root 'out\tests\portable-registry-isolation'
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null

function Get-RegistrySnapshot([string]$Key) {
    $output = & reg.exe query $Key /s 2>&1
    if ($LASTEXITCODE -eq 1) { return '<absent>' }
    if ($LASTEXITCODE -ne 0) { throw "reg.exe query failed for ${Key}: $output" }
    return (($output | ForEach-Object { [string]$_ }) -join "`n").Trim()
}

$keys = @(
    'HKCU\Software\FBETeam\FictionBook Editor Next',
    'HKCU\Software\Classes\FictionBook.2',
    'HKCU\Software\Classes\.fb2',
    'HKCU\Software\Classes\TypeLib\{37B16C7D-4400-4D7D-AA35-14C74E265EA4}',
    'HKCU\Software\Classes\CLSID\{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}',
    'HKCU\Software\Classes\CLSID\{C3098839-EF69-4DE5-B27D-1E80051CA843}',
    'HKCU\Software\Classes\CLSID\{09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56}',
    'HKCU\Software\Classes\CLSID\{36FCFB2D-C3D8-4B81-ABC1-5A09CA846515}'
)
$before = @{}
foreach ($key in $keys) { $before[$key] = Get-RegistrySnapshot $key }

$stdout = Join-Path $testRoot 'portable.stdout'
$stderr = Join-Path $testRoot 'portable.stderr'
Remove-Item -LiteralPath $stdout,$stderr -Force -ErrorAction SilentlyContinue
$process = Start-Process -FilePath $FbeExecutable -ArgumentList @('--portable', '--print-runtime-paths') -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdout -RedirectStandardError $stderr
if ($process.ExitCode -ne 0) { throw "Portable runtime CLI exited with $($process.ExitCode): $(Get-Content -Raw -LiteralPath $stderr)" }
$paths = Get-Content -Raw -LiteralPath $stdout | ConvertFrom-Json
if ($paths.mode -ne 'Portable' -or $paths.registryPersistenceAllowed) { throw 'Portable CLI did not report registry persistence as disabled.' }

foreach ($key in $keys) {
    $after = Get-RegistrySnapshot $key
    if ($after -cne $before[$key]) { throw "Portable runtime CLI changed FBE-owned registry state: $key" }
}

$guiProcess = Start-Process -FilePath $FbeExecutable -ArgumentList @('--portable') -WorkingDirectory (Split-Path -Parent $FbeExecutable) -PassThru
try {
    Start-Sleep -Seconds 5
    $guiProcess.Refresh()
    if ($guiProcess.HasExited) { throw "Portable GUI exited during startup with $($guiProcess.ExitCode)." }
}
finally {
    $guiProcess.Refresh()
    if (-not $guiProcess.HasExited) {
        $guiProcess.CloseMainWindow() | Out-Null
        if (-not $guiProcess.WaitForExit(10000)) { Stop-Process -Id $guiProcess.Id -Force }
    }
}

foreach ($key in $keys) {
    $after = Get-RegistrySnapshot $key
    if ($after -cne $before[$key]) { throw "Portable GUI startup changed FBE-owned registry state: $key" }
}

Write-Host 'Portable CLI and GUI registry isolation behavior passed.'
