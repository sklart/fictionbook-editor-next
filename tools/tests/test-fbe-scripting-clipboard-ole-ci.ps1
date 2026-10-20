[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),[switch]$ProbeOnly)
$ErrorActionPreference='Stop'
# This launcher creates a disposable batch logon with its own noninteractive
# window station (and therefore its own clipboard), never the runner desktop.
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'Real Clipboard integration is restricted to disposable GitHub-hosted Windows runners'
}
$source=if(!$ProbeOnly){(Resolve-Path -LiteralPath $FbeExe).Path}else{$null}
$id=[guid]::NewGuid().ToString('N')
$userName='fbeole'+$id.Substring(0,10)
$taskName='FBE-OLE-'+$id
$root=Join-Path $env:PUBLIC ('fo-'+$id.Substring(0,10))
$userCreated=$false;$taskCreated=$false;$rootCreated=$false;$batchGranted=$false
# The PowerShell Task Scheduler registration API does not guarantee the batch
# logon right assigned by the interactive Task Scheduler wizard.
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class FbeClipboardBatchLogon {
    [StructLayout(LayoutKind.Sequential)] struct Attributes {
        public uint Length; public IntPtr RootDirectory, ObjectName;
        public uint Flags; public IntPtr SecurityDescriptor, SecurityQualityOfService;
    }
    [StructLayout(LayoutKind.Sequential)] struct UnicodeString {
        public ushort Length, MaximumLength; public IntPtr Buffer;
    }
    [DllImport("advapi32.dll")] static extern uint LsaOpenPolicy(IntPtr system, ref Attributes attributes, uint access, out IntPtr policy);
    [DllImport("advapi32.dll")] static extern uint LsaAddAccountRights(IntPtr policy, byte[] sid, ref UnicodeString rights, uint count);
    [DllImport("advapi32.dll")] static extern uint LsaRemoveAccountRights(IntPtr policy, byte[] sid, [MarshalAs(UnmanagedType.U1)] bool all, IntPtr rights, uint count);
    [DllImport("advapi32.dll")] static extern uint LsaClose(IntPtr policy);
    [DllImport("advapi32.dll")] static extern uint LsaNtStatusToWinError(uint status);
    static void Check(uint status) { if(status != 0) throw new Win32Exception((int)LsaNtStatusToWinError(status)); }
    public static void Set(byte[] sid, bool grant) {
        var attributes = new Attributes(); attributes.Length = (uint)Marshal.SizeOf(typeof(Attributes));
        IntPtr policy; Check(LsaOpenPolicy(IntPtr.Zero, ref attributes, 0x810, out policy));
        try {
            if(!grant) { Check(LsaRemoveAccountRights(policy, sid, true, IntPtr.Zero, 0)); return; }
            const string name = "SeBatchLogonRight";
            var right = new UnicodeString { Length = (ushort)(name.Length * 2), MaximumLength = (ushort)((name.Length + 1) * 2), Buffer = Marshal.StringToHGlobalUni(name) };
            try { Check(LsaAddAccountRights(policy, sid, ref right, 1)); }
            finally { Marshal.FreeHGlobal(right.Buffer); }
        } finally { LsaClose(policy); }
    }
}
'@
try {
    New-Item -ItemType Directory -Path $root | Out-Null
    $rootCreated=$true
    $password='Fbe!'+[Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))+'9aZ!'
    $user=New-LocalUser -Name $userName -Password (ConvertTo-SecureString $password -AsPlainText -Force) -AccountNeverExpires -PasswordNeverExpires
    $userCreated=$true
    # New-LocalUser does not add the account to Users. That membership is
    # required to execute the installed PowerShell/runtime under Program Files.
    Add-LocalGroupMember -SID 'S-1-5-32-545' -Member $userName
    $sidBytes=[byte[]]::new($user.SID.BinaryLength);$user.SID.GetBinaryForm($sidBytes,0)
    [FbeClipboardBatchLogon]::Set($sidBytes,$true);$batchGranted=$true
    $acl=Get-Acl -LiteralPath $root
    $acl.SetAccessRuleProtection($true,$false)
    foreach($sid in @($user.SID,[Security.Principal.SecurityIdentifier]::new('S-1-5-18'),[Security.Principal.SecurityIdentifier]::new('S-1-5-32-544'))) {
        $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,'FullControl','ContainerInherit,ObjectInherit','None','Allow'))
    }
    Set-Acl -LiteralPath $root -AclObject $acl
    if(!$ProbeOnly) {
        $runtime=New-Item -ItemType Directory -Path (Join-Path $root 'runtime')
        Get-ChildItem -LiteralPath (Split-Path -Parent $source) -Force | Copy-Item -Destination $runtime.FullName -Recurse -Force
    }
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
    if (@@PROBE@@) { 'Private batch logon: PASS (no FBE/Clipboard access)' | Set-Content (Join-Path $PSScriptRoot 'result.txt') }
    else {
        & (Join-Path $PSScriptRoot 'test-fbe-scripting-stage3c.ps1') -FbeExe (Join-Path $PSScriptRoot 'runtime/FBE.exe') -Part clipboard-ole -Mode portable *>&1 |
            Out-File (Join-Path $PSScriptRoot 'result.txt') -Encoding utf8
    }
    'PASS' | Set-Content (Join-Path $PSScriptRoot 'finished.txt')
} catch {
    'FAIL: '+$_.Exception.Message | Add-Content (Join-Path $PSScriptRoot 'result.txt')
    'FAIL' | Set-Content (Join-Path $PSScriptRoot 'finished.txt')
    exit 1
}
'@
    $probeLiteral=if($ProbeOnly){'$true'}else{'$false'}
    $worker.Replace('@@SID@@',$user.SID.Value).Replace('@@PROBE@@',$probeLiteral) | Set-Content (Join-Path $root 'worker.ps1') -Encoding utf8
    $action=New-ScheduledTaskAction -Execute (Join-Path $PSHOME 'pwsh.exe') -Argument ('-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+(Join-Path $root 'worker.ps1')+'"') -WorkingDirectory $root
    $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 2) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
    # Password logon is noninteractive; InteractiveToken would share WinSta0.
    $trigger=New-ScheduledTaskTrigger -Once -At ([DateTime]::Now.AddSeconds(5))
    Register-ScheduledTask -TaskName $taskName -Action $action -Settings $settings -Trigger $trigger -User "$env:COMPUTERNAME\$userName" -Password $password -RunLevel Limited | Out-Null
    $taskCreated=$true;$password=$null
    $deadline=[DateTime]::UtcNow.AddSeconds(110)
    while (!(Test-Path -LiteralPath (Join-Path $root 'finished.txt')) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 500 }
    $result=Join-Path $root 'result.txt'
    if(Test-Path -LiteralPath $result){Get-Content -LiteralPath $result | Write-Host}
    if (!(Test-Path -LiteralPath (Join-Path $root 'finished.txt')) -or
        (Get-Content -LiteralPath (Join-Path $root 'finished.txt') -Raw).Trim() -ne 'PASS') {
        $info=Get-ScheduledTaskInfo -TaskName $taskName
        Get-WinEvent -FilterHashtable @{LogName='Microsoft-Windows-TaskScheduler/Operational';StartTime=[DateTime]::Now.AddMinutes(-3)} -ErrorAction SilentlyContinue |
            Where-Object { $_.Message -like "*$taskName*" } | ForEach-Object {Write-Host $_.Message}
        throw ('Private-user OLE Clipboard roundtrip failed or timed out; task result=0x{0:X8}; state={1}' -f $info.LastTaskResult,(Get-ScheduledTask -TaskName $taskName).State)
    }
    if(!$ProbeOnly){Write-Host 'Real OLE Clipboard: PASS (disposable account, separate batch logon and window station)'}
} finally {
    if($taskCreated){Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue;Unregister-ScheduledTask -TaskName $taskName -Confirm:$false}
    if($batchGranted){try{[FbeClipboardBatchLogon]::Set($sidBytes,$false)}catch{Write-Warning "Disposable account rights cleanup failed: $($_.Exception.Message)"}}
    if($userCreated){Remove-LocalUser -Name $userName}
    # Only this exact, newly created directory can be removed.
    if ((Split-Path -Parent ([IO.Path]::GetFullPath($root))) -ne [IO.Path]::GetFullPath($env:PUBLIC)) {throw 'Unexpected CI staging path'}
    if($rootCreated -and (Test-Path -LiteralPath $root)){Remove-Item -LiteralPath $root -Recurse -Force}
}
