<# Exercises a materialised portable payload and, optionally, its ZIP. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackageDirectory,
    [string]$PortableZip
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$package = (Resolve-Path -LiteralPath $PackageDirectory).Path
& (Join-Path $root 'tools\build\verify-package-stage.ps1') -Kind Portable -StageDirectory $package
foreach ($name in @('Settings','Scripts','Dictionaries','Themes','Logs','Diagnostics','Recovery','Cache','Temp')) {
    if (-not (Test-Path -LiteralPath (Join-Path $package "Data\\$name") -PathType Container)) { throw "Portable payload misses Data\\$name." }
}
foreach ($name in @('FBShell.dll','FBShell64.dll','FBE.Sequence.propdesc','InstallerTools','Lang\\Shell','uninst.exe')) {
    if (Test-Path -LiteralPath (Join-Path $package $name)) { throw "Portable payload contains forbidden item: $name" }
}

$testRoot = Join-Path $root 'out\tests\portable-package-smoke'
Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
function Get-Paths([string]$Directory) {
    $stdout = Join-Path $Directory 'paths.stdout'; $stderr = Join-Path $Directory 'paths.stderr'
    $process = Start-Process -FilePath (Join-Path $Directory 'FBE.exe') -ArgumentList '--print-runtime-paths' -WorkingDirectory $Directory -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if ($process.ExitCode -ne 0) { throw "Portable runtime failed: $(Get-Content -Raw -LiteralPath $stderr)" }
    return Get-Content -Raw -LiteralPath $stdout | ConvertFrom-Json
}
function Assert-Portable([object]$Paths, [string]$Directory) {
    if ($Paths.mode -ne 'Portable' -or $Paths.registryPersistenceAllowed) { throw 'Payload did not run as registry-free Portable.' }
    foreach ($property in @('dataRoot','settingsDirectory','diagnosticsDirectory','recoveryDirectory','logsDirectory','cacheDirectory','tempDirectory','dictionariesDirectory','themesDirectory','scriptsDirectory')) {
        if (-not $Paths.($property).StartsWith($Directory, [StringComparison]::OrdinalIgnoreCase)) { throw "Portable $property escaped its package root: '$($Paths.($property))' not under '$Directory'." }
    }
}
function Get-FileTreeSnapshot([string]$Directory) {
    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) { return '<absent>' }
    return (Get-ChildItem -LiteralPath $Directory -Recurse -File | Sort-Object FullName | ForEach-Object {
        "$($_.FullName)|$($_.Length)|$($_.LastWriteTimeUtc.Ticks)|$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)"
    }) -join "`n"
}
function Invoke-PortableGui([string]$Directory, [string[]]$Arguments) {
    $process = Start-Process -FilePath (Join-Path $Directory 'FBE.exe') -ArgumentList $Arguments -WorkingDirectory $Directory -PassThru
    try {
        Start-Sleep -Seconds 8
        $process.Refresh()
        if ($process.HasExited) { throw "Portable GUI exited during startup with $($process.ExitCode)." }
    } finally {
        $process.Refresh()
        if (-not $process.HasExited) {
            $process.CloseMainWindow() | Out-Null
            if (-not $process.WaitForExit(15000)) { Stop-Process -Id $process.Id -Force; throw 'Portable GUI did not exit cleanly.' }
        }
    }
}

$copyA = Join-Path $testRoot 'Portable A'; $copyB = Join-Path $testRoot 'Portable B'
Copy-Item -LiteralPath $package -Destination $copyA -Recurse -Force
Copy-Item -LiteralPath $package -Destination $copyB -Recurse -Force
$pathsA = Get-Paths $copyA; $pathsB = Get-Paths $copyB
Assert-Portable $pathsA $copyA; Assert-Portable $pathsB $copyB
if ($pathsA.dataRoot -eq $pathsB.dataRoot) { throw 'Independent portable copies share Data.' }
$installedData = Join-Path $env:LOCALAPPDATA 'FBE Next'
$installedBefore = Get-FileTreeSnapshot $installedData
$fixture = Join-Path $copyA 'portable-state-smoke.fb2'
Copy-Item -LiteralPath (Join-Path $root 'tools\tests\fb2-metadata-cyrillic-smoke.fb2') -Destination $fixture
Invoke-PortableGui $copyA @($fixture)
foreach ($name in @('Settings.xml','Hotkeys.xml','Words.xml')) {
    if (-not (Test-Path -LiteralPath (Join-Path $copyA "Data\Settings\$name") -PathType Leaf)) { throw "Portable GUI did not persist $name in Data\\Settings." }
}
Invoke-PortableGui $copyA @()
foreach ($name in @('Settings.xml','Hotkeys.xml','Words.xml')) {
    if (-not (Test-Path -LiteralPath (Join-Path $copyA "Data\Settings\$name") -PathType Leaf)) { throw "Portable state $name was not retained across restart." }
}
if ((Get-FileTreeSnapshot $installedData) -cne $installedBefore) { throw '%LOCALAPPDATA%\\FBE Next changed during portable GUI state test.' }
$moved = Join-Path $testRoot 'Тест перенесённый FBE'
Move-Item -LiteralPath $copyA -Destination $moved
$movedPaths = Get-Paths $moved; Assert-Portable $movedPaths $moved
if (-not $movedPaths.dataRoot.EndsWith('\Data\')) { throw 'Relocated package did not retain relative Data root.' }

if ($PortableZip) {
    $zip = (Resolve-Path -LiteralPath $PortableZip).Path
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($zip)
    try {
        $entries = @($archive.Entries.FullName)
        foreach ($required in @('portable.ini','Data/','Data/Settings/','Data/Scripts/','Data/Dictionaries/','Data/Themes/','Data/Logs/','Data/Diagnostics/','Data/Recovery/','Data/Cache/','Data/Temp/')) { if (-not ($entries | Where-Object { $_ -eq $required -or $_.StartsWith($required) })) { throw "ZIP misses $required" } }
        if ($entries | Where-Object { $_ -match '(^|/)(FBShell(64)?\\.dll|FBE\\.Sequence\\.propdesc|uninst\\.exe)$|(^|/)InstallerTools/' -or $_ -match '\\.(pdb|obj|lib|exp)$' }) { throw 'ZIP contains forbidden integration or build artifacts.' }
    } finally { $archive.Dispose() }
    $extracted = Join-Path $testRoot 'ZIP распаковка'
    [IO.Compression.ZipFile]::ExtractToDirectory($zip, $extracted)
    Assert-Portable (Get-Paths $extracted) $extracted
}
Write-Host 'Portable package/ZIP smoke passed.'
