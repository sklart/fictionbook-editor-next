#requires -Version 5.1
<# Explicit migration to one portable copy. HKCU is read-only; replacing
   existing JSON profiles requires the explicit -ReplaceExisting switch. #>
[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$PortableDirectory,
    [ValidateSet('import-epub','export-html','export-docx','export-epub')][string[]]$Plugin = @('import-epub','export-html','export-docx','export-epub'),
    [switch]$ReplaceExisting,
    [string]$RegistryRoot = 'Software\FBETeam\FictionBook Editor Next'
)
$ErrorActionPreference = 'Stop'
$directory = (Resolve-Path -LiteralPath $PortableDirectory).Path
$marker = Join-Path $directory 'portable.ini'
if(!(Test-Path -LiteralPath $marker -PathType Leaf)) { throw 'The selected copy has no portable.ini.' }
if(!('FbePluginMigrationIni' -as [type])) {
    Add-Type -TypeDefinition @'
using System.Text;
using System.Runtime.InteropServices;
public static class FbePluginMigrationIni {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode)]
    public static extern uint GetPrivateProfileString(string section,string key,string fallback,StringBuilder value,uint size,string file);
}
'@
}
$buffer = New-Object Text.StringBuilder 32768
[void][FbePluginMigrationIni]::GetPrivateProfileString('Portable','DataPath','Data',$buffer,32768,$marker)
$dataPath = $buffer.ToString()
if(!$dataPath -or $dataPath.Contains('..') -or $dataPath.IndexOfAny([char[]]':/\') -ge 0) { $dataPath = 'Data' }
$destination = Join-Path $directory "$dataPath\Settings\Plugins"
$keys = @{'import-epub'='ImportEPUB';'export-html'='PluginSettings\ExportHTML';'export-docx'='ExportDOCX';'export-epub'='Plugins\ExportEPUB'}
$base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser,[Microsoft.Win32.RegistryView]::Registry32)
try {
    foreach($name in $Plugin) {
        $target = Join-Path $destination "$name.json"
        if((Test-Path -LiteralPath $target) -and !$ReplaceExisting) { throw "Profile already exists; no overwrite was requested: $target" }
        $key = $base.OpenSubKey("$RegistryRoot\$($keys[$name])", $false)
        if(!$key) { Write-Verbose "No legacy settings: $name"; continue }
        $values = [ordered]@{}
        try {
            foreach($field in $key.GetValueNames()) {
                $kind = $key.GetValueKind($field)
                $value = $key.GetValue($field, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                if($kind -eq [Microsoft.Win32.RegistryValueKind]::DWord) {
                    $values[$field] = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$value),0)
                } elseif($kind -eq [Microsoft.Win32.RegistryValueKind]::String -or $kind -eq [Microsoft.Win32.RegistryValueKind]::ExpandString) {
                    if($name -eq 'export-html' -and $field -in @('Template','CustomCss') -and ([string]$value).StartsWith($directory + '\',[StringComparison]::OrdinalIgnoreCase)) {
                        $value = ([string]$value).Substring($directory.Length + 1)
                    }
                    $values[$field] = [string]$value
                } else { throw "Unsupported legacy value type: $name/$field ($kind)" }
            }
        } finally { $key.Dispose() }
        $json = [ordered]@{schemaVersion=1;values=$values} | ConvertTo-Json -Depth 4
        if(!$PSCmdlet.ShouldProcess($target, 'Import legacy plug-in settings')) { continue }
        [void][IO.Directory]::CreateDirectory($destination)
        $temporary = Join-Path $destination ([guid]::NewGuid().ToString('N') + '.tmp')
        $backup = Join-Path $destination ([guid]::NewGuid().ToString('N') + '.bak')
        try {
            $bytes = [Text.UTF8Encoding]::new($false, $true).GetBytes($json)
            if($bytes.Length -gt 1MB) { throw "Profile exceeds the supported 1 MiB limit: $target" }
            $stream = [IO.File]::Open($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
            try { $stream.Write($bytes,0,$bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
            if(Test-Path -LiteralPath $target) {
                if(!$ReplaceExisting) { throw "Profile appeared during migration: $target" }
                [IO.File]::Replace($temporary,$target,$backup)
            } else { [IO.File]::Move($temporary,$target) }
            if(Test-Path -LiteralPath $backup) { [IO.File]::Delete($backup) }
            Write-Host "Imported: $target"
        } finally {
            if(Test-Path -LiteralPath $temporary) { [IO.File]::Delete($temporary) }
            # Failed replacement backups remain available for manual recovery.
        }
    }
} finally { $base.Dispose() }
