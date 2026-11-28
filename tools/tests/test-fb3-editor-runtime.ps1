[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [string]$BatchOutputDirectory,
    [ValidateSet('portable', 'installed')][string[]]$Modes = @('portable', 'installed')
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
foreach ($mode in $Modes) {
    $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name "fb3-$mode"
    $passed = $false
    try {
        $null = New-IsolatedFbeSettingsSeed $iso
        $source = Join-Path $iso.Root 'Исходная.fb2'
        [IO.File]::WriteAllText($source, @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
<description><title-info><genre>prose</genre><author><nickname>Автор</nickname></author><book-title>Проверка FB3</book-title><lang>ru</lang></title-info>
<document-info><author><nickname>Редактор</nickname></author><date value="2026-11-28">28.11.2026</date><id>9e85883b-d85a-4d7e-b2bf-64db62d59f6b</id><version>1.0</version></document-info></description>
<body><section><title><p>Глава</p></title><p>FB3_RUNTIME_ORIGINAL <a l:href="#note-one" type="note">[1]</a> <a l:href="#note-two" type="note">[2]</a></p></section></body>
<body name="notes"><title><p>Примечания</p></title><section id="note-one"><p>Первая сноска.</p></section></body>
<body name="notes"><title><p>Комментарии</p></title><section id="note-two"><p>Вторая сноска.</p></section></body></FictionBook>
'@)
        $sourceHash = (Get-FileHash -LiteralPath $source).Hash
        $inputFolder = Join-Path $iso.Root 'input'
        $export = Join-Path $iso.Runtime 'ExportFB3Batch.exe'
        $import = Join-Path $iso.Runtime 'ImportFB3Batch.exe'
        if ($BatchOutputDirectory) {
            foreach ($name in 'ImportFB3Batch.exe', 'ExportFB3Batch.exe') {
                Copy-Item -LiteralPath (Join-Path $BatchOutputDirectory $name) -Destination $iso.Runtime -Force
            }
        }
        & $export $source --output $inputFolder
        if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the editor FB3 fixture.' }
        $inputPackage = Join-Path $inputFolder 'Исходная.fb3'
        $before = (Get-FileHash -LiteralPath $inputPackage).Hash
        $saved = Join-Path $iso.Root 'Сохранена.fb2'
        $exported = Join-Path $iso.Root 'Экспортирована.fb3'
        $report = Join-Path $iso.Root "fb3-$mode.txt"
        Set-IsolatedRuntimeStage $iso "fb3-editor/$mode"
        $environment = @{
            FBE_NEXT_TEST_MODE = '1'
            FBE_NEXT_TEST_SCENARIO = 'fb3-editor-runtime'
            FBE_NEXT_TEST_SETTINGS_DIRECTORY = (Join-Path $iso.DataPath 'Settings')
            FBE_NEXT_TEST_IMPORT_FB3_PATH = $inputPackage
            FBE_NEXT_TEST_EXPORT_FB3_PATH = $exported
            FBE_NEXT_TEST_SAVE_PATH = $saved
        }
        $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList @("--$mode", '-b', $report, $source) `
            -Environment $environment -WindowStyle Hidden -PassThru
        $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario "fb3-editor/$mode" -Report $report -TimeoutSeconds 55
        $reportText = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { 'missing report' }
        if ($code -ne 0 -or $reportText -notmatch 'result=PASS') { throw "FB3 $mode editor runtime failed (exit=$code): $reportText" }
        $reimport = Join-Path $iso.Root 'reimport'
        & $import $exported --output $reimport
        if ($LASTEXITCODE -ne 0) { throw 'Editor FB3 export could not be reimported.' }
        [xml]$reimportedBook = Get-Content -LiteralPath (Join-Path $reimport 'Экспортирована.fb2') -Raw -Encoding UTF8
        $ns = [Xml.XmlNamespaceManager]::new($reimportedBook.NameTable)
        $ns.AddNamespace('f', 'http://www.gribuser.ru/xml/fictionbook/2.0')
        if ($reimportedBook.DocumentElement.InnerText -notmatch 'FB3_RUNTIME_EDITED') {
            throw 'Editor export did not preserve the edited text.'
        }
        $notes = @($reimportedBook.SelectNodes('//f:body[@name="notes"]/f:section', $ns))
        $noteLinks = @($reimportedBook.SelectNodes('//f:a[@type="note"]', $ns))
        if ($notes.Count -ne 2 -or $noteLinks.Count -ne 2) { throw 'Editor round-trip changed the number of notes or note links.' }
        $groupTitles = @($reimportedBook.SelectNodes('//f:body[@name="notes"]/f:title/f:p', $ns) | ForEach-Object InnerText)
        if (($groupTitles -join '|') -cne 'Примечания|Комментарии') { throw 'Editor round-trip changed the two note groups.' }
        for ($index = 0; $index -lt $notes.Count; ++$index) {
            $id = $notes[$index].GetAttribute('id')
            $href = $noteLinks[$index].GetAttribute('href', 'http://www.w3.org/1999/xlink')
            $expected = @('Первая сноска.', 'Вторая сноска.')[$index]
            if (!$id -or $href -cne "#$id" -or $notes[$index].InnerText -cne $expected) {
                throw "Editor round-trip changed the target of note $($index + 1)."
            }
        }
        if ((Get-FileHash -LiteralPath $inputPackage).Hash -ne $before -or (Get-FileHash -LiteralPath $source).Hash -ne $sourceHash) {
            throw 'FB3 editor runtime modified its input files.'
        }
        Write-Host "FB3 $mode editor runtime passed."
        Write-Host $reportText
        $passed = $true
    } finally {
        Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed -PreserveContents
    }
}
