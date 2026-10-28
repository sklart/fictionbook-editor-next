# Shared finding classification used by the runner and its deliberately failing self-test.
Set-StrictMode -Version Latest
function Import-SanitizerProcessMonitor {
    if ('SanitizerProcessMonitor' -as [type]) { return }
    Add-Type -Path (Join-Path $PSScriptRoot 'SanitizerProcessMonitor.cs')
}
function Complete-SanitizerStack {
    param([string[]]$Logs, [string]$Executable, [switch]$Force)
    $symbolizer=Join-Path (Split-Path -Parent $Executable) 'llvm-symbolizer.exe'
    if(-not (Test-Path -LiteralPath $symbolizer)) { return }
    $root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path.TrimEnd('\')+'\'
    foreach($log in $Logs) {
        $raw=Get-Content -Raw -LiteralPath $log
        # The MSVC in-process symbolizer connection can close after the first
        # stack. Resolve retained module offsets directly, keeping the raw log.
        if(-not $Force -and $raw -notmatch "Can't (read from|write to) symbolizer") { continue }
        $frames=@([regex]::Matches($raw,'\((?<module>[^\r\n]+)\+0x(?<offset>[0-9a-fA-F]+)\)') |
            Where-Object { $_.Groups['module'].Value.StartsWith($root,[StringComparison]::OrdinalIgnoreCase) -and $_.Groups['module'].Value -notmatch 'clang_rt\.asan' } |
            ForEach-Object { [pscustomobject]@{module=$_.Groups['module'].Value;offset=('0x'+$_.Groups['offset'].Value)} })
        $resolved=@(foreach($group in ($frames | Group-Object module)) {
            if(-not (Test-Path -LiteralPath $group.Name)) { continue }
            $addresses=@($group.Group.offset | Select-Object -Unique)
            "Module: $($group.Name)"
            $lines=@(& $symbolizer --no-inlines --addresses ("--obj="+$group.Name) @addresses 2>&1)
            if($LASTEXITCODE -ne 0) {
                $lines | Set-Content -LiteralPath ($log+'.symbolizer-failure.log') -Encoding utf8
                throw "Offline ASan symbolization failed: $($group.Name)"
            }
            $lines
        })
        if($resolved.Count) {
            $path=$log+'.symbolized.log'
            $resolved | Set-Content -LiteralPath $path -Encoding utf8
            $path
        }
    }
}
function Invoke-SanitizerCase {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Worker,
        [string[]]$WorkerArguments=@(), [Parameter(Mandatory)][string]$Directory,
        [string]$Executable, [int]$TimeoutSeconds=600, [int[]]$ExpectedChildExitCodes=@(0), [string[]]$NativeNames=@('FBE.exe','FBV.exe','ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe','powershell.exe'), [switch]$OfflineSymbolization, [ValidateSet('Address','Undefined')][string]$Sanitizer='Address')
    Import-SanitizerProcessMonitor
    if($Sanitizer -eq 'Address' -and 3 -in $ExpectedChildExitCodes) { throw 'Exit 3 is reserved for ASan abort and cannot be an expected application exit.' }
    New-Item -ItemType Directory -Force $Directory | Out-Null
    $directoryPath=(Resolve-Path -LiteralPath $Directory).Path
    # VCAsan 14.44 truncates long dump paths before creating the file. Use a
    # short unique temporary name, then preserve the completed dump with the
    # case artifacts after the monitored job has exited.
    $dumpPrefix='fbe-asan-'+[guid]::NewGuid().ToString('N')
    $dumpRoot=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Temp'
    $dump=Join-Path $dumpRoot ($dumpPrefix+'.dmp')
    $saved=@{}
    foreach($key in @('ASAN_OPTIONS','ASAN_SAVE_DUMPS','UBSAN_OPTIONS')) { $saved[$key]=[Environment]::GetEnvironmentVariable($key) }
    if($Sanitizer -eq 'Address') {
        if (-not (Test-Path -LiteralPath $dumpRoot -PathType Container) -or $dump.Length -gt 110) { throw "A short ASan dump path is unavailable: $dumpRoot" }
        $env:ASAN_SAVE_DUMPS=$dump
        $logPrefix=(Join-Path $directoryPath 'asan').Replace('\','/')
        $symbolize=if($OfflineSymbolization) {0} else {1}
        $env:ASAN_OPTIONS="halt_on_error=1:abort_on_error=1:windows_hook_rtl_allocators=1:alloc_dealloc_mismatch=1:symbolize=${symbolize}:symbolize_inline_frames=0:log_path='$logPrefix'"
        $env:UBSAN_OPTIONS=$null
    } else {
        $env:ASAN_OPTIONS=$null; $env:ASAN_SAVE_DUMPS=$null
        $logPrefix=(Join-Path $directoryPath 'ubsan').Replace('\','/')
        $env:UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1:symbolize=1:log_path='$logPrefix'"
    }
    $monitor=$null; $process=$null; $timedOut=$false
    try {
        $telemetry=if($env:VCToolsInstallDir) { Join-Path $env:VCToolsInstallDir 'bin/HostX64/x86/VCTIP.EXE' } else { $null }
        $monitor=[SanitizerProcessMonitor]::new($telemetry,$Sanitizer)
        $stdout=Join-Path $directoryPath 'stdout.log'; $stderr=Join-Path $directoryPath 'stderr.log'
        # The suspended worker receives one command line; reject embedded quotes in paths.
        if ($Worker.Contains('"') -or @($WorkerArguments | Where-Object { $_.Contains('"') }).Count) { throw 'Unsupported quote in worker argument.' }
        $arguments=@('-NoProfile','-File',('"'+$Worker+'"'))+@($WorkerArguments | ForEach-Object {'"'+$_+'"'})
        $pwsh=Join-Path $PSHOME 'pwsh.exe'
        $workerId=$monitor.Launch($pwsh, ('"'+$pwsh+'" '+($arguments -join ' ')), $directoryPath, $stdout, $stderr)
        $process=[Diagnostics.Process]::GetProcessById($workerId)
        $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while(-not $process.WaitForExit(500)) {
            if([DateTime]::UtcNow -gt $deadline) {
                $timedOut=$true
                & taskkill.exe /PID $process.Id /T /F | Out-Null
                break
            }
        }
        $process.WaitForExit(); $process.Refresh()
        # ASan dump/symbolizer children may outlive the harness. Keep their pipes
        # and handles alive until the entire job completes, within the case budget.
        $remaining=[Math]::Max(0, [int]($deadline-[DateTime]::UtcNow).TotalMilliseconds)
        if(-not $monitor.WaitForCompletion($remaining)) { $timedOut=$true }
        $trace=@($monitor.Snapshot())
        $workerExit=@($trace | Where-Object Id -eq $workerId)[0].ExitCode
        $children=@($trace | Where-Object { $_.Id -ne $workerId -and ($_.Instrumented -or $_.Name -in $NativeNames) })
        $childFailures=@($children | Where-Object { $_.ExitCode -notin $ExpectedChildExitCodes })
        $uninstrumented=@($children | Where-Object { $_.Name -in @('FBE.exe','FBV.exe','ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe') -and -not $_.Instrumented })
        $logs=@(Get-ChildItem -LiteralPath $directoryPath -File | Where-Object { $_.Name -like 'asan.*' -or $_.Name -like 'ubsan.*' -or $_.Extension -eq '.log' })
        $pattern=if($Sanitizer -eq 'Address') {'ERROR:\s*(AddressSanitizer|LeakSanitizer)|AddressSanitizer:|AddressSanitizer failed|ASan runtime|Sanitizer CHECK failed|Shadow memory range interleaves'} else {'runtime error:|UndefinedBehaviorSanitizer|(?m)^\s*ubsan\b|Sanitizer CHECK failed'}
        $diagnostics=@($logs | Where-Object { Select-String -LiteralPath $_.FullName -Pattern $pattern -Quiet })
        $completedStacks=if($Sanitizer -eq 'Address') {@(Complete-SanitizerStack -Logs @($diagnostics | ForEach-Object FullName) -Executable $Executable -Force:$OfflineSymbolization)} else {@()}
        if ($Sanitizer -eq 'Address') {
            Get-ChildItem -LiteralPath $dumpRoot -File -Filter ($dumpPrefix+'*.dmp') | ForEach-Object {
                Move-Item -LiteralPath $_.FullName -Destination (Join-Path $directoryPath $_.Name)
            }
        }
        $dumps=@(Get-ChildItem -LiteralPath $directoryPath -File -Filter '*.dmp')
        $failure=($null -ne $monitor.ObservationError) -or $timedOut -or $workerExit -ne 0 -or $childFailures.Count -gt 0 -or $diagnostics.Count -gt 0 -or $dumps.Count -gt 0 -or $uninstrumented.Count -gt 0
        $result=[ordered]@{name=$Name;executable=$Executable;commit=(& git -C (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path rev-parse HEAD).Trim();sanitizer=$Sanitizer;status=$(if($failure){'FAIL'}else{'PASS'});exitCode=$workerExit;workerId=$workerId;allProcesses=$trace;timedOut=$timedOut;stdout=$stdout;stderr=$stderr;observationError=$monitor.ObservationError;stackTraceFiles=@($diagnostics | ForEach-Object FullName);dumpPaths=@($dumps | ForEach-Object FullName);childFailures=$childFailures;processes=$children}
        $result['expectedChildExitCodes']=$ExpectedChildExitCodes
        $result['infrastructureCleanup']=$monitor.InfrastructureCleanup
        $result.stackTraceFiles+=@($completedStacks)
        $result['uninstrumentedProductProcesses']=$uninstrumented
        $result['executables']=@($children | ForEach-Object Executable | Select-Object -Unique)
        if($childFailures.Count) { $result.executable=$childFailures[0].Executable }
        elseif($uninstrumented.Count) { $result.executable=$uninstrumented[0].Executable }
        $result | ConvertTo-Json -Depth 7 | Set-Content (Join-Path $directoryPath 'result.json') -Encoding utf8
        return [pscustomobject]$result
    } finally {
        if($monitor) { $monitor.Dispose() }
        foreach($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key,$saved[$key]) }
        if($process) { $process.Dispose() }
    }
}
