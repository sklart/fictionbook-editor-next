<# Verify bounded fallback and exact UTF-8 Source content for long malformed input. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [int]$TimeoutSeconds = 30, [switch]$KeepArtifacts)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_MALFORMED_SOURCE_PATH','FBE_NEXT_TEST_SOURCE_DUMP')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
$utf8 = [Text.UTF8Encoding]::new($false)
$emoji = [char]::ConvertFromUtf32(0x1F642)
$valid = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Test</book-title><lang>en</lang></title-info><document-info><author><nickname>Test</nickname></author><date value="2026-01-01">2026</date><id>x</id><version>1.0</version></document-info></description><body><section><p>Valid</p></section></body></FictionBook>'
$cases = @(
    @{Name='utf8-separated'; Declaration='<?xml version="1.0" encoding="utf-8"?>' + "`n"; Fill=70000; Unicode="Ж$emoji"; Encoding='utf-8'; Bom=$false},
    @{Name='utf8-minified'; Declaration='<?xml version="1.0" encoding="utf-8"?>'; Fill=70000; Unicode="Ж$emoji"; Encoding='utf-8'; Bom=$false},
    @{Name='utf8-bom'; Declaration='<?xml version="1.0" encoding="utf-8"?>'; Fill=65535; Unicode="Ж$emoji"; Encoding='utf-8'; Bom=$true},
    @{Name='cp1251-minified'; Declaration='<?xml version="1.0" encoding="windows-1251"?>'; Fill=70000; Unicode='Русский'; Encoding='windows-1251'; Bom=$false},
    @{Name='no-declaration'; Declaration=''; Fill=65534; Unicode="Ж$emoji"; Encoding='utf-8'; Bom=$false}
)
try {
    foreach ($case in $cases) {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'malformed-longline'
        $passed = $false
        try {
            $null = New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts')
            $good = Join-Path $iso.Root 'valid.fb2'; $bad = Join-Path $iso.Root 'malformed.fb2'
            $report = Join-Path $iso.Root 'report.txt'; $dump = Join-Path $iso.Root 'source.utf8'
            [IO.File]::WriteAllText($good, $valid, $utf8)
            $payload = '<FictionBook><body><section><p>START-' + ('X' * $case.Fill) + '-' + $case.Unicode + '-END</p>'
            $encoding = if ($case.Encoding -eq 'utf-8') { [Text.UTF8Encoding]::new($case.Bom) } else { [Text.Encoding]::GetEncoding(1251) }
            [IO.File]::WriteAllText($bad, $case.Declaration + $payload, $encoding)
            $before = (Get-FileHash -LiteralPath $bad -Algorithm SHA256).Hash
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'malformed-longline-runtime'
            $env:FBE_NEXT_TEST_MALFORMED_SOURCE_PATH = $bad; $env:FBE_NEXT_TEST_SOURCE_DUMP = $dump
            $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',('"' + $report + '"'),('"' + $good + '"')) -PassThru
            $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
            $state = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing>' }
            if ($code -ne 0 -or $state -notmatch '(?m)^fallback=1\r?$' -or $state -notmatch '(?m)^dumped=1\r?$' -or !(Test-Path -LiteralPath $dump)) { throw "$($case.Name): failed fallback; exit=$code; $state" }
            $expected = $utf8.GetBytes($payload + "`r`n"); $actual = [IO.File]::ReadAllBytes($dump)
            $sha = [Security.Cryptography.SHA256]::Create()
            try { $same = [Convert]::ToBase64String($sha.ComputeHash($expected)) -ceq [Convert]::ToBase64String($sha.ComputeHash($actual)) } finally { $sha.Dispose() }
            if (!$same -or $actual.Length -ne $expected.Length) { throw "$($case.Name): Source bytes differ; expected=$($expected.Length), actual=$($actual.Length)" }
            if ((Get-FileHash -LiteralPath $bad -Algorithm SHA256).Hash -ne $before) { throw "$($case.Name): input file changed" }
            $passed = $true; Write-Host "$($case.Name): bounded fallback and exact Source bytes PASS"
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed -KeepArtifacts:$KeepArtifacts }
    }
} finally { foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) } }
