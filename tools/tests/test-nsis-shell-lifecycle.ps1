<# Real file operations and registry snapshots; all registrations stay in isolated HKCU. #>
[CmdletBinding()]param()
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$work=Join-Path $repo ('build\tests\shell-lifecycle-'+[Guid]::NewGuid().ToString('N'))
$null=[IO.Directory]::CreateDirectory($work)
. (Join-Path $repo 'tools\build\shell-integration-lifecycle.ps1')
$realShellChild=(Get-Command Invoke-ShellChild).ScriptBlock
$realSaveData=(Get-Command Save-ShellData).ScriptBlock
$hive='Software\FBENextInstallerTests\'+[Guid]::NewGuid().ToString('N')
$null=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($hive)
function Assert([bool]$Value,[string]$Message){if(-not $Value){throw $Message}}
function Open-ShellRegistry{return [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($hive,$true)}
function Protect-ShellDirectory($Context){$null=[IO.Directory]::CreateDirectory((Join-Path $Context.Directory 'State'))}
$script:schemas=@();$script:registerCount=0;$script:removeCount=0;$script:failure='';$script:held=$null
function Get-ShellSchemaPaths{return ,@($script:schemas)}
function Register-ShellSchema([string]$Path){$script:schemas=@($script:schemas | Where-Object {$_ -ine $Path})+@($Path)}
function Unregister-ShellSchema([string]$Path){$script:schemas=@($script:schemas | Where-Object {$_ -ine $Path})}
function Write-FixtureRegistration([string]$Path){
    $root=Open-ShellRegistry
    try {
        foreach($id in $script:ShellClassIds){$key=$root.CreateSubKey('SOFTWARE\Classes\CLSID\'+$id+'\InprocServer32');$key.SetValue('',$Path);$key.SetValue('ThreadingModel','Both');$key.SetValue('FixtureDword',7,[Microsoft.Win32.RegistryValueKind]::DWord);$key.SetValue('FixtureBinary',[byte[]]@(0,17,255),[Microsoft.Win32.RegistryValueKind]::Binary);$key.SetValue('FixtureMulti',[string[]]@('one','two'),[Microsoft.Win32.RegistryValueKind]::MultiString);$key.Close()}
        for($i=2;$i -lt 5;++$i){$id=$script:ShellClassIds[1];if($i -eq 2){$id=$script:ShellClassIds[0]};$key=$root.CreateSubKey($script:ShellRegistryPaths[$i]);$key.SetValue('',$id);$key.Close()}
        $key=$root.CreateSubKey('SOFTWARE\Classes\FictionBook.2');$key.SetValue('InfoTip','previous metadata');$key.Close()
    }finally{$root.Close()}
}
function Save-ShellData([string]$Path,$Data){
    if($script:failure -eq 'remove-receipt' -and $Data.Cleanup -eq $true -and $Path.EndsWith('InstalledState.clixml')){$script:failure='';throw 'Injected removal receipt failure'}
    & $realSaveData $Path $Data
}
function Invoke-ShellChild($Context,[string]$Name,[object[]]$Arguments,[string]$Section){
    if($Name -eq 'update-shell-files.ps1'){
        $status=Join-Path $Context.ToolsDirectory 'payload.ini'
        $parameters=@{Directory=$Context.Directory;ExpectedX86Sha256=$Context.Hashes[0];ExpectedX64Sha256=$Context.Hashes[1];ExpectedSchemaSha256=$Context.Hashes[2];StatusFilePath=$status}
        & (Join-Path $repo 'tools\build\update-shell-files.ps1') @parameters
        $text=[IO.File]::ReadAllText($status)
        if($text -notmatch 'Result=ok'){throw ('Result=pending; '+$text)}
    }elseif($Name -eq 'register-sequence-property-schema.ps1'){
        if($script:failure -eq 'schema'){throw 'Injected schema failure'}
        Register-ShellSchema (Join-Path $Context.Directory 'FBE.Sequence.propdesc')
    }elseif($Name -eq 'register-modern-property-handler.ps1'){
        ++$script:registerCount
        $root=Open-ShellRegistry
        $key=$root.CreateSubKey('SOFTWARE\Classes\CLSID\'+$script:ShellClassIds[0]+'\InprocServer32');$key.SetValue('',(Join-Path $Context.Directory $Context.NativeName));$key.SetValue('ThreadingModel','Apartment');$key.Close();$root.Close()
        if($script:failure -eq 'rollback-lock'){$script:held=[IO.File]::Open((Join-Path $Context.Directory $Context.NativeName),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);throw 'Injected registration and recovery failure'}
        if($script:failure -eq 'com'){throw 'Injected partial COM failure'}
        Write-FixtureRegistration (Join-Path $Context.Directory $Context.NativeName)
        if($script:failure -eq 'backup-lock'){$script:held=[IO.File]::OpenRead((Join-Path $Context.Directory 'State\old-FBShell64.dll'))}
    }elseif($Name -eq 'unregister-modern-property-handler.ps1'){
        ++$script:removeCount
        # Require the actual old hashes passed to the real unregister helper.
        $dllIndex=[Array]::IndexOf($Arguments,'-ExpectedDllSha256')+1
        $receipt=Get-ShellReceipt $Context
        Assert ($Arguments[$dllIndex] -eq $receipt.Files[$Context.NativeName]) 'Unregister used the new embedded digest.'
        $root=Open-ShellRegistry
        try {foreach($path in $script:ShellRegistryPaths){$root.DeleteSubKeyTree($path)}}finally{$root.Close()}
        $script:schemas=@()
    }else{throw "Unexpected child $Name"}
}
function New-Case([string]$Name,[switch]$Legacy){
    $root=Open-ShellRegistry
    try {foreach($path in $root.GetSubKeyNames()){$root.DeleteSubKeyTree($path)}}finally{$root.Close()}
    $script:schemas=@();$script:failure='';$script:registerCount=0;$script:removeCount=0
    $base=Join-Path $work $Name;$shared=Join-Path $base 'Shared';$old=$shared
    if($Legacy){$old=Join-Path $base 'Legacy'}
    foreach($dir in @($shared,$old,(Join-Path $base 'New'),(Join-Path $base 'Tools'))){$null=[IO.Directory]::CreateDirectory($dir)}
    $hashes=@();foreach($name in $script:ShellPayloadNames){[IO.File]::WriteAllText((Join-Path $old $name),'old '+$name);[IO.File]::WriteAllText((Join-Path $base ('New\'+$name)),'new '+$name);$hashes+=Get-ShellHash (Join-Path $base ('New\'+$name))}
    $ctx=@{Directory=$shared;InstallationDirectory=(Join-Path $base 'Application');PayloadDirectory=(Join-Path $base 'New');ToolsDirectory=(Join-Path $base 'Tools');Hashes=$hashes;Platform='x64';NativeName='FBShell64.dll';NativeIndex=1;LegacyDirectories=@((Join-Path $base 'Legacy'));PreviousProperty=1;PreviousSystem=1;PreviousInstallLocation=(Join-Path $base 'Application')}
    if(-not $Legacy){$oldctx=$ctx.Clone();$oldctx.Hashes=@($script:ShellPayloadNames | ForEach-Object {Get-ShellHash (Join-Path $old $_)});$null=[IO.Directory]::CreateDirectory((Join-Path $shared 'State'));Write-ShellOwner $oldctx}
    Write-FixtureRegistration (Join-Path $old $ctx.NativeName)
    Register-ShellSchema (Join-Path $old 'FBE.Sequence.propdesc')
    return $ctx
}
function Assert-Old($Context,[string]$Directory=''){
    if(-not $Directory){$Directory=$Context.Directory}
    foreach($name in $script:ShellPayloadNames){Assert ([IO.File]::ReadAllText((Join-Path $Directory $name)) -eq ('old '+$name)) "Old file not restored: $name"}
    $state=Get-ShellRegistration @((Join-Path $Directory $Context.NativeName))
    Assert ($state.Complete -and -not $state.Foreign) 'Old COM registration not restored.'
    Assert ($script:schemas.Count -eq 1 -and $script:schemas[0] -eq (Join-Path $Directory 'FBE.Sequence.propdesc')) 'Old schema not restored.'
    Assert ((Get-ShellRegistryValue ('SOFTWARE\Classes\CLSID\'+$script:ShellClassIds[0]+'\InprocServer32') 'ThreadingModel') -eq 'Both') 'COM values not restored exactly.'
    Assert ((Get-ShellRegistryValue ('SOFTWARE\Classes\CLSID\'+$script:ShellClassIds[0]+'\InprocServer32') 'FixtureDword') -eq 7) 'DWORD not restored.'
    Assert (((Get-ShellRegistryValue ('SOFTWARE\Classes\CLSID\'+$script:ShellClassIds[0]+'\InprocServer32') 'FixtureBinary') -join ',') -eq '0,17,255') 'Binary data not restored.'
    Assert (((Get-ShellRegistryValue ('SOFTWARE\Classes\CLSID\'+$script:ShellClassIds[0]+'\InprocServer32') 'FixtureMulti') -join ',') -eq 'one,two') 'Multi-string not restored.'
    Assert ((Get-ShellRegistryValue 'SOFTWARE\Classes\FictionBook.2' 'InfoTip') -eq 'previous metadata') 'Metadata not restored.'
}
try {
    $ctx=New-Case 'success';$result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.Result -eq 'ok' -and $result.EffectiveInstalled -eq 1) ('Update: '+($result | Out-String))
    $owner=Read-ShellOwner $ctx.Directory;Assert ($owner['X64Sha256'] -eq $ctx.Hashes[1]) 'Owner does not match new payload.'
    $result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.Code -eq 'ALREADY_CURRENT' -and $script:registerCount -eq 1) 'Repeated install registered twice.'
    $result=Invoke-ShellLifecycle $ctx 'Remove';Assert ($result.Result -eq 'ok' -and $result.SystemIntegrationInstalled -eq 0) ('Deselected integration not removed: '+($result | Out-String))

    foreach($injectedFailure in @('com','schema')){
        $ctx=New-Case ('failure-'+$injectedFailure);$oldOwner=[IO.File]::ReadAllBytes((Join-Path $ctx.Directory 'Owner.ini'));$script:failure=$injectedFailure
        $result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.Result -eq 'error' -and $result.EffectiveInstalled -eq 1 -and $result.RecoveryPending -eq 0) ('Rollback: '+($result | Out-String));Assert-Old $ctx
        Assert ([Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $ctx.Directory 'Owner.ini'))) -eq [Convert]::ToBase64String($oldOwner)) 'Owner bytes changed after rollback.'
        $script:failure='';$result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.Result -eq 'ok') 'Retry after rollback failed.'
    }
    $ctx=New-Case 'backup-cleanup';$script:failure='backup-lock';$result=Invoke-ShellLifecycle $ctx 'Update'
    Assert ($result.Code -eq 'BACKUP_CLEANUP_PENDING' -and $result.EffectiveInstalled -eq 1) 'Locked backup rolled back a confirmed registration.'
    $result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.Result -eq 'pending' -and $result.RecoveryPending -eq 0 -and $script:registerCount -eq 1) 'Backup cleanup retry registered again or reported incomplete COM.'
    $script:held.Close();$script:held=$null;$script:failure='';$result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.Code -eq 'ALREADY_CURRENT') 'Backup cleanup did not finish on retry.'
    $ctx=New-Case 'locked-update';$locked=[IO.File]::Open((Join-Path $ctx.Directory $ctx.NativeName),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {$result=Invoke-ShellLifecycle $ctx 'Update'}finally{$locked.Close()}
    Assert ($result.Result -eq 'pending' -and $result.EffectiveInstalled -eq 1 -and $result.RecoveryPending -eq 0) ('Locked update: '+($result | Out-String));Assert-Old $ctx
    Assert ($script:registerCount -eq 0) 'Registration ran despite file lock.'
    $result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.Result -eq 'ok') 'Retry after unlocking failed.'

    $ctx=New-Case 'remove-rollback';$script:failure='remove-receipt';$result=Invoke-ShellLifecycle $ctx 'Remove'
    Assert ($result.Result -eq 'error' -and $result.EffectiveInstalled -eq 1 -and $result.RecoveryPending -eq 0) ('Removal rollback lost markers: '+($result | Out-String));Assert-Old $ctx
    $result=Invoke-ShellLifecycle $ctx 'Remove';Assert ($result.Result -eq 'ok') 'Removal retry after rollback failed.'
    $ctx=New-Case 'old-uninstall';$result=Invoke-ShellLifecycle $ctx 'Remove';Assert ($result.Result -eq 'ok') ('New uninstaller failed on old payload: '+($result | Out-String))
    $ctx=New-Case 'locked-remove';$locked=[IO.File]::Open((Join-Path $ctx.Directory $ctx.NativeName),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {$result=Invoke-ShellLifecycle $ctx 'Remove'}finally{$locked.Close()}
    Assert ($result.Result -eq 'pending' -and $result.CleanupPending -eq 1 -and $result.EffectiveInstalled -eq 0) ('Locked removal: '+($result | Out-String))
    Assert ([IO.File]::Exists((Join-Path $ctx.Directory 'Owner.ini')) -and [IO.File]::Exists((Join-Path $ctx.Directory 'InstalledState.clixml'))) 'Pending removal lost ownership.'
    $result=Invoke-ShellLifecycle $ctx 'Remove';Assert ($result.Result -eq 'ok' -and $script:removeCount -eq 1) ('Cleanup retry: '+($result | Out-String))

    $ctx=New-Case 'locked-helper';$null=[IO.Directory]::CreateDirectory((Join-Path $ctx.Directory 'InstallerTools'));$helper=Join-Path $ctx.Directory 'InstallerTools\update-shell-files.ps1';[IO.File]::WriteAllText($helper,'old helper');$locked=[IO.File]::OpenRead($helper)
    try {$result=Invoke-ShellLifecycle $ctx 'Remove'}finally{$locked.Close()}
    Assert ($result.CleanupPending -eq 1 -and [IO.File]::Exists((Join-Path $ctx.Directory 'Owner.ini'))) 'Helper lock lost cleanup receipt.'
    $result=Invoke-ShellLifecycle $ctx 'Remove';Assert ($result.Result -eq 'ok') 'Helper cleanup retry failed.'

    foreach($foreign in @('owner','com')){
        $ctx=New-Case ('foreign-'+$foreign)
        if($foreign -eq 'owner'){$other=$ctx.Clone();$other.InstallationDirectory='D:\AnotherFBE';$other.Hashes=@($script:ShellPayloadNames | ForEach-Object {Get-ShellHash (Join-Path $ctx.Directory $_)});Write-ShellOwner $other}else{Write-FixtureRegistration 'D:\AnotherFBE\FBShell64.dll'}
        $before=Get-ShellHash (Join-Path $ctx.Directory 'Owner.ini')
        foreach($op in @('Update','Remove')){$result=Invoke-ShellLifecycle $ctx $op;Assert ($result.Result -eq 'skipped') "Foreign $foreign changed by $op"}
        Assert ((Get-ShellHash (Join-Path $ctx.Directory 'Owner.ini')) -eq $before) 'Foreign owner was modified.'
    }
    $ctx=New-Case 'unowned-files';[IO.File]::Delete((Join-Path $ctx.Directory 'Owner.ini'));$root=Open-ShellRegistry;foreach($path in $script:ShellRegistryPaths){$root.DeleteSubKeyTree($path)};$root.Close();$script:schemas=@()
    $result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.Result -eq 'skipped') 'Unowned existing files overwritten.'
    $ctx=New-Case 'fresh';$root=Open-ShellRegistry;foreach($path in $script:ShellRegistryPaths){$root.DeleteSubKeyTree($path)};$root.Close();$script:schemas=@();foreach($name in @($script:ShellPayloadNames)+@('Owner.ini')){[IO.File]::Delete((Join-Path $ctx.Directory $name))};$ctx.PreviousProperty=0;$ctx.PreviousSystem=0
    $result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.Result -eq 'ok' -and $result.EffectiveInstalled -eq 1) ('Fresh install failed: '+($result | Out-String))
    $ctx=New-Case 'recovery';$script:failure='rollback-lock'
    $result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.RecoveryPending -eq 1) ('Recovery failure not recorded: '+($result | Out-String))
    $result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.Code -eq 'RECOVERY_REQUIRED' -and $script:registerCount -eq 1) 'Interrupted operation repeated registration.'
    $script:held.Close();$script:held=$null;$script:failure=''
    $backup=Join-Path $ctx.Directory 'State\old-FBShell64.dll';$backupBytes=[IO.File]::ReadAllBytes($backup);[IO.File]::WriteAllText($backup,'tampered snapshot')
    $result=Invoke-ShellLifecycle $ctx 'Recover';Assert ($result.Result -eq 'error' -and $result.RecoveryPending -eq 1) 'Changed backup accepted or unfinished recovery hidden.'
    [IO.File]::WriteAllBytes($backup,$backupBytes)
    $result=Invoke-ShellLifecycle $ctx 'Recover';Assert ($result.Result -eq 'ok') ('Explicit recovery failed: '+($result | Out-String));Assert-Old $ctx

    $ctx=New-Case 'migration' -Legacy;$legacy=$ctx.LegacyDirectories[0];$result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.Result -eq 'ok') ('Migration failed: '+($result | Out-String));Assert ([IO.File]::Exists((Join-Path $legacy $ctx.NativeName))) 'Migration loaded/deleted old legacy DLL.'
    $ctx=New-Case 'migration-failure' -Legacy;$legacy=$ctx.LegacyDirectories[0];$script:failure='com';$result=Invoke-ShellLifecycle $ctx 'Update';Assert ($result.RecoveryPending -eq 0 -and $result.EffectiveInstalled -eq 1) 'Migration rollback failed.';Assert-Old $ctx $legacy
    $ctx=New-Case 'native-payload-helper'
    Copy-Item -LiteralPath (Join-Path $repo 'tools\build\update-shell-files.ps1') -Destination $ctx.ToolsDirectory
    foreach($name in $script:ShellPayloadNames){[IO.File]::Copy((Join-Path $ctx.PayloadDirectory $name),(Join-Path $ctx.Directory ($name+'.new')))}
    & $realShellChild $ctx 'update-shell-files.ps1' @('-Directory',$ctx.Directory,'-ExpectedX86Sha256',$ctx.Hashes[0],'-ExpectedX64Sha256',$ctx.Hashes[1],'-ExpectedSchemaSha256',$ctx.Hashes[2]) 'Payload'
    Assert ((Get-ShellHash (Join-Path $ctx.Directory $ctx.NativeName)) -eq $ctx.Hashes[1]) 'Native child helper arguments/status failed.'
    Write-Host "Shell lifecycle checks passed. Actual file locks and isolated HKCU snapshots; COM/schema calls simulated. Artifacts: $work"
}finally{if($null -ne $script:held){$script:held.Close()};[Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($hive)}
