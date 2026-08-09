<#
.SYNOPSIS
Generates compact FB2 autocomplete metadata from the runtime XSD files.

.DESCRIPTION
The generated header is deliberately data-only.  Runtime code performs XML
context parsing; this script remains the sole source for schema vocabulary.
#>
[CmdletBinding()]
param(
    [string]$SchemaDirectory = (Join-Path $PSScriptRoot '..\..\runtime'),
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\..\src\fbe\generated\Fb2SchemaMetadata.h')
)

$ErrorActionPreference = 'Stop'
$SchemaDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($SchemaDirectory)
$OutputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
$schemaFiles = @('FictionBook.xsd', 'FictionBookLinks.xsd', 'FictionBookLang.xsd', 'FictionBookGenres.xsd') |
    ForEach-Object { Join-Path $SchemaDirectory $_ }
foreach ($file in $schemaFiles) { if (-not (Test-Path -LiteralPath $file)) { throw "Schema not found: $file" } }

$nameTable = [System.Xml.NameTable]::new()
$ns = [System.Xml.XmlNamespaceManager]::new($nameTable)
$ns.AddNamespace('xs', 'http://www.w3.org/2001/XMLSchema')
$documents = @()
foreach ($file in $schemaFiles) {
    $document = [System.Xml.XmlDocument]::new($nameTable)
    $document.Load($file)
    $documents += $document
}

function Local-Name([string]$name) { if ($name.Contains(':')) { return $name.Substring($name.IndexOf(':') + 1) }; return $name }
function Cpp-Escape([string]$value) { return $value.Replace('\\', '\\\\').Replace('"', '\\"') }

$types = @{}
$globalElements = @{}
$globalAttributes = @{}
foreach ($document in $documents) {
    foreach ($node in $document.SelectNodes('/xs:schema/xs:complexType[@name]', $ns)) { $types[$node.GetAttribute('name')] = $node }
    foreach ($node in $document.SelectNodes('/xs:schema/xs:simpleType[@name]', $ns)) { $types[$node.GetAttribute('name')] = $node }
    foreach ($node in $document.SelectNodes('/xs:schema/xs:element[@name]', $ns)) { $globalElements[$node.GetAttribute('name')] = $node }
    foreach ($node in $document.SelectNodes('/xs:schema/xs:attribute[@name]', $ns)) { $globalAttributes[$node.GetAttribute('name')] = $node }
}

function Get-ElementType($element) {
    if ($element.HasAttribute('type')) { return $types[(Local-Name $element.GetAttribute('type'))] }
    return $element.SelectSingleNode('xs:complexType|xs:simpleType', $ns)
}

function Add-ParticleChildren($node, $set) {
    foreach ($child in $node.ChildNodes) {
        if ($child.NamespaceURI -ne 'http://www.w3.org/2001/XMLSchema') { continue }
        if ($child.LocalName -eq 'element') {
            $name = if ($child.HasAttribute('ref')) { Local-Name $child.GetAttribute('ref') } else { $child.GetAttribute('name') }
            if ($name) { [void]$set.Add($name) }
            continue
        }
        if ($child.LocalName -in @('sequence', 'choice', 'all', 'group')) { Add-ParticleChildren $child $set }
    }
}

function Get-TypeChildren($type) {
    $result = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    if ($null -eq $type) { return $result }
    foreach ($particle in $type.SelectNodes('./xs:sequence|./xs:choice|./xs:all|./xs:complexContent/xs:extension/xs:sequence|./xs:complexContent/xs:extension/xs:choice', $ns)) {
        Add-ParticleChildren $particle $result
    }
    $extension = $type.SelectSingleNode('./xs:complexContent/xs:extension[@base]', $ns)
    if ($extension) { foreach ($name in (Get-TypeChildren $types[(Local-Name $extension.GetAttribute('base'))])) { [void]$result.Add($name) } }
    return $result
}

function Get-TypeAttributes($type) {
    $result = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    if ($null -eq $type) { return $result }
    foreach ($attribute in $type.SelectNodes('./xs:attribute|./xs:complexContent/xs:extension/xs:attribute|./xs:simpleContent/xs:extension/xs:attribute', $ns)) {
        # Keep imported XLink references qualified.  The runtime component
        # replaces xlink: with the prefix actually bound by the document.
        $name = if ($attribute.HasAttribute('ref')) { $attribute.GetAttribute('ref') } else { $attribute.GetAttribute('name') }
        if ($name) { [void]$result.Add($name) }
    }
    $extension = $type.SelectSingleNode('./xs:complexContent/xs:extension[@base]|./xs:simpleContent/xs:extension[@base]', $ns)
    if ($extension) { foreach ($name in (Get-TypeAttributes $types[(Local-Name $extension.GetAttribute('base'))])) { [void]$result.Add($name) } }
    return $result
}

function Get-TypeEnumerations($type) {
    $result = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    if ($null -eq $type) { return $result }
    foreach ($enumeration in $type.SelectNodes('.//xs:enumeration[@value]', $ns)) { [void]$result.Add($enumeration.GetAttribute('value')) }
    $extension = $type.SelectSingleNode('./xs:restriction[@base]|./xs:simpleContent/xs:extension[@base]', $ns)
    if ($extension) { foreach ($value in (Get-TypeEnumerations $types[(Local-Name $extension.GetAttribute('base'))])) { [void]$result.Add($value) } }
    return $result
}

$metadata = @{}
function Add-ElementMetadata($element) {
    $name = if ($element.HasAttribute('ref')) { Local-Name $element.GetAttribute('ref') } else { $element.GetAttribute('name') }
    if (-not $name) { return }
    if (-not $metadata.ContainsKey($name)) { $metadata[$name] = @{ Children = [System.Collections.Generic.HashSet[string]]::new(); Attributes = [System.Collections.Generic.HashSet[string]]::new(); Values = [System.Collections.Generic.HashSet[string]]::new() } }
    $type = Get-ElementType $element
    foreach ($item in (Get-TypeChildren $type)) { [void]$metadata[$name].Children.Add($item) }
    foreach ($item in (Get-TypeAttributes $type)) { [void]$metadata[$name].Attributes.Add($item) }
    foreach ($item in (Get-TypeEnumerations $type)) { [void]$metadata[$name].Values.Add($item) }
}
foreach ($document in $documents) { foreach ($element in $document.SelectNodes('//xs:element[@name or @ref]', $ns)) { Add-ElementMetadata $element } }

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('// Generated by tools/build/generate-fb2-schema-metadata.ps1. Do not edit manually.')
$lines.Add('#pragma once')
$lines.Add('')
$lines.Add('struct Fb2SchemaElementMetadata { const char* name; const char* children; const char* attributes; const char* values; };')
$lines.Add('')
$lines.Add('static const Fb2SchemaElementMetadata kFb2SchemaMetadata[] = {')
foreach ($name in ($metadata.Keys | Sort-Object)) {
    $item = $metadata[$name]
    $children = (($item.Children | Sort-Object) -join ' ')
    $attributes = (($item.Attributes | Sort-Object) -join ' ')
    $values = (($item.Values | Sort-Object) -join ' ')
    $lines.Add(('    {{ "{0}", "{1}", "{2}", "{3}" }},' -f (Cpp-Escape $name), (Cpp-Escape $children), (Cpp-Escape $attributes), (Cpp-Escape $values)))
}
$lines.Add('};')
$lines.Add('static const size_t kFb2SchemaMetadataCount = sizeof(kFb2SchemaMetadata) / sizeof(kFb2SchemaMetadata[0]);')

$directory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $directory -Force | Out-Null
[System.IO.File]::WriteAllLines($OutputPath, $lines, [System.Text.UTF8Encoding]::new($false))
