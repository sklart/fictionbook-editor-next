[CmdletBinding()] param([string]$Configuration = 'Release', [string]$RuntimeDirectory, [switch]$IncludeBatch)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $RuntimeDirectory) { $RuntimeDirectory = Join-Path $root "out\$Configuration" }
$dll = Join-Path $RuntimeDirectory "Plugins\ExportDOCX.dll"
if (-not (Test-Path -LiteralPath $dll)) { throw "Missing ExportDOCX.dll: $dll" }
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$apiDirectory = & (Join-Path $PSScriptRoot 'ensure-fbe-api.ps1') -Configuration $Configuration
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'export-docx-v2'
$exe = Join-Path $scratch.Root 'export-docx-v2-runtime.exe'
$objectDir = Join-Path $scratch.Root 'obj'
$oldTemp, $oldTmp = $env:TEMP, $env:TMP; $passed = $false; $docx = ''; $outputLog = Join-Path $scratch.Root 'harness.log'
try {
    $env:TEMP = $scratch.Root; $env:TMP = $scratch.Root
    New-Item -ItemType Directory -Path $objectDir | Out-Null
    & cl.exe /nologo /EHsc /std:c++14 /utf-8 /DFBE_TEST_EXPORT_DOCX /DUNICODE /D_UNICODE "/I$apiDirectory" "/Fo:$objectDir\\" (Join-Path $PSScriptRoot 'export-html-v2-runtime-harness.cpp') (Join-Path $apiDirectory 'FBE_i.c') /link ole32.lib oleaut32.lib "/OUT:$exe"
    if ($LASTEXITCODE -ne 0) { throw 'ExportDOCX v2 runtime harness did not compile.' }
    $process = Start-Process -FilePath $exe -ArgumentList @($dll) -Wait -PassThru -NoNewWindow -RedirectStandardOutput $outputLog
    if ($process.ExitCode -ne 0) { throw "ExportDOCX v2 runtime harness failed: $($process.ExitCode)" }
    $lines = @(Get-Content -LiteralPath $outputLog)
    $docxLine = [string]($lines | Where-Object { $_ -like 'DOCX=*' } | Select-Object -First 1)
    $docx = if ($docxLine.StartsWith('DOCX=')) { $docxLine.Substring(5).Trim() } else { '' }
    if ([string]::IsNullOrWhiteSpace($docx) -or -not (Test-Path -LiteralPath $docx)) { throw 'Runtime harness did not return DOCX output.' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem; $zip = [System.IO.Compression.ZipFile]::OpenRead($docx)
    try {
        $names = @($zip.Entries | ForEach-Object FullName)
        foreach ($required in @('[Content_Types].xml','_rels/.rels','word/document.xml','word/styles.xml')) { if ($names -notcontains $required) { throw "DOCX lacks required part: $required" } }
        $entry = $zip.GetEntry('word/document.xml'); if (-not $entry -or $entry.Length -eq 0) { throw 'DOCX document.xml is empty.' }
        $reader = [IO.StreamReader]::new($entry.Open(), [Text.Encoding]::UTF8, $true); try { $xml = $reader.ReadToEnd() } finally { $reader.Dispose() }
        if (-not $xml.Contains('Привет, мир')) { throw 'DOCX lost Cyrillic fixture text.' }
        foreach ($marker in @([string][char]0x00D0,[string][char]0x00D1)) { if ($xml.Contains($marker)) { throw 'DOCX contains mojibake.' } }
    } finally { $zip.Dispose() }
    if($IncludeBatch) {
        $batch=Join-Path $RuntimeDirectory 'ExportDOCXBatch.exe'
        $fixture=Join-Path $PSScriptRoot 'fb2-metadata-cyrillic-smoke.fb2'
        $batchOutput=Join-Path $objectDir ('batch-'+[guid]::NewGuid().ToString('N')+'.docx')
        & $batch $fixture $batchOutput -Quiet
        if($LASTEXITCODE -ne 0) { throw 'DOCX batch conversion failed.' }
        $batchZip=[IO.Compression.ZipFile]::OpenRead($batchOutput)
        try {
            $part=$batchZip.GetEntry('word/document.xml')
            if(-not $part -or -not $batchZip.GetEntry('[Content_Types].xml')) { throw 'DOCX batch package is incomplete.' }
            $reader=[IO.StreamReader]::new($part.Open(),[Text.Encoding]::UTF8,$true)
            try { $word=[xml]$reader.ReadToEnd() } finally { $reader.Dispose() }
            $source=[xml][IO.File]::ReadAllText($fixture)
            $expected=@($source.GetElementsByTagName('p','http://www.gribuser.ru/xml/fictionbook/2.0'))[0].InnerText
            if(-not $word.DocumentElement.InnerText.Contains($expected)) { throw 'DOCX batch lost fixture text.' }
        } finally { $batchZip.Dispose() }
        Remove-Item -LiteralPath $batchOutput -Force
        Write-Host 'DOCX batch conversion and package content passed.'
    }
    $passed = $true
} finally {
    $env:TEMP = $oldTemp; $env:TMP = $oldTmp
    if ($passed -and $docx) {
        Remove-Item -LiteralPath $docx -Force -ErrorAction SilentlyContinue
        $parent = Split-Path -Parent $docx
        if ($parent -and (Test-Path -LiteralPath $parent -PathType Container)) { try { [IO.Directory]::Delete($parent) } catch {} }
    }
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed -PreserveContents
}
