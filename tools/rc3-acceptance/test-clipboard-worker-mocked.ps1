$ErrorActionPreference='Stop'
$launcher=Get-Content -Raw (Join-Path $PSScriptRoot '../tests/test-fbe-scripting-clipboard-ole-ci.ps1')
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseInput($launcher,[ref]$tokens,[ref]$errors)
if($errors.Count){throw ($errors -join "`n")}
$assignment=$ast.Find({param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -eq '$worker'},$true)
if(!$assignment){throw 'Worker assignment not found'}
$worker=$assignment.Right.Expression.Value.Replace('@@SID@@','test').Replace('@@PROBE@@','$false')
$worker=$worker -replace '(?m)^    if \(\[Security\.Principal\.WindowsIdentity\].*?\r?\n',''
$worker=$worker -replace '(?s)        Add-Type -TypeDefinition @"\r?\n.*?\r?\n"@\r?\n        \$station=\[FbePrivateClipboardStation\]::Name\(\)','        $station=''mock-private'''
$null=[Management.Automation.Language.Parser]::ParseInput($worker,[ref]$tokens,[ref]$errors)
if($errors.Count -or $worker -match 'GetCurrent\(\)|Add-Type -TypeDefinition'){throw 'Worker mock isolation failed.'}
$root=Join-Path ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../out'))) ('clipboard-worker-mock-'+[guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Force (Join-Path $root 'tools/tests'),(Join-Path $root 'build'),(Join-Path $root 'runtime') | Out-Null
    Set-Content -LiteralPath (Join-Path $root 'worker.ps1') -Value $worker
    Set-Content -LiteralPath (Join-Path $root 'test-fbe-scripting-stage3c.ps1') -Value 'if($env:FBE_MOCK_STAGE_FAIL -eq ''1''){throw ''stage failed''}'
    Set-Content -LiteralPath (Join-Path $root 'tools/tests/test-fbe-native-copy-paste-runtime.ps1') -Value @'
if($env:FBE_MOCK_NATIVE_FAIL -eq '1'){throw 'native failed'}
$destination=Join-Path $PSScriptRoot '../../build/community-native-copy-mock'
New-Item -ItemType Directory -Force $destination | Out-Null
foreach($case in @('word','formatted')){'result=PASS' | Set-Content (Join-Path $destination ($case+'.txt'))}
'@
    foreach($case in @(@{stage='0';native='0';result='PASS'},@{stage='1';native='0';result='FAIL'},@{stage='0';native='1';result='FAIL'})) {
        $env:FBE_MOCK_STAGE_FAIL=$case.stage;$env:FBE_MOCK_NATIVE_FAIL=$case.native
        & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $root 'worker.ps1')
        $actual=(Get-Content -Raw (Join-Path $root 'finished.txt')).Trim()
        $summary=Get-Content -Raw (Join-Path $root 'evidence/clipboard-summary.json') | ConvertFrom-Json
        if($actual -cne $case.result -or $summary.stage3c -cne $(if($case.stage -eq '0'){'PASS'}else{'FAIL'}) -or $summary.nativeCopyPaste -cne $(if($case.native -eq '0'){'PASS'}else{'FAIL'})){throw 'Worker failed to preserve independent scenario outcomes.'}
    }
    Write-Host 'Clipboard worker independent outcomes and evidence: PASS (mock only)'
} finally {
    Remove-Item Env:FBE_MOCK_STAGE_FAIL,Env:FBE_MOCK_NATIVE_FAIL -ErrorAction SilentlyContinue
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
}
