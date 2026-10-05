<# Verifies additive IExternalHelper XML API compatibility without copying the full IDL. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$idl = Get-Content -Raw (Join-Path $root 'src\contracts\fbe.idl')
$interface = [regex]::Match($idl, '(?s)interface\s+IExternalHelper\s*:\s*IDispatch\s*\{(?<body>.*?)\n\s*\};')
if(-not $interface.Success) { throw 'IExternalHelper is missing.' }
$body = $interface.Groups['body'].Value
if($idl -notmatch 'uuid\(7269066E-2089-4408-B3F3-E8D75984D5A6\)') { throw 'IExternalHelper IID changed.' }
for($id = 1; $id -le 31; ++$id) { if($body -notmatch "\[.*?id\($id\)") { throw "Existing DISPID $id is missing." } }
$expected = @{ 'GetSourceText'=32; 'ValidateSourceText'=33; 'GetLastSourceDiagnostic'=34; 'ApplySourceText'=35 }
foreach($name in $expected.Keys) { if($body -notmatch "\[id\($($expected[$name])\).*?$name") { throw "$name has an unexpected DISPID." } }
$positions = @($expected.Keys | ForEach-Object { $body.IndexOf($_) })
if(($positions | Measure-Object -Minimum).Minimum -le $body.IndexOf('GetLocalizedString')) { throw 'New methods are not appended.' }
$status = & 'C:\Program Files\Git\cmd\git.exe' -C $root status --porcelain -- build/generated
if($status) { throw "Generated MIDL output is tracked or staged: $status" }
Write-Host 'IExternalHelper XML API compatibility contract passed.'
