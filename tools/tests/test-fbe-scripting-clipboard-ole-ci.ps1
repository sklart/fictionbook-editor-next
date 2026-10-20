[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference='Stop'
# This launcher creates a disposable batch logon with its own noninteractive
# window station (and therefore its own clipboard), never the runner desktop.
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'Real Clipboard integration is restricted to disposable GitHub-hosted Windows runners'
}
$source=(Resolve-Path -LiteralPath $FbeExe).Path
$id=[guid]::NewGuid().ToString('N')
$userName='fbeole'+$id.Substring(0,10)
$taskName='FBE-OLE-'+$id
$root=Join-Path $env:PUBLIC ('fo-'+$id.Substring(0,10))
$userCreated=$false;$taskCreated=$false;$rootCreated=$false
try {
    New-Item -ItemType Directory -Path $root | Out-Null
    $rootCreated=$true
    $password='Fbe!'+[Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))+'9aZ!'
    $user=New-LocalUser -Name $userName -Password (ConvertTo-SecureString $password -AsPlainText -Force) -AccountNeverExpires -PasswordNeverExpires
    $userCreated=$true
    # New-LocalUser does not add the account to Users. That membership is
    # required to execute the installed PowerShell/runtime under Program Files.
    Add-LocalGroupMember -SID 'S-1-5-32-545' -Member $userName
    $acl=Get-Acl -LiteralPath $root
    $acl.SetAccessRuleProtection($true,$false)
    foreach($sid in @($user.SID,[Security.Principal.SecurityIdentifier]::new('S-1-5-18'),[Security.Principal.SecurityIdentifier]::new('S-1-5-32-544'))) {
        $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,'FullControl','ContainerInherit,ObjectInherit','None','Allow'))
    }
    Set-Acl -LiteralPath $root -AclObject $acl
    $runtime=New-Item -ItemType Directory -Path (Join-Path $root 'runtime')
    Get-ChildItem -LiteralPath (Split-Path -Parent $source) -Force | Copy-Item -Destination $runtime.FullName -Recurse -Force
    foreach($file in @('test-fbe-scripting-stage3c.ps1','RuntimeTestIsolation.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $root
    }
    $worker=@'
$ErrorActionPreference='Stop'
try {
    $env:GITHUB_ACTIONS='true';$env:RUNNER_ENVIRONMENT='github-hosted'
    $env:TEMP=$PSScriptRoot;$env:TMP=$PSScriptRoot
    $env:FBE_NEXT_CLIPBOARD_CI_ACCOUNT='@@SID@@'
    if ([Security.Principal.WindowsIdentity]::GetCurrent().User.Value -ne $env:FBE_NEXT_CLIPBOARD_CI_ACCOUNT) { throw 'Private CI logon identity mismatch' }
    & (Join-Path $PSScriptRoot 'test-fbe-scripting-stage3c.ps1') -FbeExe (Join-Path $PSScriptRoot 'runtime/FBE.exe') -Part clipboard-ole -Mode portable *>&1 |
        Out-File (Join-Path $PSScriptRoot 'result.txt') -Encoding utf8
    'PASS' | Set-Content (Join-Path $PSScriptRoot 'finished.txt')
} catch {
    'FAIL: '+$_.Exception.Message | Add-Content (Join-Path $PSScriptRoot 'result.txt')
    'FAIL' | Set-Content (Join-Path $PSScriptRoot 'finished.txt')
    exit 1
}
'@
    $worker.Replace('@@SID@@',$user.SID.Value) | Set-Content (Join-Path $root 'worker.ps1') -Encoding utf8
    $action=New-ScheduledTaskAction -Execute (Join-Path $PSHOME 'pwsh.exe') -Argument ('-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+(Join-Path $root 'worker.ps1')+'"') -WorkingDirectory $root
    $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 2) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
    # Password logon is noninteractive; InteractiveToken would share WinSta0.
    Register-ScheduledTask -TaskName $taskName -Action $action -Settings $settings -User "$env:COMPUTERNAME\$userName" -Password $password -RunLevel Limited | Out-Null
    $taskCreated=$true;$password=$null
    Start-ScheduledTask -TaskName $taskName
    $deadline=[DateTime]::UtcNow.AddSeconds(110)
    while (!(Test-Path -LiteralPath (Join-Path $root 'finished.txt')) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 500 }
    $result=Join-Path $root 'result.txt'
    if(Test-Path -LiteralPath $result){Get-Content -LiteralPath $result | Write-Host}
    if (!(Test-Path -LiteralPath (Join-Path $root 'finished.txt')) -or
        (Get-Content -LiteralPath (Join-Path $root 'finished.txt') -Raw).Trim() -ne 'PASS') {
        $info=Get-ScheduledTaskInfo -TaskName $taskName
        throw ('Private-user OLE Clipboard roundtrip failed or timed out; task result=0x{0:X8}; state={1}' -f $info.LastTaskResult,(Get-ScheduledTask -TaskName $taskName).State)
    }
    Write-Host 'Real OLE Clipboard: PASS (disposable account, separate batch logon and window station)'
} finally {
    if($taskCreated){Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue;Unregister-ScheduledTask -TaskName $taskName -Confirm:$false}
    if($userCreated){Remove-LocalUser -Name $userName}
    # Only this exact, newly created directory can be removed.
    if ((Split-Path -Parent ([IO.Path]::GetFullPath($root))) -ne [IO.Path]::GetFullPath($env:PUBLIC)) {throw 'Unexpected CI staging path'}
    if($rootCreated -and (Test-Path -LiteralPath $root)){Remove-Item -LiteralPath $root -Recurse -Force}
}
