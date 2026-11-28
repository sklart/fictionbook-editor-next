[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputPackage,
    [string]$RuntimeRoot
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$RuntimeRoot) { $RuntimeRoot = Join-Path $root 'out/Release' }
$inputPackagePath = (Resolve-Path -LiteralPath $InputPackage).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'fb3-export-batch'
$passed = $false
try {
    foreach ($executable in 'ImportFB3Batch.exe', 'ExportFB3Batch.exe') {
        Copy-Item -LiteralPath (Join-Path $RuntimeRoot $executable) -Destination $scratch.Root
    }
    foreach ($schema in 'FictionBook.xsd', 'FictionBookGenres.xsd', 'FictionBookLang.xsd', 'FictionBookLinks.xsd', 'FB3Schemas') {
        Copy-Item -LiteralPath (Join-Path $root "runtime/$schema") -Destination $scratch.Root -Recurse
    }
    $import = Join-Path $scratch.Root 'ImportFB3Batch.exe'
    $export = Join-Path $scratch.Root 'ExportFB3Batch.exe'
    $inputDirectory = Join-Path $scratch.Root 'input'
    $outputDirectory = Join-Path $scratch.Root 'output'
    $inputCopy = Join-Path $scratch.Root 'Книга.fb3'
    Copy-Item -LiteralPath $inputPackagePath -Destination $inputCopy
    & $import $inputCopy --output $inputDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare a source FB2.' }
    $source = Join-Path $inputDirectory 'Книга.fb2'
    $sourceHash = (Get-FileHash -LiteralPath $source).Hash
    [IO.File]::WriteAllText((Join-Path $inputDirectory 'Повреждённая.fb2'), '<FictionBook>')
    $report = Join-Path $scratch.Root 'export'
    & $export $inputDirectory --output $outputDirectory --report $report
    if ($LASTEXITCODE -ne 1) { throw 'Mixed FB3 export did not return partial failure.' }
    $summary = Get-Content -LiteralPath "$report.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($summary.converted -ne 1 -or $summary.failed -ne 1 -or !(Test-Path -LiteralPath "$report.txt") -or
        (Test-Path -LiteralPath (Join-Path $outputDirectory 'Повреждённая.fb3'))) { throw 'FB3 export batch results are incorrect.' }
    $output = Join-Path $outputDirectory 'Книга.fb3'
    $outputHash = (Get-FileHash -LiteralPath $output).Hash
    & $export $source --output $outputDirectory
    if ($LASTEXITCODE -ne 1 -or (Get-FileHash -LiteralPath $output).Hash -ne $outputHash) { throw 'Batch overwrote an existing FB3.' }
    $reimportDirectory = Join-Path $scratch.Root 'reimported'
    & $import $output --output $reimportDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Could not reimport the generated FB3.' }
    [xml]$before = Get-Content -LiteralPath $source -Raw -Encoding UTF8
    [xml]$after = Get-Content -LiteralPath (Join-Path $reimportDirectory 'Книга.fb2') -Raw -Encoding UTF8
    if ((@($before.FictionBook.body | ForEach-Object InnerText) -join '') -cne
        (@($after.FictionBook.body | ForEach-Object InnerText) -join '') -or
        (Get-FileHash -LiteralPath $source).Hash -ne $sourceHash -or
        @(Get-ChildItem -LiteralPath $outputDirectory -Force -Filter '*.tmp').Count) {
        throw 'FB3 export changed the text or source, or left temporary files.'
    }
    $passed = $true
    'FB3 batch export tests passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed -PreserveContents
}
