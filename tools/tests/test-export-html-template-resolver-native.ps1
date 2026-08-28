<# Executes the production TemplateResolver and SupportsEmbeddedImages helpers. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$compiler = Join-Path $env:VCToolsInstallDir 'bin\Hostx64\x86\cl.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw "v143 compiler is not available: $compiler" }
$out = Join-Path $repoRoot 'out\tests\export-html-template-resolver-native'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$source = Join-Path $PSScriptRoot 'export-html-template-resolver-test.cpp'
$exe = Join-Path $out 'export-html-template-resolver-test.exe'
& $compiler /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /MD /W3 "/I$repoRoot\src\export-html" "/I$repoRoot\third_party" $source (Join-Path $repoRoot 'src\export-html\Utils.cpp') "/Fe:$exe" /link ole32.lib oleaut32.lib comsuppw.lib comdlg32.lib
if ($LASTEXITCODE -ne 0) { throw 'Native ExportHTML template resolver test did not compile.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw "Native ExportHTML template resolver test failed: $LASTEXITCODE" }
Write-Host 'Native ExportHTML template resolver regression passed.'
