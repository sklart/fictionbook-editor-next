[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$temp = Join-Path ([IO.Path]::GetTempPath()) ("fbe-link-navigation-" + $PID)
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $source = Join-Path $root 'tools\tests\link-navigation-test.cpp'
    $exe = Join-Path $temp 'link-navigation-test.exe'
    & $compiler /nologo /utf-8 /EHsc /std:c++17 /I (Join-Path $root 'src\fbe') $source /Fe$exe
    if($LASTEXITCODE -ne 0) { throw 'Link navigation test compilation failed.' }
    & $exe
    if($LASTEXITCODE -ne 0) { throw 'Link navigation test failed.' }
    $view = Get-Content -Raw (Join-Path $root 'src\fbe\FBEview.cpp')
    foreach($contract in @(
        'FindNearestLinkElement',
        'element = element->parentElement;',
        'const bool ctrlClick = oe->ctrlKey == VARIANT_TRUE;',
        'm_link_navigation_origin_ordinal = GetLinkTargetOrdinal',
        'ClearLinkNavigationHistory();',
        'ShellExecuteW(m_hWnd, L"open"')) {
        if(-not $view.Contains($contract)) { throw "Missing link-navigation contract: $contract" }
    }
    Write-Host 'Link navigation helper test passed.'
}
finally { Remove-Item -Recurse -Force -LiteralPath $temp -ErrorAction SilentlyContinue }
