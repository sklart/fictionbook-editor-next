[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$Platform = 'Win32',
    [string]$Target = (Join-Path $PSScriptRoot '..\..\FBE.sln'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\..\out\code-audit'),
    [ValidateRange(0, 64)]
    [int]$Threads = 0,
    [switch]$Intermodular,
    [switch]$FailOnFindings
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $PSScriptRoot 'Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$Target = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Target)
$OutputDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)

if (-not (Test-Path -LiteralPath $Target -PathType Leaf)) {
    throw "Не найден target: $Target"
}

$programFilesX86 = ${env:ProgramFiles(x86)}
$toolCandidates = @(
    $env:FBE_CODE_AUDIT_COMMAND,
    (Join-Path $programFilesX86 'PVS-Studio\PVS-Studio_Cmd.exe'),
    'PVS-Studio_Cmd.exe'
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

$tool = $null
foreach ($candidate in $toolCandidates) {
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $tool = (Resolve-Path -LiteralPath $candidate).Path
        break
    }
    $command = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($command) {
        $tool = $command.Source
        break
    }
}
if ($null -eq $tool) {
    throw 'Не найден локально установленный инструмент анализа.'
}

$converter = Join-Path (Split-Path -Parent $tool) 'PlogConverter.exe'
if (-not (Test-Path -LiteralPath $converter -PathType Leaf)) {
    throw "Не найден конвертер отчётов: $converter"
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$reportBase = Join-Path $OutputDirectory ("solution-{0}-{1}" -f $Configuration, $Platform)
$report = "$reportBase.plog"
$arguments = @(
    '--target', $Target,
    '--platform', $Platform,
    '--configuration', $Configuration,
    '--output', $report,
    '--sourceTreeRoot', $repositoryRoot,
    '--ignoreGlobalRulesConfig',
    '--progress'
)
if ($Threads -gt 0) {
    $arguments += '--threads', $Threads
}
if ($Intermodular) {
    $arguments += '--intermodular'
}

& $tool @arguments
$analysisExitCode = $LASTEXITCODE
$analysisFailure = (($analysisExitCode -band 255) -ne 0)
if ((-not (Test-Path -LiteralPath $report -PathType Leaf)) -or (($analysisExitCode -band 128) -ne 0)) {
    throw "Анализ не сформировал пригодный отчёт, код $analysisExitCode."
}

$reportName = Split-Path -Leaf $reportBase

& $converter --outputDir $OutputDirectory --srcRoot $repositoryRoot --pathTransformationMode toRelative --renderTypes 'Totals,JSON,Markdown' --outputNameTemplate $reportName $report
if ($LASTEXITCODE -ne 0) {
    throw "Преобразование отчёта завершилось с кодом $LASTEXITCODE."
}

& $converter --outputDir $OutputDirectory --srcRoot $repositoryRoot --pathTransformationMode toAbsolute --renderTypes 'FullHtml' --outputNameTemplate "$reportName-html" $report
if ($LASTEXITCODE -ne 0) {
    throw "Преобразование HTML-отчёта завершилось с кодом $LASTEXITCODE."
}

if ($analysisFailure) {
    throw "Анализ сформировал частичный отчёт, код $analysisExitCode."
}

if ($FailOnFindings -and (($analysisExitCode -band 256) -ne 0)) {
    throw "Анализ выявил замечания, код $analysisExitCode."
}

Write-Host "Отчёты сохранены в: $OutputDirectory"
