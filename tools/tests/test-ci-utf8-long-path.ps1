<# Exercise the real UTF-8 bootstrap with ordinary and cmd-incompatible PATHs. #>
[CmdletBinding()]
param([string]$BootstrapPath=(Join-Path $PSScriptRoot '../build/Initialize-CiUtf8.ps1'))
$ErrorActionPreference='Stop'
$savedPath=$env:Path
$cmd=Join-Path $env:SystemRoot 'System32\cmd.exe'
$expected='Проверка UTF-8: Ёж'
try {
    foreach($longPath in @($false,$true)) {
        $env:Path=if($longPath){(($env:SystemRoot+'\System32;')*1000)+$savedPath}else{$savedPath}
        $before=$env:Path
        & $BootstrapPath
        if($LASTEXITCODE -ne 0){throw "UTF-8 bootstrap failed: longPath=$longPath exit=$LASTEXITCODE"}
        if($env:Path -cne $before){throw 'UTF-8 bootstrap changed PATH.'}
        foreach($encoding in @([Console]::InputEncoding,[Console]::OutputEncoding,$OutputEncoding)) {
            if($encoding.CodePage -ne 65001){throw 'Managed UTF-8 encoding was not applied.'}
        }
        $actual=(& $cmd /d /c "echo $expected").Trim()
        if($LASTEXITCODE -ne 0 -or $actual -cne $expected){throw "Native UTF-8 round-trip failed: longPath=$longPath"}
        Write-Host "UTF-8 bootstrap PASS: longPath=$longPath pathLength=$($before.Length) nativeExit=$LASTEXITCODE"
    }
} finally {$env:Path=$savedPath}
