<# Normalizes the Windows console used by PowerShell, cmd.exe and native build tools. #>
[CmdletBinding()]
param()

$utf8 = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8
[Console]::OutputEncoding = $utf8
$OutputEncoding = $utf8

# chcp changes the shared console code page inherited by cmd.exe and native tools.
& cmd.exe /d /c 'chcp 65001 >nul'
if ($LASTEXITCODE -ne 0) {
    throw "Не удалось включить кодовую страницу UTF-8 для CI-консоли."
}
