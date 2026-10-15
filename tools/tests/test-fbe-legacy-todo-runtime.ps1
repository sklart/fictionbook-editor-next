[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [ValidateSet('drop-normalization-runtime','source-regex-diagnostics-runtime','drop-files-runtime')]
    [string[]]$Scenarios = @('drop-normalization-runtime','source-regex-diagnostics-runtime','drop-files-runtime'),
    [int]$TimeoutSeconds = 60,
    [switch]$KeepArtifacts
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'legacy-todo'
New-IsolatedFbeSettingsSeed -Isolation $isolation | Out-Null
$passed = $false
try {
    foreach ($scenario in $Scenarios) {
        $fixture = Join-Path $isolation.Root ('книга-' + $scenario + '.fb2')
        $report = Join-Path $isolation.Root ($scenario + '.tsv')
        $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Legacy TODO</book-title><lang>en</lang></title-info><document-info><id>legacy-todo-test</id><version>1.0</version></document-info></description><body><section><p>Anchor text</p></section></body></FictionBook>'
        [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
        Copy-Item -LiteralPath $fixture -Destination ($fixture + '.other.fb2')
        $png = [Convert]::FromBase64String('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAF/gL+MZ7M0QAAAABJRU5ErkJggg==')
        foreach ($suffix in '.first.png','.second.png','.third.png','.single.png') { [IO.File]::WriteAllBytes($fixture + $suffix, $png) }
        [IO.File]::WriteAllText($fixture + '.bad.png', 'invalid image')
        [IO.File]::WriteAllText($fixture + '.unsupported.txt', 'unsupported')
        $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
        try {
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = $scenario
            $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
            $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario $scenario -Report $report -TimeoutSeconds $TimeoutSeconds
        } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
        $text = if(Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing report>' }
        if($exitCode -ne 0 -or $text -match 'fail' -or $text -notmatch 'pass') { throw "$scenario failed: exit=$exitCode`n$text" }
        Write-Host "PASS $scenario`n$text"
    }
    $passed = $true
} finally {
    if($KeepArtifacts) { Write-Host "Artifacts: $($isolation.Root)" }
    else { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed }
}
