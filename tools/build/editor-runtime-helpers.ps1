function Get-EditorDependencyVersion {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Name
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        if ($Name -eq "Lexilla") {
            throw "Lexilla submodule не инициализирован. Выполните: git submodule update --init --recursive"
        }
        throw "Не найден файл версии ${Name}: $Path."
    }

    $versionCode = (Get-Content -Raw -LiteralPath $Path).Trim()
    if ($versionCode -match '^\d{3}$') {
        return "{0}.{1}.{2}" -f $versionCode.Substring(0, 1), $versionCode.Substring(1, 1), $versionCode.Substring(2, 1)
    }
    if ($versionCode -match '^\d{4}$') {
        return "{0}.{1}.{2}" -f $versionCode.Substring(0, 1), $versionCode.Substring(1, 2), $versionCode.Substring(3, 1)
    }
    throw "Не удалось прочитать версию ${Name} из ${Path}: '$versionCode'."
}

function ConvertFrom-EditorRuntimeFingerprintJson {
    param([Parameter(Mandatory)][string]$Json)

    try {
        return $Json | ConvertFrom-Json -ErrorAction Stop
    }
    catch {
        return $null
    }
}

function Test-EditorRuntimeFingerprint {
    param(
        [object]$Fingerprint,
        [Parameter(Mandatory)][string]$CompatibilityTarget,
        [Parameter(Mandatory)][string]$PlatformToolset,
        [Parameter(Mandatory)][string]$VCToolsVersion,
        [Parameter(Mandatory)][string]$ScintillaVersion,
        [Parameter(Mandatory)][string]$LexillaVersion
    )

    if ($null -eq $Fingerprint -or
        $Fingerprint.compatibilityTarget -ne $CompatibilityTarget -or
        $Fingerprint.platformToolset -ne $PlatformToolset -or
        [string]::IsNullOrWhiteSpace([string]$Fingerprint.vcToolsVersion) -or
        $Fingerprint.vcToolsVersion -ne $VCToolsVersion -or
        [string]::IsNullOrWhiteSpace([string]$Fingerprint.scintillaVersion) -or
        $Fingerprint.scintillaVersion -ne $ScintillaVersion -or
        [string]::IsNullOrWhiteSpace([string]$Fingerprint.lexillaVersion) -or
        $Fingerprint.lexillaVersion -ne $LexillaVersion) {
        return $false
    }
    if ($CompatibilityTarget -eq "Win7" -and -not ([string]$Fingerprint.vcToolsVersion).StartsWith("14.44")) {
        return $false
    }
    return $true
}

function Test-SubmoduleCommitMatch {
    param(
        [string]$ExpectedCommit,
        [string]$ActualCommit
    )

    return -not [string]::IsNullOrWhiteSpace($ExpectedCommit) -and
        -not [string]::IsNullOrWhiteSpace($ActualCommit) -and
        $ExpectedCommit.Trim() -eq $ActualCommit.Trim()
}

function Assert-LexillaSubmoduleCheckout {
    param([Parameter(Mandatory)][string]$RepositoryRoot)

    $gitDirectory = Join-Path $RepositoryRoot ".git"
    if (-not (Test-Path -LiteralPath $gitDirectory)) {
        return
    }

    $git = Get-Command git.exe -ErrorAction SilentlyContinue
    if (-not $git) {
        Write-Warning "Git недоступен; проверка gitlink Lexilla пропущена."
        return
    }

    $expectedCommit = & $git.Source -C $RepositoryRoot rev-parse HEAD:third_party/lexilla 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "Не удалось прочитать gitlink Lexilla из текущего репозитория."
    }
    $actualCommit = & $git.Source -C (Join-Path $RepositoryRoot "third_party\lexilla") rev-parse HEAD 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "Lexilla submodule не инициализирован. Выполните: git submodule update --init --recursive"
    }
    if (-not (Test-SubmoduleCommitMatch -ExpectedCommit $expectedCommit -ActualCommit $actualCommit)) {
        throw "Lexilla submodule находится не на commit, зафиксированном текущим репозиторием. Выполните: git submodule update --init --recursive"
    }
}
