[CmdletBinding()]
param(
    [string]$FilePath = "",
    [string]$SchemaRoot = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

if ([string]::IsNullOrWhiteSpace($FilePath)) {
    $FilePath = Join-Path $PSScriptRoot "fb2-validate-valid-smoke.fb2"
}
$FilePath = (Resolve-Path -LiteralPath $FilePath).Path

if ([string]::IsNullOrWhiteSpace($SchemaRoot)) {
    $SchemaRoot = Join-Path $repoRoot "runtime"
}
$SchemaRoot = (Resolve-Path -LiteralPath $SchemaRoot).Path

$mainSchemaPath = Join-Path $SchemaRoot "FictionBook.xsd"
$genresSchemaPath = Join-Path $SchemaRoot "FictionBookGenres.xsd"
$linksSchemaPath = Join-Path $SchemaRoot "FictionBookLinks.xsd"
$langSchemaPath = Join-Path $SchemaRoot "FictionBookLang.xsd"

foreach ($schemaPath in @($mainSchemaPath, $genresSchemaPath, $linksSchemaPath, $langSchemaPath)) {
    if (-not (Test-Path -LiteralPath $schemaPath -PathType Leaf)) {
        throw "Не найден XSD-файл: $schemaPath"
    }
}

$schemaSet = [System.Xml.Schema.XmlSchemaSet]::new()
$schemaSet.XmlResolver = [System.Xml.XmlUrlResolver]::new()
$null = $schemaSet.Add("http://www.gribuser.ru/xml/fictionbook/2.0", $mainSchemaPath)
$null = $schemaSet.Add("http://www.gribuser.ru/xml/fictionbook/2.0/genres", $genresSchemaPath)
$null = $schemaSet.Add("http://www.w3.org/1999/xlink", $linksSchemaPath)
$null = $schemaSet.Add("http://www.w3.org/XML/1998/namespace", $langSchemaPath)
$schemaSet.Compile()

$settings = [System.Xml.XmlReaderSettings]::new()
$settings.Schemas = $schemaSet
$settings.ValidationType = [System.Xml.ValidationType]::Schema
$settings.DtdProcessing = [System.Xml.DtdProcessing]::Prohibit
$settings.ValidationFlags = [System.Xml.Schema.XmlSchemaValidationFlags]::ProcessInlineSchema -bor
    [System.Xml.Schema.XmlSchemaValidationFlags]::ProcessSchemaLocation -bor
    [System.Xml.Schema.XmlSchemaValidationFlags]::ReportValidationWarnings

$issues = New-Object System.Collections.Generic.List[string]
$settings.add_ValidationEventHandler({
    param($sender, $eventArgs)
    $line = if ($eventArgs.Exception) { $eventArgs.Exception.LineNumber } else { 0 }
    $position = if ($eventArgs.Exception) { $eventArgs.Exception.LinePosition } else { 0 }
    $issues.Add(("{0}: строка {1}, позиция {2}: {3}" -f $eventArgs.Severity, $line, $position, $eventArgs.Message))
})

$reader = [System.Xml.XmlReader]::Create($FilePath, $settings)
try {
    while ($reader.Read()) {
    }
}
finally {
    $reader.Dispose()
}

if ($issues.Count -gt 0) {
    Write-Host "XSD-проверка FB2 завершилась с замечаниями:"
    foreach ($issue in $issues) {
        Write-Host "  $issue"
    }
    throw "Файл не прошёл XSD-валидацию: $FilePath"
}

Write-Host "XSD-валидация FB2 прошла успешно."
Write-Host "  Файл: $FilePath"
Write-Host "  Схема: $mainSchemaPath"
Write-Host "  Следующий ручной шаг: откройте этот .fb2 через shell-команду Validate и убедитесь, что FBV не показывает ошибок."
