<# Audits localized General and Hotkeys controls in an isolated, visible FBE dialog. #>
[CmdletBinding()]
param([string]$FbeExe)
$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'settings-general-layout'
$temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
if (-not [IO.Path]::GetFullPath($isolation.Root).StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Unexpected isolated runtime path.'
}
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class SettingsGeneralLayoutWindow
{
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    public delegate bool Callback(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback, IntPtr data);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
    [DllImport("user32.dll")] static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window, IntPtr dc, uint flags);
    public static IntPtr Find(int processId, bool settings)
    {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, data) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner != (uint)processId) return true;
            var name = new StringBuilder(128);
            GetClassName(window, name, name.Capacity);
            if (settings ? GetDlgItem(window, 1625) != IntPtr.Zero : name.ToString() == "FictionBookEditorFrame")
            {
                found = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static void OpenSettings(IntPtr frame)
    {
        ShowWindow(frame, 5);
        PostMessage(frame, 0x0111, (IntPtr)32834, IntPtr.Zero);
    }
    public static void SelectHotkeys(IntPtr settings)
    {
        var navigation = GetDlgItem(settings, 1625);
        SendMessage(navigation, 0x0186, (IntPtr)5, IntPtr.Zero); // LB_SETCURSEL
        SendMessage(settings, 0x0111, (IntPtr)(1625 | (1 << 16)), navigation); // LBN_SELCHANGE
    }
    public static void Close(IntPtr window) { PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero); }
}
'@
$fixture = (Resolve-Path (Join-Path $PSScriptRoot 'fb2-validate-valid-smoke.fb2')).Path
try {
    & pwsh -NoProfile -File (Join-Path $PSScriptRoot '..\localization\export-runtime-lang.ps1') -OutputDirectory (Join-Path $isolation.Runtime 'Lang') -Clean | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Runtime localization export failed.' }
    $catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $PSScriptRoot '..\..\localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
    $languageIds = @{ 'ru-RU' = 0xFBE002; 'en-US' = 0xFBE001; 'de-DE' = 0xFBE004; 'uk-UA' = 0xFBE003; 'es-ES' = 0xFBE006; 'bg-BG' = 0xFBE00C }
    $settingsDirectory = Join-Path $isolation.DataPath 'Settings'
    New-Item -ItemType Directory -Path $settingsDirectory -Force | Out-Null
    $settingsTemplate = [IO.File]::ReadAllText((Join-Path $isolation.Runtime 'Data\Settings\Settings.xml'))
    if ($settingsTemplate -notmatch '<IntefaceLangID>[0-9]+</IntefaceLangID>') {
        throw 'The isolated settings template lacks the interface language.'
    }
    $languagePattern = [regex]::new('<IntefaceLangID>[0-9]+</IntefaceLangID>')
    foreach ($locale in 'ru-RU', 'en-US', 'de-DE', 'uk-UA', 'es-ES', 'bg-BG') {
        $process = $null
        try {
            $settings = $languagePattern.Replace($settingsTemplate,
                '<IntefaceLangID>' + $languageIds[$locale] + '</IntefaceLangID>', 1)
            [IO.File]::WriteAllText((Join-Path $settingsDirectory 'Settings.xml'), $settings, [Text.UTF8Encoding]::new($false))
            $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', ('"' + $fixture + '"')) -PassThru
            $frame = [IntPtr]::Zero
            for ($attempt = 0; $attempt -lt 60 -and $frame -eq [IntPtr]::Zero; ++$attempt) {
                Start-Sleep -Milliseconds 100
                if ($process.HasExited) { throw "FBE exited before opening Settings: $($process.ExitCode)" }
                $frame = [SettingsGeneralLayoutWindow]::Find($process.Id, $false)
            }
            if ($frame -eq [IntPtr]::Zero) { throw "$locale main frame missing." }
            [SettingsGeneralLayoutWindow]::OpenSettings($frame)
            $settings = [IntPtr]::Zero
            for ($attempt = 0; $attempt -lt 80; ++$attempt) {
                Start-Sleep -Milliseconds 100
                $settings = [SettingsGeneralLayoutWindow]::Find($process.Id, $true)
                if ($settings -ne [IntPtr]::Zero -and [SettingsGeneralLayoutWindow]::IsWindowVisible($settings)) { break }
            }
            if ($settings -eq [IntPtr]::Zero -or -not [SettingsGeneralLayoutWindow]::IsWindowVisible($settings)) {
                throw "$locale Settings dialog missing."
            }
            $report = Join-Path $PSScriptRoot "..\..\out\tests\settings-general-$locale.json"
            & (Join-Path $PSScriptRoot '..\ui\invoke-ui-layout-audit.ps1') -ProcessId $process.Id -Language $locale -OutputPath $report | Out-Null
            $audit = Get-Content -Raw -Encoding UTF8 -LiteralPath $report | ConvertFrom-Json
            if ($locale -eq 'ru-RU') {
                Add-Type -AssemblyName System.Drawing
                $rect = New-Object SettingsGeneralLayoutWindow+Rect
                [void][SettingsGeneralLayoutWindow]::GetWindowRect($settings, [ref]$rect)
                $bitmap = New-Object Drawing.Bitmap(($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top))
                $graphics = [Drawing.Graphics]::FromImage($bitmap)
                try {
                    $dc = $graphics.GetHdc()
                    try { [void][SettingsGeneralLayoutWindow]::PrintWindow($settings, $dc, 0) }
                    finally { $graphics.ReleaseHdc($dc) }
                    $bitmap.Save((Join-Path $PSScriptRoot '..\..\out\tests\settings-general-ru-RU.bmp'))
                }
                finally { $graphics.Dispose(); $bitmap.Dispose() }
            }
            $settingsHandle = '0x' + $settings.ToInt64().ToString('X')
            $dialog = @($audit.controls | Where-Object Hwnd -eq $settingsHandle)[0].Dialog
            $rows = @($audit.controls | Where-Object Dialog -eq $dialog)
            $expectedBackup = $catalog.strings.'fbe.dialog.idd_setting_next.create_backup_file'.translations.$locale
            $backup = @($rows | Where-Object Id -eq 1128)
            if ($backup.Count -ne 1 -or $backup[0].Text -ne $expectedBackup) {
                throw "$locale General page did not use the selected language."
            }
            $expected = @(1120, 1154, 1160, 1053, 1603, 1051, 1128, 1130, 1158)
            foreach ($id in $expected) {
                $control = @($rows | Where-Object Id -eq $id)
                if ($control.Count -ne 1 -or $control[0].TextMayClip) {
                    throw "$locale General control $id is missing or clips its text."
                }
            }
            Write-Host "$locale General page labels and actions fit."
            [SettingsGeneralLayoutWindow]::SelectHotkeys($settings)
            Start-Sleep -Milliseconds 100
            $hotkeysReport = Join-Path $PSScriptRoot "..\..\out\tests\settings-hotkeys-$locale.json"
            & (Join-Path $PSScriptRoot '..\ui\invoke-ui-layout-audit.ps1') -ProcessId $process.Id -Language $locale -OutputPath $hotkeysReport | Out-Null
            $hotkeysAudit = Get-Content -Raw -Encoding UTF8 -LiteralPath $hotkeysReport | ConvertFrom-Json
            foreach ($button in @(
                @{ Id = 1071; Key = 'fbe.dialog.idd_hotkeys.default' },
                @{ Id = 1076; Key = 'fbe.dialog.idd_hotkeys.delete' },
                @{ Id = 1077; Key = 'fbe.dialog.idd_hotkeys.assign' },
                @{ Id = 1074; Key = 'fbe.dialog.idd_hotkeys.groups' },
                @{ Id = 1075; Key = 'fbe.dialog.idd_hotkeys.commands' },
                @{ Id = 1622; Key = 'fbe.dialog.idd_hotkeys.shortcut' },
                @{ Id = 1067; Key = 'fbe.dialog.idd_setting_other.change_keyboard' },
                @{ Id = 1617; Key = 'fbe.dialog.idd_setting_other.layout' })) {
                $control = @($hotkeysAudit.controls | Where-Object Id -eq $button.Id)
                $expectedText = $catalog.strings.($button.Key).translations.$locale
                if ($control.Count -ne 1 -or $control[0].Text -ne $expectedText -or $control[0].TextMayClip) {
                    throw "$locale Hotkeys control $($button.Id) is missing, untranslated, or clips its text."
                }
            }
            Write-Host "$locale Hotkeys labels and action buttons fit."
            [SettingsGeneralLayoutWindow]::Close($settings)
            [SettingsGeneralLayoutWindow]::Close($frame)
            [void]$process.WaitForExit(5000)
        }
        finally {
            if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
        }
    }
    Write-Host 'Settings General and Hotkeys localized layout passed for ru/en/de/uk/es/bg.'
}
finally {
    for ($cleanupAttempt = 0; $cleanupAttempt -lt 20; ++$cleanupAttempt) {
        try {
            Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $true -ErrorAction Stop
            break
        }
        catch {
            if ($cleanupAttempt -eq 19) { throw }
            Start-Sleep -Milliseconds 500
        }
    }
}
