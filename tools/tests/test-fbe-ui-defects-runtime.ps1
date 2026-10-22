[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('light','dark','automatic')][string]$Theme='light',
    [ValidateSet('ru-RU','en-US')][string]$Locale='ru-RU',
    [string]$ArtifactDirectory,
    [ValidateRange(1,100)][int]$MenuCycles=20)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'ui-defects'
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_UI_LOCALE','FBE_NEXT_TEST_REQUESTED_THEME','FBE_NEXT_TEST_ARTIFACT_DIR','FBE_NEXT_TEST_MENU_CYCLES','FBE_NEXT_TEST_MENU_THEME')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name)}
try {
    $seed=New-IsolatedFbeSettingsSeed $iso
    $scripts=Join-Path $iso.Root 'ProbeScripts';New-Item -ItemType Directory -Path $scripts | Out-Null
    [IO.File]::WriteAllText((Join-Path $scripts 'Probe.js'),'function Run() {}',[Text.UTF8Encoding]::new($false))
    $requestedTheme=if($Theme -eq 'dark'){2}elseif($Theme -eq 'automatic'){0}else{1}
    $settings=[IO.File]::ReadAllText($seed).Replace('<InterfaceTheme>0</InterfaceTheme>',"<InterfaceTheme>$requestedTheme</InterfaceTheme>")
    $settings=$settings.Replace('</Settings>',"<IntefaceLangID>$(if($Locale -eq 'ru-RU'){1049}else{1033})</IntefaceLangID><ScriptsFolder>$scripts</ScriptsFolder></Settings>")
    [IO.File]::WriteAllText($seed,$settings,[Text.UTF8Encoding]::new($false))
    $fixture=Join-Path $iso.Root 'probe.fb2';$report=Join-Path $iso.Root 'ui-defects.txt'
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fb2-metadata-cyrillic-smoke.fb2') -Destination $fixture
    $env:FBE_NEXT_TEST_MENU_CYCLES=[string]$MenuCycles;$env:FBE_NEXT_TEST_MENU_THEME=$null
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='ui-defects-runtime'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Split-Path $seed;$env:FBE_NEXT_UI_LOCALE=$null
    if(!$ArtifactDirectory){$ArtifactDirectory=Join-Path $PSScriptRoot "../../out/issue64/menu-$Theme-$Locale"}
    New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
    $env:FBE_NEXT_TEST_ARTIFACT_DIR=(Resolve-Path $ArtifactDirectory).Path;$env:FBE_NEXT_TEST_REQUESTED_THEME="$requestedTheme"
    @{exe=(Get-FileHash $iso.Exe -Algorithm SHA256).Hash;os=[Environment]::OSVersion.VersionString;theme=$Theme;locale=$Locale;cycles=$MenuCycles;profile=$seed} | ConvertTo-Json | Set-Content (Join-Path $ArtifactDirectory 'inputs.json') -Encoding utf8
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'ui-defects' -Report $report -TimeoutSeconds 300
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    [IO.File]::WriteAllText((Join-Path $ArtifactDirectory 'ui-defects.txt'),$text,[Text.UTF8Encoding]::new($false))
    $caret=Join-Path (Split-Path $seed) 'ui-caret-probe.txt'
    if(Test-Path $caret){Write-Host (Get-Content $caret -Raw)}
    Write-Host $text
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "UI defects runtime failed: $Theme/$Locale (exit=$code)"}
    $passed=$true;Write-Host "UI defects $Theme/$Locale PASS"
} finally {
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name])}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
