<# Verifies that an ordinary build or validation did not rewrite tracked editor dependencies. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$trackedRuntimeDlls = @('runtime/Scintilla.dll', 'runtime/Lexilla.dll')

# This is deliberately a Git content check, rather than a contract test for
# build-script text: it catches a future copy step from any source.
$diffOutput = & git -C $repoRoot diff --exit-code -- @trackedRuntimeDlls 2>&1
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    $details = ($diffOutput | Out-String).Trim()
    $message = 'Обычная сборка или FAST validation изменили tracked dependency DLL: runtime/Scintilla.dll и/или runtime/Lexilla.dll. ' +
        'Результаты сборки должны оставаться в out; изменять tracked DLL разрешено только через apply-third-party-update-and-test.ps1 для Scintilla/Lexilla.'
    if ($details) { $message += "`nGit diff:`n$details" }
    throw $message
}

Write-Host 'Tracked Scintilla/Lexilla runtime DLLs are unchanged relative to Git.'
