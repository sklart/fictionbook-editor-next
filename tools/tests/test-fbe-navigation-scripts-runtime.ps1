<# Exercises the navigation Scripts mode against a real FBE process and a
   nested user-script fixture. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [ValidateRange(30, 300)][int]$TimeoutSeconds = 120)

$ErrorActionPreference = 'Stop'
$FbeExe = (Resolve-Path -LiteralPath $FbeExe).Path
$exeDirectory = Split-Path -Parent $FbeExe
$portableIni = Join-Path $exeDirectory 'portable.ini'
$hadIni = Test-Path -LiteralPath $portableIni
$oldIni = if($hadIni) { Get-Content -LiteralPath $portableIni -Raw } else { $null }
$dataDirectory = [IO.Path]::GetFullPath((Join-Path $exeDirectory 'NavigationScriptsRuntime'))
if((Split-Path -Parent $dataDirectory) -ne [IO.Path]::GetFullPath($exeDirectory)) { throw 'Unsafe navigation runtime test directory.' }

try {
    [IO.File]::WriteAllText($portableIni, "[Portable]`r`nDataPath=NavigationScriptsRuntime`r`n", [Text.UTF8Encoding]::new($false))
    if(Test-Path -LiteralPath $dataDirectory) { Remove-Item -LiteralPath $dataDirectory -Recurse -Force }
    $scripts = Join-Path $dataDirectory 'Scripts'
    New-Item -ItemType Directory -Path (Join-Path $scripts 'FolderA'),(Join-Path $scripts 'FolderA\FolderB') -Force | Out-Null
    foreach($path in @('Root.js', 'FolderA\Child.js', 'FolderA\FolderB\Deep.js')) {
        [IO.File]::WriteAllText((Join-Path $scripts $path), "function Run() {}`r`n", [Text.UTF8Encoding]::new($false))
    }
    $repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    Copy-Item -LiteralPath (Join-Path $repoRoot 'src\export-docx\res\ExportDOCX.ico') -Destination (Join-Path $scripts 'Root.ico')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'packaging\nsis\res\fbe-wizard.bmp') -Destination (Join-Path $scripts 'FolderA\Child.bmp')
	Copy-Item -LiteralPath (Join-Path $repoRoot 'runtime\Scripts\01_Регистр.ico') -Destination (Join-Path $scripts 'FolderA.ico')
    $document = Join-Path $dataDirectory 'navigation-runtime.fb2'
    [IO.File]::WriteAllText($document, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Navigation runtime</book-title><lang>ru</lang></title-info></description><body><section><title><p>Test</p></title><p>Test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $savedMode = $env:FBE_NEXT_TEST_MODE; $savedScenario = $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'navigation-scripts-runtime'
        $process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @('--portable', $document) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Navigation scripts runtime test timed out.' }
        $report = Join-Path $dataDirectory 'Diagnostics\portable-state-report.txt'
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
        if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^script-image-size=20$' -or $text -notmatch '(?m)^script-legacy-expanders=1$' -or $text -notmatch '(?m)^script-metrics=1$' -or $text -notmatch '(?m)^result=pass$') { throw "Navigation scripts runtime failed:`n$text" }
        $env:FBE_NEXT_TEST_SCENARIO = 'navigation-scripts-reload-runtime'
        $process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @('--portable', $document) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Navigation scripts reload runtime test timed out.' }
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
        if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^phase=navigation-scripts-reload$' -or $text -notmatch '(?m)^result=pass$') { throw "Navigation scripts reload runtime failed:`n$text" }
    } finally { $env:FBE_NEXT_TEST_MODE = $savedMode; $env:FBE_NEXT_TEST_SCENARIO = $savedScenario }
    Write-Host 'Navigation scripts runtime regression passed.'
} finally {
    if($hadIni) { [IO.File]::WriteAllText($portableIni, $oldIni, [Text.UTF8Encoding]::new($false)) } else { Remove-Item -LiteralPath $portableIni -Force -ErrorAction SilentlyContinue }
    if(Test-Path -LiteralPath $dataDirectory) { Remove-Item -LiteralPath $dataDirectory -Recurse -Force -ErrorAction SilentlyContinue }
}
