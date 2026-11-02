Set-StrictMode -Version Latest

function New-IsolatedFbeScratchDirectory {
    param([Parameter(Mandatory)][string]$Name, [switch]$RuntimePayload)
    if ([string]::IsNullOrWhiteSpace($Name) -or $Name -match '[\\/:*?"<>|]') { throw 'Invalid isolated test name.' }
    Remove-AbandonedIsolatedFbeRuntimes
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
    # Keep this deliberately shallow: bundled user-script names are long and
    # the legacy script host still observes MAX_PATH.
    $root = Join-Path ([IO.Path]::GetTempPath()) ('fr-' + [guid]::NewGuid().ToString('N'))
    $runtime = $root
    New-Item -ItemType Directory -Path $root -ErrorAction Stop | Out-Null
    try {
        $dataName = "$Name-" + [guid]::NewGuid().ToString('N')
        $dataPath = Join-Path $runtime $dataName
        $owner = @{ version = 1; pid = $PID; startTicks = (Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks; dataName = $dataName }
        [IO.File]::WriteAllText((Join-Path $root '.fbe-runtime-owner.json'), ($owner | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
        Get-ChildItem -LiteralPath (Split-Path -Parent $sourceExe) -Force -ErrorAction Stop | Where-Object { $_.Name -notin @('.fbe-runtime-owner.json', 'Data', 'test-obj') -and $_.Extension -notin @('.pdb', '.lib', '.exp') } | Copy-Item -Destination $runtime -Recurse -Force -ErrorAction Stop
        New-Item -ItemType Directory -Path $dataPath -ErrorAction Stop | Out-Null
        [IO.File]::WriteAllText((Join-Path $runtime 'portable.ini'), "[Portable]`r`nDataPath=$dataName`r`n", [Text.UTF8Encoding]::new($false))
        return [pscustomobject]@{ Root = $root; Runtime = $runtime; Exe = Join-Path $runtime 'FBE.exe'; DataPath = $dataPath; StagePath = Join-Path $root 'test-stage.txt' }
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
                $isolation = [pscustomobject]@{ Root = $entry.FullName; Exe = Join-Path $entry.FullName 'FBE.exe'; DataPath = Join-Path $entry.FullName $owner.dataName }
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
    return @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
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
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
        throw "FBE runtime scenario '$Scenario' timed out after $TimeoutSeconds seconds. PID=$($Process.Id); command_line=$($Process.StartInfo.FileName) $($Process.StartInfo.Arguments); $(Get-IsolatedRuntimeDiagnostics $Isolation $Report)"
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
    if ($KeepArtifacts) {
        $marker = Join-Path $root '.fbe-runtime-owner.json'
        if (Test-Path -LiteralPath $marker) { Remove-Item -LiteralPath $marker -Force -ErrorAction Stop }
        Write-Host "Runtime failure artifacts: $root"
        if ($leftovers.Count -gt 0) { throw "Disposable runtime leaked process(es): $($leftovers.ProcessId -join ', ')." }
        return
    }
    if (-not $Passed) {
        $artifactRoot = if ($env:FBE_RUNTIME_FAILURE_ARTIFACTS) { $env:FBE_RUNTIME_FAILURE_ARTIFACTS } else { Join-Path $PSScriptRoot '..\..\out\tests\runtime-failure' }
        $destination = Join-Path $artifactRoot ([IO.Path]::GetFileName($root))
        if ([IO.Path]::GetFullPath($destination).StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "Runtime diagnostics destination is inside the runtime: $destination" }
        New-Item -ItemType Directory -Path $destination -Force -ErrorAction Stop | Out-Null
        # Keep useful evidence bounded: test runtimes can contain an entire
        # release tree, generated books or crash dumps hundreds of MB in size.
        $maxFileBytes = 5MB; $maxTotalBytes = 20MB; $totalBytes = 0; $copied = 0; $skipped = 0
        $allowedExtensions = @('.txt', '.tsv', '.log', '.json', '.xml', '.fb2', '.fbd', '.html', '.htm', '.css', '.js', '.epub', '.docx', '.zip', '.png', '.jpg', '.bmp')
        $candidates = @(if ($PreserveContents) { Get-ChildItem -LiteralPath $root -File -Recurse -Force -ErrorAction Stop } else { Get-ChildItem -LiteralPath $root -File -Force -ErrorAction Stop })
        $recovery = Join-Path $Isolation.DataPath 'Recovery'
        $diagnostics = Join-Path $Isolation.DataPath 'Diagnostics'
        if (-not $PreserveContents) { foreach ($folder in @($recovery, $diagnostics)) { if (Test-Path -LiteralPath $folder) { $candidates += @(Get-ChildItem -LiteralPath $folder -File -Recurse -Force -ErrorAction Stop) } } }
        foreach ($file in $candidates) {
            if ($file.Name -eq '.fbe-runtime-owner.json') { continue }
            if (($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $file.Extension.ToLowerInvariant() -notin $allowedExtensions -or $file.Length -gt $maxFileBytes -or $totalBytes + $file.Length -gt $maxTotalBytes -or $copied -ge 500) { $skipped++; continue }
            $relative = $file.FullName.Substring($root.Length).TrimStart('\')
            if (-not $PreserveContents -and $file.FullName.StartsWith($recovery + '\', [StringComparison]::OrdinalIgnoreCase)) { $relative = 'Recovery\' + $file.FullName.Substring($recovery.Length).TrimStart('\') }
            if (-not $PreserveContents -and $file.FullName.StartsWith($diagnostics + '\', [StringComparison]::OrdinalIgnoreCase)) { $relative = 'Diagnostics\' + $file.FullName.Substring($diagnostics.Length).TrimStart('\') }
            $target = Join-Path $destination $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force -ErrorAction Stop | Out-Null
            Copy-Item -LiteralPath $file.FullName -Destination $target -Force -ErrorAction Stop
            $totalBytes += $file.Length; $copied++
        }
        [IO.File]::WriteAllText((Join-Path $destination 'artifact-summary.txt'), "copied=$copied`r`nskipped=$skipped`r`nbytes=$totalBytes`r`nmax_file_bytes=$maxFileBytes`r`nmax_total_bytes=$maxTotalBytes`r`n", [Text.UTF8Encoding]::new($false))
        Write-Host "Runtime failure artifacts: $destination"
    }
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction Stop
    if ($leftovers.Count -gt 0) { throw "Disposable runtime leaked process(es): $($leftovers.ProcessId -join ', ')." }
}
