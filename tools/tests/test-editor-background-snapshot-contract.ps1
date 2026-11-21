[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'test-editor-background-settings.ps1'), [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Background settings test does not parse.' }
$function = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-Snapshot' }, $true)
if (-not $function) { throw 'Background snapshot function is missing.' }
. ([scriptblock]::Create($function.Extent.Text))
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-snapshot-' + [guid]::NewGuid().ToString('N'))
$file = Join-Path $directory '[profile].xml'
try {
    New-Item -ItemType Directory -Path $directory | Out-Null
    [IO.File]::WriteAllText($file, 'before')
    $before = Get-Snapshot $directory
    [IO.File]::WriteAllText($file, 'after')
    $after = Get-Snapshot $directory
    if (-not $before.Contains('[profile].xml|') -or $before -eq $after) { throw 'Snapshot failed to detect a change to a literal wildcard filename.' }
    Write-Host 'Background snapshot literal-path contract passed.'
} finally {
    if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
    if (Test-Path -LiteralPath $directory) { Remove-Item -LiteralPath $directory -Force }
}
