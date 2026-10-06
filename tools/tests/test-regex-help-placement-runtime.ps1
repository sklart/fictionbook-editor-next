[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot "..\..\out\Release\FBE.exe"), [int]$TimeoutSeconds = 45)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe)) { throw "FBE не найден: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-regex-help-placement-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $fixture = Join-Path $root 'help.fb2'; $report = Join-Path $root 'report.txt'
    '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Help</book-title><lang>en</lang></title-info><document-info><id>regex-help-placement</id><version>1.0</version></document-info></description><body><section><p>Help placement runtime smoke.</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode,$oldScenario = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='regex-help-placement-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList '-b',("`"$report`""),("`"$fixture`"") -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Regex Help placement runtime smoke timed out.' }
        if ($process.ExitCode -ne 0) { throw "Regex Help placement runtime smoke failed with exit code $($process.ExitCode)." }
    } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO=$oldMode,$oldScenario }
    $rows=@{}; Get-Content -LiteralPath $report | ForEach-Object { $pair=$_ -split '=',2; if($pair.Count -eq 2){$rows[$pair[0]]=$pair[1]} }
    foreach($key in 'default_large','saved_restore','outside_clamp') { if($rows[$key] -ne '1') { throw "Regex Help placement runtime failed: $key=$($rows[$key]); report=$($rows | Out-String)" } }
    if($rows['result'] -ne 'pass') { throw "Regex Help placement runtime result=$($rows['result']); report=$($rows | Out-String)" }
    Write-Host 'Regex Help placement runtime smoke passed.'
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }