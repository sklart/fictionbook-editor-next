<# Exercises FBE.exe diagnostic runtime-paths mode without starting its GUI. #>
[CmdletBinding()]
param([string]$FbeExecutable)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $FbeExecutable) { $FbeExecutable = Join-Path $root 'out\Release\FBE.exe' }
$FbeExecutable = (Resolve-Path -LiteralPath $FbeExecutable).Path
$testDirectory = Join-Path $root 'out\tests\runtime-paths-cli'
New-Item -ItemType Directory -Force -Path $testDirectory | Out-Null

function Invoke-RuntimePaths([string[]]$Arguments, [int]$ExpectedExitCode) {
    $stdout = Join-Path $testDirectory (($Arguments -join '_').Replace('--', '') + '.stdout')
    $stderr = Join-Path $testDirectory (($Arguments -join '_').Replace('--', '') + '.stderr')
    Remove-Item -LiteralPath $stdout,$stderr -Force -ErrorAction SilentlyContinue
    $process = Start-Process -FilePath $FbeExecutable -ArgumentList $Arguments -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if ($process.ExitCode -ne $ExpectedExitCode) { throw "FBE $Arguments завершился с кодом $($process.ExitCode), ожидался $ExpectedExitCode." }
    return @{ Stdout = (Get-Content -LiteralPath $stdout -Raw -ErrorAction SilentlyContinue); Stderr = (Get-Content -LiteralPath $stderr -Raw -ErrorAction SilentlyContinue) }
}

$installed = (Invoke-RuntimePaths @('--print-runtime-paths') 0).Stdout | ConvertFrom-Json
if ($installed.mode -ne 'Installed' -or -not $installed.registryPersistenceAllowed -or [string]::IsNullOrWhiteSpace($installed.settingsDirectory)) { throw 'Installed runtime paths are invalid.' }
$portable = (Invoke-RuntimePaths @('--portable', '--print-runtime-paths') 0).Stdout | ConvertFrom-Json
if ($portable.mode -ne 'Portable' -or $portable.registryPersistenceAllowed -or -not $portable.dataRoot.EndsWith('\Data\')) { throw 'Portable runtime paths are invalid.' }
$conflict = Invoke-RuntimePaths @('--portable', '--installed', '--print-runtime-paths') 2
if ($conflict.Stderr -notmatch 'cannot be used together') { throw 'Conflicting mode diagnostic is missing.' }
Write-Host 'Runtime paths CLI behavior passed.'
