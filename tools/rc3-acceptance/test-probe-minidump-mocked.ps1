$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ProbeMinidump.ps1')
function New-MockMinidump {
    $stream = [IO.MemoryStream]::new()
    $stream.SetLength(512)
    $writer = [IO.BinaryWriter]::new($stream, [Text.Encoding]::UTF8, $true)
    try {
        $stream.Position = 0
        $writer.Write([uint32]0x504d444d); $writer.Write([uint32]0xa793); $writer.Write([uint32]4); $writer.Write([uint32]32)
        $stream.Position = 32
        foreach ($entry in @(@(6,168,80),@(3,4,264),@(4,4,268),@(7,4,272))) {
            foreach ($value in $entry) { $writer.Write([uint32]$value) }
        }
        $stream.Position = 80
        $writer.Write([uint32]123); $writer.Write([uint32]0); $writer.Write([uint32]3221226505)
        $stream.Position = 240
        $writer.Write([uint32]16); $writer.Write([uint32]248)
        $writer.Flush()
        $stream.Position = 0
        return ,$stream
    } finally { $writer.Dispose() }
}
function Set-MockUInt32([IO.Stream]$Stream, [long]$Offset, [uint32]$Value) {
    $writer = [IO.BinaryWriter]::new($Stream, [Text.Encoding]::UTF8, $true)
    try { $Stream.Position = $Offset; $writer.Write($Value); $writer.Flush() } finally { $writer.Dispose() }
}
$cases = @(
    @{name='valid structure';offset=-1;value=0;pass=$true},
    @{name='bad signature';offset=0;value=0;pass=$false},
    @{name='directory outside file';offset=12;value=510;pass=$false},
    @{name='missing exception';offset=32;value=11;pass=$false},
    @{name='truncated exception';offset=36;value=12;pass=$false},
    @{name='wrong exception';offset=88;value=1;pass=$false},
    @{name='missing context';offset=240;value=0;pass=$false},
    @{name='context outside file';offset=244;value=510;pass=$false},
    @{name='missing module stream';offset=56;value=11;pass=$false}
)
foreach ($case in $cases) {
    $stream = New-MockMinidump
    try {
        if ($case.offset -ge 0) { Set-MockUInt32 $stream $case.offset $case.value }
        $passed = $false
        try { $null = Assert-Rc3FastFailMinidump -Stream $stream; $passed = $true } catch { }
        if ($passed -ne $case.pass) { throw "Minidump fixture classification failed: $($case.name)" }
    } finally { $stream.Dispose() }
}
Write-Host 'Mock minidump structure checks PASS; no processes were started.'
