[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputDirectory = Join-Path $repoRoot 'build\tests\script-preview-icons'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
Push-Location $outputDirectory
try {
    $visualStyles = "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
    & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /utf-8 (Join-Path $PSScriptRoot 'script-preview-icons-test.cpp') /Fe:script-preview-icons-test.exe /link $visualStyles comctl32.lib gdi32.lib user32.lib
    if($LASTEXITCODE -ne 0) { throw 'Script preview icon test compilation failed.' }
    $icons = @((Join-Path $repoRoot 'runtime\Scripts\01_Регистр.ico'), (Join-Path $repoRoot 'runtime\Scripts\06_Чистка.ico'))
    foreach($icon in $icons) {
        $bytes = [IO.File]::ReadAllBytes($icon)
        if($bytes.Length -lt 6 -or [BitConverter]::ToUInt16($bytes, 0) -ne 0 -or [BitConverter]::ToUInt16($bytes, 2) -ne 1) { throw "Invalid ICO: $icon" }
        $count = [BitConverter]::ToUInt16($bytes, 4)
        if($count -eq 0 -or $bytes.Length -lt 6 + 16 * $count) { throw "Invalid ICO directory: $icon" }
        $frames = for($i = 0; $i -lt $count; ++$i) {
            $offset = 6 + 16 * $i
            $width = if($bytes[$offset] -eq 0) { 256 } else { $bytes[$offset] }
            $height = if($bytes[$offset + 1] -eq 0) { 256 } else { $bytes[$offset + 1] }
            "${width}x${height}"
        }
        Write-Host "$(Split-Path -Leaf $icon): $($frames -join ', ')"
    }
    & .\script-preview-icons-test.exe @icons
    if($LASTEXITCODE -ne 0) { throw 'Script preview icon test failed.' }
} finally {
    Pop-Location
}
