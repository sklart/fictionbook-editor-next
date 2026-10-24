<# Finalise an isolated diagnostic build; never used by packaging. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output=(Resolve-Path -LiteralPath $OutputDirectory).Path
if ($output -ne (Join-Path $root 'out\asan\Release')) { throw 'Unexpected sanitizer output directory.' }
$compilerBin=Join-Path $env:VCToolsInstallDir 'bin\HostX64\x86'
$runtime=Join-Path $compilerBin 'clang_rt.asan_dynamic-i386.dll'
if (-not (Test-Path -LiteralPath $runtime)) { throw 'Install the VC 14.44 x86 AddressSanitizer component.' }
Copy-Item -LiteralPath $runtime -Destination $output -Force
$symbolizer=Join-Path $compilerBin 'llvm-symbolizer.exe'
if (Test-Path -LiteralPath $symbolizer) { Copy-Item -LiteralPath $symbolizer -Destination $output -Force }
$modules=@('FBE.exe','FBV.exe','FBShell.dll','Plugins\ExportHTML.dll','Plugins\ExportDOCX.dll','Plugins\ExportEPUB.dll','Plugins\ImportEPUB.dll','Plugins\ImportEPUBLunaSVG.dll','ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe')
$coverage=@(foreach ($name in $modules) {
    $path=Join-Path $output $name
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing sanitizer module: $name" }
    $imports=(& (Join-Path $compilerBin 'dumpbin.exe') /imports $path) -join "`n"
    if ($LASTEXITCODE -ne 0 -or $imports -notmatch 'clang_rt\.asan_dynamic-i386\.dll') { throw "ASan runtime import missing: $name" }
    $symbols=[IO.Path]::ChangeExtension($path,'.pdb')
    if (-not (Test-Path -LiteralPath $symbols)) { throw "Missing sanitizer symbols: $name" }
    @{path=$name;sha256=(Get-FileHash $path).Hash;symbols=[IO.Path]::GetFileName($symbols);instrumented=$true}
})
$commit=(& git -C $root rev-parse HEAD).Trim()
$units=@(foreach($log in Get-ChildItem (Join-Path $root 'build/asan/obj') -Recurse -Filter 'CL.command.1.tlog') {
    $lines=[IO.File]::ReadAllLines($log.FullName,[Text.Encoding]::Unicode)
    for($index=0;$index -lt $lines.Length-1;++$index) {
        if(-not $lines[$index].StartsWith('^')) { continue }
        $command=$lines[$index+1]
        if($command -notmatch '(?i)(^|\s)/fsanitize=address(\s|$)') { throw "Uninstrumented translation unit in $($log.FullName): $($lines[$index])" }
        foreach($source in $lines[$index].Substring(1).Split('|')) {
            @{source=$source;commandLog=$log.FullName;asan=$true;optimized=($command -match '(?i)/O2|/O1');wholeProgram=($command -match '(?i)/GL')}
        }
    }
})
if($units.Count -lt 200) { throw 'Incomplete sanitizer compilation evidence.' }
@{schemaVersion=1;sanitizer='Address';configuration='Release';platform='Win32';platformToolset='v143';vcToolsVersion=$env:VCToolsVersion;gitCommit=$commit;minimumDiagnosticOS='Windows 10';publishable=$false;modules=$coverage;translationUnits=$units;stlContainerAnnotations=$false;uninstrumented=@('Scintilla','Lexilla','PCRE2','Hunspell','libarchive','image stack','lunasvg/plutovg vendor libraries','Windows/COM/MSHTML')} |
    ConvertTo-Json -Depth 6 | Set-Content (Join-Path $output 'fbe-sanitizer.json') -Encoding utf8
Write-Host "Isolated AddressSanitizer build ready: $output"
