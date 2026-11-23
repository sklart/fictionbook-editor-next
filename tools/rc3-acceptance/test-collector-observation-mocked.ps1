$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CollectorObservation.ps1')
$tempRoot = Join-Path $PSScriptRoot 'mock-temp-root'
$imagePath = Join-Path $tempRoot 'fr-0123456789abcdef0123456789abcdef/FBE.exe'
$goodLog = "Process: FBE.exe (1234)`nProcess image: $imagePath`n[12:34:56] Process Exit: PID 1234, Exit Code 0x00000000`n"
$tests = @(
    @{name='clean'; forced=$false; code=0; log=$goodLog; dumps=0; expected='PASS'},
    @{name='forced'; forced=$true; code=0; log=$goodLog; dumps=0; expected='FAIL'},
    @{name='exit'; forced=$false; code=1; log=$goodLog; dumps=0; expected='FAIL'},
    @{name='no attachment'; forced=$false; code=0; log='Waiting for FBE.exe'; dumps=0; expected='FAIL'},
    @{name='wrong process'; forced=$false; code=0; log='Process: FBV.exe (12)'; dumps=0; expected='FAIL'},
    @{name='missing exit'; forced=$false; code=0; log='Process: FBE.exe (1234)'; dumps=0; expected='FAIL'},
    @{name='other PID'; forced=$false; code=0; log=($goodLog -replace 'PID 1234','PID 4321'); dumps=0; expected='FAIL'},
    @{name='crash exit'; forced=$false; code=0; log=($goodLog -replace '0x00000000','0xC0000409'); dumps=0; expected='FAIL'},
    @{name='other image'; forced=$false; code=0; log=($goodLog.Replace($imagePath, (Join-Path $tempRoot 'other/FBE.exe'))); dumps=0; expected='FAIL'},
    @{name='dump despite passing case'; forced=$false; code=0; log=$goodLog; dumps=1; expected='FAIL'}
)
foreach ($test in $tests) {
    $result = Get-Rc3CollectorObservation -ForcedStop $test.forced -ExitCode $test.code -Log $test.log -DumpCount $test.dumps -ExpectedTempRoot $tempRoot
    if ($result.status -ne $test.expected) { throw "Collector classification failed: $($test.name)" }
}
Write-Host 'Mock collector observation checks PASS; no collector or process was started.'
