[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$FbeExe,
    [ValidateRange(150, 300)][int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE.exe: $FbeExe" }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-cli-юникод-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
try {
    $archive = Join-Path $root 'книга с пробелом.zip'; $report = Join-Path $root 'отчёт с пробелом.txt'
    $book = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>CLI</first-name><last-name>Test</last-name></author><book-title>CLI</book-title><lang>ru</lang></title-info><document-info><program-used>FBE Next</program-used><id>22222222-2222-2222-2222-222222222222</id><version>1.0</version></document-info></description><body><section><p>CLI_UNICODE</p></section></body></FictionBook>'
    $stream = [IO.File]::Open($archive, [IO.FileMode]::Create)
    try { $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create); try { $entry = $zip.CreateEntry('книга.fb2'); $writer = [IO.StreamWriter]::new($entry.Open(), [Text.UTF8Encoding]::new($false)); try { $writer.Write($book) } finally { $writer.Dispose() } } finally { $zip.Dispose() } } finally { $stream.Dispose() }
    $oldMode,$oldScenario = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='archive-open-runtime'
        $quote = { param([string]$value) '"' + $value.Replace('"', '\"') + '"' }
        $arguments = '--portable -b {0} {1}' -f (& $quote $report), (& $quote $archive)
        $process = Start-Process -FilePath $FbeExe -ArgumentList $arguments -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "CLI regression timed out after $TimeoutSeconds seconds." }
        $state = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
        if($process.ExitCode -ne 0) { throw "CLI regression exited $($process.ExitCode): $state" }
        # Successful loading of this Unicode-named archive proves that the
        # complete CLI argument reached the resolver. The diagnostic report is
        # deliberately ANSI for Win7 compatibility, so do not compare its
        # Unicode entry spelling byte-for-byte.
        foreach($line in @('archive=1','fb2=1','mshtml=1')) { if($state -notmatch [regex]::Escape($line)) { throw "CLI regression report missing $line" } }
    } finally {
        foreach($pair in @(@('FBE_NEXT_TEST_MODE',$oldMode),@('FBE_NEXT_TEST_SCENARIO',$oldScenario))) { if($null -eq $pair[1]) { Remove-Item ("Env:" + $pair[0]) -ErrorAction SilentlyContinue } else { Set-Item ("Env:" + $pair[0]) $pair[1] } }
    }
    Write-Host 'FBE CLI switches, spaces, and Unicode path regression passed.'
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
