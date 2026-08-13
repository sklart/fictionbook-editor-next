<#
.SYNOPSIS
Saves and reopens representative FBD files with the production editor.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$fixtures = Join-Path $PSScriptRoot 'fixtures\fbd'
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-fbd-roundtrip-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)

function Invoke-FbeBatch([string]$File, [string]$Report, [bool]$Save) {
    $arguments = if ($Save) { @('-s', '-b', $Report, $File) } else { @('-b', $Report, $File) }
    $process = Start-Process -FilePath $FbeExe -ArgumentList $arguments -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        throw "FBE не завершил FBD round-trip: $File"
    }
    if ($process.ExitCode -ne 0) { throw "FBE вернул код $($process.ExitCode) для FBD: $File" }
    if (-not (Test-Path -LiteralPath $Report)) { throw "FBE не создал batch-отчёт: $File" }
}

try {
    foreach ($fixtureName in 'description_only.fbd', 'with_cover.fbd', 'unicode_metadata.fbd') {
        $file = Join-Path $directory $fixtureName
        Copy-Item -LiteralPath (Join-Path $fixtures $fixtureName) -Destination $file
        Invoke-FbeBatch $file (Join-Path $directory ($fixtureName + '.save.tsv')) $true
        Invoke-FbeBatch $file (Join-Path $directory ($fixtureName + '.reopen.tsv')) $false
        $text = Get-Content -LiteralPath $file -Raw
        if ($text -notmatch '<description>') { throw "Production Save потерял description в $fixtureName." }
        if ($fixtureName -eq 'description_only.fbd' -and $text -match '<body(?:\s|>)') {
            throw 'Production Save записал синтетическое тело в body-less FBD.'
        }
        if ($fixtureName -eq 'with_cover.fbd' -and $text -notmatch 'cover\.jpg') {
            throw 'Production Save потерял ссылку на обложку FBD.'
        }
        if ($fixtureName -eq 'unicode_metadata.fbd' -and $text -notmatch 'Zoë|déjà vu') {
            throw 'Production Save потерял Unicode metadata FBD.'
        }
    }
    Write-Host 'Production FBD Save -> reopen round-trip passed.'
}
finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
