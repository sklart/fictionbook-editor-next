<# Focused file, registry-fixture, architecture and compatibility checks. #>
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$work=Join-Path $repoRoot ('build\tests\shell-maintenance-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work -Force | Out-Null
function Assert([bool]$Value,[string]$Message) { if(-not $Value){throw $Message} }
function Load-Functions([string]$Name) {
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $repoRoot "tools\build\$Name"),[ref]$tokens,[ref]$errors)
    Assert (-not $errors) "Parse failed: $Name"
    return $ast
}
$registerAst=Load-Functions 'register-modern-property-handler.ps1'
$unregisterAst=Load-Functions 'unregister-modern-property-handler.ps1'
foreach($fn in $registerAst.EndBlock.Statements | Where-Object {$_ -is [Management.Automation.Language.FunctionDefinitionAst]}) { . ([ScriptBlock]::Create($fn.Extent.Text)) }
# Executable selection uses controlled files; no COM server is loaded by these tests.
$windows=Join-Path $work 'Windows'
foreach($dir in @('System32','SysWOW64','Sysnative')) {New-Item -ItemType Directory -Path (Join-Path $windows $dir) -Force | Out-Null; Set-Content (Join-Path $windows "$dir\regsvr32.exe") 'fixture'}
foreach($case in @(@('Win32',$false,$false,'System32'),@('Win32',$true,$false,'SysWOW64'),@('Win32',$true,$true,'SysWOW64'),@('x64',$true,$true,'System32'),@('x64',$true,$false,'Sysnative'))) {
    $Platform=$case[0]
    $actual=Get-RegSvr32Path -Is64BitOS $case[1] -Is64BitProcess $case[2] -WindowsDirectory $windows
    Assert ($actual -eq (Join-Path $windows ($case[3]+'\regsvr32.exe'))) "Architecture selection failed: $case"
}
$Platform='x64';$rejected=$false
try {Get-RegSvr32Path -Is64BitOS $false -Is64BitProcess $false -WindowsDirectory $windows | Out-Null} catch {$rejected=$true}
Assert $rejected 'x64 registration must fail on x86 Windows.'
Remove-Item -LiteralPath (Join-Path $windows 'System32\regsvr32.exe')
$rejected=$false;try {Get-RegSvr32Path -Is64BitOS $true -Is64BitProcess $true -WindowsDirectory $windows | Out-Null} catch {$rejected=$true}
Assert $rejected 'Missing regsvr32 must fail before launch.'

# Run real registry helper operations in an isolated HKCU subtree.
$hivePath='Software\FBENextInstallerTests\'+[Guid]::NewGuid().ToString('N')
$null=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($hivePath)
$DllPath=Join-Path $work 'Обработчик.dll';[IO.File]::WriteAllText($DllPath,'owned payload')
$ExpectedDllSha256=(Get-FileHash $DllPath -Algorithm SHA256).Hash
$schema=Join-Path $work 'FBE.Sequence.propdesc';[IO.File]::WriteAllText($schema,'owned schema')
$ExpectedSchemaSha256=(Get-FileHash $schema -Algorithm SHA256).Hash
$InstallationDirectory='D:\Книги\FBE'
$StatusFilePath=Join-Path $work 'status.ini'
$PropertyHandlerClsid='{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}'
$ThumbnailProviderClsid='{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}'
function Open-MachineRegistry {return [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($hivePath,$true)}
function Test-IsAdministrator {return $true}
function Get-RegSvr32Path {return 'fixture-regsvr32.exe'}
function Invoke-RegSvr32 {return 1}
try {
    $entry=($registerAst.EndBlock.Statements | Where-Object {$_ -is [Management.Automation.Language.TryStatementAst]} | Select-Object -Last 1).Extent.Text
    . ([ScriptBlock]::Create($entry))
    Assert (Get-ComClassRegistrationState -Clsid $PropertyHandlerClsid -ExpectedDllPath $DllPath) 'Property COM fallback not registered.'
    Assert ((Get-Content $StatusFilePath -Raw) -match 'Result=ok') 'Registration did not report ok.'
    foreach($fn in $unregisterAst.EndBlock.Statements | Where-Object {$_ -is [Management.Automation.Language.FunctionDefinitionAst]}) { . ([ScriptBlock]::Create($fn.Extent.Text)) }
    function Open-MachineRegistry {return [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($hivePath,$true)}
    function Test-IsAdministrator {return $true}
    $entry=($unregisterAst.EndBlock.Statements | Where-Object {$_ -is [Management.Automation.Language.TryStatementAst]} | Select-Object -Last 1).Extent.Text
    [IO.File]::WriteAllLines((Join-Path $work 'Owner.ini'),[string[]]@('[Owner]',"InstallDirectory=$InstallationDirectory","X64Sha256=$ExpectedDllSha256","SchemaSha256=$ExpectedSchemaSha256"),[Text.Encoding]::Unicode)
    $InstallationDirectory='D:\Чужая установка'
    . ([ScriptBlock]::Create($entry))
    Assert ((Get-Content $StatusFilePath -Raw) -match 'Result=skipped') 'Foreign package was not skipped.'
    Assert (Test-ComClassRegistrationExists -Clsid $PropertyHandlerClsid) 'Foreign package registration was removed.'
    $InstallationDirectory='D:\Книги\FBE'
    $savedHash=$ExpectedDllSha256;$ExpectedDllSha256='0'*64
    . ([ScriptBlock]::Create($entry))
    Assert ((Get-Content $StatusFilePath -Raw) -match 'FOREIGN_PACKAGE') 'Newer package at the same shared path was not preserved.'
    Assert (Test-ComClassRegistrationExists -Clsid $PropertyHandlerClsid) 'Newer package COM class was removed.'
    $ExpectedDllSha256=$savedHash
    function Remove-OwnedSchema {throw 'Controlled schema unregistration failure'}
    $failed=$false;try {. ([ScriptBlock]::Create($entry))} catch {$failed=$true}
    Assert $failed 'Schema error was ignored.'
    Assert ((Get-Content $StatusFilePath -Raw) -match 'Result=error') 'Schema error did not overwrite stale success/skip status.'
    Assert (Test-ComClassRegistrationExists -Clsid $PropertyHandlerClsid) 'COM removed after schema error.'
    function Remove-OwnedSchema {}
    $root=Open-MachineRegistry
    $key=$root.OpenSubKey("SOFTWARE\Classes\CLSID\$PropertyHandlerClsid\InprocServer32",$true)
    $key.SetValue('','D:\Newer\FBShell.dll');$key.Close();$root.Close()
    . ([ScriptBlock]::Create($entry))
    Assert ((Get-Content $StatusFilePath -Raw) -match 'FOREIGN_REGISTRATION') 'Foreign COM registration was not preserved.'
    $root=Open-MachineRegistry;$key=$root.OpenSubKey("SOFTWARE\Classes\CLSID\$PropertyHandlerClsid\InprocServer32",$true);$key.SetValue('',$DllPath);$key.Close();$root.Close()
    . ([ScriptBlock]::Create($entry))
    Assert ((Get-Content $StatusFilePath -Raw) -match 'Result=ok') 'Owned unregister did not report success.'
    Assert (-not (Test-ComClassRegistrationExists -Clsid $PropertyHandlerClsid)) 'Owned COM class remains.'
}
finally { [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($hivePath) }

# Real filesystem replacement, hash rejection and rollback with a locked second DLL.
$payload=Join-Path $work 'payload';New-Item -ItemType Directory -Path $payload | Out-Null
$names=@('FBShell.dll','FBShell64.dll','FBE.Sequence.propdesc')
$hashes=@();foreach($name in $names){[IO.File]::WriteAllText((Join-Path $payload $name),'old '+$name);[IO.File]::WriteAllText((Join-Path $payload ($name+'.new')),'new '+$name);$hashes+=(Get-FileHash (Join-Path $payload ($name+'.new')) -Algorithm SHA256).Hash}
$args=@{Directory=$payload;ExpectedX86Sha256=$hashes[0];ExpectedX64Sha256=$hashes[1];ExpectedSchemaSha256=$hashes[2];StatusFilePath=(Join-Path $work 'payload-status.ini')}
$lock=[IO.File]::Open((Join-Path $payload 'FBShell64.dll'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try { & (Join-Path $repoRoot 'tools\build\update-shell-files.ps1') @args } finally {$lock.Close()}
Assert ((Get-Content $args.StatusFilePath -Raw) -match 'Result=pending') 'Locked DLL did not report pending.'
foreach($name in $names){Assert ([IO.File]::ReadAllText((Join-Path $payload $name)) -eq ('old '+$name)) "Rollback changed $name"}
# The first staged file was consumed by the attempted replacement, so recreate all staging.
foreach($name in $names){[IO.File]::WriteAllText((Join-Path $payload ($name+'.new')),'new '+$name)}
& (Join-Path $repoRoot 'tools\build\update-shell-files.ps1') @args
Assert ((Get-Content $args.StatusFilePath -Raw) -match 'Result=ok') 'Immediate replacement failed.'
foreach($name in $names){Assert ([IO.File]::ReadAllText((Join-Path $payload $name)) -eq ('new '+$name)) "Replacement failed: $name";[IO.File]::WriteAllText((Join-Path $payload ($name+'.new')),'tampered')}
$rejected=$false;try {& (Join-Path $repoRoot 'tools\build\update-shell-files.ps1') @args} catch {$rejected=$true}
Assert $rejected 'Tampered payload accepted.'
foreach($name in $names){Assert ([IO.File]::ReadAllText((Join-Path $payload $name)) -eq ('new '+$name)) 'Hash rejection changed installed payload.'}

$verifier=Join-Path $repoRoot 'tools\build\verify-installer-payload.ps1'
& $verifier -InputDirectory $payload -X86Sha256 $hashes[0] -X64Sha256 $hashes[1] -SchemaSha256 $hashes[2]
[IO.File]::WriteAllText((Join-Path $payload 'FBShell64.dll'),'changed after preparation')
$rejected=$false;try {& $verifier -InputDirectory $payload -X86Sha256 $hashes[0] -X64Sha256 $hashes[1] -SchemaSha256 $hashes[2]} catch {$rejected=$true}
Assert $rejected 'Build accepted a changed payload with a correctly formatted old digest.'

foreach($name in @('register-modern-property-handler.ps1','unregister-modern-property-handler.ps1','register-sequence-property-schema.ps1','update-shell-files.ps1','manage-shell-integration.ps1','shell-integration-lifecycle.ps1')) {
    $text=Get-Content -LiteralPath (Join-Path $repoRoot "tools\build\$name") -Raw
    Assert ($text -notmatch '::new\(|::IsNullOrWhiteSpace\(|RegistryView|::OpenBaseKey\(|\$PSScriptRoot|\[pscustomobject\]|\s-Raw\b|\?\?|\?\.') "Unsupported baseline construct remains: $name"
}
Write-Host "Shell maintenance checks passed. Registry writes were isolated to HKCU test keys. Artifacts: $work"
