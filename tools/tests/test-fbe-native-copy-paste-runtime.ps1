[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'), [int]$TimeoutSeconds = 90, [string[]]$Cases = @('char','word','paragraph','within','between','section','formatted','repeat','cross-document'))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$evidence = Join-Path (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path ('build/community-native-copy-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
$fixtureXml = @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><nickname>Fixture</nickname></author><book-title>Native copy</book-title><lang>ru</lang></title-info><document-info><id>native-copy</id><version>1.0</version></document-info></description><body><section id="s1"><p id="copy-src">КОПИЯ_Ж Ω текст</p><p id="copy-rich">Формат <strong>жирный</strong> <a type="note" l:href="#note1">1</a></p><p id="paste-target">ЦЕЛЬ</p></section><section id="s2"><p id="paste-other">ДРУГАЯ</p></section></body><body name="notes"><section id="note1"><p>Сноска</p></section></body></FictionBook>
'@
$savedEnvironment = @{}
foreach($name in @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_COPY_CASE','FBE_NEXT_COPY_ROLE','FBE_NEXT_COPY_READY','FBE_NEXT_COPY_DONE','FBE_NEXT_TRACE')) { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name) }
function Start-CopyProbe($Isolation, [string]$Case, [string]$Role, [string]$Tag) {
    $fixture = Join-Path $Isolation.Root ($Tag + '.fb2'); $report = Join-Path $Isolation.Root ($Tag + '.txt')
    [IO.File]::WriteAllText($fixture,$fixtureXml,[Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_COPY_CASE=$Case; $env:FBE_NEXT_COPY_ROLE=$Role
    $process = Start-Process -FilePath $Isolation.Exe -WorkingDirectory $Isolation.Runtime -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    return @{Process=$process;Report=$report;Tag=$Tag}
}
function Complete-CopyProbe($Probe,$Isolation) {
    $exitCode=Wait-IsolatedFbeProcess -Process $Probe.Process -Isolation $Isolation -Scenario 'community-native-copy-paste' -Report $Probe.Report -TimeoutSeconds $TimeoutSeconds
    if(Test-Path -LiteralPath $Probe.Report) { Copy-Item -LiteralPath $Probe.Report -Destination (Join-Path $evidence ($Probe.Tag+'.txt')) }
    $text=if(Test-Path -LiteralPath $Probe.Report){Get-Content -LiteralPath $Probe.Report -Raw}else{'<missing>'}
    if($exitCode -ne 0 -or $text -notmatch '(?m)^result=PASS\r?$' -or $text -match '(?m)^[a-z_]+=0\r?$') {throw "Native copy/paste failed: $($Probe.Tag), exit=$exitCode`n$text"}
    Write-Host "Native copy/paste PASS: $($Probe.Tag)"
}
try {
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='community-native-copy-paste'; $env:FBE_NEXT_TRACE='1'
    foreach($case in @($Cases | Where-Object { $_ -ne 'cross-document' })) {
        $iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'native-copy'; $passed=$false
        try {
            $null=New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts')
            $probe=Start-CopyProbe $iso $case '' $case; Complete-CopyProbe $probe $iso; $passed=$true
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
    }
    if ($Cases -contains 'cross-document') {
    # Keep the real MSHTML clipboard owner alive in a separate editor process.
    $source=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'native-source'; $target=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'native-target'
    $passed=$false; $sourceProbe=$null
    $ready=Join-Path $source.Root 'ready'; $done=Join-Path $source.Root 'done'
    try {
        $null=New-IsolatedFbeSettingsSeed -Isolation $source -StartupScriptsDirectory (Join-Path $source.Root 'empty-scripts')
        $null=New-IsolatedFbeSettingsSeed -Isolation $target -StartupScriptsDirectory (Join-Path $target.Root 'empty-scripts')
        $env:FBE_NEXT_COPY_READY=$ready; $env:FBE_NEXT_COPY_DONE=$done
        $sourceProbe=Start-CopyProbe $source 'cross-document' 'source' 'cross-source'
        $until=[DateTime]::UtcNow.AddSeconds(30)
        while(-not(Test-Path -LiteralPath $ready) -and [DateTime]::UtcNow -lt $until -and -not $sourceProbe.Process.HasExited){Start-Sleep -Milliseconds 20}
        if(-not(Test-Path -LiteralPath $ready)){throw 'Native clipboard source did not become ready.'}
        $targetProbe=Start-CopyProbe $target 'cross-document' 'target' 'cross-target'
        try {Complete-CopyProbe $targetProbe $target} finally {[IO.File]::WriteAllText($done,'done')}
        Complete-CopyProbe $sourceProbe $source; $passed=$true
    } finally {
        [IO.File]::WriteAllText($done,'done')
        if($sourceProbe -and -not $sourceProbe.Process.HasExited){$null=$sourceProbe.Process.WaitForExit(10000)}
        Complete-IsolatedFbeRuntime -Isolation $source -Passed $passed
        Complete-IsolatedFbeRuntime -Isolation $target -Passed $passed
    }
    }
    Get-FileHash -LiteralPath $FbeExe | Format-List | Out-File (Join-Path $evidence 'executable-sha256.txt')
    Write-Host "Native MSHTML Copy/Cut -> Paste -> Undo/Redo -> Save/Reopen passed. Evidence: $evidence"
} finally {foreach($name in $savedEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name])}}
