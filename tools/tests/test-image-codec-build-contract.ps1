param([string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))

$ErrorActionPreference = 'Stop'
$resolver = Get-Content -LiteralPath (Join-Path $RepoRoot 'tools\build\Resolve-VsCmake.ps1') -Raw
if ($resolver -notmatch "'-version', '\[17\.0,18\.0\)'" -or $resolver -notmatch "Generator = 'Visual Studio 17 2022'") {
    throw 'Resolve-VsCmake должен выбирать установленную Visual Studio 2022 для генератора Visual Studio 17 2022.'
}
if ($resolver -notmatch 'Для PlatformToolset v143 требуется установленная Visual Studio 2022') {
    throw 'Для v143 должна быть понятная ошибка при отсутствии Visual Studio 2022.'
}
if ($resolver -match 'hosted by a newer Visual Studio') {
    throw 'Нельзя брать CMake от другой major-версии Visual Studio для генератора VS2022.'
}
Write-Host 'Контракт выбора Visual Studio/CMake для image codecs проверен.'
