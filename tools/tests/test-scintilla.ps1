[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

$testDir = Join-Path $repoRoot "out\tests"
$testExe = Join-Path $testDir "scintilla-smoke.exe"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

function Convert-ToUtf8Hex([string]$Text) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    return -join ($bytes | ForEach-Object { $_.ToString("x2") })
}

function Copy-FileWithRetry(
    [string]$Source,
    [string]$Destination,
    [int]$Attempts = 10,
    [int]$DelayMilliseconds = 200
) {
    for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
        try {
            Copy-Item -LiteralPath $Source -Destination $Destination -Force -ErrorAction Stop
            return
        }
        catch {
            if ($attempt -eq $Attempts) {
                throw
            }
            Start-Sleep -Milliseconds $DelayMilliseconds
        }
    }
}

& cl.exe /nologo /EHsc /std:c++17 /MT `
    "/I$(Join-Path $repoRoot "third_party\scintilla\include")" `
    "/Fo$(Join-Path $testDir "scintilla-smoke.obj")" `
    (Join-Path $PSScriptRoot "scintilla-smoke.cpp") `
    /link /SUBSYSTEM:CONSOLE user32.lib "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

function Invoke-IsolatedTest {
    param(
        [Parameter(Mandatory)]
        [string]$Name,

        [Parameter(Mandatory)]
        [string[]]$Libraries,

        [Parameter(Mandatory)]
        [int]$ExpectedExitCode,

        [string[]]$Arguments = @()
    )

    $caseDir = Join-Path $testDir $Name
    if (Test-Path -LiteralPath $caseDir) {
        Remove-Item -LiteralPath $caseDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $caseDir | Out-Null
    Copy-FileWithRetry -Source $testExe -Destination $caseDir
    foreach ($library in $Libraries) {
        Copy-FileWithRetry -Source (Join-Path $repoRoot "runtime\$library") `
            -Destination $caseDir
    }

    $process = Start-Process -FilePath (Join-Path $caseDir "scintilla-smoke.exe") `
        -ArgumentList $Arguments `
        -WorkingDirectory $caseDir -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne $ExpectedExitCode) {
        throw "$Name завершился с кодом $($process.ExitCode), ожидался код $ExpectedExitCode."
    }
}

$fixturesPath = Join-Path $PSScriptRoot "regex-fixtures.json"
$fixtures = Get-Content -Raw -LiteralPath $fixturesPath | ConvertFrom-Json

foreach ($case in $fixtures.scintilla.search) {
    $arguments = @(
        "search",
        (Convert-ToUtf8Hex ([string]$case.subject)),
        (Convert-ToUtf8Hex ([string]$case.pattern)),
        ([string]$(if ($null -ne $case.fromOffset) { $case.fromOffset } else { 0 })),
        ([string]$(if ([bool]$case.ignoreCase) { 1 } else { 0 })),
        ([string]$(if ($null -ne $case.expectedFound) { if ([bool]$case.expectedFound) { 1 } else { 0 } } else { 1 })),
        ([string]$(if ($null -ne $case.expectedStart) { $case.expectedStart } else { -1 })),
        ([string]$(if ($null -ne $case.expectedEnd) { $case.expectedEnd } else { -1 }))
    )

    Invoke-IsolatedTest -Name $case.id `
        -Libraries @("Scintilla.dll", "Lexilla.dll") `
        -ExpectedExitCode 0 `
        -Arguments $arguments
}

foreach ($case in $fixtures.scintilla.replace) {
    $arguments = @(
        "replace",
        (Convert-ToUtf8Hex ([string]$case.subject)),
        (Convert-ToUtf8Hex ([string]$case.pattern)),
        (Convert-ToUtf8Hex ([string]$case.replacement)),
        (Convert-ToUtf8Hex ([string]$case.expectedText)),
        ([string]$(if ($null -ne $case.fromOffset) { $case.fromOffset } else { 0 })),
        ([string]$(if ([bool]$case.ignoreCase) { 1 } else { 0 }))
    )

    Invoke-IsolatedTest -Name $case.id `
        -Libraries @("Scintilla.dll", "Lexilla.dll") `
        -ExpectedExitCode 0 `
        -Arguments $arguments

    if (-not [bool]$case.global) {
        $guiArguments = @(
            "replace-gui",
            (Convert-ToUtf8Hex ([string]$case.subject)),
            (Convert-ToUtf8Hex ([string]$case.pattern)),
            (Convert-ToUtf8Hex ([string]$case.replacement)),
            (Convert-ToUtf8Hex ([string]$case.expectedText)),
            ([string]$(if ($null -ne $case.fromOffset) { $case.fromOffset } else { 0 })),
            ([string]$(if ([bool]$case.ignoreCase) { 1 } else { 0 }))
        )

        Invoke-IsolatedTest -Name "$($case.id)-gui" `
            -Libraries @("Scintilla.dll", "Lexilla.dll") `
            -ExpectedExitCode 0 `
            -Arguments $guiArguments
    }
}

$dependencyProbe = $fixtures.scintilla.search | Select-Object -First 1
$dependencyArguments = @(
    "search",
    (Convert-ToUtf8Hex ([string]$dependencyProbe.subject)),
    (Convert-ToUtf8Hex ([string]$dependencyProbe.pattern)),
    "0",
    "0",
    "1",
    ([string]$dependencyProbe.expectedStart),
    ([string]$dependencyProbe.expectedEnd)
)

Invoke-IsolatedTest -Name "missing-lexilla" `
    -Libraries @("Scintilla.dll") -ExpectedExitCode 1 `
    -Arguments $dependencyArguments
Invoke-IsolatedTest -Name "missing-scintilla" `
    -Libraries @("Lexilla.dll") -ExpectedExitCode 1 `
    -Arguments $dependencyArguments

Write-Host "Smoke-тесты Scintilla, включая проверки отказа зависимостей, прошли успешно."
