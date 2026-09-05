<# Exercises CSettings Save/Load in an isolated portable profile. #>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
function Get-Snapshot([string]$Path) { if(-not (Test-Path -LiteralPath $Path)) { return '<absent>' }; return (Get-ChildItem -LiteralPath $Path -Recurse -File | Sort-Object FullName | ForEach-Object { "$($_.FullName)|$($_.Length)|$((Get-FileHash $_.FullName -Algorithm SHA256).Hash)" }) -join "`n" }
function Invoke-SettingsScenario([string]$Name, [hashtable]$Values, [hashtable]$Expected, [string]$SettingsXml) {
    $data = Join-Path $portable 'TestData'; Remove-Item -LiteralPath $data -Recurse -Force -ErrorAction SilentlyContinue
    $settings = Join-Path $data 'Settings'; New-Item -ItemType Directory -Path $settings -Force | Out-Null
    if($SettingsXml) { Set-Content -LiteralPath (Join-Path $settings 'Settings.xml') -Value $SettingsXml -Encoding utf8 }
    $report = Join-Path $root "$Name.tsv"; $old = @{}
    foreach($key in $Values.Keys) { $old[$key] = [Environment]::GetEnvironmentVariable($key, 'Process'); [Environment]::SetEnvironmentVariable($key, $Values[$key], 'Process') }
    $oldMode = $env:FBE_NEXT_TEST_MODE; $oldScenario = $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'editor-background-settings'
        $process = Start-Process -FilePath $portableExe -ArgumentList @('-b', $report, '--portable', $fixture) -WorkingDirectory $portable -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "$Name timed out." }
        if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "$Name did not produce a settings report." }
    } finally { foreach($key in $Values.Keys) { [Environment]::SetEnvironmentVariable($key, $old[$key], 'Process') }; $env:FBE_NEXT_TEST_MODE = $oldMode; $env:FBE_NEXT_TEST_SCENARIO = $oldScenario }
    $phase = if($Values.Count) { 'roundtrip' } else { 'loaded' }; $row = @(Import-Csv -LiteralPath $report -Delimiter "`t" | Where-Object phase -eq $phase)
    if($row.Count -ne 1) { throw "$Name has no expected settings report row." }
    foreach($key in $Expected.Keys) { $property = @{ kind='kind'; id='id'; customPath='custom_path'; layout='layout' }[$key]; if($row[0].$property -cne $Expected[$key]) { throw "${Name}: $key expected '$($Expected[$key])', got '$($row[0].$property)'." } }
    return $settings
}
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-editor-background-settings-' + [guid]::NewGuid().ToString('N'))
$installed = Join-Path $env:LOCALAPPDATA 'FBE Next'; $installedBefore = Get-Snapshot $installed
try {
    Copy-Item -LiteralPath (Split-Path $FbeExe -Parent) -Destination $root -Recurse -Force
    $portable = $root; $portableExe = Join-Path $portable 'FBE.exe'
    "[Portable]`r`nDataPath=TestData`r`n" | Set-Content -LiteralPath (Join-Path $portable 'portable.ini') -Encoding utf8NoBOM
    $fixture = Join-Path $root 'settings.fb2'
    '<?xml version="1.0"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>T</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>settings-test</id><version>1</version></document-info></description><body><section><p>T</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    Invoke-SettingsScenario 'legacy' @{} @{ kind='none'; layout='tile' } $null | Out-Null
    Invoke-SettingsScenario 'unknown' @{} @{ kind='none'; layout='tile' } '<FBE><EditorBackgroundKind>remote</EditorBackgroundKind><EditorBackgroundLayout>stretch</EditorBackgroundLayout></FBE>' | Out-Null
    foreach($kind in @('none','builtin','custom')) { foreach($layout in @('tile','center','contain','cover')) {
        $path = if($kind -eq 'custom') { 'C:\Фоны FBE # % (тест).jpeg' } else { '' }
        $settings = Invoke-SettingsScenario "$kind-$layout" @{ FBE_NEXT_TEST_SETTINGS_KIND=$kind; FBE_NEXT_TEST_SETTINGS_ID='01_clean_white'; FBE_NEXT_TEST_SETTINGS_PATH=$path; FBE_NEXT_TEST_SETTINGS_LAYOUT=$layout } @{ kind=$kind; id='01_clean_white'; customPath=$path; layout=$layout } $null
        $saved = Get-Content -LiteralPath (Join-Path $settings 'Settings.xml') -Raw
        foreach($key in @('EditorBackgroundKind','EditorBackgroundId','EditorBackgroundCustomPath','EditorBackgroundLayout')) { if($saved -notmatch $key) { throw "Saved Settings.xml missed $key." } }
    } }
    Write-Host 'Editor background CSettings portable Save/Load round-trip verified.'
} finally {
    if((Get-Snapshot $installed) -cne $installedBefore) { throw '%LOCALAPPDATA%\FBE Next changed during isolated settings round-trip.' }
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
