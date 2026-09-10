[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$FbeExe,
    [Parameter(Mandatory = $true)][string]$Rar4Archive,
    [string]$Entry = '',
    [ValidateRange(150, 300)][int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE.exe: $FbeExe" }
if(-not (Test-Path -LiteralPath $Rar4Archive -PathType Leaf)) { throw "Не найден RAR4-архив: $Rar4Archive" }
$signature = [IO.File]::ReadAllBytes($Rar4Archive)
if($signature.Length -lt 8) { throw 'Файл слишком мал для сигнатуры RAR4.' }
$rar4Signature = [byte[]](0x52,0x61,0x72,0x21,0x1A,0x07,0x00)
$rar5Signature = [byte[]](0x52,0x61,0x72,0x21,0x1A,0x07,0x01,0x00)
if([Linq.Enumerable]::SequenceEqual([byte[]]$signature[0..7], $rar5Signature)) { throw 'Ожидался RAR4, но передан RAR5.' }
if(-not [Linq.Enumerable]::SequenceEqual([byte[]]$signature[0..6], $rar4Signature)) { throw 'Ожидался настоящий RAR4 с сигнатурой 52 61 72 21 1A 07 00.' }
$report = Join-Path ([IO.Path]::GetTempPath()) ("fbe-rar4-runtime-" + [guid]::NewGuid().ToString('N') + '.txt')
$before = (Get-FileHash -LiteralPath $Rar4Archive -Algorithm SHA256).Hash
$oldMode,$oldScenario,$oldEntry = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_ARCHIVE_ENTRY
try {
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='archive-open-runtime'; $env:FBE_NEXT_TEST_ARCHIVE_ENTRY=$Entry
    $quote = { param([string]$value) '"' + $value.Replace('"', '\"') + '"' }
    $arguments = '--portable -b {0} {1}' -f (& $quote $report), (& $quote $Rar4Archive)
    $process = Start-Process -FilePath $FbeExe -ArgumentList $arguments -WorkingDirectory (Split-Path $FbeExe) -PassThru
    if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "RAR4 runtime test timed out after $TimeoutSeconds seconds." }
    $state = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
    if($process.ExitCode -ne 0) { throw "RAR4 runtime test exited $($process.ExitCode): $state" }
    foreach($line in @('archive=1','fb2=1','mshtml=1','rar=1')) { if($state -notmatch [regex]::Escape($line)) { throw "RAR4 runtime report missing $line" } }
    if($Entry -and $Entry -cmatch '^[\x20-\x7e]+$' -and $state -notmatch [regex]::Escape("entry=$Entry")) { throw "RAR4 runtime report selected the wrong entry: $Entry" }
    if((Get-FileHash -LiteralPath $Rar4Archive -Algorithm SHA256).Hash -cne $before) { throw 'RAR4 open changed the source archive.' }
    Write-Host 'Local RAR4 archive runtime integration passed.'
} finally {
    foreach($pair in @(@('FBE_NEXT_TEST_MODE',$oldMode),@('FBE_NEXT_TEST_SCENARIO',$oldScenario),@('FBE_NEXT_TEST_ARCHIVE_ENTRY',$oldEntry))) { if($null -eq $pair[1]) { Remove-Item ("Env:" + $pair[0]) -ErrorAction SilentlyContinue } else { Set-Item ("Env:" + $pair[0]) $pair[1] } }
    Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
}
