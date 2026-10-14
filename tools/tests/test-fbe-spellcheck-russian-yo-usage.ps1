[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe was not found: $FbeExe" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-small-dialogs.json') -Raw | ConvertFrom-Json
foreach ($key in @('fbe.settings.spelling.yo.option', 'fbe.settings.spelling.yo.help',
    'fbe.spelling.yo.warning', 'fbe.spelling.yo.word_label')) {
    $entry = $catalog.strings.PSObject.Properties[$key].Value
    if (-not $entry) { throw "Missing Russian ё localization: $key" }
    foreach ($language in $catalog.targetLanguages) {
        if ([string]::IsNullOrWhiteSpace($entry.translations.PSObject.Properties[$language].Value)) {
            throw "Missing Russian ё localization: $key / $language"
        }
    }
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-yo-usage-' + [guid]::NewGuid().ToString('N'))
try {
    Copy-Item -LiteralPath (Split-Path $FbeExe -Parent) -Destination $directory -Recurse -Force
    [IO.File]::WriteAllText((Join-Path $directory 'portable.ini'), "[Portable]`r`nDataPath=TestData`r`n", [Text.UTF8Encoding]::new($false))
    $fixture = Join-Path $directory 'russian-yo-usage.fb2'
    $report = Join-Path $directory 'yo-usage.txt'
    @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Russian yo usage</book-title><lang>ru</lang></title-info><document-info><id>russian-yo-usage</id><version>1.0</version></document-info></description><body><section><p>газёта дёлал ёжик</p></section></body></FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $originalHash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
    $custom = Join-Path $directory 'yo-custom.dic'
    [IO.File]::WriteAllText($custom, "газёта`n", [Text.Encoding]::GetEncoding(1251))
    $oldMode, $oldScenario, $oldCustom = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_YO_CUSTOM_PATH
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'spellcheck-russian-yo-usage'
        $env:FBE_NEXT_TEST_YO_CUSTOM_PATH = $custom
        $process = Start-Process -FilePath (Join-Path $directory 'FBE.exe') -ArgumentList @('--portable', '-b', $report, $fixture) -WorkingDirectory $directory -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $process.Id -Force
            throw 'FBE timed out during Russian ё usage runtime test.'
        }
        if ($process.ExitCode -ne 0) {
            $details = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<report missing>' }
            throw "Russian ё usage runtime test failed: exit $($process.ExitCode). $details"
        }
    }
    finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_YO_CUSTOM_PATH = $oldMode, $oldScenario, $oldCustom }
    $result = Get-Content -LiteralPath $report -Raw
    foreach ($key in @('legacy', 'detected', 'correct', 'ambiguous', 'marked', 'cleared', 'custom', 'ignored', 'unchanged')) {
        if ($result -notmatch "(?m)^$key=1$") { throw "Russian ё usage check failed ($key): $result" }
    }
    if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $originalHash) {
        throw 'Russian ё usage check changed the FB2 file.'
    }
    Write-Host 'Russian ё usage runtime regression passed.'
}
finally {
    $target = [IO.Path]::GetFullPath($directory)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($target.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($target).StartsWith('fbe-yo-usage-', [StringComparison]::Ordinal)) {
        Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction SilentlyContinue
    }
}
