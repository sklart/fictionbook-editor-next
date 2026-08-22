<# Exercises portable marker discovery from two independent executable roots. #>
[CmdletBinding()]
param([string]$FbeExecutable)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $FbeExecutable) { $FbeExecutable = Join-Path $root 'out\Release\FBE.exe' }
$FbeExecutable = (Resolve-Path -LiteralPath $FbeExecutable).Path
$testRoot = Join-Path $root 'out\tests\portable-copies-isolation'
Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null

function New-PortableCopy([string]$Name, [string]$DataPath) {
    $directory = Join-Path $testRoot $Name
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    Copy-Item -LiteralPath $FbeExecutable -Destination (Join-Path $directory 'FBE.exe')
    "[Portable]`r`nDataPath=$DataPath`r`n" | Set-Content -LiteralPath (Join-Path $directory 'portable.ini') -Encoding utf8NoBOM
    return Join-Path $directory 'FBE.exe'
}
function Get-PortablePaths([string]$Executable) {
    $name = [IO.Path]::GetRandomFileName()
    $stdout = Join-Path $testRoot "$name.stdout"
    $stderr = Join-Path $testRoot "$name.stderr"
    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) { throw "Portable executable is missing: $Executable" }
    $process = Start-Process -FilePath $Executable -ArgumentList '--print-runtime-paths' -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if ($process.ExitCode -ne 0) { throw "Portable CLI exited with $($process.ExitCode): $(Get-Content -LiteralPath $stderr -Raw)" }
    return Get-Content -LiteralPath $stdout -Raw | ConvertFrom-Json
}

$copyA = Get-PortablePaths (New-PortableCopy 'PortableA' 'StateA')
$copyB = Get-PortablePaths (New-PortableCopy 'PortableB' 'StateB')
$default = Get-PortablePaths (New-PortableCopy 'DefaultPath' 'Data')
$nested = Get-PortablePaths (New-PortableCopy 'NestedPath' 'sub\dir')
$invalid = @(
    (Get-PortablePaths (New-PortableCopy 'EscapeOverride' '..\escape'))
    (Get-PortablePaths (New-PortableCopy 'DriveOverride' 'C:\absolute'))
    (Get-PortablePaths (New-PortableCopy 'RootOverride' '\absolute'))
)
if ($copyA.mode -ne 'Portable' -or -not $copyA.dataRoot.EndsWith('\PortableA\StateA\')) { throw 'PortableA data root is invalid.' }
if ($copyB.mode -ne 'Portable' -or -not $copyB.dataRoot.EndsWith('\PortableB\StateB\')) { throw 'PortableB data root is invalid.' }
if ($copyA.dataRoot -eq $copyB.dataRoot) { throw 'Portable copies share a data root.' }
if (-not $default.dataRoot.EndsWith('\DefaultPath\Data\')) { throw 'Default DataPath is invalid.' }
if (-not $nested.dataRoot.EndsWith('\NestedPath\Data\')) { throw 'Nested DataPath must fall back to the safe Data directory.' }
foreach ($paths in $invalid) {
    if (-not $paths.dataRoot.EndsWith('\Data\')) { throw 'Unsafe DataPath did not fall back to Data.' }
}
Write-Host 'Portable copies isolation behavior passed.'
