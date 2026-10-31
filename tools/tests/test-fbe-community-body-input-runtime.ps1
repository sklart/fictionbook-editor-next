[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe,
      [string]$ArtifactDirectory = (Join-Path $PSScriptRoot '..\..\build\community-body-input'))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'community-body-input'
$names = @('FBE_NEXT_TEST_MODE', 'FBE_NEXT_TEST_SCENARIO', 'FBE_NEXT_TEST_BODY_INPUT_OUTPUT')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name) }
$passed = $false
try {
    New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts') | Out-Null
    $fixture = Join-Path $iso.DataPath 'input.fb2'; $report = Join-Path $iso.DataPath 'report.txt'
    $saved = Join-Path $iso.DataPath 'saved.fb2'
    $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Test</first-name><last-name>Author</last-name></author><book-title>Body input</book-title><lang>en</lang></title-info><document-info><author><first-name>Test</first-name><last-name>Author</last-name></author><date>2026</date><id>body-input</id><version>1.0</version></document-info></description><body><section><p id="input-first">Beginning text</p><p id="input-middle">Before <emphasis>formattedtext</emphasis> after</p><p id="input-last">Last paragraph text</p></section></body></FictionBook>'
    [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'community-body-input'; $env:FBE_NEXT_TEST_BODY_INPUT_OUTPUT = $saved
    $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden `
        -ArgumentList @('--portable', '-b', ('"' + $report + '"'), ('"' + $fixture + '"')) -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'community-body-input' -Report $report -TimeoutSeconds 180
    $text = if (Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing>' }
    $ArtifactDirectory = [IO.Path]::GetFullPath($ArtifactDirectory); New-Item -ItemType Directory -Force -Path $ArtifactDirectory | Out-Null
    foreach ($path in @($report, $fixture, $saved)) { if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination $ArtifactDirectory -Force } }
    (Get-FileHash -LiteralPath $FbeExe -Algorithm SHA256).Hash | Set-Content -LiteralPath (Join-Path $ArtifactDirectory 'exe-sha256.txt')
    if ($code -ne 0 -or $text -notmatch '(?m)^cases=12\r?$' -or $text -notmatch '(?m)^result=PASS\r?$') { throw "Body input runtime failed: $text" }
    $settings = [Xml.XmlReaderSettings]::new(); $settings.ValidationType = [Xml.ValidationType]::Schema
    $settings.Schemas.XmlResolver = [Xml.XmlUrlResolver]::new()
    [void]$settings.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0', (Join-Path $root 'runtime\FictionBook.xsd'))
    $reader = [Xml.XmlReader]::Create($saved, $settings); try { while ($reader.Read()) {} } finally { $reader.Dispose() }
    $passed = $true
    Write-Host 'Body input after Source transition: 12 cases, arrows, Undo/Redo, Save/Reopen and XSD PASS; Copy/Paste NOT TESTED.'
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name]) }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
