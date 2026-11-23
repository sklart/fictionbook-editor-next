# Minimal structural preflight, not a debugger or a complete dump validator.
function Assert-Rc3FastFailMinidump {
    param([Parameter(Mandatory)][IO.Stream]$Stream)
    if (-not $Stream.CanRead -or -not $Stream.CanSeek -or $Stream.Length -lt 32) { throw 'Truncated minidump header.' }
    $reader = [IO.BinaryReader]::new($Stream, [Text.Encoding]::UTF8, $true)
    try {
        $Stream.Position = 0
        if ($reader.ReadUInt32() -ne [uint32]0x504d444d) { throw 'Incorrect minidump signature.' }
        $null = $reader.ReadUInt32() # Version
        $count = $reader.ReadUInt32()
        $directory = $reader.ReadUInt32()
        if ($count -eq 0 -or $count -gt 4096 -or $directory -lt 32 -or
            [long]$directory + [long]$count * 12 -gt $Stream.Length) { throw 'Invalid minidump stream directory.' }
        $seen = @{}
        for ($i = 0; $i -lt $count; $i++) {
            $Stream.Position = [long]$directory + [long]$i * 12
            $type = $reader.ReadUInt32(); $size = $reader.ReadUInt32(); $rva = $reader.ReadUInt32()
            if ($size -gt 0 -and ([long]$rva + [long]$size -gt $Stream.Length)) { throw 'Minidump stream exceeds file bounds.' }
            if ($type -in @(3,4,6,7,8)) {
                if ($seen.ContainsKey([int]$type)) { throw 'Duplicate required minidump stream.' }
                $seen[[int]$type] = @{size=$size;rva=$rva}
            }
        }
        if ((-not $seen.ContainsKey(3) -and -not $seen.ContainsKey(8)) -or
            -not $seen.ContainsKey(4) -or -not $seen.ContainsKey(6) -or -not $seen.ContainsKey(7)) { throw 'Required minidump streams missing.' }
        foreach ($type in @(4,7)) { if ($seen[$type].size -lt 4) { throw 'Required minidump stream is empty.' } }
        $threadType = if ($seen.ContainsKey(3)) { 3 } else { 8 }
        if ($seen[$threadType].size -lt 4 -or $seen[6].size -lt 168) { throw 'Minidump thread or exception stream is truncated.' }
        $Stream.Position = [long]$seen[6].rva
        $threadId = $reader.ReadUInt32()
        $null = $reader.ReadUInt32() # alignment
        $code = $reader.ReadUInt32()
        if ($threadId -eq 0 -or $code -ne [uint32]3221226505) { throw 'Probe minidump lacks the expected fast-fail exception.' }
        $Stream.Position = [long]$seen[6].rva + 160
        $contextSize = $reader.ReadUInt32(); $contextRva = $reader.ReadUInt32()
        if ($contextSize -eq 0 -or [long]$contextRva + [long]$contextSize -gt $Stream.Length) { throw 'Probe exception context is absent or truncated.' }
        return [pscustomobject]@{exceptionCode='0xC0000409';threadId=$threadId;contextBytes=$contextSize;streamCount=$count}
    } finally { $reader.Dispose() }
}
