[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$python = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"
$pythonArguments = @()
if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
    # The VS dev environment can prepend MSYS Python, which cannot reliably
    # resolve Windows test paths. Prefer the Windows launcher when no project
    # virtual environment has been created.
    $launcher = Get-Command py.exe -ErrorAction SilentlyContinue
    if ($launcher) {
        $python = $launcher.Source
        $pythonArguments = @('-3')
    }
    else {
        $pythonCommand = Get-Command python -All | Where-Object {
            $_.Source -notmatch '[\\/]devkitPro[\\/]msys2[\\/]'
        } | Select-Object -First 1
        if (-not $pythonCommand) {
            throw 'Windows Python was not found. Create spellcheck-corpus/.venv or install Python.'
        }
        $python = $pythonCommand.Source
    }
}

& $python @pythonArguments -m unittest discover -s (Join-Path $PSScriptRoot "tests") -v
if ($LASTEXITCODE -ne 0) {
    throw "Spellcheck corpus tests failed with exit code $LASTEXITCODE."
}
