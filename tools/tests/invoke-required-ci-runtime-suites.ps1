<# Run the workflow's mandatory matrices locally without copying their cases. #>
[CmdletBinding()]
param([string]$FbeExe, [Parameter(Mandatory)][string]$ReportPath)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$exe = (Resolve-Path -LiteralPath $FbeExe).Path
$results = @()
Push-Location $root
try {
    foreach ($suite in (& (Join-Path $PSScriptRoot 'get-required-ci-runtime-suites.ps1'))) {
        if ($suite.RequiresGitHubHosted -and ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted')) {
            $receiptPath = Join-Path $root 'out/tests/required-ci-runtime/clipboard-ole.json'
            if (Test-Path -LiteralPath $receiptPath) {
                $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
                $git = Join-Path $env:ProgramFiles 'Git/cmd/git.exe'
                if (!(Test-Path -LiteralPath $git)) { $git = (Get-Command git -ErrorAction Stop).Source }
                $head = (& $git -C $root rev-parse HEAD).Trim()
                if ($LASTEXITCODE -ne 0) { throw 'Cannot determine current SHA for external OLE evidence.' }
                $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($suite.Command)))
                if ($receipt.runId -notmatch '^\d+$') { throw 'Invalid external CI run ID.' }
                $runJson = & gh api "repos/sklart/fictionbook-editor-next/actions/runs/$($receipt.runId)"
                if ($LASTEXITCODE -ne 0) { throw 'Cannot authenticate external OLE CI evidence through GitHub.' }
                $run = $runJson | ConvertFrom-Json
                & (Join-Path $PSScriptRoot 'Assert-CiOleRuntimeEvidence.ps1') -Receipt $receipt -Run $run -Head $head -CommandSha256 $hash
                Write-Host "Real OLE Clipboard: PASS (external CI evidence $($run.html_url)); local execution NOT RUN."
                $results += @{ id=$suite.Id; result='PASS'; execution='external CI'; url=$run.html_url }
                continue
            }
            Write-Warning 'Real OLE Clipboard: NOT RUN; requires a disposable GitHub-hosted account/window station. Full validation is incomplete.'
            $results += @{ id=$suite.Id; result='NOT RUN'; reason='disposable GitHub-hosted logon required' }
            continue
        }
        $command = '$ErrorActionPreference="Stop"' + "`n" + $suite.Command.Replace('out/Release/FBE.exe', ("'" + $exe.Replace("'", "''") + "'"))
        & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -Command $command
        if ($LASTEXITCODE -ne 0) { throw "Required runtime matrix failed: $($suite.Id), exit=$LASTEXITCODE" }
        $results += @{ id=$suite.Id; result='PASS' }
    }
} finally {
    Pop-Location
    $directory = Split-Path -Parent $ReportPath
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    @{ suites=$results; complete=($results.Count -eq 4 -and @($results | Where-Object result -ne 'PASS').Count -eq 0) } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ReportPath -Encoding utf8
}
