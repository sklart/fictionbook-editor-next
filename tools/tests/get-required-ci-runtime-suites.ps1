<# The existing workflow is the single source for these required runtime matrices. #>
[CmdletBinding()]
param([string]$WorkflowPath=(Join-Path $PSScriptRoot '../../.github/workflows/build.yml'))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$workflow = Get-Content -LiteralPath $WorkflowPath -Raw
$required = [ordered]@{
    'file-close' = 'Run File Close lifecycle regression in isolated profiles'
    'scripting14' = 'Run scripting API 1.4 dialogs, clipboard and status regression'
    'scripting15' = 'Run scripting API 1.5 save and snapshot regression'
    'clipboard-ole' = 'Run real OLE Clipboard in a disposable user logon'
}
foreach ($id in $required.Keys) {
    $pattern = '(?ms)^      - name: ' + [regex]::Escape($required[$id]) + '\r?\n(?<step>.*?)(?=^      - |^  \w|\z)'
    $match = [regex]::Match($workflow, $pattern)
    $run = [regex]::Match($match.Groups['step'].Value, '(?ms)^        run: \|\r?\n(?<body>(?:          .*\r?\n|\r?\n)+)')
    if (!$match.Success -or !$run.Success) { throw "Missing required CI runtime matrix: $id" }
    $body = [regex]::Replace($run.Groups['body'].Value, '(?m)^          ', '').Replace("`r`n", "`n").TrimEnd() + "`n"
    [pscustomobject]@{ Id=$id; Name=$required[$id]; Command=$body; RequiresGitHubHosted=($id -eq 'clipboard-ole') }
}
