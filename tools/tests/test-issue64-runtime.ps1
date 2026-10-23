[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),[ValidateSet('portable','installed')][string]$Mode='portable',[string]$ArtifactDirectory,[switch]$BlankDocument,[switch]$SimulateTooltipDefect,[switch]$DeferSettingsDiscovery,[switch]$SimulateMissingSettings)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if(!$ArtifactDirectory){$ArtifactDirectory=Join-Path $repoRoot "out/issue64/$Mode"}
New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
$ArtifactDirectory=(Resolve-Path $ArtifactDirectory).Path
function Get-FbeRegistryFingerprint {
    $root='HKCU:\Software\FBETeam'
    if(!(Test-Path -LiteralPath $root)){return 'absent'}
    $rows=@(Get-Item -LiteralPath $root)+@(Get-ChildItem -LiteralPath $root -Recurse)
    $data=foreach($key in ($rows | Sort-Object Name)) {foreach($name in ($key.GetValueNames() | Sort-Object)) { @($key.Name,$name,[string]$key.GetValueKind($name),$key.GetValue($name)) | ConvertTo-Json -Compress -Depth 8 }}
    $sha=[Security.Cryptography.SHA256]::Create();try{[Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($data -join "`n"))))}finally{$sha.Dispose()}
}
$registryBefore=Get-FbeRegistryFingerprint
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'issue64'
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TEST_ARTIFACT_DIR','FBE_NEXT_TEST_PLUGIN_SETTINGS','FBE_NEXT_UI_LOCALE','FBE_NEXT_TEST_ISSUE64_TOOLTIP_DEFECT','FBE_NEXT_TEST_ISSUE64_DEFER_SETTINGS','FBE_NEXT_TEST_ISSUE64_MISSING_SETTINGS')
$saved=@{};foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
try {
    $seed=New-IsolatedFbeSettingsSeed $iso
    $scripts=Join-Path $iso.Root 'ProbeScripts';New-Item -ItemType Directory -Path $scripts | Out-Null
    [IO.File]::WriteAllText((Join-Path $scripts 'Probe.js'),'function Run() {}',[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($seed,([IO.File]::ReadAllText($seed).Replace('</Settings>',"<ScriptsFolder>$scripts</ScriptsFolder></Settings>")),[Text.UTF8Encoding]::new($false))
    $report=Join-Path $iso.Root 'issue64.txt'
    $fixture=(Resolve-Path (Join-Path $PSScriptRoot 'fb2-validate-valid-smoke.fb2')).Path
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='issue64-runtime';$env:FBE_NEXT_TEST_PLUGIN_SETTINGS=$null;$env:FBE_NEXT_UI_LOCALE='ru-RU'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Split-Path $seed;$env:FBE_NEXT_TEST_ARTIFACT_DIR=$ArtifactDirectory
    $env:FBE_NEXT_TEST_ISSUE64_TOOLTIP_DEFECT=if($SimulateTooltipDefect){'1'}else{$null}
    $env:FBE_NEXT_TEST_ISSUE64_DEFER_SETTINGS=if($DeferSettingsDiscovery){'1'}else{$null}
    $env:FBE_NEXT_TEST_ISSUE64_MISSING_SETTINGS=if($SimulateMissingSettings){'1'}else{$null}
    $captureStart=[DateTime]::UtcNow
    $arguments=@("--$Mode",'-b',$report);if(!$BlankDocument){$arguments+=$fixture}
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'issue64-runtime' -Report $report -TimeoutSeconds 150
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if(Test-Path $report){Copy-Item $report (Join-Path $ArtifactDirectory 'runtime.txt')}
    Write-Host $text
    if($code -ne 0 -or $text -notmatch '(?m)^result=PASS\r?$'){throw "Issue64 runtime failed ($Mode), exit=$code"}
    foreach($theme in @(1,2,0)) { foreach($plugin in @(0,1,2,3)) {
        $pages=if($plugin -eq 2){5}else{4}
        for($page=0;$page -lt $pages;$page++) {
            $path=Join-Path $ArtifactDirectory "issue64-theme$theme-plugin$plugin-nested-options-$page.png"
            $capture=Get-Item -LiteralPath $path -ErrorAction SilentlyContinue
            if(!$capture -or $capture.LastWriteTimeUtc -lt $captureStart){throw "Missing fresh nested options capture: $path; exists=$([bool]$capture); lastWriteUtc=$(if($capture){$capture.LastWriteTimeUtc.ToString('o')}else{'absent'}); captureStartUtc=$($captureStart.ToString('o'))"}
        }
    } }
    if((Get-FbeRegistryFingerprint) -ne $registryBefore){throw 'User FBE registry settings changed during the isolated test'}
    Write-Host 'user_registry_unchanged=PASS'
    $passed=$true
} catch {
    # Retain only this test's captures and safe file metadata. A native PASS
    # must not hide a missing/stale screenshot, and CI must preserve the proof.
    $captureFiles=@(Get-ChildItem -LiteralPath $ArtifactDirectory -File | ForEach-Object {
        @{name=$_.Name;length=$_.Length;lastWriteUtc=$_.LastWriteTimeUtc.ToString('o')}
    })
    @{captureStartUtc=$(if(Get-Variable captureStart -ErrorAction SilentlyContinue){$captureStart.ToString('o')}else{$null});error=$_.Exception.Message;files=$captureFiles} |
        ConvertTo-Json -Depth 5 | Set-Content (Join-Path $ArtifactDirectory 'capture-failure.json') -Encoding utf8
    throw
} finally {
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name])}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
