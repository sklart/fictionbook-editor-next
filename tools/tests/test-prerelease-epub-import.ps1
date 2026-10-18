[CmdletBinding()]
param([string]$Importer)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if(-not $Importer){$Importer=Join-Path $repo 'out/Release/ImportEPUBBatch.exe'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$work=Join-Path $repo ('build/tests/epub-boundary-'+[guid]::NewGuid().ToString('N'))
$null=[IO.Directory]::CreateDirectory($work)
function Add-ZipText($zip,[string]$name,[string]$text) {
    $entry=$zip.CreateEntry($name);$writer=[IO.StreamWriter]::new($entry.Open(),[Text.UTF8Encoding]::new($false))
    try{$writer.Write($text)}finally{$writer.Dispose()}
}
function New-Epub([string]$name,[string]$chapter='chapter.xhtml',[string]$opf='book.opf',[switch]$Missing) {
    $path=Join-Path $work ($name+'.epub');$zip=[IO.Compression.ZipFile]::Open($path,'Create')
    try {
        Add-ZipText $zip 'mimetype' 'application/epub+zip'
        Add-ZipText $zip 'META-INF/container.xml' ('<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="'+[Security.SecurityElement]::Escape($opf)+'" media-type="application/oebps-package+xml"/></rootfiles></container>')
        Add-ZipText $zip 'book.opf' ('<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Boundary</dc:title><dc:language>en</dc:language><dc:identifier id="id">boundary</dc:identifier></metadata><manifest><item id="one" href="'+[Security.SecurityElement]::Escape($chapter)+'" media-type="application/xhtml+xml"/><item id="two" href="last.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="one"/><itemref idref="two"/></spine></package>')
        if(-not $Missing){Add-ZipText $zip 'chapter.xhtml' '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>First</title></head><body><p>FIRST_REQUIRED_CHAPTER</p></body></html>'}
        Add-ZipText $zip 'last.xhtml' '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Last</title></head><body><p>LAST_REQUIRED_CHAPTER</p></body></html>'
    } finally {$zip.Dispose()}
    return $path
}
function Invoke-Import([string]$path,[string]$name,[bool]$success,[switch]$Reparse) {
    $output=Join-Path $work ($name+'.fb2');$log=Join-Path $work ($name+'.txt')
    $process=[Diagnostics.Process]::new()
    $process.StartInfo.FileName=$Importer
    $process.StartInfo.WorkingDirectory=$repo
    $process.StartInfo.Arguments='"{0}" "{1}" --no-images --no-cover --no-notes --no-validate' -f $path,$output
    $process.StartInfo.UseShellExecute=$false;$process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true;$process.StartInfo.RedirectStandardError=$true
    $null=$process.Start();$stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
    if($Reparse) {
        $deadline=[DateTime]::UtcNow.AddSeconds(5)
        while(-not [IO.File]::Exists($env:FBE_NEXT_TEST_EPUB_ROOT_REPORT) -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 20}
        $extracted=[IO.File]::ReadAllText($env:FBE_NEXT_TEST_EPUB_ROOT_REPORT).Trim()
        $parent=[IO.Path]::GetDirectoryName($extracted)
        if([IO.Path]::GetFileName($extracted) -ne 'epub' -or [IO.Path]::GetFileName($parent) -notmatch '^ImportEPUB_[A-F0-9-]{36}$' -or [IO.Path]::GetDirectoryName($parent).TrimEnd('\') -ine [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')){throw 'Unexpected test-owned extraction path'}
        $null=New-Item -ItemType Junction -Path (Join-Path $extracted 'redirect') -Target $work
    }
    if(-not $process.WaitForExit(75000)){Stop-Process -Id $process.Id -Force;throw "Import timeout: $log"}
    $process.WaitForExit();[IO.File]::WriteAllText($log,$stdout.Result);[IO.File]::WriteAllText($log+'.stderr',$stderr.Result)
    $code=$process.ExitCode
    if($success) {
        if($code -ne 0 -or -not [IO.File]::Exists($output)){throw "Valid import failed ($code): $log"}
        $text=[IO.File]::ReadAllText($output)
        if(-not $text.Contains('FIRST_REQUIRED_CHAPTER') -or -not $text.Contains('LAST_REQUIRED_CHAPTER')){throw 'Required chapter was silently lost'}
    } elseif($code -eq 0 -or [IO.File]::Exists($output)){throw "Unsafe/incomplete EPUB accepted: $name"}
}
$oldMode,$oldDelay,$oldRootReport=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_EPUB_DELAY_ENTRY,$env:FBE_NEXT_TEST_EPUB_ROOT_REPORT
try {
    $valid=New-Epub 'valid';Invoke-Import $valid 'valid' $true
    Invoke-Import (New-Epub 'relative' -chapter 'sub/../chapter.xhtml') 'relative' $true
    Invoke-Import (New-Epub 'encoded-relative' -chapter '%63hapter.xhtml') 'encoded-relative' $true
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_EPUB_DELAY_ENTRY='last.xhtml'
    $watch=[Diagnostics.Stopwatch]::StartNew();Invoke-Import $valid 'delayed' $true
    if($watch.ElapsedMilliseconds -lt 1100){throw 'Delayed extraction was not exercised'}
    $env:FBE_NEXT_TEST_EPUB_ROOT_REPORT=Join-Path $work 'extracted-root.txt'
    $sentinel=Join-Path $work 'external-sentinel.txt';[IO.File]::WriteAllText($sentinel,'external data must survive cleanup')
    $sentinelHash=(Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash
    Invoke-Import $valid 'reparse' $false -Reparse
    if((Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash -cne $sentinelHash){throw 'Reparse cleanup touched external data'}
    $env:FBE_NEXT_TEST_EPUB_ROOT_REPORT=$null
    $env:FBE_NEXT_TEST_EPUB_DELAY_ENTRY=$null
    Invoke-Import (New-Epub 'missing' -Missing) 'missing' $false
    $outside=Join-Path $work 'external.xhtml';[IO.File]::WriteAllText($outside,'EXTERNAL_SECRET_MUST_NOT_BE_READ')
    $cases=@('../external.xhtml','%2e%2e%2fexternal.xhtml','..\external.xhtml','/external.xhtml',$outside,('file:///'+$outside.Replace('\','/')),'\\server\share\external.xhtml')
    for($i=0;$i -lt $cases.Count;++$i){Invoke-Import (New-Epub ('unsafe-'+$i) -chapter $cases[$i]) ('unsafe-'+$i) $false}
    Invoke-Import (New-Epub 'opf-outside' -opf ('../external.xhtml')) 'opf-outside' $false
    Write-Host "PASS: complete and delayed Shell extraction, missing mandatory chapter, OPF and manifest path confinement. Evidence: $work"
} finally {$env:FBE_NEXT_TEST_MODE=$oldMode;$env:FBE_NEXT_TEST_EPUB_DELAY_ENTRY=$oldDelay;$env:FBE_NEXT_TEST_EPUB_ROOT_REPORT=$oldRootReport}
