[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputPackage,
    [string]$BatchPath
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$BatchPath) { $BatchPath = Join-Path $root 'out/Release/ImportFB3Batch.exe' }
$source = (Resolve-Path -LiteralPath $InputPackage).Path
$batch = (Resolve-Path -LiteralPath $BatchPath).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'fb3-import-batch'
$passed = $false
try {
    $exe = Join-Path $scratch.Root 'ImportFB3Batch.exe'
    Copy-Item -LiteralPath $batch -Destination $exe
    foreach ($schema in 'FictionBook.xsd', 'FictionBookGenres.xsd', 'FictionBookLang.xsd', 'FictionBookLinks.xsd') {
        Copy-Item -LiteralPath (Join-Path $root "runtime/$schema") -Destination $scratch.Root
    }
    $inputRoot = Join-Path $scratch.Root 'input'
    $nested = New-Item -ItemType Directory -Path (Join-Path $inputRoot 'Подкаталог') -Force
    $inputFile = Join-Path $inputRoot 'Книга.FB3'
    $nestedFile = Join-Path $nested.FullName 'Книга.fb3'
    Copy-Item -LiteralPath $source -Destination $inputFile
    Copy-Item -LiteralPath $source -Destination $nestedFile
    [IO.File]::WriteAllBytes((Join-Path $inputRoot 'bad.fb3'), [byte[]]@(0x50,0x4b,3,4))
    $original = (Get-FileHash -LiteralPath $inputFile).Hash
    $nestedHash = (Get-FileHash -LiteralPath $nestedFile).Hash
    $single = Join-Path $scratch.Root 'single'
    & $exe $inputFile --output $single
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath (Join-Path $single 'Книга.fb2'))) { throw 'Single-file batch import failed.' }
    $output = Join-Path $scratch.Root 'output'
    $report = Join-Path $scratch.Root 'report'
    & $exe $inputRoot --output $output --recursive --report $report
    if ($LASTEXITCODE -ne 1) { throw 'Mixed batch must return partial failure.' }
    $summary = Get-Content -LiteralPath "$report.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($summary.converted -ne 2 -or $summary.failed -ne 1 -or $summary.warnings -lt 1 -or
        @($summary.files | Where-Object status -eq 'failed').error -notlike '*Invalid FB3 package*') {
        throw 'Mixed conversion counts or diagnostics are incorrect.'
    }
    if (!(Test-Path -LiteralPath "$report.txt") -or (Test-Path -LiteralPath (Join-Path $output 'bad.fb2'))) {
        throw 'Text report missing or invalid input produced a result.'
    }
    $rootBook = Join-Path $output 'Книга.fb2'
    $nestedBook = Join-Path $output 'Подкаталог/Книга.fb2'
    if ((Get-FileHash -LiteralPath $rootBook).Hash -ne (Get-FileHash -LiteralPath $nestedBook).Hash) {
        throw 'Matching source files produced different FB2 results.'
    }
    [IO.File]::WriteAllText($rootBook, 'KEEP THIS FILE')
    $report2 = Join-Path $scratch.Root 'rerun'
    & $exe $inputRoot --output $output --report $report2
    if ($LASTEXITCODE -ne 1 -or [IO.File]::ReadAllText($rootBook) -ne 'KEEP THIS FILE') { throw 'Batch overwrote an existing destination.' }
    $rerun = Get-Content -LiteralPath "$report2.json" -Raw | ConvertFrom-Json
    if ($rerun.converted -ne 0 -or $rerun.failed -ne 2 -or !(@($rerun.files.error) -like '*Output already exists*')) {
        throw 'Collision policy or non-recursive scope is incorrect.'
    }
    & $exe $inputFile
    if ($LASTEXITCODE -ne 2) { throw 'Invalid batch arguments must return exit code 2.' }
    if ((Get-FileHash -LiteralPath $inputFile).Hash -ne $original -or (Get-FileHash -LiteralPath $nestedFile).Hash -ne $nestedHash -or
        @(Get-ChildItem -LiteralPath $output -Recurse -Force -Filter '*.tmp').Count) { throw 'Batch modified its input or leaked a temporary file.' }
    $passed = $true
    'FB3 batch import tests passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed -PreserveContents
}
