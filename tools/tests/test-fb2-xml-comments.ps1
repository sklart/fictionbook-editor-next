[CmdletBinding()]
param([string] $RepositoryRoot)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
} else {
    $RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
}

$fixture = Join-Path $PSScriptRoot "fb2-xml-comments-source.fb2"
$document = [System.Xml.XmlDocument]::new()
$document.PreserveWhitespace = $true
$document.Load($fixture)
$comments = @($document.SelectNodes("//comment()"))
$expected = @(
    "before title-info", "before document-info", "before first section",
    "between paragraphs", "between sections", "after body, inside FictionBook"
)
if ($comments.Count -ne $expected.Count) {
    throw "Ожидалось $($expected.Count) XML-комментариев, найдено $($comments.Count)."
}
for ($index = 0; $index -lt $expected.Count; ++$index) {
    if ($comments[$index].Value.Trim() -ne $expected[$index]) {
        throw "Комментарий $index имеет неожиданное значение: '$($comments[$index].Value)'."
    }
}

& (Join-Path $PSScriptRoot "test-fbv-fixture.ps1") -FilePath $fixture
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Фикстур XML-комментариев структурно корректен и проходит XSD-проверку."