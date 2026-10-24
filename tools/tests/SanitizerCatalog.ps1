# The release catalog remains authoritative; sanitizer selection adds metadata.
function Get-FbeSanitizerMetadata {
    param($Entry, [string]$RepositoryRoot)
    $tokens=$null; $errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $RepositoryRoot $Entry.path),[ref]$tokens,[ref]$errors)
    $parameters=@($ast.ParamBlock.Parameters | ForEach-Object { $_.Name.VariablePath.UserPath })
    $binaryParameters=@($parameters | Where-Object { $_ -in @('FbeExe','FbeExecutable','BatchPath','BatchExecutable','Importer','RuntimeDirectory','DllPath') })
    $reason=$null
    if(-not $binaryParameters.Count) { $reason='No runtime artifact override: source/contract, standalone unit, or production/package-only check.' }
    if($Entry.id -match 'performance|benchmark') { $reason='Release performance thresholds are not sanitizer pass/fail criteria.' }
    if(@($Entry.requirements | Where-Object { $_ -in @('private-window-station','materialized-portable-package','external-ci-package') }).Count) { $reason='Separate user-logon/package boundary; not a child of the sanitizer worker.' }
    if($errors.Count) { $reason='Existing script does not parse; not silently executed as sanitizer coverage.' }
    $contours=@($Entry.contours)
    if($Entry.id -match '^release\.(fbd-production-roundtrip|fbe-binary-production-roundtrip|fbe-clipboard-fallback-runtime|fbe-file-close-runtime|fbe-new-document-safety-runtime|plugin-settings-runtime)$') { $contours+= 'FAST' }
    $expected=@(0)
    if($Entry.id -match '^release\.(runtime-paths-cli|fbe-cli-argument-parsing|prerelease-serialization)$') { $expected+=2 }
    if($Entry.id -match '^release\.(export-epub-v2-runtime|import-epub-fidelity|prerelease-epub-import)$') { $expected+=@(1,2) }
    if($Entry.id -eq 'release.import-epub-batch-dll-abi') { $expected+=1 }
    if($Entry.id -eq 'release.fbe-cite-poem-undo-production') { $expected+=1 }
    if($Entry.id -match '^release\.fbe-table-failure-safety(?:\.|$)') { $expected+=1 }
    if($Entry.id -match '^release\.fbe-(save-decision|themed-message)-runtime$') { $expected+=73 }
    return [pscustomobject]@{eligible=($null -eq $reason);reason=$reason;contours=@($contours | Select-Object -Unique);parameters=$parameters;binaryParameters=$binaryParameters;expectedChildExitCodes=$expected}
}

function Get-FbeSanitizerMatrices {
    param([string]$RepositoryRoot)
    $sources=@(Get-Content -Raw (Join-Path $RepositoryRoot 'tools/build/verify-release.ps1'))
    $sources+=@(& (Join-Path $RepositoryRoot 'tools/tests/get-required-ci-runtime-suites.ps1') | ForEach-Object Command)
    $matrices=@{}
    foreach($source in $sources) {
        $tokens=$null; $errors=$null
        $ast=[Management.Automation.Language.Parser]::ParseInput($source,[ref]$tokens,[ref]$errors)
        foreach($loop in $ast.FindAll({param($node) $node -is [Management.Automation.Language.ForEachStatementAst]},$true)) {
            $condition=$loop.Condition.Extent.Text
            # Only literal enumerations, never arbitrary code from a loop condition.
            if($condition -notmatch '^[\s@()'',"A-Za-z0-9_-]+$' -or $condition -notmatch '[''"]') { continue }
            $values=@(& ([scriptblock]::Create($condition)))
            $name=$loop.Variable.VariablePath.UserPath
            $matrices[$name]=@(@($matrices[$name]) + $values | Where-Object { $null -ne $_ } | Select-Object -Unique)
        }
    }
    return $matrices
}

function Get-FbeSanitizerCommands {
    param($Entry,[hashtable]$Matrices)
    $commands=@()
    foreach($invocation in $Entry.invocations) {
        $command=$invocation.command
        $command=$command -replace '(?i)out[/\\]Release[/\\]FBE\.exe', '(Join-Path $outputDir ''FBE.exe'')'
        if($command.TrimStart().StartsWith('-')) { $command='& (Join-Path $repoRoot '''+$Entry.path+''') '+$command }
        foreach($parameter in $Entry.sanitizer.binaryParameters) {
            if($command -match ('(?i)-'+$parameter+'\b')) { continue }
            switch($parameter) {
                {$_ -in 'FbeExe','FbeExecutable'} { $command+=' -'+$parameter+' (Join-Path $outputDir ''FBE.exe'')' }
                'RuntimeDirectory' { $command+=' -RuntimeDirectory $outputDir' }
                'DllPath' {
                    $plugin=if($Entry.id -match 'import-epub') { 'ImportEPUB' } else { 'ExportEPUB' }
                    $command+=' -DllPath (Join-Path $outputDir ''Plugins/'+$plugin+'.dll'')'
                }
            }
        }
        if('IncludeBatch' -in $Entry.sanitizer.parameters -and $command -notmatch '-IncludeBatch\b') { $command+=' -IncludeBatch' }
        # Instrumented view transitions need the harness's existing upper
        # budget; ordinary Release retains its default 25-second timeout.
        if($Entry.id -eq 'release.fbe-editor-view-lifecycle-runtime' -and $command -notmatch '-TimeoutSeconds\b') { $command+=' -TimeoutSeconds 45' }
        $expanded=@($command)
        foreach($name in $Matrices.Keys) {
            $pattern='\$'+[regex]::Escape($name)+'\b'
            if($command -notmatch $pattern) { continue }
            $expanded=@(foreach($item in $expanded) { foreach($value in $Matrices[$name]) { [regex]::Replace($item,$pattern,(''''+[string]$value+'''')) } })
        }
        $commands+=$expanded
    }
    return @($commands | Select-Object -Unique)
}
