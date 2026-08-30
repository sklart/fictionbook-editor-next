[CmdletBinding()]
param(
    [switch]$RegisterSchema,
    [switch]$Installed,
    [ValidateSet('ru', 'en')]
    [string]$ExpectedLanguage
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

$labels = [ordered]@{
    'FBE.Sequence'        = @{ Id = 201; Resource = 'IDS_FBE_SEQUENCE_LABEL' }
    'FBE.Genre'           = @{ Id = 202; Resource = 'IDS_FBE_GENRE_LABEL' }
    'FBE.DocumentVersion' = @{ Id = 203; Resource = 'IDS_FBE_DOCUMENT_VERSION_LABEL' }
    'FBE.DocumentDate'    = @{ Id = 204; Resource = 'IDS_FBE_DOCUMENT_DATE_LABEL' }
    'FBE.Keywords'        = @{ Id = 205; Resource = 'IDS_FBE_KEYWORDS_LABEL' }
    'FBE.DocumentId'      = @{ Id = 206; Resource = 'IDS_FBE_DOCUMENT_ID_LABEL' }
}
$schemaText = Get-Content -Raw -LiteralPath $schemaPath
$resourceText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbshell\FBShell.rc')
foreach ($entry in $labels.GetEnumerator()) {
    if ($schemaText -notmatch ([regex]::Escape("name=`"$($entry.Key)`"") + '[\s\S]*?' + [regex]::Escape("label=`"@FBShell.dll,-$($entry.Value.Id)`""))) { throw "Schema resource mapping is missing for $($entry.Key)." }
    $matches = [regex]::Matches($resourceText, ('(?m)^\s*' + [regex]::Escape($entry.Value.Resource) + '\s+"(?<label>[^"]+)"'))
    if ($matches.Count -ne 2) { throw "RU/EN resource contract is missing for $($entry.Key)." }
    $entry.Value.ru = $matches[0].Groups['label'].Value
    $entry.Value.en = $matches[1].Groups['label'].Value
}

if ($RegisterSchema -or $Installed) {
    $language = if ($ExpectedLanguage) { $ExpectedLanguage } elseif ([Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'ru') { 'ru' } else { 'en' }
    foreach ($entry in $labels.GetEnumerator()) {
        $displayName = [FbePropertyDescriptionProbe]::DisplayName($entry.Key)
        if ($displayName -ne $entry.Value[$language]) { throw "$($entry.Key): expected '$($entry.Value[$language])', got '$displayName'." }
    }
}

Write-Host 'FBE property-label localization contract passed.'
