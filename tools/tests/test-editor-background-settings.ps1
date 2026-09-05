<# Exercises CSettings Save/Load using a CSettings-generated seed in a portable profile. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180, [switch]$KeepArtifacts)
$ErrorActionPreference = 'Stop'; $FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe)) { throw "Не найден FBE: $FbeExe" }
function Get-Snapshot([string]$Path) { if(-not (Test-Path -LiteralPath $Path)) { return '<absent>' }; (Get-ChildItem -LiteralPath $Path -Recurse -File | Sort-Object FullName | ForEach-Object { "$($_.FullName)|$($_.Length)|$((Get-FileHash $_.FullName -Algorithm SHA256).Hash)" }) -join "`n" }
function Invoke-Fbe([string]$Name, [hashtable]$Environment) {
    $report = Join-Path $root "$Name.tsv"; $old = @{}
    foreach($key in $Environment.Keys) { $old[$key] = [Environment]::GetEnvironmentVariable($key, 'Process'); [Environment]::SetEnvironmentVariable($key, $Environment[$key], 'Process') }
    $oldMode=$env:FBE_NEXT_TEST_MODE; $oldScenario=$env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='editor-background-settings'
        $p=Start-Process -FilePath $portableExe -ArgumentList @('-b',$report,'--portable',$fixture) -WorkingDirectory $portable -PassThru
        if(-not $p.WaitForExit($TimeoutSeconds*1000)) { Stop-Process -Id $p.Id -Force; throw "$Name timed out." }
        if($p.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "$Name did not produce a report." }
        return @(Import-Csv -LiteralPath $report -Delimiter "`t")
    } finally { foreach($key in $Environment.Keys) { [Environment]::SetEnvironmentVariable($key,$old[$key],'Process') }; $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
}
function Set-Seed([string]$Xml) { New-Item -ItemType Directory -Path $settingsDir -Force | Out-Null; Set-Content -LiteralPath $settingsFile -Value $Xml -Encoding utf8 }
function Assert-Row($Rows,[string]$Phase,[hashtable]$Expected,[string]$Name) {
    $row=@($Rows|Where-Object phase -eq $Phase); if($row.Count-ne 1){throw "$Name has no $Phase row."}
    foreach($key in $Expected.Keys) { $property=@{customPath='custom_path';colorBg='color_bg'}[$key]; if(-not $property){$property=$key}; if([string]$row[0].$property -cne [string]$Expected[$key]) { throw "${Name}: $key expected '$($Expected[$key])', got '$($row[0].$property)'." } }
}
$root=Join-Path ([IO.Path]::GetTempPath()) ('fbe-editor-background-settings-'+[guid]::NewGuid().ToString('N')); $installed=Join-Path $env:LOCALAPPDATA 'FBE Next'; $before=Get-Snapshot $installed; $completed=$false
try {
    Copy-Item -LiteralPath (Split-Path $FbeExe -Parent) -Destination $root -Recurse -Force; $portable=$root; $portableExe=Join-Path $portable 'FBE.exe'
    "[Portable]`r`nDataPath=TestData`r`n"|Set-Content -LiteralPath (Join-Path $portable 'portable.ini') -Encoding utf8NoBOM
    $fixture=Join-Path $root 'settings.fb2'; '<?xml version="1.0"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>T</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>settings-test</id><version>1</version></document-info></description><body><section><p>T</p></section></body></FictionBook>'|Set-Content -LiteralPath $fixture -Encoding utf8
    $settingsDir=Join-Path $portable 'TestData\Settings'; $settingsFile=Join-Path $settingsDir 'Settings.xml'; $sentinel='1193046'
    # Bootstrap is structurally valid, then the application itself serializes the canonical seed.
    Set-Seed "<?xml version=`"1.0`" encoding=`"utf-8`"?><FBE><Settings ID=`"0`"><ColorBG>$sentinel</ColorBG></Settings></FBE>"
    $seedRows=Invoke-Fbe 'seed-writer' @{FBE_NEXT_TEST_SETTINGS_SEED='1'}; Assert-Row $seedRows 'seeded' @{kind='none';layout='tile';colorBg=$sentinel} 'seed-writer'
    $seed=Get-Content -LiteralPath $settingsFile -Raw; [xml]$seedDocument=$seed; if($null-eq$seedDocument.SelectSingleNode('/FBE/Settings[@ID="0"]/ColorBG')) { throw 'CSettings did not create a canonical Settings.xml seed.' }
    # Legacy is the canonical XML with only the four new fields removed.
    [xml]$legacy=$seed; $node=$legacy.SelectSingleNode('/FBE/Settings[@ID="0"]'); foreach($name in @('EditorBackgroundKind','EditorBackgroundId','EditorBackgroundCustomPath','EditorBackgroundLayout')) { $child=$node.SelectSingleNode($name); if($child){[void]$node.RemoveChild($child)} }; Set-Seed $legacy.OuterXml
    Assert-Row (Invoke-Fbe 'legacy' @{}) 'loaded' @{kind='none';layout='tile';colorBg=$sentinel} 'legacy'
    # Unknown values are placed into the real serialized Settings node, not a synthetic root.
    [xml]$unknown=$seed; $node=$unknown.SelectSingleNode('/FBE/Settings[@ID="0"]'); foreach($pair in @{EditorBackgroundKind='remote';EditorBackgroundLayout='stretch'}.GetEnumerator()){ $child=$node.SelectSingleNode($pair.Key); if(-not $child){$child=$unknown.CreateElement($pair.Key);[void]$node.AppendChild($child)};$child.InnerText=$pair.Value }; Set-Seed $unknown.OuterXml
    Assert-Row (Invoke-Fbe 'unknown' @{}) 'loaded' @{kind='none';layout='tile';colorBg=$sentinel} 'unknown'
    foreach($kind in 'none','builtin','custom'){foreach($layout in 'tile','center','contain','cover'){
        Set-Seed $seed; $path=if($kind-eq'custom'){'C:\Фоны FBE # % (тест).jpeg'}else{''}; $rows=Invoke-Fbe "$kind-$layout-writer" @{FBE_NEXT_TEST_SETTINGS_KIND=$kind;FBE_NEXT_TEST_SETTINGS_ID='01_clean_white';FBE_NEXT_TEST_SETTINGS_PATH=$path;FBE_NEXT_TEST_SETTINGS_LAYOUT=$layout}
        Assert-Row $rows 'saved' @{kind=$kind;id='01_clean_white';customPath=$path;layout=$layout;colorBg=$sentinel} "$kind-$layout-writer"
        $saved=Get-Content -LiteralPath $settingsFile -Raw; foreach($key in 'EditorBackgroundKind','EditorBackgroundId','EditorBackgroundCustomPath','EditorBackgroundLayout'){if($saved-notmatch $key){throw "$kind-$layout did not save $key."}}
        Assert-Row (Invoke-Fbe "$kind-$layout-reader" @{}) 'loaded' @{kind=$kind;id='01_clean_white';customPath=$path;layout=$layout;colorBg=$sentinel} "$kind-$layout-reader"
    }}
    $completed=$true; Write-Host 'Editor background CSettings portable Save/Load round-trip verified.'
} finally { if((Get-Snapshot $installed)-cne$before){throw '%LOCALAPPDATA%\FBE Next changed during isolated settings round-trip.'}; if($completed -or -not $KeepArtifacts){Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue}else{Write-Host "Settings test artifacts: $root"; Get-ChildItem -LiteralPath (Join-Path $portable 'TestData\Settings') -Force -ErrorAction SilentlyContinue | Select-Object Name,Length} }
