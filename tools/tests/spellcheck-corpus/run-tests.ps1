[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$python = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"
if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
    $pythonCommand = Get-Command python -ErrorAction Stop
    $python = $pythonCommand.Source
}

& $python -m unittest discover -s (Join-Path $PSScriptRoot "tests") -v
if ($LASTEXITCODE -ne 0) {
    throw "Spellcheck corpus tests failed with exit code $LASTEXITCODE."
}
