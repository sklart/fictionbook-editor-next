<# Called only after the isolated OLE producer succeeds on a hosted runner. #>
[CmdletBinding()]
param([string]$FbeExe='out/Release/FBE.exe')
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') { throw 'OLE receipt requires GitHub-hosted CI.' }
$suite = & (Join-Path $PSScriptRoot 'get-required-ci-runtime-suites.ps1') | Where-Object Id -eq 'clipboard-ole'
$hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($suite.Command)))
$directory = Join-Path $PSScriptRoot '../../out/tests/required-ci-runtime'
New-Item -ItemType Directory -Path $directory -Force | Out-Null
@{ schemaVersion=1; result='PASS'; suite='clipboard-ole'; head=$env:GITHUB_SHA; runId=$env:GITHUB_RUN_ID; repository=$env:GITHUB_REPOSITORY; commandSha256=$hash; executableSha256=(Get-FileHash -LiteralPath $FbeExe).Hash } |
    ConvertTo-Json | Set-Content (Join-Path $directory 'clipboard-ole.json') -Encoding utf8
