# Shell-specific state and recovery helpers. Kept compatible with Windows PowerShell 2.0.
$script:ShellClassIds=@('{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}','{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}')
$script:ShellRegistryPaths=@(
    ('SOFTWARE\Classes\CLSID\'+$script:ShellClassIds[0]),
    ('SOFTWARE\Classes\CLSID\'+$script:ShellClassIds[1]),
    'SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2',
    'SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}',
    'SOFTWARE\Classes\FictionBook.2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}'
)
$script:ShellPayloadNames=@('FBShell.dll','FBShell64.dll','FBE.Sequence.propdesc')
function Open-ShellRegistry { return [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey('', $true) }
function Get-ShellHash([string]$Path) {
    $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try { $sha=[Security.Cryptography.SHA256]::Create(); try {return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')} finally {$sha.Clear()} } finally {$stream.Close()}
}
function Assert-ShellOrdinaryPath([string]$Path) {
    $item=New-Object IO.DirectoryInfo -ArgumentList ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Path)))
    while($null -ne $item) { if($item.Exists -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Reparse directory rejected: $($item.FullName)"};$item=$item.Parent }
    if(([IO.File]::Exists($Path) -or [IO.Directory]::Exists($Path)) -and ([IO.File]::GetAttributes($Path) -band [IO.FileAttributes]::ReparsePoint)){throw "Reparse path rejected: $Path"}
}
function Read-ShellOwner([string]$Directory) {
    $result=@{};$path=Join-Path $Directory 'Owner.ini';Assert-ShellOrdinaryPath $path
    if([IO.File]::Exists($path)){foreach($line in [IO.File]::ReadAllLines($path)){ $index=$line.IndexOf('=');if($index -gt 0){$result[$line.Substring(0,$index)]=$line.Substring($index+1)} }}
    return $result
}
function Save-ShellData([string]$Path,$Data) {
    Assert-ShellOrdinaryPath $Path
    $next=$Path+'.next';$previous=$Path+'.previous'
    Assert-ShellOrdinaryPath $next;Assert-ShellOrdinaryPath $previous
    $Data | Export-Clixml -LiteralPath $next -Depth 60 -Encoding UTF8
    if([IO.File]::Exists($Path)){[IO.File]::Delete($previous);[IO.File]::Replace($next,$Path,$previous);[IO.File]::Delete($previous)}else{[IO.File]::Move($next,$Path)}
}
function Get-ShellRegistryValue([string]$Path,[string]$Name='') {
    $root=Open-ShellRegistry
    try {$key=$root.OpenSubKey($Path,$false);if($null -eq $key){return $null};try {return $key.GetValue($Name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)}finally{$key.Close()}}finally{$root.Close()}
}
function Get-ShellSchemaPaths {
    $root=Open-ShellRegistry;$result=New-Object Collections.ArrayList
    try {$key=$root.OpenSubKey('SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertySchema',$false);if($null -ne $key){try {foreach($name in $key.GetSubKeyNames()){$child=$key.OpenSubKey($name,$false);try {$path=[string]$child.GetValue('','');$uri=[string]$child.GetValue('URI','');if($uri -ieq 'fbe.sequence.propdesc' -or [IO.Path]::GetFileName($path) -ieq 'FBE.Sequence.propdesc'){ $null=$result.Add($path) }}finally{$child.Close()}}}finally{$key.Close()}}}finally{$root.Close()}
    return ,@($result.ToArray())
}
function Export-ShellRegistryTree($Key) {
    $values=@{};$children=@{}
    foreach($name in $Key.GetValueNames()){$values[$name]=@{Kind=[int]$Key.GetValueKind($name);Data=$Key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)}}
    foreach($name in $Key.GetSubKeyNames()){$child=$Key.OpenSubKey($name,$false);try {$children[$name]=Export-ShellRegistryTree $child}finally{$child.Close()}}
    return @{Values=$values;Children=$children}
}
function Import-ShellRegistryTree($Key,$Tree) {
    foreach($name in $Tree.Values.Keys){
        $value=$Tree.Values[$name];$kind=[Microsoft.Win32.RegistryValueKind][int]$value.Kind;$data=$value.Data
        switch([int]$kind){1 {$data=[string]$data};2 {$data=[string]$data};3 {$data=[byte[]]$data};4 {$data=[int]$data};7 {$data=[string[]]$data};11 {$data=[long]$data};-1 {$data=[byte[]]$data}}
        $Key.SetValue($name,$data,$kind)
    }
    foreach($name in $Tree.Children.Keys){$child=$Key.CreateSubKey($name);try {Import-ShellRegistryTree $child $Tree.Children[$name]}finally{$child.Close()}}
}
function Get-ShellRegistrySnapshot {
    $result=@{};$root=Open-ShellRegistry
    try {foreach($path in $script:ShellRegistryPaths){$key=$root.OpenSubKey($path,$false);if($null -eq $key){$result[$path]=$null}else{try {$result[$path]=Export-ShellRegistryTree $key}finally{$key.Close()}}}}finally{$root.Close()}
    $metadata=@{};$root=Open-ShellRegistry
    try {$key=$root.OpenSubKey('SOFTWARE\Classes\FictionBook.2',$false);if($null -ne $key){try {foreach($name in @('InfoTip','TileInfo','Details','PreviewDetails')){if($key.GetValueNames() -contains $name){$metadata[$name]=@{Kind=[int]$key.GetValueKind($name);Data=$key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)}}}}finally{$key.Close()}}}finally{$root.Close()}
    $result['_Metadata']=$metadata
    return $result
}
function Restore-ShellRegistry($Snapshot) {
    $root=Open-ShellRegistry
    try {
        foreach($path in $script:ShellRegistryPaths){$key=$root.OpenSubKey($path,$false);if($null -ne $key){$key.Close();$root.DeleteSubKeyTree($path)};if($null -ne $Snapshot[$path]){$key=$root.CreateSubKey($path);try {Import-ShellRegistryTree $key $Snapshot[$path]}finally{$key.Close()}}}
        $key=$root.OpenSubKey('SOFTWARE\Classes\FictionBook.2',$true)
        if($null -ne $key){try {foreach($name in @('InfoTip','TileInfo','Details','PreviewDetails')){if($Snapshot['_Metadata'].ContainsKey($name)){$entry=$Snapshot['_Metadata'][$name];$key.SetValue($name,$entry.Data,[Microsoft.Win32.RegistryValueKind][int]$entry.Kind)}else{$key.DeleteValue($name,$false)}}}finally{$key.Close()}}
    }finally{$root.Close()}
}
function Initialize-ShellSchemaApi {
    if(-not ('FbeShellLifecycleSchema' -as [type])){Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;
public static class FbeShellLifecycleSchema {
 [DllImport("propsys.dll", CharSet=CharSet.Unicode)] public static extern int PSRegisterPropertySchema(string path);
 [DllImport("propsys.dll", CharSet=CharSet.Unicode)] public static extern int PSUnregisterPropertySchema(string path);
 [DllImport("propsys.dll")] public static extern int PSRefreshPropertySchema();
}
"@}
}
function Register-ShellSchema([string]$Path) { Initialize-ShellSchemaApi;$hr=[FbeShellLifecycleSchema]::PSRegisterPropertySchema($Path);if($hr -lt 0){throw "Schema register failed: Path=$Path; HRESULT=$hr"};$hr=[FbeShellLifecycleSchema]::PSRefreshPropertySchema();if($hr -lt 0){throw "Schema refresh failed: HRESULT=$hr"} }
function Unregister-ShellSchema([string]$Path) { Initialize-ShellSchemaApi;$hr=[FbeShellLifecycleSchema]::PSUnregisterPropertySchema($Path);if($hr -lt 0){throw "Schema unregister failed: Path=$Path; HRESULT=$hr"};$hr=[FbeShellLifecycleSchema]::PSRefreshPropertySchema();if($hr -lt 0){throw "Schema refresh failed: HRESULT=$hr"} }
function Get-ShellRegistration([string[]]$AllowedPaths) {
    $present=$false;$complete=$true;$native='';$foreign=$false
    foreach($id in $script:ShellClassIds){$value=[string](Get-ShellRegistryValue ('SOFTWARE\Classes\CLSID\'+$id+'\InprocServer32'));if($value){$present=$true;if($AllowedPaths -notcontains $value){$foreign=$true};if($native -and $native -ine $value){$complete=$false};$native=$value}else{$complete=$false}}
    for($i=2;$i -lt 5;++$i){$expected=$script:ShellClassIds[1];if($i -eq 2){$expected=$script:ShellClassIds[0]};$value=[string](Get-ShellRegistryValue $script:ShellRegistryPaths[$i]);if($value){$present=$true;if($value -ine $expected){$foreign=$true}}else{$complete=$false}}
    return @{Present=$present;Complete=$complete;Foreign=$foreign;DllPath=$native}
}
function Get-ShellReceipt($Context) {
    $owner=Read-ShellOwner $Context.Directory
    if($owner.Count -and $owner['InstallDirectory'] -ine $Context.InstallationDirectory){return @{Foreign=$true}}
    $path=Join-Path $Context.Directory 'InstalledState.clixml';Assert-ShellOrdinaryPath $path
    if([IO.File]::Exists($path)){$receipt=Import-Clixml -LiteralPath $path;if($receipt.Version -ne 1 -or $receipt.InstallationDirectory -ine $Context.InstallationDirectory -or (@($Context.Directory)+@($Context.LegacyDirectories)) -notcontains $receipt.Directory){return @{Foreign=$true}};foreach($schema in (Get-ShellSchemaPaths)){if($schema -ine (Join-Path $receipt.Directory 'FBE.Sequence.propdesc') -and $schema -ine (Join-Path $Context.Directory 'FBE.Sequence.propdesc')){return @{Foreign=$true}}};return $receipt}
    $directory=$Context.Directory
    if(-not $owner.Count){
        $allowed=@();foreach($dir in @($Context.Directory)+@($Context.LegacyDirectories)){$allowed+=Join-Path $dir $Context.NativeName}
        $registration=Get-ShellRegistration $allowed
        if($registration.Foreign){return @{Foreign=$true}}
        if(-not $registration.Present){foreach($name in $script:ShellPayloadNames){if([IO.File]::Exists((Join-Path $directory $name))){return @{Foreign=$true}}};if((Get-ShellSchemaPaths).Count){return @{Foreign=$true}};return @{Version=1;InstallationDirectory=$Context.InstallationDirectory;Directory=$directory;Files=@{};Installed=$false;Cleanup=$false}}
        if($Context.PreviousProperty -ne 1 -or -not $registration.Complete -or -not $registration.DllPath){return @{Foreign=$true}}
        $directory=Split-Path -Parent $registration.DllPath
    }
    $files=@{}
    foreach($name in $script:ShellPayloadNames){$file=Join-Path $directory $name;Assert-ShellOrdinaryPath $file;if([IO.File]::Exists($file)){$files[$name]=Get-ShellHash $file}}
    if($owner.Count){foreach($pair in @(@('FBShell.dll','X86Sha256'),@('FBShell64.dll','X64Sha256'),@('FBE.Sequence.propdesc','SchemaSha256'))){if($owner[$pair[1]] -notmatch '^[0-9a-fA-F]{64}$' -or $files[$pair[0]] -ine $owner[$pair[1]]){throw 'Installed owner hashes do not match the payload.'}}}
    $registration=Get-ShellRegistration @((Join-Path $directory $Context.NativeName))
    if($registration.Foreign){return @{Foreign=$true}}
    $schemas=Get-ShellSchemaPaths
    foreach($schema in $schemas){if($schema -ine (Join-Path $directory 'FBE.Sequence.propdesc')){return @{Foreign=$true}}}
    return @{Version=1;InstallationDirectory=$Context.InstallationDirectory;Directory=$directory;Files=$files;Installed=[bool]$registration.Present;Cleanup=$false;Schemas=$schemas}
}
function Test-ShellReceiptFiles($Receipt,[bool]$AllowMissing) {
    foreach($name in $Receipt.Files.Keys){$path=Join-Path $Receipt.Directory $name;Assert-ShellOrdinaryPath $path;if(-not [IO.File]::Exists($path)){if($AllowMissing){continue};throw "Installed payload is missing: $path"};if((Get-ShellHash $path) -ine $Receipt.Files[$name]){throw "Installed payload changed: $path"}}
}
function New-ShellSnapshot($Context,$Receipt) {
    $state=Join-Path $Context.Directory 'State';$null=[IO.Directory]::CreateDirectory($state)
    $files=@{}
    foreach($name in @($script:ShellPayloadNames)+@('Owner.ini','InstalledState.clixml')){
        $target=Join-Path $Context.Directory $name;Assert-ShellOrdinaryPath $target
        $entry=@{Exists=[IO.File]::Exists($target);Hash='';Backup=('old-'+$name)}
        if($entry.Exists){$entry.Hash=Get-ShellHash $target;[IO.File]::Copy($target,(Join-Path $state $entry.Backup),$false)}
        $files[$name]=$entry
    }
    $schemas=@{}
    foreach($path in (Get-ShellSchemaPaths)){Assert-ShellOrdinaryPath $path;if(-not [IO.File]::Exists($path)){throw "Registered schema is missing: $path"};$schemas[$path]=Get-ShellHash $path}
    return @{Version=1;PreviousInstallLocation=$Context.PreviousInstallLocation;Phase='prepared';InstallationDirectory=$Context.InstallationDirectory;Directory=$Context.Directory;Receipt=$Receipt.Clone();Registry=(Get-ShellRegistrySnapshot);Schemas=$schemas;Files=$files;PreviousProperty=$Context.PreviousProperty;PreviousSystem=$Context.PreviousSystem}
}
function Save-ShellJournal($Context,$Snapshot) {Save-ShellData (Join-Path $Context.Directory 'State\Transaction.clixml') $Snapshot}
function Remove-ShellState($Context) {
    $state=Join-Path $Context.Directory 'State'
    if(-not [IO.Directory]::Exists($state)){return}
    foreach($file in @([IO.Directory]::GetFiles($state) | Sort-Object {if([IO.Path]::GetFileName($_) -eq 'Transaction.clixml'){1}else{0}})){Assert-ShellOrdinaryPath $file;$name=[IO.Path]::GetFileName($file);if(@('old-FBShell.dll','old-FBShell64.dll','old-FBE.Sequence.propdesc','old-Owner.ini','old-InstalledState.clixml','replaced-owner.ini','Transaction.clixml','Transaction.clixml.next','Transaction.clixml.previous') -notcontains $name){throw "Unknown file in recovery state: $file"};[IO.File]::Delete($file)}
    [IO.Directory]::Delete($state,$false)
}
function Restore-ShellSnapshot($Context,$Snapshot) {
    $paths=@((Join-Path $Context.Directory $Context.NativeName),(Join-Path $Snapshot.Receipt.Directory $Context.NativeName))
    if((Get-ShellRegistration $paths).Foreign){throw 'Recovery refused: COM registration belongs to another installation.'}
    $owner=Read-ShellOwner $Context.Directory;if($owner.Count -and $owner['InstallDirectory'] -ine $Context.InstallationDirectory){throw 'Recovery refused: package owner changed.'}
    foreach($path in (Get-ShellSchemaPaths)){if($Snapshot.Schemas.Keys -notcontains $path -and $path -ine (Join-Path $Context.Directory 'FBE.Sequence.propdesc')){throw 'Recovery refused: property schema belongs to another installation.'}}
    # Restore only the fixed payload files represented in this shell snapshot.
    foreach($name in $script:ShellPayloadNames){$entry=$Snapshot.Files[$name];$target=Join-Path $Context.Directory $name;if($entry.Exists){if(-not [IO.File]::Exists($target) -or (Get-ShellHash $target) -ine $entry.Hash){$backup=Join-Path $Context.Directory ('State\'+$entry.Backup);Assert-ShellOrdinaryPath $backup;if((Get-ShellHash $backup) -ine $entry.Hash){throw ('Recovery backup changed: '+$backup)};[IO.File]::Copy($backup,$target,$true)}}elseif([IO.File]::Exists($target)){[IO.File]::Delete($target)}}
    if($Snapshot.Phase -ne 'files' -and $Snapshot.Phase -ne 'prepared'){
        Restore-ShellRegistry $Snapshot.Registry
        foreach($path in (Get-ShellSchemaPaths)){Unregister-ShellSchema $path}
        foreach($path in $Snapshot.Schemas.Keys){if((Get-ShellHash $path) -ine $Snapshot.Schemas[$path]){throw "Previous schema changed: $path"};Register-ShellSchema $path}
    }
    foreach($name in @('Owner.ini','InstalledState.clixml')){$entry=$Snapshot.Files[$name];$target=Join-Path $Context.Directory $name;if($entry.Exists){$backup=Join-Path $Context.Directory ('State\'+$entry.Backup);Assert-ShellOrdinaryPath $backup;if((Get-ShellHash $backup) -ine $entry.Hash){throw ('Recovery backup changed: '+$backup)};[IO.File]::Copy($backup,$target,$true)}elseif([IO.File]::Exists($target)){[IO.File]::Delete($target)}}
    $Snapshot.Phase='rolled-back';Save-ShellJournal $Context $Snapshot
    Remove-ShellState $Context
}
function Invoke-ShellChild($Context,[string]$Name,[object[]]$Arguments,[string]$Section) {
    $status=Join-Path $Context.ToolsDirectory ('child-'+$Section+'.ini');[IO.File]::Delete($status)
    $path=Join-Path $Context.ToolsDirectory $Name
    & (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe') -NoProfile -ExecutionPolicy Bypass -File $path @Arguments -StatusFilePath $status
    if($LASTEXITCODE -ne 0){throw "Shell helper failed: $Name; ExitCode=$LASTEXITCODE"}
    $values=@{};foreach($line in [IO.File]::ReadAllLines($status)){$i=$line.IndexOf('=');if($i -gt 0){$values[$line.Substring(0,$i)]=$line.Substring($i+1)}}
    if($values['Result'] -ne 'ok'){throw "Shell helper did not complete: $Name; Result=$($values['Result']); Code=$($values['Code'])"}
}
function Install-ShellPayload($Context) {
    foreach($name in $script:ShellPayloadNames){$staged=Join-Path $Context.Directory ($name+'.new');Assert-ShellOrdinaryPath $staged;[IO.File]::Delete($staged);[IO.File]::Copy((Join-Path $Context.PayloadDirectory $name),$staged,$false)}
    Invoke-ShellChild $Context 'update-shell-files.ps1' @('-Directory',$Context.Directory,'-ExpectedX86Sha256',$Context.Hashes[0],'-ExpectedX64Sha256',$Context.Hashes[1],'-ExpectedSchemaSha256',$Context.Hashes[2]) 'Payload'
}
function Install-ShellRegistration($Context) {
    Invoke-ShellChild $Context 'register-sequence-property-schema.ps1' @('-SchemaPath',(Join-Path $Context.Directory 'FBE.Sequence.propdesc'),'-ExpectedSchemaSha256',$Context.Hashes[2],'-NoRestartExplorer') 'Schema'
    Invoke-ShellChild $Context 'register-modern-property-handler.ps1' @('-DllPath',(Join-Path $Context.Directory $Context.NativeName),'-ExpectedDllSha256',$Context.Hashes[$Context.NativeIndex],'-Platform',$Context.Platform) 'Shell'
}
function Remove-ShellRegistration($Context,$Receipt) {
    $install=$Context.InstallationDirectory;if(-not [IO.File]::Exists((Join-Path $Receipt.Directory 'Owner.ini'))){$install=''}
    $arguments=@('-DllPath',(Join-Path $Receipt.Directory $Context.NativeName),'-Platform',$Context.Platform,'-ExpectedDllSha256',$Receipt.Files[$Context.NativeName],'-ExpectedSchemaSha256',$Receipt.Files['FBE.Sequence.propdesc'],'-UnregisterSchema')
    if($install){$arguments+=@('-InstallationDirectory',$install)}
    Invoke-ShellChild $Context 'unregister-modern-property-handler.ps1' $arguments 'Shell'
}
function New-ShellResult([string]$Result,[string]$Code,[bool]$Installed,[bool]$Cleanup,[bool]$Recovery,[string]$Message) {
    return @{Result=$Result;Code=$Code;EffectiveInstalled=[int]$Installed;CleanupPending=[int]$Cleanup;RecoveryPending=[int]$Recovery;SystemIntegrationInstalled=[int]($Installed -or $Cleanup -or $Recovery);Message=$Message}
}
function Write-ShellOwner($Context) {
    $path=Join-Path $Context.Directory 'Owner.ini';Assert-ShellOrdinaryPath $path
    $next=$path+'.next';Assert-ShellOrdinaryPath $next
    [IO.File]::WriteAllLines($next,[string[]]@('[Owner]',('InstallDirectory='+$Context.InstallationDirectory),('X86Sha256='+$Context.Hashes[0]),('X64Sha256='+$Context.Hashes[1]),('SchemaSha256='+$Context.Hashes[2])),[Text.Encoding]::Unicode)
    if([IO.File]::Exists($path)){[IO.File]::Replace($next,$path,(Join-Path $Context.Directory 'State\replaced-owner.ini'))}else{[IO.File]::Move($next,$path)}
}
function Complete-ShellRemoval($Context,$Receipt) {
    Test-ShellReceiptFiles $Receipt $true
    $registration=Get-ShellRegistration @((Join-Path $Receipt.Directory $Context.NativeName))
    if($registration.Present -or (Get-ShellSchemaPaths).Count){return New-ShellResult 'skipped' 'REGISTRATION_CHANGED' $false $false $false 'Registration changed during cleanup; files retained.'}
    try {
        foreach($name in $script:ShellPayloadNames){[IO.File]::Delete((Join-Path $Receipt.Directory $name))}
        # Legacy helper files are removed only after all owned DLLs and the schema.
        $oldTools=Join-Path $Receipt.Directory 'InstallerTools'
        foreach($name in @('register-modern-property-handler.ps1','register-sequence-property-schema.ps1','unregister-modern-property-handler.ps1','update-shell-files.ps1')){$path=Join-Path $oldTools $name;Assert-ShellOrdinaryPath $path;if([IO.File]::Exists($path)){[IO.File]::Delete($path)}}
        Remove-ShellState $Context
        [IO.File]::Delete((Join-Path $Receipt.Directory 'Owner.ini'))
        [IO.File]::Delete((Join-Path $Context.Directory 'InstalledState.clixml'))
        return New-ShellResult 'ok' 'REMOVED' $false $false $false 'System integration removed.'
    }
    catch {return New-ShellResult 'pending' 'CLEANUP_PENDING' $false $true $false ('Files retained for retry: '+$_.Exception.Message)}
}
function Protect-ShellDirectory($Context) {
    foreach($directory in @($Context.Directory,(Join-Path $Context.Directory 'State'))) {
        Assert-ShellOrdinaryPath (Join-Path $directory 'probe')
        $null=[IO.Directory]::CreateDirectory($directory)
        & (Join-Path $env:WINDIR 'System32\icacls.exe') $directory /reset | Out-Null
        if($LASTEXITCODE -ne 0){throw "Cannot protect shell state directory: $directory"}
    }
    foreach($name in @($script:ShellPayloadNames)+@('Owner.ini','InstalledState.clixml')){
        $path=Join-Path $Context.Directory $name;Assert-ShellOrdinaryPath $path
        if([IO.File]::Exists($path)){& (Join-Path $env:WINDIR 'System32\icacls.exe') $path /reset | Out-Null;if($LASTEXITCODE -ne 0){throw "Cannot protect shell payload: $path"}}
    }
}
function Invoke-ShellLifecycle($Context,[string]$Operation) {
    $snapshot=$null;$receipt=$null;$changed=$false;$lock=$null
    try {
        $lockPath=Join-Path (Split-Path -Parent $Context.Directory) 'ShellLifecycle.lock';Assert-ShellOrdinaryPath $lockPath
        $lock=[IO.File]::Open($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
        Assert-ShellOrdinaryPath (Join-Path $Context.Directory 'Owner.ini')
        $receipt=Get-ShellReceipt $Context
        if($receipt.Foreign){return New-ShellResult 'skipped' 'FOREIGN_REGISTRATION' $false $false $false 'System integration belongs to another installation; retained.'}
        if($Operation -ne 'Inspect'){Protect-ShellDirectory $Context;$receipt=Get-ShellReceipt $Context;if($receipt.Foreign){return New-ShellResult 'skipped' 'FOREIGN_PACKAGE' $false $false $false 'Package owner changed; retained.'}}
        $journal=Join-Path $Context.Directory 'State\Transaction.clixml';Assert-ShellOrdinaryPath $journal
        if([IO.File]::Exists($journal)){
            $saved=Import-Clixml -LiteralPath $journal
            if($saved.Version -ne 1 -or $saved.Directory -ine $Context.Directory -or $saved.InstallationDirectory -ine $Context.InstallationDirectory){throw 'Recovery snapshot belongs to another installation.'}
            if($saved.Phase -eq 'cleanup' -and $receipt.Cleanup){if($Operation -eq 'Remove'){return Complete-ShellRemoval $Context $receipt};return New-ShellResult 'pending' 'CLEANUP_PENDING' $false $true $false 'Complete the pending removal before reinstalling integration.'}
            if($saved.Phase -eq 'committed' -or $saved.Phase -eq 'rolled-back'){try {Remove-ShellState $Context}catch{return New-ShellResult 'pending' 'BACKUP_CLEANUP_PENDING' ([bool]$receipt.Installed) $false $false 'Confirmed integration state retained; release the backup files and retry.'}}
            elseif($Operation -eq 'Recover'){$snapshot=$saved;Restore-ShellSnapshot $Context $snapshot;return New-ShellResult 'ok' 'RECOVERED' ([bool]$snapshot.Receipt.Installed) $false $false 'Previous integration restored.'}
            else{return New-ShellResult 'error' 'RECOVERY_REQUIRED' ([bool]$receipt.Installed) $false $true 'An interrupted integration operation requires explicit recovery.'}
        }
        if($receipt.Cleanup){if($Operation -eq 'Remove'){return Complete-ShellRemoval $Context $receipt};return New-ShellResult 'pending' 'CLEANUP_PENDING' $false $true $false 'Complete the pending removal first.'}
        Test-ShellReceiptFiles $receipt $false
        $allowed=@((Join-Path $Context.Directory $Context.NativeName),(Join-Path $receipt.Directory $Context.NativeName))
        $registration=Get-ShellRegistration $allowed
        if($registration.Foreign){return New-ShellResult 'skipped' 'FOREIGN_REGISTRATION' $false $false $false 'Foreign registration retained.'}
        if($receipt.Installed -and -not $registration.Complete){throw 'Owned registration is incomplete; no changes were made.'}
        if($Operation -eq 'Update' -and $receipt.Installed -and $receipt.Directory -ieq $Context.Directory -and $registration.Complete){
            $same=$true;for($i=0;$i -lt 3;++$i){if($receipt.Files[$script:ShellPayloadNames[$i]] -ine $Context.Hashes[$i]){$same=$false}}
            if($same -and [IO.File]::Exists((Join-Path $Context.Directory 'Owner.ini')) -and (Get-ShellSchemaPaths).Count -eq 1){return New-ShellResult 'ok' 'ALREADY_CURRENT' $true $false $false 'Installed package is already registered.'}
        }
        if($Operation -eq 'Inspect'){return New-ShellResult 'ok' 'INSPECTED' ([bool]$receipt.Installed) $false $false 'Installed integration inspected.'}
        if($Operation -eq 'Recover'){return New-ShellResult 'ok' 'NO_RECOVERY_NEEDED' ([bool]$receipt.Installed) $false $false 'No unfinished transaction.'}
        if($Operation -eq 'Remove' -and -not $receipt.Installed){return New-ShellResult 'ok' 'NOT_INSTALLED' $false $false $false 'No owned integration is installed.'}
        $null=[IO.Directory]::CreateDirectory($Context.Directory)
        # Adopt the observed installed package before a newer uninstaller replaces the old one.
        Save-ShellData (Join-Path $Context.Directory 'InstalledState.clixml') $receipt
        Remove-ShellState $Context
        $snapshot=New-ShellSnapshot $Context $receipt;Save-ShellJournal $Context $snapshot
        if($Operation -eq 'Remove'){
            $snapshot.Phase='unregistering';Save-ShellJournal $Context $snapshot;$changed=$true
            Remove-ShellRegistration $Context $receipt
            $receipt.Installed=$false;$receipt.Cleanup=$true
            Save-ShellData (Join-Path $Context.Directory 'InstalledState.clixml') $receipt
            $snapshot.Phase='cleanup';Save-ShellJournal $Context $snapshot
            return Complete-ShellRemoval $Context $receipt
        }
        $snapshot.Phase='files';Save-ShellJournal $Context $snapshot;$changed=$true
        Install-ShellPayload $Context
        $snapshot.Phase='registering';Save-ShellJournal $Context $snapshot
        foreach($path in $snapshot.Schemas.Keys){Unregister-ShellSchema $path}
        Install-ShellRegistration $Context
        $installed=Get-ShellRegistration @((Join-Path $Context.Directory $Context.NativeName))
        if($installed.Foreign -or -not $installed.Complete){throw 'New COM registration did not match the installed DLL.'}
        $schemas=Get-ShellSchemaPaths
        if($schemas.Count -ne 1 -or $schemas[0] -ine (Join-Path $Context.Directory 'FBE.Sequence.propdesc')){throw 'New property schema registration was not confirmed.'}
        $snapshot.Phase='owner';Save-ShellJournal $Context $snapshot
        Write-ShellOwner $Context
        $files=@{};for($i=0;$i -lt 3;++$i){$files[$script:ShellPayloadNames[$i]]=$Context.Hashes[$i]}
        $receipt=@{Version=1;InstallationDirectory=$Context.InstallationDirectory;Directory=$Context.Directory;Files=$files;Installed=$true;Cleanup=$false;Schemas=$schemas}
        Save-ShellData (Join-Path $Context.Directory 'InstalledState.clixml') $receipt
        $snapshot.Phase='committed';Save-ShellJournal $Context $snapshot
        $changed=$false
        try {Remove-ShellState $Context} catch {return New-ShellResult 'pending' 'BACKUP_CLEANUP_PENDING' $true $false $false 'New integration is registered; backup cleanup requires a retry.'}
        return New-ShellResult 'ok' 'INSTALLED' $true $false $false 'System integration installed.'
    }
    catch {
        $failure=$_.Exception;$installed=($Context.PreviousProperty -eq 1);if($null -ne $receipt){$installed=[bool]$receipt.Installed};if($null -ne $snapshot){$installed=[bool]$snapshot.Receipt.Installed}
        if($null -ne $snapshot -and $changed){
            try {Restore-ShellSnapshot $Context $snapshot}
            catch {return New-ShellResult 'error' 'RECOVERY_REQUIRED' $installed $false $true ($failure.Message+'; recovery failed: '+$_.Exception.Message)}
        }
        if($null -eq $snapshot -and -not $changed){try {if(-not [IO.File]::Exists((Join-Path $Context.Directory 'State\Transaction.clixml'))){Remove-ShellState $Context}}catch{}}
        while($null -ne $failure.InnerException){$failure=$failure.InnerException}
        $code=[Runtime.InteropServices.Marshal]::GetHRForException($failure) -band 65535
        if($code -eq 32 -or $code -eq 33 -or $code -eq 1224 -or $failure.Message -like '*Result=pending*'){return New-ShellResult 'pending' 'FILES_IN_USE' $installed $false $false 'Previous integration retained. Close Explorer or restart Windows and retry.'}
        if([IO.File]::Exists((Join-Path $Context.Directory 'State\Transaction.clixml'))){return New-ShellResult 'error' 'RECOVERY_REQUIRED' $installed $false $true $failure.Message}
        return New-ShellResult 'error' 'UPDATE_FAILED' $installed $false $false $failure.Message
    }
    finally {if($null -ne $lock){$lock.Close()}}
}
