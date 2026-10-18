<# Builds and executes direct writer ownership and rollback coverage for ExportHTML. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('fbe-export-html-writer-' + $PID)
$out = Join-Path $scratch 'export-html-writer.exe'
$obj = Join-Path $scratch 'obj'
New-Item -ItemType Directory -Force -Path $obj | Out-Null
try {
    & cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE "/I$(Join-Path $root 'src\export-html')" "/I$(Join-Path $root 'third_party')" "/Fo:$obj\\" `
        (Join-Path $PSScriptRoot 'export-html-writer-harness.cpp') `
        (Join-Path $root 'src\export-html\HtmlExportWriter.cpp') `
        (Join-Path $root 'src\export-html\HtmlExportWriterHelpers.cpp') `
        (Join-Path $root 'src\common\fb2\Fb2BinaryInspector.cpp') `
        /link /OUT:$out ole32.lib oleaut32.lib comsuppw.lib msxml6.lib
    if ($LASTEXITCODE -ne 0) { throw 'ExportHTML writer harness did not compile.' }
    & $out
    if ($LASTEXITCODE -ne 0) { throw "ExportHTML writer harness failed: $LASTEXITCODE" }
    $existing=Join-Path $scratch 'existing.html'
    [IO.File]::WriteAllText($existing,'previous valid result',[Text.UTF8Encoding]::new($false))
    $before=(Get-FileHash -LiteralPath $existing -Algorithm SHA256).Hash
    foreach($failure in 'cancel','write-fail','short-write','commit-fail') {
        & $out $failure $existing
        if($LASTEXITCODE -ne 0 -or (Get-FileHash -LiteralPath $existing -Algorithm SHA256).Hash -cne $before){throw "Existing HTML changed during $failure"}
    }
    Write-Host "PASS: existing HTML SHA-256 preserved on cancellation, write failure, short write and replacement failure: $before"
    $first=Join-Path $scratch 'first.html';$second=Join-Path $scratch 'second.html'
    [IO.File]::WriteAllText($first,'original first');[IO.File]::WriteAllText($second,'original second')
    $firstHash=(Get-FileHash -LiteralPath $first -Algorithm SHA256).Hash;$secondHash=(Get-FileHash -LiteralPath $second -Algorithm SHA256).Hash
    & $out 'multi-fail' $existing
    if($LASTEXITCODE -ne 0 -or (Get-FileHash -LiteralPath $first -Algorithm SHA256).Hash -cne $firstHash -or (Get-FileHash -LiteralPath $second -Algorithm SHA256).Hash -cne $secondHash){throw 'Multi-file HTML rollback changed an existing document'}
    if(@(Get-ChildItem -LiteralPath $scratch -Filter 'fbe*.tmp').Count){throw 'HTML transaction leaked staging/backup files'}
    $transactionTest = Join-Path $scratch 'transaction.exe'
    & cl.exe /nologo /EHsc /W4 /WX /std:c++17 /utf-8 "/Fo:$obj\\" (Join-Path $PSScriptRoot 'export-file-transaction-harness.cpp') /link /OUT:$transactionTest
    if ($LASTEXITCODE -ne 0) { throw 'Transaction failure harness did not compile' }
    foreach ($mode in 'restore-succeeds','restore-fails') {
        [IO.File]::WriteAllText($existing,'previous valid result',[Text.UTF8Encoding]::new($false))
        & $transactionTest $mode $existing
        if ($LASTEXITCODE -ne 0) { throw "Partial replacement probe failed: $mode" }
        $backups = @(Get-ChildItem -LiteralPath $scratch -Filter 'fbe*.tmp')
        if ($mode -eq 'restore-succeeds') {
            if ((Get-FileHash -LiteralPath $existing -Algorithm SHA256).Hash -cne $before -or $backups.Count) { throw 'Partial replacement did not restore the original' }
        } else {
            if ((Test-Path -LiteralPath $existing) -or $backups.Count -ne 1 -or (Get-FileHash -LiteralPath $backups[0].FullName -Algorithm SHA256).Hash -cne $before) { throw 'Failed restoration lost the original backup' }
            Remove-Item -LiteralPath $backups[0].FullName -Force
        }
    }
    Write-Host 'PASS: partial ReplaceFile failure restores the original, or retains its SHA-256-identical backup when restoration fails'
}
finally {
    Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath ($out -replace '\.exe$', '.pdb') -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue
}
$writerSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\export-html\HtmlExportWriter.cpp')
if ($writerSource -match 'ERROR_CANCELLED') { throw 'Writer must not translate cancellation into a writer failure.' }
Write-Host 'ExportHTML writer harness passed.'
