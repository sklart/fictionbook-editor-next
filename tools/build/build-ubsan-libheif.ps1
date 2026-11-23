[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildRoot,
    [Parameter(Mandatory)][string]$ClangCl
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$clang=(Resolve-Path -LiteralPath $ClangCl).Path
$destination=[IO.Path]::GetFullPath($BuildRoot)
if(Test-Path -LiteralPath $destination){throw "UBSan libheif staging already exists: $destination"}
New-Item -ItemType Directory -Path $destination | Out-Null
$vs=& (Join-Path $PSScriptRoot 'Resolve-VsCmake.ps1') -PlatformToolset v143
$ninja=Join-Path $vs.GeneratorInstance 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
if(!(Test-Path -LiteralPath $ninja)){throw "Ninja not found: $ninja"}
$build=Join-Path $destination 'build';$install=Join-Path $destination 'install'
$prefix=(Join-Path $root 'build/libde265/install/Release')+';'+(Join-Path $root 'build/aom/install/Release')
$vendor=Join-Path $root 'third_party/libheif'
$vendorCommit=(& git -C $vendor rev-parse HEAD | Select-Object -First 1)
if($LASTEXITCODE -ne 0 -or !$vendorCommit){throw 'Cannot identify libheif source revision.'}
$expectedCommit=(& git -C $root rev-parse 'HEAD:third_party/libheif' | Select-Object -First 1)
if($LASTEXITCODE -ne 0 -or $vendorCommit -cne $expectedCommit){throw 'libheif checkout does not match the recorded gitlink.'}
$vendorChanges=@(& git -C $vendor status --porcelain=v1)
if($LASTEXITCODE -ne 0 -or $vendorChanges.Count){throw 'UBSan libheif source must be clean.'}
$dependencyPaths=@((Join-Path $root 'build/libde265/install/Release/lib/libde265.lib'),(Join-Path $root 'build/aom/install/Release/lib/aom.lib'))
$dependencyHashes=@($dependencyPaths | ForEach-Object {(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash})
# CheckIncludeFileCXX links an executable. That unrelated link can fail inside
# the sanitizer process environment and incorrectly report an absent header.
# Verify the actual C++20 facility with the selected compiler before seeding
# CMake's cached header result; a failed or missing compile is never accepted.
$probe=Join-Path $destination 'bit-probe.cpp';$object=Join-Path $destination 'bit-probe.obj'
[IO.File]::WriteAllText($probe, "#include <bit>`n#include <cstdint>`nstatic_assert(std::bit_cast<std::uint32_t>(0.0f) == 0);`n")
& $clang --target=i686-pc-windows-msvc /nologo /std:c++20 /MT /EHsc /c $probe ('/Fo'+$object)
if($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $object -PathType Leaf)){throw 'The selected clang cannot compile the required C++20 bit header.'}
& $vs.CMake -S (Join-Path $root 'third_party/libheif') -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_C_COMPILER=$clang" "-DCMAKE_CXX_COMPILER=$clang" '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_INSTALL_PREFIX=$install" '-DCMAKE_CXX_STANDARD=20' '-DHAVE_BIT=1' '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded' '-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' '-DCMAKE_CXX_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /EHsc /DLIBDE265_STATIC_BUILD /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' "-DCMAKE_PREFIX_PATH=$prefix" -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DWITH_EXAMPLES=OFF -DENABLE_PLUGIN_LOADING=OFF -DWITH_LIBDE265=ON -DWITH_LIBDE265_PLUGIN=OFF -DWITH_AOM_DECODER=ON -DWITH_AOM_DECODER_PLUGIN=OFF -DWITH_AOM_ENCODER=OFF -DWITH_X264=OFF -DWITH_X265=OFF -DWITH_LIBSHARPYUV=OFF -DWITH_JPEG_DECODER=OFF -DWITH_JPEG_ENCODER=OFF -DWITH_OpenJPEG_DECODER=OFF -DWITH_OpenJPEG_ENCODER=OFF -DWITH_FFMPEG_DECODER=OFF -DWITH_OpenH264_DECODER=OFF -DWITH_GDK_PIXBUF=OFF -DWITH_UNCOMPRESSED_CODEC=OFF
if($LASTEXITCODE){throw 'UBSan libheif configure failed.'}
& $vs.CMake --build $build --target install --parallel 2
if($LASTEXITCODE){throw 'UBSan libheif build failed.'}
$library=Join-Path $install 'lib/heif.lib';$header=Join-Path $install 'include/libheif/heif.h'
if(!(Test-Path -LiteralPath $library -PathType Leaf) -or !(Test-Path -LiteralPath $header -PathType Leaf)){throw 'UBSan libheif build did not produce its library and headers.'}
for($index=0;$index -lt $dependencyPaths.Count;++$index){if((Get-FileHash -LiteralPath $dependencyPaths[$index] -Algorithm SHA256).Hash -cne $dependencyHashes[$index]){throw 'A dependent library changed while building UBSan libheif.'}}
$vendorAfter=(& git -C $vendor rev-parse HEAD | Select-Object -First 1)
if($LASTEXITCODE -ne 0 -or $vendorAfter -cne $vendorCommit){throw 'libheif revision changed during the build.'}
$vendorChanges=@(& git -C $vendor status --porcelain=v1)
if($LASTEXITCODE -ne 0 -or $vendorChanges.Count){throw 'libheif source changed during the build.'}
@{compiler=$clang;compilerSha256=(Get-FileHash $clang -Algorithm SHA256).Hash;librarySha256=(Get-FileHash $library -Algorithm SHA256).Hash;libheifCommit=$vendorCommit;libde265Sha256=$dependencyHashes[0];aomSha256=$dependencyHashes[1];configuration='Release';platform='Win32';sanitizer='vendor-uninstrumented'} | ConvertTo-Json | Set-Content (Join-Path $destination 'provenance.json') -Encoding utf8
# The sanitizer collector treats a line starting with its name as a finding.
Write-Host ('ImageImport helper dependency provenance: ' + ((Get-Content -Raw (Join-Path $destination 'provenance.json') | ConvertFrom-Json | ConvertTo-Json -Compress)))
Write-Host "ImageImport helper libheif built in isolated staging: $destination"
