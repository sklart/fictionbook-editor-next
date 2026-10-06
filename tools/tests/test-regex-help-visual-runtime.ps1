[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot "..\..\out\Release\FBE.exe"), [string]$ArtifactDirectory, [int]$TimeoutSeconds = 45)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe)) { throw "FBE не найден: $FbeExe" }
if (-not $ArtifactDirectory) { throw 'Укажите -ArtifactDirectory для visual smoke Full Help.' }
$ArtifactDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ArtifactDirectory)
New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-regex-help-visual-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $fixture = Join-Path $root 'help.fb2'; $report = Join-Path $root 'report.txt'
    '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Help</book-title><lang>en</lang></title-info><document-info><id>regex-help-visual</id><version>1.0</version></document-info></description><body><section><p>Help visual smoke.</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode,$oldScenario,$oldArtifacts=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_ARTIFACT_DIR
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='regex-help-visual-runtime'; $env:FBE_NEXT_TEST_ARTIFACT_DIR=$ArtifactDirectory
        $process = Start-Process -FilePath $FbeExe -ArgumentList '-b',("`"$report`""),("`"$fixture`"") -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Regex Help visual smoke timed out.' }
        if ($process.ExitCode -ne 0) { throw "Regex Help visual smoke failed with exit code $($process.ExitCode)." }
    } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_ARTIFACT_DIR=$oldMode,$oldScenario,$oldArtifacts }
    $rows=@{}; Get-Content -LiteralPath $report | ForEach-Object { $pair=$_ -split '=',2; if($pair.Count -eq 2){$rows[$pair[0]]=$pair[1]} }
    foreach($key in 'initial','restored') { if($rows[$key] -ne '1') { throw "Regex Help visual smoke failed: $key=$($rows[$key])" } }
    foreach($name in 'full-help-initial-design.bmp','full-help-restored-code.bmp') { if(-not (Test-Path -LiteralPath (Join-Path $ArtifactDirectory $name))) { throw "Full Help screenshot missing: $name" } }
    Write-Host 'Regex Help visual placement and rendering smoke passed.'
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }