<# Contract for the prepared image-decoder cache and its safe reuse path. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$workflow = Get-Content -Raw -LiteralPath (Join-Path $root '.github\workflows\build.yml')
$build = Get-Content -Raw -LiteralPath (Join-Path $root 'tools\build\build.ps1')
$cache = [regex]::Match($workflow, '(?s)- name: Restore prepared image stack cache.*?(?=\n\s*- name:)').Value
if (-not $cache) { throw 'Image-stack cache step is missing.' }
foreach ($required in @('id: image-stack-cache','build/libwebp','build/openjpeg','build/aom','build/libde265','build/libheif','third_party/libwebp/**','third_party/openjpeg/**','Resolve-VsCmake.ps1','CmakeBuildTree.ps1')) {
    if (-not $cache.Contains($required)) { throw "Image-stack cache lacks '$required'." }
}
if (-not $workflow.Contains('steps.image-stack-cache.outputs.cache-hit') -or -not $workflow.Contains('ReusePreparedImageStack = $true')) {
    throw 'The build job must pass -ReusePreparedImageStack only after an image-stack cache hit.'
}
foreach ($required in @('[switch]$ReusePreparedImageStack','Image-stack cache was reported as reusable','CMake configure/build пропущены.','build\libwebp\install\$Configuration\include','build\openjpeg\install\$Configuration\include','build\aom\install\$Configuration\include','build\libde265\install\$Configuration\include','build\libheif\install\$Configuration\include')) {
    if (-not $build.Contains($required)) { throw "build.ps1 lacks safe image-stack reuse invariant '$required'." }
}
$preparedVerifier = [regex]::Match($build, '(?s)function Assert-PreparedDependencies.*?(?=\nfunction )').Value
if (-not $preparedVerifier.Contains('Test-Path -LiteralPath $_)')) {
    throw 'Prepared dependency verification must accept include directories as well as library files.'
}
if ($preparedVerifier.Contains('Test-Path -LiteralPath $_ -PathType Leaf')) {
    throw 'Prepared dependency verification must not require include directories to be Leaf paths.'
}

foreach ($script in @('build-libwebp.ps1','build-openjpeg.ps1')) {
    $contents = Get-Content -Raw -LiteralPath (Join-Path $root "tools\build\$script")
    foreach ($required in @('Resolve-VsCmake.ps1','CmakeBuildTree.ps1','-Action Prepare','-Action Write','-DCMAKE_SYSTEM_VERSION=6.1')) {
        if (-not $contents.Contains($required)) { throw "$script must use the shared VS/CMake fingerprint scheme: missing '$required'." }
    }
    if ($contents -match 'vswhere.*-latest') { throw "$script must not select an arbitrary latest Visual Studio instance." }
}
Write-Host 'Prepared image-stack cache contract passed.'