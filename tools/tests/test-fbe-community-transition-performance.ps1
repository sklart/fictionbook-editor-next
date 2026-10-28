[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$BaselineExe,
 [Parameter(Mandatory)][string]$CandidateExe,
 [Parameter(Mandatory)][string]$EvidenceDirectory,
 [int[]]$SizesMiB=@(1,20,50),
 [ValidateRange(1,600)][int]$TimeoutSeconds=600
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot RuntimeTestIsolation.ps1)
$evidence=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceDirectory)
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TRACE','FBE_NEXT_TEST_PERF_LEGACY');$saved=@{};foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
$utf8=[Text.UTF8Encoding]::new($false)
$summary=[Collections.Generic.List[object]]::new()
try {
 foreach($size in $SizesMiB) {
  $fixture=Join-Path $evidence "fixture-$size.fb2"
  if(!(Test-Path -LiteralPath $fixture)) {
   $header='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><nickname>Fixture</nickname></author><book-title>Followup performance</book-title><lang>ru</lang></title-info><document-info><author><nickname>Fixture</nickname></author><date value="2026-10-28">2026-10-28</date><id>followup-perf</id><version>1.0</version></document-info></description><body>'
   $tail='<section><p id="perf-last">Last Unicode '+[char]0x0416+' '+[char]0x00a0+' '+[char]0x03a9+' repeated text <strong>bold</strong> tail</p></section></body><body name="notes"><section id="note"><p>Note text</p></section></body></FictionBook>'
   $line='<p>Repeated identical phrase '+[char]0x0416+' '+[char]0x00a0+' '+[char]::ConvertFromUtf32(0x1F600)+' &amp; <strong>bold <emphasis>inline</emphasis></strong> <a l:href="#note" type="note">1</a> '+('ordinary prose ' * 20)+'</p>'
   $chunk='<section><title><p>Repeated section</p></title><section>'+($line*8)+'</section><section><p>Nested repeated phrase</p></section></section>'
   $writer=[IO.StreamWriter]::new($fixture,$false,$utf8);$bytes=$utf8.GetByteCount($header)+$utf8.GetByteCount($tail);$chunkBytes=$utf8.GetByteCount($chunk)
   try{$writer.Write($header);while($bytes -lt $size*1MB){$writer.Write($chunk);$bytes+=$chunkBytes};$writer.Write($tail)}finally{$writer.Dispose()}
  }
  $schema=New-Object -ComObject Msxml2.XMLSchemaCache.6.0
  $schema.add('http://www.gribuser.ru/xml/fictionbook/2.0',(Join-Path $PSScriptRoot '../../runtime/FictionBook.xsd'))
  $validation=New-Object -ComObject Msxml2.DOMDocument.6.0;$validation.async=$false;$validation.schemas=$schema
  try{if(!$validation.load($fixture)){throw "Invalid performance FB2: $($validation.parseError.reason)"}}finally{[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($validation);[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($schema)}
  Get-FileHash -LiteralPath $fixture | ConvertTo-Json | Set-Content (Join-Path $evidence "fixture-$size-hash.json")
  foreach($entry in @(@{Name='baseline';Exe=$BaselineExe},@{Name='candidate';Exe=$CandidateExe})) {
   $iso=New-IsolatedFbeRuntime -FbeExe $entry.Exe -Name transition-perf;$passed=$false
   try {
    $null=New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root empty-scripts)
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='community-transition-performance';$env:FBE_NEXT_TRACE='0';$env:FBE_NEXT_TEST_PERF_LEGACY=if($entry.Name -eq 'baseline'){'1'}else{'0'}
    $report=Join-Path $evidence ("{0}-{1}.tsv" -f $entry.Name,$size)
    $process=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $exitCode=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
    Get-FileHash -LiteralPath $entry.Exe | ConvertTo-Json | Set-Content (Join-Path $evidence ($entry.Name+'-exe.json'))
    if($exitCode -ne 0){throw "Performance process failed: $($entry.Name), $size MiB, exit=$exitCode"}
    $rows=@(Import-Csv -LiteralPath $report -Delimiter "`t" | Where-Object { [int]$_.repeat -ge 0 })
    if($rows.Count -ne 10){throw "Expected five measured caret and range repeats: $report"}
    foreach($kind in 'caret','range') {
     $samples=@($rows | Where-Object kind -eq $kind)
     foreach($metric in @('body_source_ms','source_body_ms','index_ms','map_body_ms','map_source_ms','private_bytes','peak_private_bytes','index_live_delta','heartbeat_max_ms','heartbeat_timeouts')) {
      $values=@($samples | ForEach-Object { [double]::Parse($_.$metric,[Globalization.CultureInfo]::InvariantCulture) } | Sort-Object)
      $summary.Add([pscustomobject]@{Build=$entry.Name;SizeMiB=$size;FixtureBytes=(Get-Item $fixture).Length;Kind=$kind;Metric=$metric;Median=$values[2];Minimum=$values[0];Maximum=$values[4]})
     }
    }
    $passed=$true;Write-Host "PASS measurements: $($entry.Name), $size MiB"
   } finally {Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed;$summary | Export-Csv -LiteralPath (Join-Path $evidence summary.csv) -NoTypeInformation}
  }
 }
} finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name])}}
