<# Verify the redirection and Unicode decoding used by the script fallbacks. #>
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\build'))
$testDirectory = Join-Path $root ('script powershell output ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testDirectory -ErrorAction Stop | Out-Null
try {
    $shell = New-Object -ComObject WScript.Shell
    $fso = New-Object -ComObject Scripting.FileSystemObject
    $output = $fso.BuildPath($testDirectory, $fso.GetTempName())
    $command = 'powershell -ExecutionPolicy Bypass -command "[Console]::OutputEncoding=[System.Text.Encoding]::Unicode; Write-Output FBE_TEST" > "' + $output + '" 2>&1'
    $exitCode = $shell.Run($shell.ExpandEnvironmentStrings('%ComSpec%') + ' /d /s /c "' + $command + '"', 0, $true)
    if ($exitCode -ne 0 -or -not $fso.FileExists($output)) { throw 'PowerShell output was not created.' }
    $reader = $fso.OpenTextFile($output, 1, $false, -1)
    try { $value = $reader.ReadAll().Trim() } finally { $reader.Close() }
    if ($value -cne 'FBE_TEST') { throw 'Unicode result was not decoded as expected.' }
    Write-Host 'WScript.Shell, cmd redirection and FSO Unicode decoding passed.'
} finally {
    $resolved = (Resolve-Path -LiteralPath $testDirectory).Path
    if (-not $resolved.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe cleanup target.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
