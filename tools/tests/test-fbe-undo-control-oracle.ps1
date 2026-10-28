[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [string[]]$Variants = @('a1','a2','a3','a3-light','a4','savepoint','savepoint-light','savepoint-direct')
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$evidence = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceDirectory)
if (Test-Path -LiteralPath $evidence) { throw "Evidence directory already exists: $evidence" }
New-Item -ItemType Directory -Path $evidence | Out-Null
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_CONTROL_VARIANT')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
$summary = @()
try {
    foreach ($kind in $Variants) {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'control-oracle'; $passed = $false
        try {
            $null = New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts')
            $fixture = Join-Path $iso.Root 'oracle.fb2'; $report = Join-Path $evidence ($kind + '.tsv')
            $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Undo control</book-title><lang>en</lang></title-info><document-info><author><nickname>Test</nickname></author><id>control-oracle</id><version>1.0</version></document-info></description><body><section id="oracle-section"><p>tail</p></section></body></FictionBook>'
            [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
            $beforeHash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'split-control-oracle'; $env:FBE_NEXT_TEST_CONTROL_VARIANT = $kind
            $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',$report,$fixture) -PassThru
            $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario "control-$kind" -Report $report -TimeoutSeconds 180
            if ($code -ne 0 -or !(Test-Path -LiteralPath $report)) { throw "Control probe failed to execute: $kind, exit=$code" }
            $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
            $result = @($rows | Where-Object phase -eq 'result'); if ($result.Count -ne 1) { throw "Missing result: $report" }
            $savedCopy = Join-Path $evidence ($kind + '-saved.fb2'); Copy-Item -LiteralPath $fixture -Destination $savedCopy
            $savedHash = (Get-FileHash -LiteralPath $savedCopy -Algorithm SHA256).Hash
            $disk = [System.Xml.XmlDocument]::new(); $disk.PreserveWhitespace = $false; $disk.Load($savedCopy)
            $models = foreach ($row in @($rows | Where-Object { $_.phase -ne 'result' })) {
                $model = [System.Xml.XmlDocument]::new(); $model.PreserveWhitespace = $false
                $loaded = $true; try { $model.LoadXml($row.fb2_model) } catch { $loaded = $false }
                [pscustomobject]@{ phase=$row.phase; modelValid=$loaded; sameAsSaved=($loaded -and $model.DocumentElement.OuterXml -ceq $disk.DocumentElement.OuterXml); dirty=$row.dirty; domMatchesSavepoint=$row.dom_equals_savepoint }
            }
            $models | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $evidence ($kind + '-model.json')) -Encoding utf8
            if ($kind -in @('save-format','save-again','save-description')) {
                $summary += [pscustomobject]@{ variant=$kind; TEXT_HISTORY='OBSERVATION'; DIRTY_SAVEPOINT='OBSERVATION'; initialFixtureSha256=$beforeHash; savedFileSha256=$savedHash; result=$result[0].dom; report=$report; models=$models }
                $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $evidence 'summary.json') -Encoding utf8
                Write-Host "$kind OBSERVATION $($result[0].dom)"; $passed = $true; continue
            }
            $savedRow = @($rows | Where-Object phase -eq 'saved'); $undone = @($rows | Where-Object { $_.phase -match '^undo-\d+$' } | Select-Object -Last 1)
            $redone = @($rows | Where-Object { $_.phase -match '^redo-\d+$' } | Select-Object -Last 1)
            $observations = @($rows | Where-Object phase -ne 'result')
            function Get-SectionText($row) { $doc = [System.Xml.XmlDocument]::new(); $doc.PreserveWhitespace = $false; $doc.LoadXml($row.fb2_model); $node = $doc.SelectSingleNode("//*[local-name()='section' and @id='oracle-section']"); if (-not $node) { throw 'Control section missing in model' }; return $node.InnerText }
            $initialText = Get-SectionText $savedRow[0]; $undoneText = Get-SectionText $undone[0]; $redoneText = Get-SectionText $redone[0]
            $expectedText = if ($kind -eq 'a4') { 'XQtail' } elseif ($kind -like 'savepoint*') { 'Qtail' } else { 'QXtail' }
            $sequenceValid = $initialText -ceq 'tail' -and $undoneText -ceq 'tail' -and $redoneText -ceq $expectedText
            if ($kind -in @('a3','a3-light')) {
                $firstUndo = @($rows | Where-Object phase -eq 'undo-1'); $firstRedo = @($rows | Where-Object phase -eq 'redo-1')
                $sequenceValid = $sequenceValid -and $result[0].dom -match 'undo_count=2' -and $firstUndo.Count -eq 1 -and $firstRedo.Count -eq 1 -and (Get-SectionText $firstUndo[0]) -ceq 'Qtail' -and (Get-SectionText $firstRedo[0]) -ceq 'Qtail' -and $firstUndo[0].selection -match 'parent=P;offset=1;length=0' -and $undone[0].selection -match 'parent=P;offset=0;length=0' -and $redone[0].selection -match 'parent=P;offset=2;length=0'
            }
            if ($kind -eq 'a4') { $sequenceValid = $sequenceValid -and $result[0].dom -match 'undo_count=3' }
            $basic = $savedRow.Count -eq 1 -and $undone.Count -eq 1 -and $redone.Count -eq 1 -and $savedRow[0].dirty -eq '0' -and $undone[0].dom_equals_savepoint -eq '1' -and $result[0].dom -match 'restored=1;redone=1' -and @($models | Where-Object phase -eq 'saved' | Where-Object sameAsSaved -eq $true).Count -eq 1
            $observerStable = @($observations | Where-Object { $_.observer_dom_unchanged -ne '1' -or $_.observer_version_unchanged -ne '1' -or $_.dirty_before -ne $_.dirty }).Count -eq 0
            $savepointRestored = $undone.Count -eq 1 -and $undone[0].dirty -eq '0' -and @($models | Where-Object phase -eq $undone[0].phase | Where-Object sameAsSaved -eq $true).Count -eq 1
            $caretRestored = $undone[0].selection -match 'parent=P;offset=0;length=0' -and $redone[0].selection -match $(if ($kind -like 'savepoint*' -or $kind -eq 'a4') { 'parent=P;offset=1;length=0' } else { 'parent=P;offset=2;length=0' })
            $selectionRestored = $caretRestored
            if ($kind -eq 'a4') { $formatUndo = @($rows | Where-Object phase -eq 'undo-2'); $selectionRestored = $selectionRestored -and $formatUndo.Count -eq 1 -and $formatUndo[0].selection -match 'parent=P;offset=1;length=4' }
            $xmlIntegrity = @($models | Where-Object { -not $_.modelValid }).Count -eq 0 -and @($models | Where-Object phase -eq $undone[0].phase | Where-Object sameAsSaved -eq $true).Count -eq 1
            $textStatus = if ($basic -and $sequenceValid -and $observerStable -and $caretRestored -and $selectionRestored -and $xmlIntegrity) { 'PASS' } else { 'FAIL' }
            $dirtyStatus = if ($savepointRestored) { 'PASS' } else { 'FAIL' }
            $summary += [pscustomobject]@{ variant=$kind; TEXT_UNDO=($undoneText -ceq 'tail'); TEXT_REDO=($redoneText -ceq $expectedText); HISTORY_ORDER=$sequenceValid; CARET_RESTORE=$caretRestored; SELECTION_RESTORE=$selectionRestored; XML_INTEGRITY=$xmlIntegrity; DIRTY_SAVEPOINT=$dirtyStatus; TEXT_HISTORY=$textStatus; basic=$basic; observerStable=$observerStable; savepointRestored=$savepointRestored; initialFixtureSha256=$beforeHash; savedFileSha256=$savedHash; result=$result[0].dom; report=$report; models=$models }
            $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $evidence 'summary.json') -Encoding utf8
            Write-Host "$kind TEXT_HISTORY=$textStatus DIRTY_SAVEPOINT=$dirtyStatus caret=$caretRestored selection=$selectionRestored XML=$xmlIntegrity $($result[0].dom)"
            $passed = $true
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
    }
    Get-FileHash -LiteralPath $FbeExe -Algorithm SHA256 | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'exe-sha256.json') -Encoding utf8
    $failed = @($summary | Where-Object TEXT_HISTORY -eq 'FAIL')
    if ($failed.Count) { throw "Text history control failed: $(($failed.variant) -join ', '); evidence: $evidence" }
    $dirty = @($summary | Where-Object DIRTY_SAVEPOINT -eq 'FAIL')
    if ($dirty.Count) { Write-Warning "Dirty/savepoint remains unresolved: $(($dirty.variant) -join ', ')" }
} finally { foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) } }
