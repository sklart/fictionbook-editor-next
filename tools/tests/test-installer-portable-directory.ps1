<# Exercise the real installer in Portable mode in a fresh test directory. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Installer, [Parameter(Mandatory)][string]$InputDirectory)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$setup = (Resolve-Path -LiteralPath $Installer).Path
$inputPath = (Resolve-Path -LiteralPath $InputDirectory).Path
$testRoot = Join-Path $root ('out/tests/installer-portable-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$testSetup = Join-Path $testRoot 'setup.exe'; Copy-Item -LiteralPath $setup -Destination $testSetup
$target = Join-Path $testRoot 'Portable выбранный каталог'
$default = Join-Path $testRoot 'FictionBook Editor Next Portable'
$keys = @('HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\FictionBook Editor Next',
    'HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\FictionBook Editor Next',
    'HKCU\Software\Classes\FictionBook.2', 'HKLM\Software\Classes\FictionBook.2')
function Snapshot([string]$key) {
    $lines = & reg.exe query $key /s 2>&1
    if ($LASTEXITCODE -eq 1) { return '<absent>' }
    if ($LASTEXITCODE -ne 0) { throw "Registry query failed for $key" }
    return (($lines | ForEach-Object { [string]$_ }) -join "`n")
}
$before = @{}; foreach($key in $keys) { $before[$key] = Snapshot $key }
$process = Start-Process -FilePath $testSetup -ArgumentList "/S /PORTABLE /D=$target" -Wait -PassThru -WindowStyle Hidden
if ($process.ExitCode -ne 0) { throw "Portable setup exited $($process.ExitCode): $testRoot" }
foreach($key in $keys) { if ((Snapshot $key) -cne $before[$key]) { throw "Portable setup changed $key" } }
if (Test-Path -LiteralPath $default) { throw "Portable setup ignored /D and created default directory: $default" }
if (Test-Path -LiteralPath (Join-Path $target 'DescriptionIcons')) { throw 'Portable setup contains unused DescriptionIcons.' }
foreach ($relative in @('FBE.exe','portable.ini','languages.txt','root_genres.xml','genres.librusec.txt','genres.rus.librusec.txt','Dict/sources.json')) {
    $actual = Join-Path $target $relative
    if (-not (Test-Path -LiteralPath $actual -PathType Leaf)) { throw "Missing installed $relative in $target" }
    if ($relative -ne 'portable.ini' -and (Get-FileHash -LiteralPath $actual).Hash -ne (Get-FileHash -LiteralPath (Join-Path $inputPath $relative)).Hash) { throw "Installed file differs: $relative" }
}
if (Test-Path -LiteralPath (Join-Path $target 'uninst.exe')) { throw 'Portable setup created an uninstaller' }
$stdout = Join-Path $testRoot 'paths.json'; $stderr = Join-Path $testRoot 'paths.err'
$process = Start-Process -FilePath (Join-Path $target 'FBE.exe') -ArgumentList '--print-runtime-paths' -WorkingDirectory $target -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
if ($process.ExitCode -ne 0) { throw "Portable FBE exited $($process.ExitCode)" }
$paths = Get-Content -Raw -Encoding UTF8 $stdout | ConvertFrom-Json
if ($paths.mode -ne 'Portable' -or $paths.registryPersistenceAllowed -or -not $paths.dataRoot.StartsWith($target, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected portable runtime paths' }
$defaultRoot = Join-Path $testRoot 'default-case'
New-Item -ItemType Directory -Path $defaultRoot | Out-Null
$defaultSetup = Join-Path $defaultRoot 'setup.exe'; Copy-Item -LiteralPath $setup -Destination $defaultSetup
$process = Start-Process -FilePath $defaultSetup -ArgumentList '/S /PORTABLE' -Wait -PassThru -WindowStyle Hidden
if ($process.ExitCode -ne 0) { throw "Default portable setup exited $($process.ExitCode)" }
$defaultTarget = Join-Path $defaultRoot 'FictionBook Editor Next Portable'
if (-not (Test-Path -LiteralPath (Join-Path $defaultTarget 'FBE.exe') -PathType Leaf)) { throw 'Portable setup without /D did not use its default directory' }
foreach($key in $keys) { if ((Snapshot $key) -cne $before[$key]) { throw "Default portable setup changed $key" } }
Write-Host "Portable setup respected /D and installed verified assets: $testRoot"
