[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$testRoot = Join-Path $root ('build\tests\dependency-pipeline-' + [Guid]::NewGuid().ToString('N'))
$buildScripts = Join-Path $testRoot 'tools\build'
$tests = Join-Path $testRoot 'tools\tests'
$hostPath = (Get-Process -Id $PID).Path
New-Item -ItemType Directory -Force -Path $buildScripts, $tests | Out-Null
foreach ($name in 'update-third-party-dependency.ps1', 'apply-third-party-update-and-test.ps1', 'apply-third-party-update.ps1') {
    Copy-Item -LiteralPath (Join-Path $root "tools\build\$name") -Destination $buildScripts
}
# Run the real orchestration/apply scripts against an isolated synthetic source
# and deterministic Git/build adapters. No network or real dependency is mutated.
@'
$global:LASTEXITCODE = 0
function Get-ThirdPartyText([string]$Base64) { [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($Base64)) }
function Format-ThirdPartyText([string]$Base64, [Parameter(ValueFromRemainingArguments)]$Arguments) { (Get-ThirdPartyText $Base64) -f $Arguments }
function Get-DependencyStatusDisplay([string]$Status) { $Status }
function Get-ThirdPartyRepoRoot { (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path }
function Get-DependencyEntry([string]$Name) {
    [pscustomobject]@{Name=$Name; DisplayName=$Name; Kind='GitSubmodule'; UpdateMode='GitSubmodule';
        RelativePath="third_party/$Name"; LocalPath=(Join-Path (Get-ThirdPartyRepoRoot) "third_party/$Name")}
}
function Get-DependencyUpdateInfo($Dependency) {
    [pscustomobject]@{Status=$env:FBE_PIPE_STATUS; LocalVersion='1.0.0'; RemoteVersion='1.0.1';
        LocalCommit=('1' * 40); RemoteCommit=('2' * 40); RemoteTag='v1.0.1'; RemoteSource='synthetic'}
}
function Invoke-GitCapture([string]$WorkingDirectory, [string[]]$Arguments) {
    Add-Content -LiteralPath $env:FBE_PIPE_LOG -Value ("git|$WorkingDirectory|" + ($Arguments -join ' '))
    switch ($Arguments[0]) {
        'status' { if ($env:FBE_PIPE_DIRTY -eq '1') { ' M user-change.cpp' } }
        'rev-parse' { if ($script:updatedHead) { $script:updatedHead } else { $env:FBE_PIPE_HEAD } }
        'checkout' { $script:updatedHead = '2' * 40 }
        'fetch' { }
        default { throw "Unexpected Git operation: $Arguments" }
    }
}
'@ | Set-Content -LiteralPath (Join-Path $buildScripts 'ThirdPartySources.ps1') -Encoding utf8
@'
param($Configuration, $PlatformToolset)
Add-Content -LiteralPath $env:FBE_PIPE_LOG -Value "build|$Configuration|$PlatformToolset"
exit ([int]$env:FBE_PIPE_BUILD_EXIT)
'@ | Set-Content -LiteralPath (Join-Path $buildScripts 'build-libheif.ps1') -Encoding utf8
@'
param($Configuration, $PlatformToolset)
Add-Content -LiteralPath $env:FBE_PIPE_LOG -Value "test|$Configuration|$PlatformToolset"
exit ([int]$env:FBE_PIPE_TEST_EXIT)
'@ | Set-Content -LiteralPath (Join-Path $tests 'test-image-import-native.ps1') -Encoding utf8
$variables = @('FBE_PIPE_LOG', 'FBE_PIPE_STATUS', 'FBE_PIPE_HEAD', 'FBE_PIPE_DIRTY', 'FBE_PIPE_BUILD_EXIT', 'FBE_PIPE_TEST_EXIT')
$saved = @{}
foreach ($name in $variables) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
$passed = 0
try {
    foreach ($dependency in 'libheif', 'aom') {
        $directory = Join-Path $testRoot "third_party/$dependency"
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
        Set-Content -LiteralPath (Join-Path $directory '.git') -Value 'synthetic marker'
    }
    foreach ($case in @(
        @{Name='libheif apply/build/test'; Script='apply-third-party-update-and-test.ps1'; Builds=1; Checkout=1},
        @{Name='aom shared route'; Script='apply-third-party-update-and-test.ps1'; Dependency='aom'; Builds=1; Checkout=1},
        @{Name='SkipApply'; Script='apply-third-party-update-and-test.ps1'; Extra='-SkipApply'; Builds=1; Checkout=0},
        @{Name='WhatIf'; Script='apply-third-party-update-and-test.ps1'; Extra='-WhatIf'; Builds=0; Checkout=0},
        @{Name='already updated HEAD'; Script='apply-third-party-update-and-test.ps1'; Head=('2' * 40); Builds=1; Checkout=0},
        @{Name='dirty source'; Script='apply-third-party-update-and-test.ps1'; Dirty='1'; Exit=1; Builds=0; Checkout=0},
        @{Name='unindexed local commit'; Script='apply-third-party-update-and-test.ps1'; Head=('3' * 40); Exit=1; Builds=0; Checkout=0},
        @{Name='newer local version'; Script='apply-third-party-update-and-test.ps1'; Status='LocalNewer'; Exit=1; Builds=0; Checkout=0},
        @{Name='review required'; Script='apply-third-party-update-and-test.ps1'; Status='NeedsReview'; Exit=1; Builds=0; Checkout=0},
        @{Name='full route newer local version'; Script='update-third-party-dependency.ps1'; Status='LocalNewer'; Exit=1; Builds=0; Checkout=0},
        @{Name='full WhatIf newer local version'; Script='update-third-party-dependency.ps1'; Status='LocalNewer'; Extra='-WhatIf'; Exit=1; Builds=0; Checkout=0},
        @{Name='build failure'; Script='apply-third-party-update-and-test.ps1'; Extra='-SkipApply'; BuildExit='23'; Exit=23; Builds=1; Tests=0; Checkout=0},
        @{Name='test failure'; Script='apply-third-party-update-and-test.ps1'; Extra='-SkipApply'; TestExit='29'; Exit=29; Builds=1; Checkout=0},
        @{Name='full libheif route'; Script='update-third-party-dependency.ps1'; Builds=1; Checkout=1},
        @{Name='full WhatIf'; Script='update-third-party-dependency.ps1'; Extra='-WhatIf'; Builds=0; Checkout=0},
        @{Name='current build only'; Script='update-third-party-dependency.ps1'; Status='UpToDate'; Extra='-AllowCurrentVersion'; Builds=1; Checkout=0},
        @{Name='current no-op'; Script='update-third-party-dependency.ps1'; Status='UpToDate'; Builds=0; Checkout=0}
    )) {
        $env:FBE_PIPE_LOG = Join-Path $testRoot 'calls.log'
        [IO.File]::WriteAllText($env:FBE_PIPE_LOG, '')
        $env:FBE_PIPE_STATUS = if ($case.Status) { $case.Status } else { 'UpdateAvailable' }
        $env:FBE_PIPE_HEAD = if ($case.Head) { $case.Head } else { '1' * 40 }
        $env:FBE_PIPE_DIRTY = if ($case.Dirty) { $case.Dirty } else { '0' }
        $env:FBE_PIPE_BUILD_EXIT = if ($case.BuildExit) { $case.BuildExit } else { '0' }
        $env:FBE_PIPE_TEST_EXIT = if ($case.TestExit) { $case.TestExit } else { '0' }
        $dependency = if ($case.Dependency) { $case.Dependency } else { 'libheif' }
        $command = "& '$(Join-Path $buildScripts $case.Script)' -Dependency $dependency -Configuration Release -Confirm:`$false $($case.Extra); exit `$LASTEXITCODE"
        & $hostPath -NoProfile -Command $command *> (Join-Path $testRoot 'output.log')
        $actualExit = $LASTEXITCODE
        $expectedExit = if ($case.Exit) { $case.Exit } else { 0 }
        $calls = @(Get-Content -LiteralPath $env:FBE_PIPE_LOG)
        $expectedTests = if ($case.ContainsKey('Tests')) { $case.Tests } else { $case.Builds }
        if ($actualExit -ne $expectedExit -or @($calls | Where-Object { $_ -eq 'build|Release|v143' }).Count -ne $case.Builds -or
            @($calls | Where-Object { $_ -eq 'test|Release|v143' }).Count -ne $expectedTests -or
            @($calls | Where-Object { $_ -like '*|checkout *' }).Count -ne $case.Checkout -or
            @($calls | Where-Object { $_ -like '*|submodule update*' }).Count) {
            Get-Content -LiteralPath (Join-Path $testRoot 'output.log') | Write-Host
            throw "Pipeline regression: $($case.Name); exit=$actualExit; calls=$($calls -join ', ')"
        }
        if (@($calls | Where-Object { $_ -like 'git|*' -and $_ -notlike "git|*third_party*$dependency|*" }).Count) {
            throw "Unexpected dependency mutation in $($case.Name)"
        }
        $passed++
        Write-Host "PASS: $($case.Name)"
    }
}
finally {
    foreach ($name in $variables) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
    $safeBuildRoot = [IO.Path]::GetFullPath((Join-Path $root 'build')) + [IO.Path]::DirectorySeparatorChar
    if ([IO.Path]::GetFullPath($testRoot).StartsWith($safeBuildRoot, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
Write-Host "Third-party update pipeline: $passed synthetic cases passed."
