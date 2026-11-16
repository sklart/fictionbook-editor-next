Set-StrictMode -Version Latest

function Assert-IsolatedFbeFreeSpace {
    param([ValidateRange(0, [long]::MaxValue)][long]$RequiredBytes = 256MB, [string]$Path = [IO.Path]::GetTempPath())
    $tempPath = [IO.Path]::GetFullPath($Path)
    $volume = [IO.Path]::GetPathRoot($tempPath)
    if ([string]::IsNullOrEmpty($volume)) { throw "Cannot determine temporary volume: $tempPath" }
    $available = ([IO.DriveInfo]::new($volume)).AvailableFreeSpace
    if ($available -lt $RequiredBytes) {
        throw "Insufficient temporary disk space: required=$RequiredBytes bytes; available=$available bytes; path=$tempPath"
    }
}

function Assert-IsolatedFbeEvidenceBudget {
    param([long]$ExistingBytes)
    if ($ExistingBytes -lt 0 -or $ExistingBytes -gt (2GB - 20MB)) { throw 'Evidence budget is full.' }
}

function New-IsolatedFbeScratchDirectory {
    param([Parameter(Mandatory)][string]$Name, [switch]$RuntimePayload)
    if ([string]::IsNullOrWhiteSpace($Name) -or $Name -match '[\\/:*?"<>|]') { throw 'Invalid isolated test name.' }
    Remove-AbandonedIsolatedFbeRuntimes
    Assert-IsolatedFbeFreeSpace
    $root = Join-Path ([IO.Path]::GetTempPath()) ('fr-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $root -ErrorAction Stop | Out-Null
    try {
        $dataName = "$Name-" + [guid]::NewGuid().ToString('N')
        $owner = @{ version = 1; pid = $PID; startTicks = (Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks; dataName = $dataName; preserveContents = -not $RuntimePayload }
        [IO.File]::WriteAllText((Join-Path $root '.fbe-runtime-owner.json'), ($owner | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
        return [pscustomobject]@{ Root = $root; Runtime = $root; Exe = Join-Path $root 'FBE.exe'; DataPath = Join-Path $root $dataName; StagePath = Join-Path $root 'test-stage.txt' }
    } catch {
        Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction Stop
        throw
    }
}

function New-IsolatedFbeRuntime {
    param([Parameter(Mandatory)][string]$FbeExe, [Parameter(Mandatory)][string]$Name)

    if ([string]::IsNullOrWhiteSpace($Name) -or $Name -match '[\\/:*?"<>|]') { throw 'Invalid isolated test name.' }

    Remove-AbandonedIsolatedFbeRuntimes
    $sourceExe = (Resolve-Path -LiteralPath $FbeExe).Path
    $sourceRoot = [IO.Path]::GetFullPath((Split-Path -Parent $sourceExe)).TrimEnd('\')
    $payload = @(Get-ChildItem -LiteralPath $sourceRoot -Force -ErrorAction Stop | Where-Object { $_.Name -notin @('.fbe-runtime-owner.json', 'Data', 'test-obj') })
    $payloadEntries = [Collections.Generic.List[object]]::new()
    $debugExtensions = @('.pdb', '.lib', '.exp', '.obj', '.ilk', '.ipdb', '.iobj')
    [long]$payloadBytes = 0
    foreach ($item in $payload) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Runtime payload contains a reparse point: $($item.FullName)" }
        $entries = @($item)
        if ($item.PSIsContainer) { $entries += @(Get-ChildItem -LiteralPath $item.FullName -Recurse -Force -ErrorAction Stop) }
        foreach ($entry in $entries) {
            if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Runtime payload contains a reparse point: $($entry.FullName)" }
            if (-not $entry.PSIsContainer -and $entry.Extension.ToLowerInvariant() -in $debugExtensions) { continue }
            if (-not $entry.PSIsContainer) { $payloadBytes += $entry.Length }
            $payloadEntries.Add($entry)
        }
    }
    Assert-IsolatedFbeFreeSpace -RequiredBytes ($payloadBytes + 256MB)
    # Keep this deliberately shallow: bundled user-script names are long and
    # the legacy script host still observes MAX_PATH.
    $root = Join-Path ([IO.Path]::GetTempPath()) ('fr-' + [guid]::NewGuid().ToString('N'))
    $runtime = $root
    New-Item -ItemType Directory -Path $root -ErrorAction Stop | Out-Null
    try {
        $dataName = "$Name-" + [guid]::NewGuid().ToString('N')
        $dataPath = Join-Path $runtime $dataName
        $owner = @{ version = 1; pid = $PID; startTicks = (Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks; dataName = $dataName; sourceRuntime = $sourceRoot }
        [IO.File]::WriteAllText((Join-Path $root '.fbe-runtime-owner.json'), ($owner | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
        foreach ($entry in $payloadEntries) {
            $relative = $entry.FullName.Substring($sourceRoot.Length).TrimStart('\')
            $target = Join-Path $runtime $relative
            if ($entry.PSIsContainer) { New-Item -ItemType Directory -Path $target -Force -ErrorAction Stop | Out-Null }
            else {
                New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force -ErrorAction Stop | Out-Null
                Copy-Item -LiteralPath $entry.FullName -Destination $target -Force -ErrorAction Stop
            }
        }
        New-Item -ItemType Directory -Path $dataPath -ErrorAction Stop | Out-Null
        [IO.File]::WriteAllText((Join-Path $runtime 'portable.ini'), "[Portable]`r`nDataPath=$dataName`r`n", [Text.UTF8Encoding]::new($false))
        return [pscustomobject]@{ Root = $root; Runtime = $runtime; Exe = Join-Path $runtime 'FBE.exe'; DataPath = $dataPath; StagePath = Join-Path $root 'test-stage.txt'; SourceRuntime = $sourceRoot }
    } catch {
        Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction Stop
        throw
    }
}

function Remove-AbandonedIsolatedFbeRuntimes {
    $temp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    $lockPath = Join-Path $temp 'fbe-runtime-cleanup.lock'
    try { $lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
    catch [IO.IOException] { return }
    try {
        foreach ($entry in @(Get-ChildItem -LiteralPath $temp -Directory -Filter 'fr-*' -ErrorAction Stop)) {
            if ($entry.Name -notmatch '^fr-[0-9a-f]{32}$' -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)) { continue }
            $marker = Join-Path $entry.FullName '.fbe-runtime-owner.json'
            if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) { continue }
            try {
                $owner = Get-Content -LiteralPath $marker -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
                if ($owner.version -ne 1 -or $owner.pid -le 0 -or $owner.startTicks -le 0 -or $owner.dataName -notmatch '^[^\\/:*?"<>|]+-[0-9a-f]{32}$') { continue }
                $process = Get-Process -Id $owner.pid -ErrorAction SilentlyContinue
                if ($process) {
                    try { if ($process.StartTime.ToUniversalTime().Ticks -eq [long]$owner.startTicks) { continue } }
                    catch { continue }
                }
                $sourceRuntime = if ($null -ne $owner.PSObject.Properties['sourceRuntime']) { [string]$owner.sourceRuntime } else { '' }
                $isolation = [pscustomobject]@{ Root = $entry.FullName; Exe = Join-Path $entry.FullName 'FBE.exe'; DataPath = Join-Path $entry.FullName $owner.dataName; SourceRuntime = $sourceRuntime }
                $preserveContents = $null -ne $owner.PSObject.Properties['preserveContents'] -and $owner.preserveContents -eq $true
                Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $false -PreserveContents:$preserveContents
            } catch { Write-Warning "Could not clean abandoned runtime $($entry.FullName): $_" }
        }
    } finally { $lock.Dispose() }
}

function New-IsolatedFbeSettingsSeed {
    param([Parameter(Mandatory)]$Isolation, [string]$StartupScriptsDirectory)

    $settingsDir = Join-Path $Isolation.DataPath 'Settings'
    New-Item -ItemType Directory -Path $settingsDir -Force | Out-Null
    $settingsFile = Join-Path $settingsDir 'Settings.xml'
    # CSettings reads /FBE/Settings[@ID="0"]; omitted fields retain its defaults.
    $xml = '<?xml version="1.0" encoding="utf-8"?><FBE><Settings ID="0"><DocumentTreeScripts>false</DocumentTreeScripts><InterfaceTheme>0</InterfaceTheme></Settings></FBE>'
    if($StartupScriptsDirectory) {
        # A fixture can exercise real startup validation with its own catalog,
        # without loading every unrelated bundled/user macro before its probe.
        $scripts = [IO.Path]::GetFullPath($StartupScriptsDirectory)
        $ownedRoot = [IO.Path]::GetFullPath($Isolation.Root).TrimEnd('\') + '\'
        if(-not $scripts.StartsWith($ownedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw "Startup script fixture must belong to its disposable runtime: $scripts" }
        New-Item -ItemType Directory -Path $scripts -Force | Out-Null
        $startupXml = '<ScriptsFolder>' + [Security.SecurityElement]::Escape($scripts) + '</ScriptsFolder><CheckScriptsOnStartup>true</CheckScriptsOnStartup>'
        $xml = $xml.Replace('</Settings>', $startupXml + '</Settings>')
    }
    [IO.File]::WriteAllText($settingsFile, $xml, [Text.UTF8Encoding]::new($false))
    return $settingsFile
}

function Set-IsolatedRuntimeStage {
    param([Parameter(Mandatory)]$Isolation, [Parameter(Mandatory)][string]$Stage)
    [IO.File]::WriteAllText($Isolation.StagePath, "stage=$Stage`r`nutc=$([DateTime]::UtcNow.ToString('o'))`r`n", [Text.UTF8Encoding]::new($false))
}

function Get-IsolatedRuntimeProcesses {
    param([Parameter(Mandatory)]$Isolation)
    $ownedRoot = [IO.Path]::GetFullPath($Isolation.Root).TrimEnd('\') + '\'
    # Failure to enumerate processes must never be interpreted as an empty list.
    return @(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
        ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($ownedRoot, [StringComparison]::OrdinalIgnoreCase)) -or
        ($_.Name -in @('FBE.exe', 'ImportEPUBBatch.exe', 'ExportEPUBBatch.exe', 'ExportDOCXBatch.exe', 'cl.exe', 'link.exe') -and $_.CommandLine -and $_.CommandLine.IndexOf($ownedRoot, [StringComparison]::OrdinalIgnoreCase) -ge 0)
    })
}

function Get-IsolatedRuntimeDiagnostics {
    param([Parameter(Mandatory)]$Isolation, [string]$Report)
    $reportExists = $Report -and (Test-Path -LiteralPath $Report)
    $reportSize = if ($reportExists) { (Get-Item -LiteralPath $Report).Length } else { 0 }
    $recovery = Join-Path $Isolation.DataPath 'Recovery'
    $recoveryFiles = if (Test-Path -LiteralPath $recovery) { (Get-ChildItem -LiteralPath $recovery -File -Recurse | ForEach-Object { "$($_.FullName) ($($_.Length) bytes, $($_.LastWriteTimeUtc.ToString('o')))" }) -join '; ' } else { '<missing>' }
    return "report_path=$Report; report_exists=$reportExists; report_size=$reportSize; recovery=$recovery; recovery_files=$recoveryFiles; stage=$($Isolation.StagePath)"
}

function Wait-IsolatedFbeProcess {
    param([Parameter(Mandatory)][Diagnostics.Process]$Process, [Parameter(Mandatory)]$Isolation, [Parameter(Mandatory)][string]$Scenario, [Parameter(Mandatory)][string]$Report, [ValidateRange(1, 600)][int]$TimeoutSeconds)
    if (-not $Process.WaitForExit($TimeoutSeconds * 1000)) {
        $processId = $Process.Id
        $commandLine = '<unavailable>'
        try {
            $observed = Get-CimInstance Win32_Process -Filter "ProcessId=$processId" -ErrorAction Stop
            if ($observed -and $observed.CommandLine) { $commandLine = [string]$observed.CommandLine }
        } catch { }
        Stop-Process -Id $processId -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
        throw "FBE runtime scenario '$Scenario' timed out after $TimeoutSeconds seconds. PID=$processId; command_line=$commandLine; $(Get-IsolatedRuntimeDiagnostics $Isolation $Report)"
    }
    $Process.WaitForExit()
    $Process.Refresh()
    if (-not $Process.HasExited) { throw "FBE runtime scenario '$Scenario': child process status unavailable. PID=$($Process.Id)" }
    return $Process.ExitCode
}

function Complete-IsolatedFbeRuntime {
    param([Parameter(Mandatory)]$Isolation, [Parameter(Mandatory)][bool]$Passed, [switch]$KeepArtifacts, [switch]$PreserveContents)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
    $root = [IO.Path]::GetFullPath($Isolation.Root).TrimEnd('\')
    if ((Split-Path -Parent $root) -ine $tempRoot -or (Split-Path -Leaf $root) -notmatch '^fr-[0-9a-f]{32}$') {
        throw "Refusing to clean an invalid disposable runtime path: $root"
    }
    if ((Get-Item -LiteralPath $root -Force -ErrorAction Stop).Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Refusing to clean a linked runtime root: $root"
    }
    $linked = @(Get-ChildItem -LiteralPath $root -Recurse -Force -ErrorAction Stop | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint } | Select-Object -First 1)
    if ($linked.Count) { throw "Refusing to clean a runtime containing a reparse point: $($linked[0].FullName)" }
    $marker = Join-Path $root '.fbe-runtime-owner.json'
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) { throw "Refusing to clean an unowned runtime: $root" }
    $owner = Get-Content -LiteralPath $marker -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
    if ($owner.version -ne 1 -or $owner.pid -le 0 -or $owner.startTicks -le 0 -or $owner.dataName -notmatch '^[^\\/:*?"<>|]+-[0-9a-f]{32}$') {
        throw "Refusing to clean a runtime with an invalid owner marker: $root"
    }
    if ($owner.pid -ne $PID) {
        $ownerProcess = Get-Process -Id $owner.pid -ErrorAction SilentlyContinue
        if ($ownerProcess) {
            try { $sameOwner = $ownerProcess.StartTime.ToUniversalTime().Ticks -eq [long]$owner.startTicks }
            catch { throw "Cannot verify runtime owner; retained: $root" }
            if ($sameOwner) { throw "Runtime owner is still running; retained: $root" }
        }
    }
    $leftovers = @(Get-IsolatedRuntimeProcesses $Isolation)
    if ($leftovers.Count -gt 0) {
        foreach ($leftover in $leftovers) {
            Stop-Process -Id $leftover.ProcessId -Force -ErrorAction SilentlyContinue
            Wait-Process -Id $leftover.ProcessId -Timeout 10 -ErrorAction SilentlyContinue
        }
        if (@(Get-IsolatedRuntimeProcesses $Isolation).Count -gt 0) { throw "Could not stop an isolated process; runtime retained: $root" }
        $Passed = $false
    }
    # The installed-mode test hive is named from this validated runtime root.
    # Delete only this runtime's subtree, including after an interrupted owner.
    $testRegistryPath = 'Software\FBE-Next-Test-Runtimes\' + (Split-Path -Leaf $root)
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($testRegistryPath, $false)
    # KeepArtifacts requests bounded evidence, never the full temporary runtime.
    if (-not $Passed -or $KeepArtifacts) {
        $artifactRoot = if ($env:FBE_RUNTIME_FAILURE_ARTIFACTS) { $env:FBE_RUNTIME_FAILURE_ARTIFACTS } else { Join-Path $PSScriptRoot '..\..\out\tests\runtime-failure' }
        $destination = Join-Path $artifactRoot ([IO.Path]::GetFileName($root))
        if ([IO.Path]::GetFullPath($destination).StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "Runtime diagnostics destination is inside the runtime: $destination" }
        $budgetLock = [Threading.Mutex]::new($false, 'Local\FbeRuntimeEvidenceBudget')
        $budgetAcquired = $false
        try {
        try { $budgetAcquired = $budgetLock.WaitOne([TimeSpan]::FromSeconds(30)) }
        catch [Threading.AbandonedMutexException] { $budgetAcquired = $true }
        if (-not $budgetAcquired) { throw "Evidence budget is busy; runtime retained: $root" }
        New-Item -ItemType Directory -Path $artifactRoot -Force -ErrorAction Stop | Out-Null
        [long]$existingEvidence = 0
        foreach ($item in @(Get-ChildItem -LiteralPath $artifactRoot -File -Recurse -Force -ErrorAction Stop)) { $existingEvidence += $item.Length }
        try { Assert-IsolatedFbeEvidenceBudget -ExistingBytes $existingEvidence }
        catch { throw "Evidence budget is full; runtime retained: $root" }
        New-Item -ItemType Directory -Path $destination -Force -ErrorAction Stop | Out-Null
        # Keep useful evidence bounded: test runtimes can contain an entire
        # release tree, generated books or crash dumps hundreds of MB in size.
        $maxFileBytes = 5MB; $maxTotalBytes = 20MB; $totalBytes = 0; $copied = 0; $skipped = 0
        $allowedExtensions = @('.txt', '.tsv', '.csv', '.log', '.stdout', '.stderr', '.json', '.xml', '.fb2', '.fbd', '.html', '.htm', '.css', '.js', '.xsl', '.epub', '.docx', '.zip', '.png', '.jpg', '.bmp', '.bin', '.dmp', '.sha256', '.md', '.ini')
        $candidates = @(Get-ChildItem -LiteralPath $root -File -Recurse -Force -ErrorAction Stop)
        $recovery = Join-Path $Isolation.DataPath 'Recovery'
        $diagnostics = Join-Path $Isolation.DataPath 'Diagnostics'
        $sourceRuntime = if ($null -ne $owner.PSObject.Properties['sourceRuntime']) { [string]$owner.sourceRuntime } else { '' }
        # Crash dumps and book fixtures take priority. If one cannot be
        # preserved within the budget, retain the marked profile for recovery.
        foreach ($file in @($candidates | Sort-Object @{ Expression = { if ($_.Extension -in @('.dmp', '.fb2', '.fbd', '.epub', '.zip')) { 0 } elseif ($_.Extension -in @('.txt', '.log', '.stdout', '.stderr', '.tsv', '.csv', '.json', '.sha256')) { 1 } else { 2 } } })) {
            if ($file.Name -eq '.fbe-runtime-owner.json') { continue }
            $required = $file.Extension -in @('.dmp', '.fb2', '.fbd', '.epub', '.zip')
            if (($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $file.Extension.ToLowerInvariant() -notin $allowedExtensions) { $skipped++; continue }
            $relative = $file.FullName.Substring($root.Length).TrimStart('\')
            # PreserveContents keeps the relative evidence layout; it must not
            # disable deduplication against the immutable source runtime.
            if ($sourceRuntime) {
                $originals = @((Join-Path $sourceRuntime $relative))
                $cardsPrefix = $owner.dataName + '\Settings\ScriptCards\'
                if ($relative.StartsWith($cardsPrefix, [StringComparison]::OrdinalIgnoreCase)) {
                    $originals += Join-Path (Join-Path $sourceRuntime 'ScriptCards') $relative.Substring($cardsPrefix.Length)
                }
                $identical = $false
                foreach ($original in $originals) {
                    if ((Test-Path -LiteralPath $original -PathType Leaf) -and (Get-Item -LiteralPath $original).Length -eq $file.Length -and
                        (Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash -eq (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) { $identical = $true; break }
                }
                if ($identical) { $skipped++; continue }
            }
            if ($required -and ($file.Length -gt $maxTotalBytes -or $totalBytes + $file.Length -gt $maxTotalBytes -or $copied -ge 500)) { throw "Required fixture or crash dump exceeds the evidence budget; runtime retained: $($file.FullName)" }
            if ((-not $required -and $file.Length -gt $maxFileBytes) -or $totalBytes + $file.Length -gt $maxTotalBytes -or $copied -ge 500) { $skipped++; continue }
            if (-not $PreserveContents -and $file.FullName.StartsWith($recovery + '\', [StringComparison]::OrdinalIgnoreCase)) { $relative = 'Recovery\' + $file.FullName.Substring($recovery.Length).TrimStart('\') }
            if (-not $PreserveContents -and $file.FullName.StartsWith($diagnostics + '\', [StringComparison]::OrdinalIgnoreCase)) { $relative = 'Diagnostics\' + $file.FullName.Substring($diagnostics.Length).TrimStart('\') }
            $target = Join-Path $destination $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force -ErrorAction Stop | Out-Null
            if ((Test-Path -LiteralPath $target -PathType Leaf) -and (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) { throw "Conflicting evidence destination; runtime retained: $target" }
            Copy-Item -LiteralPath $file.FullName -Destination $target -Force -ErrorAction Stop
            if ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash) { throw "Evidence copy verification failed; runtime retained: $($file.FullName)" }
            $totalBytes += $file.Length; $copied++
        }
        [IO.File]::WriteAllText((Join-Path $destination 'artifact-summary.txt'), "copied=$copied`r`nskipped=$skipped`r`nbytes=$totalBytes`r`nmax_file_bytes=$maxFileBytes`r`nmax_total_bytes=$maxTotalBytes`r`n", [Text.UTF8Encoding]::new($false))
        if ($Passed) { Write-Host "Runtime artifacts: $destination" }
        else { Write-Host "Runtime failure artifacts: $destination" }
        } finally {
            if ($budgetAcquired) { $budgetLock.ReleaseMutex() }
            $budgetLock.Dispose()
        }
    }
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction Stop
    if ($leftovers.Count -gt 0) { throw "Disposable runtime leaked process(es): $($leftovers.ProcessId -join ', ')." }
}
