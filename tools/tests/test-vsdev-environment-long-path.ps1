[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$savedPath = [Environment]::GetEnvironmentVariable('Path', 'Process')

try {
    # Reproduce the hosted-runner condition without depending on its image.
    [Environment]::SetEnvironmentVariable('Path', ('C:\Windows\System32;' * 1000), 'Process')
    & (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') `
        -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion '14.44'
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw 'VsDevCmd did not provide cl.exe after importing a long PATH environment.'
    }
}
finally {
    [Environment]::SetEnvironmentVariable('Path', $savedPath, 'Process')
}

Write-Host 'VsDevCmd long-PATH regression passed.'
