<# Normalizes the Windows console used by PowerShell, cmd.exe and native build tools. #>
[CmdletBinding()]
param()

$utf8 = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8
[Console]::OutputEncoding = $utf8
$OutputEncoding = $utf8

# chcp changes the shared console code page inherited by native tools. cmd.exe
# drops an inherited PATH above its 8191-character limit, so resolve this system
# tool directly rather than asking cmd to find it through PATH.
$chcp = Join-Path $env:SystemRoot 'System32\chcp.com'
& $chcp 65001 > $null
if ($LASTEXITCODE -ne 0) {
    throw "Не удалось включить кодовую страницу UTF-8 для CI-консоли."
}
