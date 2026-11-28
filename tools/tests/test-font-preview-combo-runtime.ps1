[CmdletBinding()]
param([ValidateRange(10, 600)][int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'font-preview-combo'
$exe = Join-Path $scratch.Root 'font-preview-combo-runtime.exe'
$object = Join-Path $scratch.Root 'font-preview-combo-runtime.obj'
$stdout = Join-Path $scratch.Root 'runtime.stdout.log'
$stderr = Join-Path $scratch.Root 'runtime.stderr.log'
$passed = $false
try {
    & cl.exe /nologo /EHsc /std:c++17 /W4 /WX /utf-8 /DUNICODE /D_UNICODE "/Fo:$object" "/Fe:$exe" `
        (Join-Path $PSScriptRoot 'font-preview-combo-runtime.cpp') /link user32.lib gdi32.lib comctl32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Font picker runtime harness did not compile.' }
    $process = Start-Process -FilePath $exe -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
        throw "Font picker runtime harness timed out after $TimeoutSeconds seconds."
    }
    Get-Content -LiteralPath $stdout, $stderr | Write-Host
    if ($process.ExitCode -ne 0) { throw "Font picker runtime harness failed: $($process.ExitCode)" }
    $passed = $true
} finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed
}
