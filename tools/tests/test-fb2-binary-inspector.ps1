[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-fb2-binary-inspector-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
try {
    $exe = Join-Path $directory 'fb2-binary-inspector-smoke.exe'
    & cl.exe /nologo /EHsc /std:c++17 /utf-8 /W4 /WX /MT "/I$repoRoot\src\common\fb2" "/Fo$directory\\" (Join-Path $PSScriptRoot 'fb2-binary-inspector-smoke.cpp') (Join-Path $repoRoot 'src\common\fb2\Fb2BinaryInspector.cpp') "/Fe$exe"
    if ($LASTEXITCODE -ne 0) { throw "Fb2BinaryInspector compilation failed: $LASTEXITCODE" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "Fb2BinaryInspector smoke failed: $LASTEXITCODE" }
    Write-Host 'FB2 binary inspector smoke passed.'
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
