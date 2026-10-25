[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$output=Join-Path $root 'out/tests/binary-store'
New-Item -ItemType Directory -Force $output | Out-Null
Push-Location $output
try {
    & cl.exe /nologo /EHsc /std:c++17 /MT /W4 /WX /DUNICODE /D_UNICODE /utf-8 "/I$root/src/fbe" `
        (Join-Path $PSScriptRoot 'binary-store-test.cpp') (Join-Path $root 'src/fbe/binary/BinaryStore.cpp') `
        /link /SUBSYSTEM:CONSOLE /OUT:binary-store-test.exe
    if($LASTEXITCODE){throw "BinaryStore unit build failed: $LASTEXITCODE"}
    & ./binary-store-test.exe
    if($LASTEXITCODE){throw "BinaryStore unit failed: $LASTEXITCODE"}
} finally { Pop-Location }
$header=Get-Content (Join-Path $root 'src/fbe/binary/BinaryStore.h') -Raw
if($header -notmatch 'std::unordered_map' -or $header -match 'SAFEARRAY|vector<.*BYTE') {throw 'Metadata-only hash catalog contract'}
$idl=Join-Path $root 'src/contracts/fbe.idl'
if((Get-Content $idl -Raw) -match 'BinaryStore'){throw 'Private store leaked into public IDL'}
$implementation=Get-Content (Join-Path $root 'src/fbe/binary/BinaryStore.cpp') -Raw
if($implementation -match 'SAFEARRAY|VariantCopy|vector\s*<\s*(BYTE|unsigned char)|CComVariant|_variant_t') {throw 'Catalog retains or copies binary payloads'}
$helper=Get-Content (Join-Path $root 'src/fbe/ExternalHelper.cpp') -Raw
$publicMethods=[regex]::Match($helper,'(?s)ScriptingMethods\[\]\s*=\s*\{(.*?)\};').Groups[1].Value
if(-not $publicMethods -or $publicMethods -match 'BinaryStore'){throw 'Private store leaked into public scripting method catalog'}
Write-Host 'BinaryStore native and ABI boundary contract: PASS.'
