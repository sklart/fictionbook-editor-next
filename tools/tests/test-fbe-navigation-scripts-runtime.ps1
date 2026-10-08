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
	$savedMode = $env:FBE_NEXT_TEST_MODE; $savedScenario = $env:FBE_NEXT_TEST_SCENARIO; $savedLocale = $env:FBE_NEXT_UI_LOCALE
    try {
		$report = Join-Path $dataDirectory 'Diagnostics\portable-state-report.txt'
		$env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'navigation-scripts-runtime'; $env:FBE_NEXT_UI_LOCALE = 'en-US'
		$process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @('--portable', $document) -PassThru
		if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Navigation scripts runtime test timed out.' }
		$text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
		if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^script-image-size=20$' -or $text -notmatch '(?m)^script-legacy-expanders=1$' -or $text -notmatch '(?m)^script-metrics=1$' -or $text -notmatch '(?m)^refreshed-image-list-bound=1$' -or $text -notmatch '(?m)^viewbar-elements=1$' -or $text -notmatch '(?m)^bitmap-only-script=1$' -or $text -notmatch '(?m)^result=pass$') { throw "Navigation scripts runtime failed:`n$text" }
		foreach($locale in 'ru-RU','en-US','es-ES') {
			$env:FBE_NEXT_TEST_SCENARIO = 'navigation-viewbar-elements-runtime'; $env:FBE_NEXT_UI_LOCALE = $locale
			$process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @('--portable', $document) -PassThru
			if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "Document Tree view-bar runtime test timed out for $locale." }
			$text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
			$height = [regex]::Match($text, '(?m)^viewbar-height=(\d+)$'); $minimumHeight = [regex]::Match($text, '(?m)^viewbar-minimum-height=(\d+)$')
			if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^phase=navigation-viewbar-elements$' -or $text -notmatch '(?m)^viewbar-elements=1$' -or $text -notmatch '(?m)^viewbar-light-before=1$' -or $text -notmatch '(?m)^viewbar-dark=1$' -or $text -notmatch '(?m)^viewbar-popup-prepared-dark=1$' -or $text -notmatch '(?m)^viewbar-light-after=1$' -or $text -notmatch '(?m)^viewbar-popup-prepared-dark-again=1$' -or $text -notmatch '(?m)^viewbar-checkmarks-light-before=1$' -or $text -notmatch '(?m)^viewbar-checkmarks-dark-after-popup=1$' -or $text -notmatch '(?m)^viewbar-checkmarks-light-after=1$' -or $text -notmatch '(?m)^viewbar-checkmarks-dark-again=1$' -or $text -notmatch '(?m)^viewbar-checkmarks-dpi-100-125-150-200=1$' -or -not $height.Success -or -not $minimumHeight.Success -or ([int]$height.Groups[1].Value -lt [int]$minimumHeight.Groups[1].Value) -or $text -notmatch '(?m)^result=pass$') { throw "Document Tree view-bar runtime failed for ${locale}:`n$text" }
		}
        $env:FBE_NEXT_TEST_SCENARIO = 'navigation-scripts-reload-runtime'
		$env:FBE_NEXT_UI_LOCALE = 'en-US'
        $process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @('--portable', $document) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Navigation scripts reload runtime test timed out.' }
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
        if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^phase=navigation-scripts-reload$' -or $text -notmatch '(?m)^result=pass$') { throw "Navigation scripts reload runtime failed:`n$text" }
		$env:FBE_NEXT_TEST_SCENARIO = 'script-live-reload-runtime'
		$process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @('--portable', $document) -PassThru
		if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Live script reload runtime test timed out.' }
		$text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
		if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^phase=script-live-reload$' -or $text -notmatch '(?m)^version-1=1$' -or $text -notmatch '(?m)^version-2=1$' -or $text -notmatch '(?m)^result=pass$') { throw "Live script reload runtime failed:`n$text" }
		$env:FBE_NEXT_TEST_SCENARIO = 'script-catalog-refresh-runtime'
		$process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @('--portable', $document) -PassThru
		if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Script catalogue refresh runtime test timed out.' }
		$text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
		if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^phase=script-catalog-refresh$' -or $text -notmatch '(?m)^contents=1$' -or $text -notmatch '(?m)^added=1$' -or $text -notmatch '(?m)^removed=1$' -or $text -notmatch '(?m)^renamed=1$' -or $text -notmatch '(?m)^icon=1$' -or $text -notmatch '(?m)^uid=1$' -or $text -notmatch '(?m)^tree=1$' -or $text -notmatch '(?m)^document=1$' -or $text -notmatch '(?m)^result=pass$') { throw "Script catalogue refresh runtime failed:`n$text" }
		Remove-Item -LiteralPath $scripts -Recurse -Force
		New-Item -ItemType Directory -Path (Join-Path $scripts '01_Регистр'),(Join-Path $scripts '02_Чистка') -Force | Out-Null
		$scriptIndex = 0
		foreach($path in @('01_Регистр\Case.js', '01_Регистр\Latin.js', '02_Чистка\Cleanup.js', '02_Чистка\Dash.js', 'Tables.js')) {
			$scriptIndex++
			[IO.File]::WriteAllText((Join-Path $scripts $path), "function Run() { /* $scriptIndex */ }`r`n", [Text.UTF8Encoding]::new($false))
		}
		$env:FBE_NEXT_TEST_SCENARIO = 'navigation-search-favorites-runtime'
		$process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @('--portable', $document) -PassThru
		if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Navigation search and favorites runtime test timed out.' }
		$text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
		if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^phase=navigation-search-favorites$' -or $text -notmatch '(?m)^case-insensitive=1$' -or $text -notmatch '(?m)^parent-name=1$' -or $text -notmatch '(?m)^escape-focus=1$' -or $text -notmatch '(?m)^filtered=1$' -or $text -notmatch '(?m)^expanded=1$' -or $text -notmatch '(?m)^live-added=1$' -or $text -notmatch '(?m)^live-removed=1$' -or $text -notmatch '(?m)^cleared=1$' -or $text -notmatch '(?m)^favorite=1$' -or $text -notmatch '(?m)^reload=1$' -or $text -notmatch '(?m)^rename=1$' -or $text -notmatch '(?m)^remove=1$' -or $text -notmatch '(?m)^restore=1$' -or $text -notmatch '(?m)^themes=1$' -or $text -notmatch '(?m)^mode-switch=1$' -or $text -notmatch '(?m)^dpi-metrics=1$' -or $text -notmatch '(?m)^result=pass$') { throw "Navigation search and favorites runtime failed:`n$text" }
		$env:FBE_NEXT_TEST_SCENARIO = 'navigation-favorites-read-runtime'
		$process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @('--portable', $document) -PassThru
		if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Navigation favorites persistence runtime test timed out.' }
		$text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
		if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^phase=navigation-favorites-read$' -or $text -notmatch '(?m)^persisted=1$' -or $text -notmatch '(?m)^malformed-ignored=1$' -or $text -notmatch '(?m)^result=pass$') { throw "Navigation favorites persistence runtime failed:`n$text" }
	} finally { $env:FBE_NEXT_TEST_MODE = $savedMode; $env:FBE_NEXT_TEST_SCENARIO = $savedScenario; $env:FBE_NEXT_UI_LOCALE = $savedLocale }
    Write-Host 'Navigation scripts runtime regression passed.'
} finally {
    if($hadIni) { [IO.File]::WriteAllText($portableIni, $oldIni, [Text.UTF8Encoding]::new($false)) } else { Remove-Item -LiteralPath $portableIni -Force -ErrorAction SilentlyContinue }
    if(Test-Path -LiteralPath $dataDirectory) { Remove-Item -LiteralPath $dataDirectory -Recurse -Force -ErrorAction SilentlyContinue }
}
