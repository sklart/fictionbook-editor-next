[CmdletBinding()]
param(
    [switch]$IncludePrimeDiagnostic,
    [switch]$RequireLiveShellRegistration
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$dumpDir = Join-Path $repoRoot "out\tests\fb2-shell-thumbnail-matrix"
New-Item -ItemType Directory -Path $dumpDir -Force | Out-Null

$thumbnailShellExGuid = "{e357fccd-a995-4576-b01f-234630154e96}"
$thumbnailProviderClsid = "{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}"

function Invoke-ExpectedFailure {
    param(
        [Parameter(Mandatory)]
        [string]$Name,

        [Parameter(Mandatory)]
        [scriptblock]$Action
    )

    try {
        & $Action
    }
    catch {
        Write-Host "Ожидаемый отказ [$Name]: $($_.Exception.Message)"
        return
    }

    throw "Сценарий '$Name' неожиданно завершился успехом, хотя ожидался отказ."
}

function Test-RegistryValueEquals {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$ExpectedValue
    )

    try {
        $value = (Get-ItemProperty -LiteralPath $Path -ErrorAction Stop)."(default)"
        return [string]::Equals($value, $ExpectedValue, [System.StringComparison]::OrdinalIgnoreCase)
    }
    catch {
        return $false
    }
}

function Test-LiveThumbnailRegistration {
    $shellExPaths = @(
        "HKLM:\SOFTWARE\Classes\.fb2\ShellEx\$thumbnailShellExGuid",
        "HKCU:\Software\Classes\.fb2\ShellEx\$thumbnailShellExGuid",
        "HKLM:\SOFTWARE\Classes\FictionBook.2\ShellEx\$thumbnailShellExGuid",
        "HKCU:\Software\Classes\FictionBook.2\ShellEx\$thumbnailShellExGuid"
    )

    $clsidPaths = @(
        "HKLM:\SOFTWARE\Classes\CLSID\$thumbnailProviderClsid\InprocServer32",
        "HKCU:\Software\Classes\CLSID\$thumbnailProviderClsid\InprocServer32"
    )

    $hasShellEx = $false
    foreach ($path in $shellExPaths) {
        if (Test-RegistryValueEquals -Path $path -ExpectedValue $thumbnailProviderClsid) {
            $hasShellEx = $true
            break
        }
    }

    $hasClsid = $false
    foreach ($path in $clsidPaths) {
        if (Test-Path -LiteralPath $path) {
            $hasClsid = $true
            break
        }
    }

    return ($hasShellEx -and $hasClsid)
}

$providerSmoke = Join-Path $PSScriptRoot "test-fb2-thumbnail-provider.ps1"
$cocreateSmoke = Join-Path $PSScriptRoot "test-fb2-thumbnail-provider-cocreate.ps1"
$shellSmoke = Join-Path $PSScriptRoot "test-fb2-shell-thumbnail.ps1"
$shellDump = Join-Path $PSScriptRoot "test-fb2-shell-thumbnail-dump.ps1"
$primeSmoke = Join-Path $PSScriptRoot "test-fb2-shell-thumbnail-prime.ps1"

$shellApiCases = @(
    @{ Name = "shell-api png tiny"; File = "tools\tests\fb2-cover-smoke.fb2" },
    @{ Name = "shell-api png visible"; File = "tools\tests\fb2-cover-visible-smoke.fb2" },
    @{ Name = "shell-api jpeg"; File = "tools\tests\fb2-cover-jpeg-smoke.fb2" },
    @{ Name = "shell-api bmp"; File = "tools\tests\fb2-cover-bmp-smoke.fb2" }
)

$forcedPositiveCases = @(
    @{ Name = "forced png visible"; File = "tools\tests\fb2-cover-visible-smoke.fb2"; Dump = "visible-force.bmp" },
    @{ Name = "forced jpeg"; File = "tools\tests\fb2-cover-jpeg-smoke.fb2"; Dump = "jpeg-force.bmp" },
    @{ Name = "forced bmp"; File = "tools\tests\fb2-cover-bmp-smoke.fb2"; Dump = "bmp-force.bmp" }
)

$forcedNegativeCases = @(
    @{ Name = "forced broken cover"; File = "tools\tests\fb2-cover-broken.fb2"; Dump = "broken-force.bmp" },
    @{ Name = "forced missing coverpage"; File = "tools\tests\fb2-cover-missing-coverpage.fb2"; Dump = "missing-coverpage-force.bmp" },
    @{ Name = "forced missing binary"; File = "tools\tests\fb2-cover-missing-binary.fb2"; Dump = "missing-binary-force.bmp" }
)

Write-Host "Шаг 1/5. Прямой COM smoke thumbnail provider."
& $providerSmoke

if (-not (Test-LiveThumbnailRegistration)) {
    $message = "Live shell-регистрация thumbnail provider сейчас отсутствует: пропускаем системные shell-проверки и считаем прогон implementation-only."
    if ($RequireLiveShellRegistration) {
        throw $message
    }

    Write-Warning $message

    if ($IncludePrimeDiagnostic) {
        Write-Warning "Prime-диагностика тоже пропущена, потому что нет live shell-регистрации."
    }

    Write-Host "Сводный shell thumbnail matrix smoke завершён в режиме implementation-only."
    return
}

Write-Host "Шаг 2/5. Системная COM-активация thumbnail provider."
& $cocreateSmoke -Platform x64

Write-Host "Шаг 3/5. Shell API thumbnail smoke на валидных fixture."
foreach ($case in $shellApiCases) {
    Write-Host ("  - {0}" -f $case.Name)
    & $shellSmoke -FilePath $case.File
}

Write-Host "Шаг 4/5. Принудительное извлечение thumbnail через IThumbnailCache на валидных fixture."
foreach ($case in $forcedPositiveCases) {
    $dumpPath = Join-Path $dumpDir $case.Dump
    Write-Host ("  - {0}" -f $case.Name)
    & $shellDump `
        -Fb2Path (Join-Path $repoRoot $case.File) `
        -OutputBmp $dumpPath `
        -ForceExtraction `
        -ExtractDoNotCache `
        -ExtractInProc
}

Write-Host "Шаг 5/5. Негативные forced-сценарии должны мягко отказывать без thumbnail."
foreach ($case in $forcedNegativeCases) {
    $dumpPath = Join-Path $dumpDir $case.Dump
    Invoke-ExpectedFailure -Name $case.Name -Action {
        & $shellDump `
            -Fb2Path (Join-Path $repoRoot $case.File) `
            -OutputBmp $dumpPath `
            -ForceExtraction `
            -ExtractDoNotCache `
            -ExtractInProc
    }
}

if ($IncludePrimeDiagnostic) {
    Write-Host "Дополнительная диагностика. Prime thumbnail cache для visible fixture."
    try {
        & $primeSmoke -FilePath "tools\tests\fb2-cover-visible-smoke.fb2"
    }
    catch {
        Write-Warning ("Prime-диагностика завершилась неуспешно: {0}" -f $_.Exception.Message)
    }
}

Write-Host "Сводный shell thumbnail matrix smoke прошёл успешно."
