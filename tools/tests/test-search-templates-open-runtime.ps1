[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot "..\..\out\Release\FBE.exe"), [int]$TimeoutSeconds = 60, [string]$ArtifactDirectory)
$ErrorActionPreference = "Stop"
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe)){ throw "FBE не найден: $FbeExe" }
$dir = Join-Path ([IO.Path]::GetTempPath()) ("fbe-search-templates-open-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $dir | Out-Null
try {
    $fixture = Join-Path $dir "templates.fb2"; $report = Join-Path $dir "report.txt"
    '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Templates</book-title><lang>en</lang></title-info><document-info><id>templates-smoke</id><version>1.0</version></document-info></description><body><section><p>Templates runtime smoke.</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode,$oldScenario,$oldArtifacts=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_ARTIFACT_DIR
    if($ArtifactDirectory) { $ArtifactDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ArtifactDirectory); New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null }
    try {
        $env:FBE_NEXT_TEST_MODE="1"; $env:FBE_NEXT_TEST_SCENARIO="search-templates-open-runtime"; $env:FBE_NEXT_TEST_ARTIFACT_DIR=$ArtifactDirectory
        $process = Start-Process -FilePath $FbeExe -ArgumentList "-b",("`"$report`""),("`"$fixture`"") -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; $detail = if(Test-Path $report){ Get-Content $report -Raw }else{ "<report missing>" }; throw "FBE не завершил runtime smoke Templates: $detail" }
        if($process.ExitCode -ne 0) { $detail = if(Test-Path $report){ Get-Content $report -Raw }else{ "<report missing>" }; throw "Templates runtime smoke завершился с кодом $($process.ExitCode): $detail" }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario; $env:FBE_NEXT_TEST_ARTIFACT_DIR=$oldArtifacts }
    $rows=@{}; Get-Content -LiteralPath $report | ForEach-Object { $kv=$_ -split "=",2; if($kv.Count -eq 2){ $rows[$kv[0]]=$kv[1] } }
    foreach($key in @("find","replace")){ if($rows[$key] -ne "1"){ throw "Templates runtime smoke failed: $key=$($rows[$key])" } }
    if($ArtifactDirectory) { foreach($name in "templates-short.bmp","templates-long-regexp.bmp","pin-hover.bmp","pin-pinned.bmp") { if(-not (Test-Path -LiteralPath (Join-Path $ArtifactDirectory $name))) { throw "Templates screenshot missing: $name" } } }
    Write-Host "Find/Replace Templates native runtime smoke passed."
} finally { Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue }