[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$HunspellExe,

    [Parameter(Mandatory)]
    [string]$CurrentDictionary,

    [Parameter(Mandatory)]
    [string]$CandidateDictionary,

    [string]$Prepared = (Join-Path $PSScriptRoot "data\prepared"),

    [string]$Output = (Join-Path $PSScriptRoot "results"),

    [string]$CurrentName = "current",

    [string]$CandidateName = "candidate",

    [int]$SuggestionLimit = 5000
)

$ErrorActionPreference = "Stop"
$python = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"
if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
    throw "Virtual environment not found. Run prepare-corpora.ps1 first."
}

& $python (Join-Path $PSScriptRoot "src\compare_dictionaries.py") `
    --prepared $Prepared `
    --hunspell-exe $HunspellExe `
    --dictionary "$CurrentName=$CurrentDictionary" `
    --dictionary "$CandidateName=$CandidateDictionary" `
    --suggestion-limit $SuggestionLimit `
    --output $Output

if ($LASTEXITCODE -ne 0) {
    throw "Dictionary comparison failed with exit code $LASTEXITCODE."
}

& $python (Join-Path $PSScriptRoot "src\make_fb2_review.py") `
    --disagreements (Join-Path $Output "dictionary-disagreements.csv") `
    --typo-benchmark (Join-Path $Output "typo-benchmark.csv") `
    --annotated-disagreements (Join-Path $Output "annotated-disagreements.csv") `
    --output (Join-Path $Output "spellcheck-review.fb2")

if ($LASTEXITCODE -ne 0) {
    throw "FB2 review document generation failed with exit code $LASTEXITCODE."
}
