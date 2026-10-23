[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$out = Join-Path $root ('out\tests\deployment-context\' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out -Force | Out-Null
$exe = Join-Path $out 'deployment-context-test.exe'
& cl.exe /nologo /EHsc /std:c++17 /MT /O2 /GL /W4 /WX /utf-8 /Zc:wchar_t- /DUNICODE /D_UNICODE /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 `
    ("/Fo" + (Join-Path $out 'harness.obj')) "/Fe$exe" (Join-Path $PSScriptRoot 'deployment-context-test.cpp') `
    /link /LTCG /SUBSYSTEM:CONSOLE,6.01 shell32.lib ole32.lib user32.lib
if ($LASTEXITCODE -ne 0) { throw 'DeploymentContext native test compilation failed.' }
$header = (Join-Path $root 'src\common\DeploymentContext.h').Replace('\', '/')
$probe = Join-Path $out 'analyze.cpp'
[IO.File]::WriteAllText($probe, "#include `"$header`"`r`nvoid ExerciseDeploymentPaths() { (void)DeploymentContext::ExecutableDirectory(); (void)DeploymentContext::TestSettingsDirectory(); (void)DeploymentContext::DataRoot(); }`r`n", [Text.UTF8Encoding]::new($false))
$rules = Join-Path $env:VSINSTALLDIR 'Team Tools\Static Analysis Tools\Rule Sets\NativeRecommendedRules.ruleset'
& cl.exe /nologo /c /EHsc /std:c++17 /MT /O2 /W4 /WX /utf-8 /Zc:wchar_t- /DUNICODE /D_UNICODE /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 `
    /analyze /analyze:external- /external:env:INCLUDE /external:W0 /analyze:log:format:sarif `
    "/analyze:ruleset$rules" /analyze:log (Join-Path $out 'analyze.sarif') ("/Fo" + (Join-Path $out 'analyze.obj')) $probe
if ($LASTEXITCODE -ne 0) { throw 'DeploymentContext analysis must pass without warnings.' }
$analysis = Get-Content -Raw -LiteralPath (Join-Path $out 'analyze.sarif') | ConvertFrom-Json
$findings = @($analysis.runs | ForEach-Object { $_.results } | Where-Object { $null -ne $_ })
if ($findings.Count -ne 0) { throw "DeploymentContext analysis has $($findings.Count) finding(s)." }
& $exe 2>&1 | Tee-Object -FilePath (Join-Path $out 'result.txt')
$code = $LASTEXITCODE
@{ ExitCode=$code; VCToolsVersion=$env:VCToolsVersion; HeaderSHA256=(Get-FileHash (Join-Path $root 'src\common\DeploymentContext.h')).Hash } |
    ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
Write-Host "DeploymentContext test artifacts: $out"
if ($code -ne 0) { throw "DeploymentContext tests failed: $code" }
