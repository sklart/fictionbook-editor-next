<# Exercises BODY <-> SOURCE transitions through the real FBE lifecycle. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 90,
    [ValidateRange(6, 20000)]
    [int]$ParagraphCount = 6
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-body-source-transition-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $fixture = Join-Path $directory 'transition.fb2'; $report = Join-Path $directory 'transition.txt'
    $filler = @(if ($ParagraphCount -gt 6) {
        7..$ParagraphCount | ForEach-Object { "<p>PHASE_B_BENCHMARK paragraph $_ " + ('source serialization search snapshot cache ' * 8) + '</p>' }
    })
    $split = [int][Math]::Floor($filler.Count / 2)
    $earlyFiller = @($filler | Select-Object -First $split)
    $lateFiller = @($filler | Select-Object -Skip $split)
    $prefix = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>transition</book-title><lang>en</lang></title-info><document-info><id>transition-test</id><version>1.0</version></document-info></description><body><section><title><p id="body-source-title">TITLE_BEGIN title selection TITLE_END</p></title><annotation><p id="body-source-annotation">ANNOTATION_BEGIN annotation selection ANNOTATION_END</p></annotation><p id="body-source-unicode">UNICODE_BEGIN ГРАНЬ_UNICODE UNICODE_END</p><p id="body-source-inline">PREFIX <strong>INLINE_FORMATTING</strong> SUFFIX</p><p id="body-source-repeat">REPEAT_TOKEN middle REPEAT_TOKEN_REPEAT_END</p><p id="body-source-boundary">BOUNDARY_BEGIN <emphasis>INLINE_BOUNDARY</emphasis> BOUNDARY_END</p><p id="body-source-caret">CARET_UNICODE tail</p><section id="body-source-pair"><p>P_TO_P_BEGIN first paragraph</p><p>second paragraph P_TO_P_END</p></section><section id="body-source-nested"><section><p>NESTED_BEGIN nested section NESTED_END</p></section></section><p>BODY_SOURCE_ORIGINAL</p>'
    $prefix = $prefix.Replace('<p id="body-source-unicode">', ($earlyFiller -join '') + '<p id="body-source-unicode">')
    if ($ParagraphCount -ge 100) {
        $selectionOffset = $prefix.IndexOf('id="body-source-unicode"', [StringComparison]::Ordinal)
        $totalLength = $prefix.Length + ($lateFiller -join '').Length
        if ($selectionOffset -lt $totalLength * 0.4 -or $selectionOffset -gt $totalLength * 0.6) { throw 'Selection fixture is not near the middle of the book.' }
    }
    @($prefix, ($lateFiller -join ''), '</section></body></FictionBook>') | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'body-source-transition-runtime'
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил BODY/SOURCE transition scenario.' }
		$timer.Stop()
        if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { $diagnostics = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<report missing>' }; throw "FBE BODY/SOURCE transition scenario failed: exit $($process.ExitCode).`n$diagnostics" }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    $rows = @{}; foreach($line in Get-Content -LiteralPath $report) { $parts = $line -split '='; if($parts.Count -eq 2) { $rows[$parts[0]] = $parts[1] } }
    foreach($key in @('source_checkpoint_current','checkpoint_reused','checkpoint_forced','body_checkpoint_current','body_browser_fits','folded_selection_restored','source_focus_after_selection','source_typing_after_selection','source_typing_undo')) { if($rows[$key] -ne '1') { throw "BODY/SOURCE checkpoint/layout check failed: $key (value '$($rows[$key])')." } }
    foreach($key in @('source_active','source_current','body_without_change','valid_edit','edited_source','cycles','invalid_rejected','invalid_preserved','selection_saved','title_selection','title_caret_selection','annotation_selection','unicode_inline_selection','inline_selection','repeated_text_selection','tag_boundary_selection','paragraph_pair_selection','nested_section_selection','collapsed_caret_selection','body_to_source_selection','description_to_source_no_selection')) { if($rows[$key] -ne '1') { throw "BODY/SOURCE transition check failed: $key (value '$($rows[$key])')." } }
    Write-Host "FBE BODY/SOURCE transition runtime passed: paragraphs=$ParagraphCount, fixture_bytes=$((Get-Item -LiteralPath $fixture).Length), elapsed_ms=$($timer.ElapsedMilliseconds)."
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
