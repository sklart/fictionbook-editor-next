[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),[switch]$ProbeOnly,[switch]$FullNativeMatrix)
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
    if(!$ProbeOnly) {
        # Keep the native driver's expected repository-relative evidence path
        # entirely inside the new private staging directory.
        $privateTests=Join-Path $root 'tools/tests'
        New-Item -ItemType Directory -Force -Path $privateTests | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $root 'build') | Out-Null
        if($FullNativeMatrix) { New-Item -ItemType File -Path (Join-Path $root 'full-native-matrix') | Out-Null }
        foreach($file in @('test-fbe-native-copy-paste-runtime.ps1','test-fbe-html-paste-runtime.ps1','RuntimeTestIsolation.ps1')) {
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $privateTests
        }
    }
    $worker=@'
$ErrorActionPreference='Stop'
try {
    $env:GITHUB_ACTIONS='true';$env:RUNNER_ENVIRONMENT='github-hosted'
    $env:TEMP=$PSScriptRoot;$env:TMP=$PSScriptRoot
    $env:FBE_RUNTIME_FAILURE_ARTIFACTS=Join-Path $PSScriptRoot 'evidence'
    $env:FBE_NEXT_CLIPBOARD_CI_ACCOUNT='@@SID@@'
    if ([Security.Principal.WindowsIdentity]::GetCurrent().User.Value -ne $env:FBE_NEXT_CLIPBOARD_CI_ACCOUNT) { throw 'Private CI logon identity mismatch' }
    if (@@PROBE@@) { 'Private batch logon: PASS (no FBE/Clipboard access)' | Set-Content (Join-Path $PSScriptRoot 'result.txt') }
    else {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class FbePrivateClipboardStation {
    [DllImport("user32.dll")] static extern IntPtr GetProcessWindowStation();
    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true, ExactSpelling=true)]
    static extern bool GetUserObjectInformationW(IntPtr handle, int index, StringBuilder value, int bytes, out int needed);
    public static string Name() {
        var text=new StringBuilder(256); int needed;
        var station=GetProcessWindowStation();
        return station != IntPtr.Zero && GetUserObjectInformationW(station,2,text,text.Capacity*2,out needed) ? text.ToString() : null;
    }
}
"@
        $station=[FbePrivateClipboardStation]::Name()
        if([string]::IsNullOrWhiteSpace($station) -or $station -ieq 'WinSta0'){throw 'Native Clipboard tests require a noninteractive private window station.'}
        "Private window station: $station" | Set-Content (Join-Path $PSScriptRoot 'result.txt')
        $evidence=Join-Path $PSScriptRoot 'evidence';New-Item -ItemType Directory -Force -Path $evidence | Out-Null
        $fullMatrix=Test-Path -LiteralPath (Join-Path $PSScriptRoot 'full-native-matrix')
        $cases=if($fullMatrix){@('char','word','paragraph','within','between','section','formatted','repeat','cross-document')}else{@('word','formatted')}
        $stageStatus='FAIL';$nativeStatus='FAIL';$stageError='';$nativeError=''
        try {
            & (Join-Path $PSScriptRoot 'test-fbe-scripting-stage3c.ps1') -FbeExe (Join-Path $PSScriptRoot 'runtime/FBE.exe') -Part clipboard-ole -Mode portable *>&1 |
                Out-File (Join-Path $evidence 'ole-stage3c.log') -Encoding utf8
            $stageStatus='PASS'
        } catch { $stageError=$_.Exception.Message }
        try {
            & (Join-Path $PSScriptRoot 'tools/tests/test-fbe-native-copy-paste-runtime.ps1') -FbeExe (Join-Path $PSScriptRoot 'runtime/FBE.exe') -Cases $cases *>&1 |
                Out-File (Join-Path $evidence 'native-copy-paste.log') -Encoding utf8
            if($fullMatrix) {
                & (Join-Path $PSScriptRoot 'tools/tests/test-fbe-html-paste-runtime.ps1') -FbeExe (Join-Path $PSScriptRoot 'runtime/FBE.exe') -Cases @('queue-reentrant','queue-reentrant-post-failure') *>&1 |
                    Out-File (Join-Path $evidence 'reentrant-paste.log') -Encoding utf8
            }
            $nativeStatus='PASS'
        } catch { $nativeError=$_.Exception.Message }
        $nativeEvidence=Join-Path $evidence 'native-copy-paste'
        New-Item -ItemType Directory -Force -Path $nativeEvidence | Out-Null
        $nativeDirectories=@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'build') -Directory -Filter 'community-native-copy-*' -ErrorAction SilentlyContinue)
        if($nativeStatus -eq 'PASS' -and $nativeDirectories.Count -ne 1){$nativeStatus='FAIL';$nativeError='Native evidence directory missing or ambiguous.'}
        foreach($directory in $nativeDirectories) {
            Copy-Item -LiteralPath $directory.FullName -Destination $nativeEvidence -Recurse -Force -ErrorAction Stop
            if($nativeStatus -eq 'PASS') {
                $reports=@($cases | Where-Object { $_ -ne 'cross-document' })
                if($fullMatrix){$reports+=@('cross-source','cross-target')}
                foreach($case in $reports) {
                    $original=Join-Path $directory.FullName ($case+'.txt')
                    $copy=Join-Path (Join-Path $nativeEvidence $directory.Name) ($case+'.txt')
                    if(-not (Test-Path -LiteralPath $original -PathType Leaf) -or -not (Test-Path -LiteralPath $copy -PathType Leaf) -or
                        (Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash -cne (Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash) {
                        $nativeStatus='FAIL';$nativeError="Native $case report missing or not preserved.";break
                    }
                }
            }
        }
        @{stage3c=$stageStatus;nativeCopyPaste=$nativeStatus;stageError=$stageError;nativeError=$nativeError} |
            ConvertTo-Json | Set-Content (Join-Path $evidence 'clipboard-summary.json') -Encoding utf8
        "Stage3C=$stageStatus; native Copy/Paste=$nativeStatus" | Add-Content (Join-Path $PSScriptRoot 'result.txt')
        if($stageError){'Stage3C: '+$stageError | Add-Content (Join-Path $PSScriptRoot 'result.txt')}
        if($nativeError){'Native Copy/Paste: '+$nativeError | Add-Content (Join-Path $PSScriptRoot 'result.txt')}
        if($stageStatus -ne 'PASS' -or $nativeStatus -ne 'PASS'){throw 'One or more private Clipboard scenarios failed.'}
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
    $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 14) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
    # Password logon is noninteractive; InteractiveToken would share WinSta0.
    $trigger=New-ScheduledTaskTrigger -Once -At ([DateTime]::Now.AddSeconds(5))
    Register-ScheduledTask -TaskName $taskName -Action $action -Settings $settings -Trigger $trigger -User "$env:COMPUTERNAME\$userName" -Password $password -RunLevel Limited | Out-Null
    $taskCreated=$true;$password=$null
    $deadline=[DateTime]::UtcNow.AddSeconds(780)
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
    $cleanupErrors=[Collections.Generic.List[string]]::new()
    if($taskCreated){
        try {
            Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
            $deadline=[DateTime]::UtcNow.AddSeconds(20)
            do {
                $remaining=@(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
                    ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) -or
                    ($_.CommandLine -and $_.CommandLine.IndexOf((Join-Path $root 'worker.ps1'),[StringComparison]::OrdinalIgnoreCase) -ge 0)
                })
                if($remaining.Count){Start-Sleep -Milliseconds 500}
            } while($remaining.Count -and [DateTime]::UtcNow -lt $deadline)
            if($remaining.Count){throw 'Private test processes are still running.'}
            Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction Stop
        } catch {$cleanupErrors.Add('Scheduled task/process cleanup: '+$_.Exception.Message)}
    }
    # Copy bounded test evidence before deleting the private account's staging
    # directory, including when the editor scenario itself failed.
    $evidenceError=$null
    try {
        $evidence=Join-Path $root 'evidence'
        if($rootCreated -and (Test-Path -LiteralPath $evidence -PathType Container)) {
            $archive=Join-Path $PSScriptRoot ('../../out/tests/clipboard-ole/'+$id)
            New-Item -ItemType Directory -Path $archive -Force -ErrorAction Stop | Out-Null
            Copy-Item -LiteralPath $evidence -Destination $archive -Recurse -Force -ErrorAction Stop
            Write-Host "Clipboard test evidence: $archive"
        }
    } catch { $evidenceError=$_ }
    if($batchGranted){try{[FbeClipboardBatchLogon]::Set($sidBytes,$false)}catch{$cleanupErrors.Add('Disposable account rights cleanup: '+$_.Exception.Message)}}
    if($userCreated){try{Remove-LocalUser -Name $userName -ErrorAction Stop}catch{$cleanupErrors.Add('Disposable account cleanup: '+$_.Exception.Message)}}
    # Only this exact, newly created directory can be removed.
    if ((Split-Path -Parent ([IO.Path]::GetFullPath($root))) -ne [IO.Path]::GetFullPath($env:PUBLIC)) {throw 'Unexpected CI staging path'}
    if($rootCreated -and -not $evidenceError -and -not $cleanupErrors.Count -and (Test-Path -LiteralPath $root)){Remove-Item -LiteralPath $root -Recurse -Force}
    if($evidenceError){throw "Clipboard evidence copy failed; staging retained at ${root}: $evidenceError"}
    if($cleanupErrors.Count){throw "Clipboard cleanup failed; staging retained at ${root}: $($cleanupErrors -join '; ')"}
}
