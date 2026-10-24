[CmdletBinding()]
param(
    [ValidateSet('Address')][string]$Sanitizer='Address',
    [ValidateSet('Fast','Full')][string]$Contour='Fast',
    [string]$OutputDirectory,
    [string[]]$Id=@(),
    [string]$AdditionalCases,
    [switch]$List,
    [switch]$SkipInfrastructureSelfTest,
    [int]$CaseTimeoutSeconds=900
)
$ErrorActionPreference='Stop'
if([Environment]::OSVersion.Version.Major -lt 10) { throw 'AddressSanitizer diagnostics require Windows 10/11.' }
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if(-not $OutputDirectory) { $OutputDirectory=Join-Path $root 'out/asan/Release' }
$OutputDirectory=(Resolve-Path -LiteralPath $OutputDirectory).Path
. (Join-Path $PSScriptRoot 'SanitizerCatalog.ps1')
$catalog=(& (Join-Path $PSScriptRoot 'get-release-test-catalog.ps1') -AsJson -Validate) | ConvertFrom-Json
$effective=if($Contour -eq 'Full') { @('FAST','TABLE','FULL') } else { @('FAST') }
$selected=@($catalog.tests | Where-Object { $_.sanitizer.eligible -and @($_.sanitizer.contours | Where-Object { $_ -in $effective }).Count })
if($Id.Count) {
    foreach($requested in $Id) { if($requested -notin $selected.id) { throw "Unknown/ineligible sanitizer case: $requested" } }
    $selected=@($selected | Where-Object id -in $Id)
}
if($List) { $selected | Select-Object id,@{n='sanitizer';e={$_.sanitizer}}; return }
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
if(-not $selected.Count -and -not $AdditionalCases) { throw 'Sanitizer contour is empty.' }
$manifest=Get-Content -Raw (Join-Path $OutputDirectory 'fbe-sanitizer.json') | ConvertFrom-Json
if($manifest.sanitizer -ne 'Address' -or $manifest.publishable) { throw 'Expected a diagnostic AddressSanitizer output.' }
foreach($module in $manifest.modules) {
    if((Get-FileHash (Join-Path $OutputDirectory $module.path)).Hash -ne $module.sha256) { throw "Stale sanitizer module: $($module.path)" }
}
. (Join-Path $PSScriptRoot 'SanitizerHarness.ps1')
$run=Join-Path $root ('out/tests/asan/'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $run | Out-Null
$summary=[ordered]@{schemaVersion=1;commit=(& git -C $root rev-parse HEAD).Trim();sanitizer=$Sanitizer;contour=$Contour;selectionComplete=($Id.Count -eq 0);status='RUNNING';outputDirectory=$OutputDirectory;modules=$manifest.modules;uninstrumented=$manifest.uninstrumented;selfTest='NOT RUN';results=@();excluded=@($catalog.tests | Where-Object {-not $_.sanitizer.eligible} | ForEach-Object { @{id=$_.id;reason=$_.sanitizer.reason} })}
$summaryPath=Join-Path $run 'summary.json'
$summary | ConvertTo-Json -Depth 12 | Set-Content $summaryPath -Encoding utf8
$savedPath=$env:PATH; $savedCL=$env:_CL_; $savedSymbolizer=$env:ASAN_SYMBOLIZER_PATH
try {
    $env:PATH=$OutputDirectory+';'+$savedPath
    $env:ASAN_SYMBOLIZER_PATH=Join-Path $OutputDirectory 'llvm-symbolizer.exe'
    if(-not $SkipInfrastructureSelfTest) {
        & (Join-Path $PSScriptRoot 'test-asan-infrastructure.ps1') *> (Join-Path $run 'self-test.log')
        if(-not $? -or $LASTEXITCODE -ne 0) { throw 'ASan infrastructure self-test failed.' }
        $summary.selfTest='PASS'
    }
    $matrices=Get-FbeSanitizerMatrices -RepositoryRoot $root
    $cases=@(foreach($entry in $selected) {
        $variant=0
        foreach($command in @(Get-FbeSanitizerCommands -Entry $entry -Matrices $matrices)) {
            @{name=$entry.id+'.'+(++$variant);command=$command;expectedChildExitCodes=$entry.sanitizer.expectedChildExitCodes}
        }
    })
    if($AdditionalCases) { $cases+=@(Get-Content -Raw -LiteralPath $AdditionalCases | ConvertFrom-Json) }
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($case in $cases) {
        $safeName=$case.name -replace '[^A-Za-z0-9._-]','_'
        if(-not $seen.Add($safeName)) { throw "Duplicate sanitizer case output: $safeName" }
    }
    $summary.plannedCases=@($cases | ForEach-Object name)
    $summary.expectedCount=$cases.Count
    foreach($case in $cases) {
        $directory=Join-Path $run ($case.name -replace '[^A-Za-z0-9._-]','_')
        New-Item -ItemType Directory -Force $directory | Out-Null
        $caseFile=Join-Path $directory 'case.json'
        @{command=$case.command;outputDirectory=$OutputDirectory} | ConvertTo-Json | Set-Content $caseFile -Encoding utf8
        # Native helpers compiled by existing harnesses are diagnostic too.
        $env:_CL_=($savedCL+' /fsanitize=address /Zi /D_DISABLE_VECTOR_ANNOTATION /D_DISABLE_STRING_ANNOTATION /Fd"'+(Join-Path $directory 'harness.pdb')+'"').Trim()
        Write-Host "ASan $Contour $($case.name)"
        $result=Invoke-SanitizerCase -Name $case.name -Worker (Join-Path $PSScriptRoot 'SanitizerWorker.ps1') -WorkerArguments @('-CaseFile',$caseFile) -Directory $directory -Executable (Join-Path $OutputDirectory 'FBE.exe') -TimeoutSeconds $CaseTimeoutSeconds -ExpectedChildExitCodes $case.expectedChildExitCodes
        $summary.results+=@($result)
        $summary | ConvertTo-Json -Depth 12 | Set-Content $summaryPath -Encoding utf8
    }
    $summary.status=if(@($summary.results | Where-Object status -eq 'FAIL').Count) {'FAIL'} else {'PASS'}
    if($summary.results.Count -ne $summary.expectedCount) { throw 'Sanitizer case execution is incomplete.' }
} catch {
    $summary.status='FAIL'; $summary.error=$_.ToString()
    throw
} finally {
    $env:PATH=$savedPath; $env:_CL_=$savedCL; $env:ASAN_SYMBOLIZER_PATH=$savedSymbolizer
    $summary | ConvertTo-Json -Depth 12 | Set-Content $summaryPath -Encoding utf8
    Write-Host "Sanitizer report: $summaryPath"
}
if($summary.status -ne 'PASS') { throw "AddressSanitizer $Contour failed; see $summaryPath" }
Write-Host "AddressSanitizer $Contour PASS: $($summary.results.Count) runtime invocations."
