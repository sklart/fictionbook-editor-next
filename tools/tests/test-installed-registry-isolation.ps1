[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
function Get-RegistrySnapshot([string]$Path, [Microsoft.Win32.RegistryView]$View) {
    $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser, $View)
    $root = $baseKey.OpenSubKey($Path, $false)
    if(-not $root) { $baseKey.Dispose(); return '<missing>' }
    $items = [System.Collections.Generic.List[string]]::new()
    function Visit-Key($key, [string]$relative) {
        foreach($name in @($key.GetValueNames() | Sort-Object)) {
            $value = $key.GetValue($name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            $items.Add(($relative + '|' + $name + '|' + $key.GetValueKind($name) + '|' + ($value | ConvertTo-Json -Compress -Depth 10)))
        }
        foreach($name in @($key.GetSubKeyNames() | Sort-Object)) {
            $child = $key.OpenSubKey($name, $false)
            try { Visit-Key $child ($relative + '\' + $name) } finally { $child.Dispose() }
        }
    }
    try { Visit-Key $root '' } finally { $root.Dispose(); $baseKey.Dispose() }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($items -join "`n"))
    return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
}
$realPath = 'Software\FBETeam\FictionBook Editor Next'
$typelibPath = 'Software\Classes\TypeLib'
$views = @([Microsoft.Win32.RegistryView]::Registry32, [Microsoft.Win32.RegistryView]::Registry64)
$beforeProfile = @{}; $beforeTypelib = @{}
foreach($view in $views) { $beforeProfile[$view] = Get-RegistrySnapshot $realPath $view; $beforeTypelib[$view] = Get-RegistrySnapshot $typelibPath $view }
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'installed-registry'
$testKey = 'Software\FBE-Next-Test-Runtimes\' + (Split-Path -Leaf $iso.Root)
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old = @{}; foreach($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name) }
$passed = $false
try {
    New-IsolatedFbeSettingsSeed -Isolation $iso | Out-Null
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $null
    $readOnly = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList @('--installed', '--print-update-artifact') -PassThru
    if(-not $readOnly.WaitForExit(30000)) { Stop-Process -Id $readOnly.Id -Force; $readOnly.WaitForExit(); throw 'Read-only installed query timed out.' }
    if($readOnly.ExitCode -ne 0) { throw 'Read-only installed query must remain available without a test profile.' }
    $unisolated = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList '--installed' -PassThru
    if(-not $unisolated.WaitForExit(30000)) { Stop-Process -Id $unisolated.Id -Force; $unisolated.WaitForExit(); throw 'Unisolated test launch timed out.' }
    if($unisolated.ExitCode -ne 2) { throw "Unisolated installed test did not fail safely: $($unisolated.ExitCode)" }
    $env:FBE_NEXT_TEST_SCENARIO = 'main-toolbar-size-setting-write'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = Join-Path $iso.DataPath 'Settings'
    $unownedFbeDir = Join-Path $iso.Root 'unowned-fbe'
    New-Item -ItemType Directory -Path $unownedFbeDir | Out-Null
    $unownedFbe = Join-Path $unownedFbeDir 'FBE.exe'
    Copy-Item -LiteralPath $iso.Exe -Destination $unownedFbe
    $unownedFbeProcess = Start-Process -FilePath $unownedFbe -WorkingDirectory $unownedFbeDir -ArgumentList '--installed' -PassThru
    if(-not $unownedFbeProcess.WaitForExit(30000)) { Stop-Process -Id $unownedFbeProcess.Id -Force; $unownedFbeProcess.WaitForExit(); throw 'Unowned FBE test timed out.' }
    if($unownedFbeProcess.ExitCode -ne 2) { throw 'Unowned installed FBE test did not fail safely.' }
    $report = Join-Path $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY 'Diagnostics\portable-state-report.txt'
    $p = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList '--installed' -PassThru
    $code = Wait-IsolatedFbeProcess -Process $p -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 120
    $text = if(Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing>' }
    if($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Isolated installed test failed: $text" }
    $env:FBE_NEXT_TEST_SCENARIO = 'main-toolbar-size-setting-read'
    Remove-Item -LiteralPath $report -Force
    $p = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList '--installed' -PassThru
    $code = Wait-IsolatedFbeProcess -Process $p -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 120
    $text = if(Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing>' }
    if($code -ne 0 -or $text -notmatch '(?m)^main-toolbar-setting-restart=1\r?$') { throw "Installed settings did not survive restart: $text" }
    # Probe the actual shared plug-in store using an owned, unique fake legacy key.
    . (Join-Path $PSScriptRoot '..\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
    $probe = Join-Path $iso.Root 'plugin-registry-isolation.exe'
    $probeObject = Join-Path $iso.Root 'plugin-registry-isolation.obj'
    & cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'plugin-registry-isolation.cpp') "/Fo:$probeObject" /link ole32.lib oleaut32.lib shell32.lib shlwapi.lib user32.lib "/OUT:$probe"
    if($LASTEXITCODE -ne 0) { throw 'Plugin registry isolation probe failed to build.' }
    $fakeParent = 'Software\FBE-Next-Registry-Probe-' + (Split-Path -Leaf $iso.Root)
    $fakeKey = $fakeParent + '\plugin'
    $existingFake = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($fakeParent, $false)
    if($existingFake) { $existingFake.Dispose(); throw 'Refusing to use an existing registry probe key.' }
    try {
        $probeProcess = Start-Process -FilePath $probe -WorkingDirectory $iso.Runtime -ArgumentList @('--installed', $fakeKey) -PassThru
        if(-not $probeProcess.WaitForExit(30000)) { Stop-Process -Id $probeProcess.Id -Force; $probeProcess.WaitForExit(); throw 'Plugin settings probe timed out.' }
        if($probeProcess.ExitCode -ne 0) { throw "Plugin settings probe failed: $($probeProcess.ExitCode)" }
        $inTest = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey(($testKey + '\' + $fakeKey), $false)
        if(-not $inTest) { throw 'Plug-in value was not written to the test registry.' }
        try { if($inTest.GetValue('value') -ne 12345) { throw 'Plug-in test value was not persisted.' } } finally { $inTest.Dispose() }
        $unownedDir = Join-Path $iso.Root 'unowned-probe'
        New-Item -ItemType Directory -Path $unownedDir | Out-Null
        $unownedProbe = Join-Path $unownedDir 'plugin-registry-isolation.exe'
        Copy-Item -LiteralPath $probe -Destination $unownedProbe
        $unownedProcess = Start-Process -FilePath $unownedProbe -WorkingDirectory $unownedDir -ArgumentList @('--installed', $fakeKey) -PassThru
        if(-not $unownedProcess.WaitForExit(30000)) { Stop-Process -Id $unownedProcess.Id -Force; $unownedProcess.WaitForExit(); throw 'Unowned plug-in probe timed out.' }
        if($unownedProcess.ExitCode -ne 2) { throw "Unowned plug-in probe did not reject registry access: $($unownedProcess.ExitCode)" }
        $inReal = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($fakeKey, $false)
        if($inReal) { $inReal.Dispose(); throw 'Plug-in value leaked outside the test registry.' }
    } finally { [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($fakeParent, $false) }
    $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($testKey, $false)
    if(-not $key) { throw 'Expected isolated test registry root was not created.' }
    try {
        $toolbar = $key.OpenSubKey('Toolbars', $false)
        if(-not $toolbar) { throw 'Toolbar state was not written to the isolated registry.' }
        $toolbar.Dispose()
    } finally { $key.Dispose() }
    foreach($view in $views) {
        if((Get-RegistrySnapshot $realPath $view) -cne $beforeProfile[$view]) { throw "Real FBE registry profile changed in $view view." }
        if((Get-RegistrySnapshot $typelibPath $view) -cne $beforeTypelib[$view]) { throw "Per-user type library registry changed in $view view." }
    }
    $passed = $true
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name]) }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
$remaining = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($testKey, $false)
if($remaining) { $remaining.Dispose(); throw 'The isolated registry root was not removed.' }
Write-Host 'Installed registry isolation: PASS (toolbar and plug-in settings persisted in test hive; real profile and typelib unchanged; test hive removed).'
# Simulate a killed PowerShell owner; the next runtime cleanup must remove its hive.
$ready = Join-Path ([IO.Path]::GetTempPath()) ('fbe-registry-orphan-' + [guid]::NewGuid().ToString('N') + '.txt')
$helper = (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1').Replace("'", "''")
$readyQuoted = $ready.Replace("'", "''")
$sourceQuoted = (Resolve-Path -LiteralPath $FbeExe).Path.Replace("'", "''")
$command = ". '$helper'; `$iso = New-IsolatedFbeRuntime -FbeExe '$sourceQuoted' -Name registry-orphan; `$path = 'Software\FBE-Next-Test-Runtimes\' + (Split-Path -Leaf `$iso.Root); `$key = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey(`$path); `$key.SetValue('test', 1); `$key.Dispose(); [IO.File]::WriteAllText('$readyQuoted', `$iso.Root); Start-Sleep -Seconds 120"
$encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
$pwsh = (Get-Process -Id $PID).Path
$child = Start-Process -FilePath $pwsh -ArgumentList "-NoProfile -EncodedCommand $encoded" -WindowStyle Hidden -PassThru
$previousArtifacts = $env:FBE_RUNTIME_FAILURE_ARTIFACTS
$env:FBE_RUNTIME_FAILURE_ARTIFACTS = Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'build\registry-orphan-test'
try {
    for($i = 0; $i -lt 150 -and -not (Test-Path -LiteralPath $ready) -and -not $child.HasExited; ++$i) { Start-Sleep -Milliseconds 100 }
    if(-not (Test-Path -LiteralPath $ready)) { throw 'The registry orphan fixture did not start.' }
    $orphanRoot = [IO.File]::ReadAllText($ready)
    $orphanKey = 'Software\FBE-Next-Test-Runtimes\' + (Split-Path -Leaf $orphanRoot)
    $live = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($orphanKey, $false)
    if(-not $live) { throw 'The test hive was not created.' }
    $live.Dispose()
    Stop-Process -Id $child.Id -Force
    $child.WaitForExit()
    Remove-AbandonedIsolatedFbeRuntimes
    $left = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($orphanKey, $false)
    if($left) { $left.Dispose(); throw 'Interrupted runtime hive was not cleaned.' }
    if(Test-Path -LiteralPath $orphanRoot) { throw 'Interrupted runtime directory was not cleaned.' }
} finally {
    if(-not $child.HasExited) { Stop-Process -Id $child.Id -Force -ErrorAction SilentlyContinue; $child.WaitForExit() }
    Remove-AbandonedIsolatedFbeRuntimes
    $env:FBE_RUNTIME_FAILURE_ARTIFACTS = $previousArtifacts
    Remove-Item -LiteralPath $ready -Force -ErrorAction SilentlyContinue
}
Write-Host 'Interrupted PowerShell registry and runtime cleanup: PASS.'
