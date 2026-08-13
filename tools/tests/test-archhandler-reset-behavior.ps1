<#
.SYNOPSIS
Runs production ArchHandler HTA reset handlers and verifies ZIP/RAR isolation.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$runner = Join-Path $PSScriptRoot 'archhandler-reset-behavior.js'
$zip = Join-Path $repoRoot 'runtime\Utilities\ArchHandler\ConfigZipHandler.hta'
$rar = Join-Path $repoRoot 'runtime\Utilities\ArchHandler\ConfigRarHandler.hta'
foreach($path in @($runner, $zip, $rar)) { if(-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Не найден файл ArchHandler test: $path" } }

& cscript.exe //nologo $runner $zip $rar
if($LASTEXITCODE -ne 0) { throw "Поведенческий JScript test ArchHandler завершился с кодом $LASTEXITCODE." }

$key = 'HKCU\Software\FBETeam\FBE-Next-ArchHandler-Reset-Test-' + [guid]::NewGuid().ToString('N')
try {
    & reg.exe add $key /v probe /t REG_SZ /d active /f | Out-Null
    if($LASTEXITCODE -ne 0) { throw 'reg.exe не создал изолированный ключ для Reset test.' }
    $delete = Start-Process -FilePath reg.exe -ArgumentList @('delete', $key, '/f') -PassThru
    if(-not $delete.WaitForExit(15000)) { Stop-Process -Id $delete.Id -Force; throw 'reg.exe delete завис при удалении изолированного ключа.' }
    if($delete.ExitCode -ne 0) { throw "reg.exe delete вернул код $($delete.ExitCode)." }
    & reg.exe query $key *> $null
    if($LASTEXITCODE -eq 0) { throw 'reg.exe delete не удалил изолированный ключ.' }
}
finally {
    & reg.exe delete $key /f *> $null
}
Write-Host 'ArchHandler Reset behavioral test passed.'
