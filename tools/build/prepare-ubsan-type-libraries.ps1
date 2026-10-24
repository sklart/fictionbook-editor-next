[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $PSScriptRoot 'Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$compiler=Join-Path $env:VCToolsInstallDir 'bin/HostX64/x86/cl.exe'
$output=Join-Path $root 'build/ubsan/generated/type-libraries'
# clang-cl has no #import. MSVC only generates the identical TLB declarations;
# these preparation objects are never linked. All product code uses clang-cl.
$profiles=@{
    editor=@'
#import <msxml6.dll> exclude("ISequentialStream", "_FILETIME")
#import "vbscript3.tlb"
#import <shdocvw.dll> no_auto_exclude rename_namespace("SHD") rename("FindText","FindTextX")
#import <mshtml.tlb> no_auto_exclude rename("TranslateAccelerator","TranslateAcceleratorX") rename("min","mshtml_min") rename("max","mshtml_max")
'@
    shell='#import <msxml6.dll> exclude("ISequentialStream", "_FILETIME")'
    common='#import <msxml6.dll> named_guids rename_namespace("MSXML2") exclude("ISequentialStream", "_FILETIME")'
}
foreach($name in $profiles.Keys) {
    $directory=Join-Path $output $name
    New-Item -ItemType Directory -Force $directory | Out-Null
    $previous=@{}
    foreach($file in Get-ChildItem $directory -File | Where-Object Extension -in @('.tlh','.tli')) {
        $previous[$file.FullName]=@{hash=(Get-FileHash -LiteralPath $file.FullName).Hash;time=$file.LastWriteTimeUtc}
    }
    $source=Join-Path $directory 'import.cpp'
    "#include <windows.h>`r`n#include <comdef.h>`r`n$($profiles[$name])" | Set-Content $source -Encoding utf8
    & $compiler /nologo /c /EHsc /Zc:wchar_t- /DUNICODE /D_UNICODE ("/I"+(Join-Path $root 'src/fbe')) ("/Fo"+(Join-Path $directory 'import.obj')) $source
    if($LASTEXITCODE -ne 0) { throw "TLB declaration generation failed: $name" }
    # MSVC lazily loads wrappers through implementation_key/start_map_region.
    # Clang needs the same inline definitions included as ordinary C++ instead.
    foreach($header in Get-ChildItem $directory -Filter '*.tlh') {
        # Transform ASCII compiler metadata while preserving every other byte,
        # including comments/BOM in whichever encoding MSVC emitted.
        $encoding=[Text.Encoding]::Latin1
        $text=$encoding.GetString([IO.File]::ReadAllBytes($header.FullName))
        $mapping='(?ms)^#pragma start_map_region\("(?<implementation>[^"\r\n]+)"\)\r?\n.*?^#pragma stop_map_region[^\r\n]*(?:\r?\n)?'
        $matches=[regex]::Matches($text,$mapping)
        if($matches.Count -gt 1) { throw 'Unexpected generated TLB implementation mapping.' }
        if($matches.Count -eq 1) {
            $implementation=Join-Path $directory ($header.BaseName+'.tli')
            if([IO.Path]::GetFileName($matches[0].Groups['implementation'].Value) -ne [IO.Path]::GetFileName($implementation) -or -not(Test-Path $implementation)) { throw 'Unexpected TLB implementation mapping.' }
            $include='#include "'+[IO.Path]::GetFileName($implementation)+'"'+"`r`n"
            $text=$text.Substring(0,$matches[0].Index)+$include+$text.Substring($matches[0].Index+$matches[0].Length)
            [IO.File]::WriteAllBytes($header.FullName,$encoding.GetBytes($text))
            $body=$encoding.GetString([IO.File]::ReadAllBytes($implementation))
            $body=[regex]::Replace($body,'(?m)^#pragma implementation_key\(\d+\)\r?\n','')
            [IO.File]::WriteAllBytes($implementation,$encoding.GetBytes($body))
        }
    }
    # Regeneration verifies current type libraries on every invocation, but
    # byte-identical declarations must not force all consuming PCH/TU rebuilds.
    foreach($file in Get-ChildItem $directory -File | Where-Object Extension -in @('.tlh','.tli')) {
        if($previous.ContainsKey($file.FullName) -and (Get-FileHash -LiteralPath $file.FullName).Hash -eq $previous[$file.FullName].hash) {
            [IO.File]::SetLastWriteTimeUtc($file.FullName,$previous[$file.FullName].time)
        }
    }
}
Write-Host "Prepared clang-cl TLB declarations: $output"
