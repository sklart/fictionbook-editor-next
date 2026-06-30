[CmdletBinding()]
param(
    [string]$ModulePath = "",
    [int]$ResourceId = 109,
    [string]$VersionSuffix = "v2"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($ModulePath)) {
    $ModulePath = Join-Path $repoRoot "out\Release\FBVVerbResources.dll"
}

if (-not (Test-Path -LiteralPath $ModulePath -PathType Leaf)) {
    throw "Не найден MUI-модуль: $ModulePath"
}

$source = @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class ShellApi
{
    [DllImport("shlwapi.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern int SHLoadIndirectString(
        string pszSource,
        StringBuilder pszOutBuf,
        int cchOutBuf,
        IntPtr ppvReserved);
}
"@

Add-Type -TypeDefinition $source

$indirectString = "@$ModulePath,-$ResourceId;$VersionSuffix"
$buffer = [System.Text.StringBuilder]::new(512)
$hr = [ShellApi]::SHLoadIndirectString($indirectString, $buffer, $buffer.Capacity, [IntPtr]::Zero)
$hrHex = '0x{0:X8}' -f ($hr -band 0xffffffff)

Write-Host "SHLoadIndirectString: $indirectString"
Write-Host "HRESULT: $hrHex"
Write-Host "Значение: $($buffer.ToString())"

if ($hr -ne 0) {
    throw "SHLoadIndirectString завершился с ошибкой $hrHex"
}
