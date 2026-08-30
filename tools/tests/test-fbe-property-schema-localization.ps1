[CmdletBinding()]
param(
    [switch]$RegisterSchema
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$schemaPath = Join-Path $repoRoot 'packaging\property-schema\FBE.Sequence.propdesc'

if ($RegisterSchema) {
    & (Join-Path $repoRoot 'tools\build\register-sequence-property-schema.ps1') -SchemaPath $schemaPath
    if ($LASTEXITCODE -ne 0) { throw 'Property schema registration failed.' }
}

if (-not ('FbePropertyDescriptionProbe' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential)] public struct PROPERTYKEY { public Guid fmtid; public uint pid; }
[ComImport, Guid("6F79D558-3E96-4549-A1D1-7D75D2288814"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IPropertyDescriptionProbe {
  void GetPropertyKey(out PROPERTYKEY key); void GetCanonicalName(out IntPtr name); void GetPropertyType(out ushort type); void GetDisplayName(out IntPtr name);
}
public static class FbePropertyDescriptionProbe {
 [DllImport("propsys.dll", CharSet=CharSet.Unicode)] public static extern int PSGetPropertyDescriptionByName(string name, ref Guid iid, out IPropertyDescriptionProbe description);
 public static string DisplayName(string name) { Guid iid=new Guid("6F79D558-3E96-4549-A1D1-7D75D2288814"); IPropertyDescriptionProbe d; int hr=PSGetPropertyDescriptionByName(name, ref iid, out d); if(hr<0) throw new COMException(name,hr); IntPtr text; d.GetDisplayName(out text); try { return Marshal.PtrToStringUni(text); } finally { Marshal.FreeCoTaskMem(text); } }
}
'@
}

$expected = if (([Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'ru')) { @{ 'FBE.Sequence'='FBE:Серия'; 'FBE.Genre'='FBE:Жанр' } } else { @{ 'FBE.Sequence'='FBE:Series'; 'FBE.Genre'='FBE:Genre' } }
foreach ($name in $expected.Keys) {
    $displayName = [FbePropertyDescriptionProbe]::DisplayName($name)
    if ($displayName -ne $expected[$name]) { throw "${name}: expected '$($expected[$name])', got '$displayName'." }
}

Write-Host 'Проверка локализованных FBE.* property labels прошла.'
