[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$workflow = Get-Content -Raw -LiteralPath (Join-Path $root '.github\workflows\build.yml')
$candidate = Get-Content -Raw -LiteralPath (Join-Path $root 'tools\build\new-update-manifest-candidate.ps1')

function Require([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
Require ($candidate.Contains('[string]$ReleaseTag')) 'Candidate manifest обязан принимать ReleaseTag.'
Require ((Get-Content -Raw -LiteralPath (Join-Path $root 'tools\build\build.ps1')).Contains('/p:FbeReleaseVersion=$ReleaseVersion')) 'Основная сборка обязана передавать полную release version в buildstamp.'
Require ($workflow.Contains('$arguments.ReleaseVersion = $env:GITHUB_REF_NAME.Substring(1)')) 'Tagged CI build обязан передавать full release version до компиляции FBE.exe.'
Require ($candidate.Contains('releases/download/$ReleaseTag')) 'Artifact URL обязан использовать ReleaseTag.'
Require (-not $candidate.Contains('ReleaseNotes')) 'Candidate manifest не должен копировать Release Notes.'
Require ($workflow.Contains('if: github.ref_type == ''tag''')) 'Candidate manifest обязан создаваться для каждого release tag.'
Require ($workflow.Contains('update-prerelease.xml')) 'Workflow обязан публиковать prerelease feed.'
Require ($workflow.Contains("@('update.xml', 'update-prerelease.xml')")) 'Stable release обязан публиковать оба feed.'
$existing = $workflow.IndexOf('gh release view $releaseTag')
$create = $workflow.IndexOf('$arguments = @("release", "create", $releaseTag, "--verify-tag", "--notes-file"')
Require ($existing -ge 0 -and $create -gt $existing) 'Workflow обязан различать existing и new GitHub Release.'
$existingBlock = $workflow.Substring($existing, $create - $existing)
Require (-not $existingBlock.Contains('--notes-file')) 'Повторная публикация не должна перезаписывать GitHub Release body.'
Require (-not $existingBlock.Contains('$arguments = @("release", "edit", $releaseTag)')) 'Existing stable release не должен вызывать пустой gh release edit.'
Require ($existingBlock.Contains('gh release edit $releaseTag --prerelease=true')) 'Existing prerelease должен сохранять prerelease flag.'
Require ($workflow.IndexOf('gh release upload $releaseTag', $create) -gt $create) 'Assets должны загружаться после создания release.'
Require ($workflow.IndexOf('Synchronize update manifests') -gt $workflow.IndexOf('gh release upload $releaseTag')) 'Feed должен обновляться только после upload assets.'
Write-Host 'Проверка update/release pipeline прошла успешно.'
