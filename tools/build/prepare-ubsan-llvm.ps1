[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
# Official x86 package includes i386 UBSan; the official x64 package does not.
$version='21.1.8'
$digest='0492c5c292e515ebbbc2533671e54c5895fe209e534812313f64257407233181'
$cache=Join-Path $root "out/tools/llvm-$version"
$destination=Join-Path $cache 'win32'
$archive=Join-Path $cache "LLVM-$version-win32.exe"
if(Test-Path (Join-Path $destination 'bin/clang-cl.exe')) {
    return & (Join-Path $PSScriptRoot 'Resolve-UbsanToolchain.ps1') -LlvmRoot $destination
}
New-Item -ItemType Directory -Force $cache | Out-Null
if(-not (Test-Path $archive)) {
    Invoke-WebRequest "https://github.com/llvm/llvm-project/releases/download/llvmorg-$version/LLVM-$version-win32.exe" -OutFile $archive
}
if((Get-FileHash $archive).Hash.ToLowerInvariant() -ne $digest) { throw 'Official LLVM asset SHA-256 mismatch.' }
$seven=Get-Command 7z.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
if(-not $seven) { $seven=Join-Path $env:ProgramFiles '7-Zip/7z.exe' }
if(-not (Test-Path $seven)) { throw '7-Zip required to extract LLVM without system installation.' }
if(Test-Path $destination) { throw "Incomplete LLVM cache; preserve and inspect before retrying: $destination" }
& $seven x $archive "-o$destination" -y | Out-Host
if($LASTEXITCODE -ne 0) { throw 'Official LLVM extraction failed.' }
& (Join-Path $PSScriptRoot 'Resolve-UbsanToolchain.ps1') -LlvmRoot $destination
