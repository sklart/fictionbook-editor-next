[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('light','dark')][string]$Theme='light',
    [ValidateSet('ru-RU','en-US')][string]$Locale='ru-RU')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'ui-defects'
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_UI_LOCALE')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name)}
try {
    $seed=New-IsolatedFbeSettingsSeed $iso
    $scripts=Join-Path $iso.Root 'ProbeScripts';New-Item -ItemType Directory -Path $scripts | Out-Null
    [IO.File]::WriteAllText((Join-Path $scripts 'Probe.js'),'function Run() {}',[Text.UTF8Encoding]::new($false))
    $settings=[IO.File]::ReadAllText($seed).Replace('<InterfaceTheme>0</InterfaceTheme>',"<InterfaceTheme>$(if($Theme -eq 'dark'){2}else{1})</InterfaceTheme>")
    $settings=$settings.Replace('</Settings>',"<IntefaceLangID>$(if($Locale -eq 'ru-RU'){1049}else{1033})</IntefaceLangID><ScriptsFolder>$scripts</ScriptsFolder></Settings>")
    [IO.File]::WriteAllText($seed,$settings,[Text.UTF8Encoding]::new($false))
    $fixture=Join-Path $iso.Root 'probe.fb2';$report=Join-Path $iso.Root 'ui-defects.txt'
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fb2-metadata-cyrillic-smoke.fb2') -Destination $fixture
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='ui-defects-runtime'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Split-Path $seed;$env:FBE_NEXT_UI_LOCALE=$null
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'ui-defects' -Report $report -TimeoutSeconds 60
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    $caret=Join-Path (Split-Path $seed) 'ui-caret-probe.txt'
    if(Test-Path $caret){Write-Host (Get-Content $caret -Raw)}
    Write-Host $text
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "UI defects runtime failed: $Theme/$Locale (exit=$code)"}
    $passed=$true;Write-Host "UI defects $Theme/$Locale PASS"
} finally {
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name])}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
