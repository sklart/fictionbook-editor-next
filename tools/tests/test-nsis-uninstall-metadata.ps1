<# Runs the production metadata function/final section in an isolated NSIS fixture. #>
[CmdletBinding()]param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\nsis\Installer\MakeInstaller.nsi')
$function = [regex]::Match($source, '(?ms)^Function WriteUninstallDisplayMetadata\r?\n.*?^FunctionEnd').Value
$finalizeFunction = [regex]::Match($source, '(?ms)^Function FinalizeUninstallMetadata\r?\n.*?^FunctionEnd').Value
$section = [regex]::Match($source, '(?ms)^Section "-FinalizeUninstallMetadata"\r?\n.*?^SectionEnd').Value
if (-not $function -or -not $finalizeFunction -or -not $section) { throw 'Production uninstall metadata code is missing.' }
# Prevent a regression that moves finalization ahead of the last payload section.
$lastFile = $source.LastIndexOf('File "${INPUTDIR}\Dict\de_DE.aff"', [StringComparison]::Ordinal)
if ($lastFile -lt 0 -or $source.IndexOf($section, [StringComparison]::Ordinal) -lt $lastFile) {
    throw 'Metadata finalization must follow all payload sections.'
}
$compiler = & (Join-Path $root 'tools\build\resolve-nsis.ps1')
$id = [Guid]::NewGuid().ToString('N')
$work = Join-Path $root "out\tests\nsis-uninstall-metadata-$id"
$hive = "Software\FBENextInstallerTests\$id"
$null = New-Item -ItemType Directory -Path $work
[IO.File]::WriteAllBytes((Join-Path $work 'core.bin'), [byte[]]::new(4096))
[IO.File]::WriteAllBytes((Join-Path $work 'dictionary.bin'), [byte[]]::new(8192))
$versionHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\version.h')
$version = [regex]::Match($versionHeader, '#define\s+FBE_VERSION_STRING\s+"([^"]+)"').Groups[1].Value
function Assert([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Invoke-Fixture([string]$Mode, [string]$Directory) {
    $setup = Join-Path $work "$Mode-setup.exe"
    if (-not (Test-Path -LiteralPath $setup)) {
        $template = @'
Unicode true
RequestExecutionLevel user
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!define PRODUCT_NAME "FictionBook Editor Next"
!define PRODUCT_VER_NUM "@VERSION@"
!define PRODUCT_UNINST_ROOT_KEY SHCTX
!define PRODUCT_UNINST_KEY "@HIVE@"
OutFile "@SETUP@"
Var DeploymentMode
Function .onInit
  SetShellVarContext current
  StrCpy $DeploymentMode "@MODE@"
FunctionEnd
Section "Core"
  SetOutPath "$INSTDIR"
  File "@WORK@\core.bin"
  WriteUninstaller "$INSTDIR\uninst.exe"
  ${If} $DeploymentMode != "portable"
    Call WriteUninstallDisplayMetadata
  ${EndIf}
SectionEnd
Section "Late dictionary"
  SetOutPath "$INSTDIR\Dict"
  File "@WORK@\dictionary.bin"
SectionEnd
@FUNCTION@
@FINALIZE_FUNCTION@
@SECTION@
Section "Uninstall"
  IfSilent 0 +4
  FileOpen $0 "$INSTDIR\silent-uninstall.txt" w
  FileWrite $0 "silent"
  FileClose $0
SectionEnd
'@
        $script = $template.Replace('@VERSION@', $version).Replace('@HIVE@', $hive).Replace('@SETUP@', $setup).
            Replace('@MODE@', $Mode).Replace('@WORK@', $work).Replace('@FUNCTION@', $function).Replace('@FINALIZE_FUNCTION@', $finalizeFunction).Replace('@SECTION@', $section)
        $scriptPath = Join-Path $work "$Mode.nsi"
        [IO.File]::WriteAllText($scriptPath, $script, [Text.UTF8Encoding]::new($true))
        & $compiler /V2 $scriptPath *> (Join-Path $work "$Mode-compile.log")
        if ($LASTEXITCODE -ne 0) { throw "NSIS fixture compilation failed: $work\$Mode-compile.log" }
    }
    $p = Start-Process -FilePath $setup -ArgumentList @('/S', "/D=$Directory") -Wait -PassThru -WindowStyle Hidden
    Assert ($p.ExitCode -eq 0) "NSIS $Mode fixture failed: $($p.ExitCode)"
}
function Assert-Metadata([string]$Directory, [string[]]$Dates) {
    $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($hive)
    try {
        Assert ($null -ne $key) 'Installed metadata key is missing.'
        Assert ($key.GetValue('DisplayName') -ceq 'FictionBook Editor Next') 'DisplayName contains build/version decorations.'
        Assert ($key.GetValue('DisplayVersion') -ceq $version) 'DisplayVersion differs from src/version.h.'
        Assert ($key.GetValue('UninstallString') -ceq ('"' + $Directory + '\uninst.exe"')) 'UninstallString is not quoted.'
        Assert ($key.GetValue('QuietUninstallString') -ceq ('"' + $Directory + '\uninst.exe" /S')) 'QuietUninstallString is incorrect.'
        Assert ($Dates -contains $key.GetValue('InstallDate')) 'InstallDate is not the runtime local date in yyyyMMdd format.'
        $bytes = (Get-ChildItem -LiteralPath $Directory -File -Recurse | Measure-Object -Property Length -Sum).Sum
        Assert ($key.GetValue('EstimatedSize') -eq [math]::Floor($bytes / 1024)) 'EstimatedSize does not include the final dictionary payload.'
        Assert ($key.GetValueKind('EstimatedSize') -eq [Microsoft.Win32.RegistryValueKind]::DWord) 'EstimatedSize is not REG_DWORD.'
    } finally { if ($key) { $key.Dispose() } }
}
try {
    $directory = Join-Path $work 'Installed path with spaces'
    $beforeDate = Get-Date -Format yyyyMMdd
    Invoke-Fixture 'installed' $directory
    Assert-Metadata $directory @($beforeDate, (Get-Date -Format yyyyMMdd))
    # Same-scope upgrade must replace stale values, including the old decorated name.
    $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($hive, $true)
    try {
        foreach ($name in @('DisplayName','DisplayVersion','UninstallString','QuietUninstallString','InstallDate')) { $key.SetValue($name, 'stale') }
        $key.SetValue('EstimatedSize', 1, [Microsoft.Win32.RegistryValueKind]::DWord)
    } finally { $key.Dispose() }
    Invoke-Fixture 'installed' $directory
    Assert-Metadata $directory @($beforeDate, (Get-Date -Format yyyyMMdd))
    $p = Start-Process -FilePath (Join-Path $directory 'uninst.exe') -ArgumentList @('/S', "_?=$directory") -Wait -PassThru -WindowStyle Hidden
    Assert ($p.ExitCode -eq 0 -and (Test-Path -LiteralPath (Join-Path $directory 'silent-uninstall.txt'))) 'Silent uninstall did not execute.'
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($hive)
    Invoke-Fixture 'portable' (Join-Path $work 'Portable path with spaces')
    $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($hive)
    try { Assert ($null -eq $key) 'Portable finalization wrote uninstall metadata.' } finally { if ($key) { $key.Dispose() } }
    Write-Host "NSIS uninstall metadata runtime: PASS (install, upgrade, late payload size, date, quotes, silent uninstall, portable guard). Logs: $work"
} finally {
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($hive, $false)
}
