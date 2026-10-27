[CmdletBinding()]
param(
    [ValidateSet("smoke", "standard", "full")]
    [string]$Profile = "smoke",

    [string[]]$Only,

    [string]$Output = (Join-Path $PSScriptRoot "data\prepared"),

    [string]$RncRoot,

    [switch]$SkipInstall
)

$ErrorActionPreference = "Stop"
$venv = Join-Path $PSScriptRoot ".venv"
$python = Join-Path $venv "Scripts\python.exe"

if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
    $launcher = Get-Command py -ErrorAction SilentlyContinue
    if ($launcher) {
        & $launcher.Source -3 -m venv $venv
    } else {
        $systemPython = Get-Command python -ErrorAction Stop
        & $systemPython.Source -m venv $venv
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Cannot create corpus virtual environment (exit code $LASTEXITCODE)."
    }
}
if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
    throw "Python is missing from the corpus virtual environment: $python"
}

if (-not $SkipInstall) {
    & $python -m pip install --upgrade pip
    if ($LASTEXITCODE -ne 0) {
        throw "Cannot upgrade pip in corpus virtual environment (exit code $LASTEXITCODE)."
    }
    & $python -m pip install -r (Join-Path $PSScriptRoot "requirements.txt")
    if ($LASTEXITCODE -ne 0) {
        throw "Cannot install corpus requirements (exit code $LASTEXITCODE)."
    }
}

$arguments = @(
    (Join-Path $PSScriptRoot "src\prepare_corpora.py"),
    "--manifest", (Join-Path $PSScriptRoot "corpora.json"),
    "--output", $Output,
    "--profile", $Profile
)
foreach ($corpusId in $Only) {
    $arguments += @("--only", $corpusId)
}
if ($RncRoot) {
    $arguments += @("--rnc-root", $RncRoot)
}

& $python @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Corpus preparation failed with exit code $LASTEXITCODE."
}
