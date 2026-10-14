<# Audits every Settings page in priority locales, with General and Hotkeys in six locales. #>
[CmdletBinding()]
param(
    [string]$FbeExe,
    [string]$ArtifactDirectory,
    [switch]$ChangedPagesOnly,
    [ValidateSet('ru-RU', 'en-US', 'de-DE', 'uk-UA', 'es-ES', 'bg-BG')]
    [string[]]$Locales = @('ru-RU', 'en-US', 'de-DE', 'uk-UA', 'es-ES', 'bg-BG')
)
$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
if (-not $ArtifactDirectory) {
    $ArtifactDirectory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-settings-layout-' + [guid]::NewGuid().ToString('N'))
}
$ArtifactDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ArtifactDirectory)
New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
Write-Host "Settings layout artifacts: $ArtifactDirectory"
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'settings-general-layout'
$passed = $false
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
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X, Y; }
    public delegate bool Callback(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback, IntPtr data);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
    [DllImport("user32.dll")] static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] static extern IntPtr ChildWindowFromPointEx(IntPtr parent, Point point, uint flags);
    [DllImport("user32.dll")] static extern int MapWindowPoints(IntPtr from, IntPtr to, ref Point point, uint count);
    [DllImport("user32.dll")] static extern IntPtr GetParent(IntPtr window);
    [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr window, uint command);
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", EntryPoint = "SendMessageW", CharSet = CharSet.Unicode)]
    static extern IntPtr SendMessageText(IntPtr window, uint message, IntPtr wParam, string text);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr window, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int metric);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window, IntPtr dc, uint flags);
    public static bool FooterButtonIsTopmost(IntPtr dialog, int id) {
        IntPtr button = GetDlgItem(dialog, id);
        Rect bounds;
        if (button == IntPtr.Zero || !GetWindowRect(button, out bounds)) return false;
        var center = new Point { X = (bounds.Left + bounds.Right) / 2, Y = (bounds.Top + bounds.Bottom) / 2 };
        MapWindowPoints(IntPtr.Zero, dialog, ref center, 1);
        return ChildWindowFromPointEx(dialog, center, 1) == button; // CWP_SKIPINVISIBLE
    }
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
        SelectPage(settings, 5);
    }
    public static void SelectPage(IntPtr settings, int index)
    {
        var navigation = GetDlgItem(settings, 1625);
        SendMessage(navigation, 0x0186, (IntPtr)index, IntPtr.Zero); // LB_SETCURSEL
        SendMessage(settings, 0x0111, (IntPtr)(1625 | (1 << 16)), navigation); // LBN_SELCHANGE
    }
    public static void Close(IntPtr window) { PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero); }
    public static bool HasTooltipWidth(int processId, IntPtr settings, int expected)
    {
        bool found = false;
        EnumWindows((window, data) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner != (uint)processId || GetWindow(window, 4) != settings) return true; // GW_OWNER resolves a popup's top-level owner
            var name = new StringBuilder(128);
            GetClassName(window, name, name.Capacity);
            if (name.ToString().Equals("tooltips_class32", StringComparison.OrdinalIgnoreCase) &&
                SendMessage(window, 0x0400 + 25, IntPtr.Zero, IntPtr.Zero).ToInt64() == expected)
                found = true; // TTM_GETMAXTIPWIDTH
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static IntPtr Parent(IntPtr window) { return GetParent(window); }
    public static long Query(IntPtr window, uint message, int argument) {
        return SendMessage(window, message, (IntPtr)argument, IntPtr.Zero).ToInt64();
    }
    public static void SelectCombo(IntPtr page, int id, int index) {
        IntPtr combo = GetDlgItem(page, id);
        SelectComboHandle(page, combo, id, index);
    }
    public static void SelectComboHandle(IntPtr page, IntPtr combo, int id, int index) {
        SendMessage(combo, 0x014E, (IntPtr)index, IntPtr.Zero); // CB_SETCURSEL
        SendMessage(page, 0x0111, (IntPtr)(id | (1 << 16)), combo); // CBN_SELCHANGE
    }
    public static void SelectComboText(IntPtr page, int id, string text) {
        IntPtr combo = GetDlgItem(page, id);
        int index = SendMessageText(combo, 0x0158, (IntPtr)(-1), text).ToInt32(); // CB_FINDSTRINGEXACT
        if (index < 0) throw new Exception("Combo item missing: " + text);
        SelectCombo(page, id, index);
    }
    public static void SetCheckboxHandle(IntPtr page, IntPtr checkbox, int id, bool selected) {
        SendMessage(checkbox, 0x00F1, selected ? (IntPtr)1 : IntPtr.Zero, IntPtr.Zero); // BM_SETCHECK
        SendMessage(page, 0x0111, (IntPtr)id, checkbox); // BN_CLICKED
    }
    public static void ScrollBottom(IntPtr page) {
        SendMessage(page, 0x0115, (IntPtr)7, IntPtr.Zero); // WM_VSCROLL / SB_BOTTOM
    }
}
'@
function Get-AuditControlHandle($audit, [int]$id, [IntPtr]$parent) {
    $parentHandle = '0x' + $parent.ToInt64().ToString('X')
    $control = @($audit.controls | Where-Object { $_.Id -eq $id -and $_.Parent -eq $parentHandle })
    if ($control.Count -ne 1) { throw "Expected one control $id, found $($control.Count)." }
    return [IntPtr]::new([Convert]::ToInt64($control[0].Hwnd.Substring(2), 16))
}
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
    foreach ($locale in $Locales) {
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
            $report = Join-Path $ArtifactDirectory "settings-general-$locale.json"
            & (Join-Path $PSScriptRoot '..\ui\invoke-ui-layout-audit.ps1') -ProcessId $process.Id -Language $locale -OutputPath $report | Out-Null
            $audit = Get-Content -Raw -Encoding UTF8 -LiteralPath $report | ConvertFrom-Json
            foreach ($id in 1, 2) {
                if (-not [SettingsGeneralLayoutWindow]::FooterButtonIsTopmost($settings, $id)) {
                    throw "$locale Settings footer button $id is covered by another child window."
                }
            }
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
                    $bitmap.Save((Join-Path $ArtifactDirectory 'settings-general-ru-RU.bmp'))
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
            if ($locale -eq 'ru-RU') {
                $backupControl = [IntPtr]::new([Convert]::ToInt64($backup[0].Hwnd.Substring(2), 16))
                $page = [SettingsGeneralLayoutWindow]::Parent($backupControl)
                $pageRow = @($rows | Where-Object Hwnd -eq ('0x' + $page.ToInt64().ToString('X')))[0]
                $expectedWidth = [int][Math]::Round(400.0 * $pageRow.Dpi / 96, [MidpointRounding]::AwayFromZero)
                if (-not [SettingsGeneralLayoutWindow]::HasTooltipWidth($process.Id, $settings, $expectedWidth)) {
                    throw "Settings tooltip does not use its page DPI for maximum width ($expectedWidth px)."
                }
            }
            $expected = if ($ChangedPagesOnly) { @() } else { @(1120, 1154, 1160, 1053, 1603, 1051, 1128, 1130, 1158) }
            foreach ($id in $expected) {
                $control = @($rows | Where-Object Id -eq $id)
                if ($control.Count -ne 1 -or $control[0].TextMayClip) {
                    throw "$locale General control $id is missing or clips its text."
                }
            }
            Write-Host "$locale General page labels and actions fit."
            [SettingsGeneralLayoutWindow]::SelectHotkeys($settings)
            Start-Sleep -Milliseconds 100
            foreach ($id in 1, 2) {
                if (-not [SettingsGeneralLayoutWindow]::FooterButtonIsTopmost($settings, $id)) {
                    throw "$locale Hotkeys page covers Settings footer button $id."
                }
            }
            $hotkeysReport = Join-Path $ArtifactDirectory "settings-hotkeys-$locale.json"
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
            if ($ChangedPagesOnly -or $locale -in @('ru-RU', 'en-US', 'de-DE', 'uk-UA')) {
                $wideRect = New-Object SettingsGeneralLayoutWindow+Rect
                [void][SettingsGeneralLayoutWindow]::GetWindowRect($settings, [ref]$wideRect)
                $wideWidth = [Math]::Min(900, [SettingsGeneralLayoutWindow]::GetSystemMetrics(0) - 40)
                $wideLeft = [Math]::Max(20, [Math]::Min($wideRect.Left,
                    [SettingsGeneralLayoutWindow]::GetSystemMetrics(0) - $wideWidth - 20))
                [void][SettingsGeneralLayoutWindow]::SetWindowPos($settings, [IntPtr]::Zero,
                    $wideLeft, $wideRect.Top, $wideWidth, ($wideRect.Bottom - $wideRect.Top), 0x14)
                $pageIndices = if ($ChangedPagesOnly) { @(1, 2) } else { @(1, 2, 3, 4, 6, 7) }
                foreach ($pageIndex in $pageIndices) {
                    [SettingsGeneralLayoutWindow]::SelectPage($settings, $pageIndex)
                    Start-Sleep -Milliseconds 100
                    foreach ($id in 1, 2) {
                        if (-not [SettingsGeneralLayoutWindow]::FooterButtonIsTopmost($settings, $id)) {
                            throw "$locale Settings page $pageIndex covers footer button $id."
                        }
                    }
                    $pageReport = Join-Path $ArtifactDirectory "settings-page-$pageIndex-$locale.json"
                    & (Join-Path $PSScriptRoot '..\ui\invoke-ui-layout-audit.ps1') -ProcessId $process.Id -Language $locale -OutputPath $pageReport | Out-Null
                    $pageAudit = Get-Content -Raw -Encoding UTF8 -LiteralPath $pageReport | ConvertFrom-Json
                    $rect = New-Object SettingsGeneralLayoutWindow+Rect
                    [void][SettingsGeneralLayoutWindow]::GetWindowRect($settings, [ref]$rect)
                    $bitmap = New-Object Drawing.Bitmap(($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top))
                    $graphics = [Drawing.Graphics]::FromImage($bitmap)
                    try {
                        $dc = $graphics.GetHdc()
                        try { [void][SettingsGeneralLayoutWindow]::PrintWindow($settings, $dc, 0) }
                        finally { $graphics.ReleaseHdc($dc) }
                        $bitmap.Save((Join-Path $ArtifactDirectory "settings-page-$pageIndex-$locale.bmp"))
                    }
                    finally { $graphics.Dispose(); $bitmap.Dispose() }
                    $pageHandles = @($pageAudit.controls | Where-Object Dialog -eq $dialog | ForEach-Object Hwnd)
                    $findings = @($pageAudit.issues | Where-Object { -not $_.ignored -and $pageHandles -contains $_.hwnd })
                    if ($findings.Count) {
                        $details = @($findings | ForEach-Object {
                            $finding = $_
                            $control = @($pageAudit.controls | Where-Object Hwnd -eq $finding.hwnd)[0]
                            "$($control.Id): $($control.Text) [$($finding.kind)]"
                        }) -join '; '
                        throw "$locale Settings page $pageIndex has layout findings: $details"
                    }
                    Write-Host "$locale Settings page $pageIndex fits."
                    if ($locale -eq 'ru-RU' -and $pageIndex -eq 1) {
                        $sizeControl = @($pageAudit.controls | Where-Object Id -eq 1016)[0]
                        $sizeHandle = [IntPtr]::new([Convert]::ToInt64($sizeControl.Hwnd.Substring(2), 16))
                        $editorPage = [SettingsGeneralLayoutWindow]::Parent($sizeHandle)
                        [SettingsGeneralLayoutWindow]::SelectComboText($editorPage, 1016, '20')
                        [SettingsGeneralLayoutWindow]::SelectCombo($editorPage, 1100, 2)
                        $changedBitmap = New-Object Drawing.Bitmap(($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top))
                        $changedGraphics = [Drawing.Graphics]::FromImage($changedBitmap)
                        try {
                            $changedDc = $changedGraphics.GetHdc()
                            try { [void][SettingsGeneralLayoutWindow]::PrintWindow($settings, $changedDc, 0) }
                            finally { $changedGraphics.ReleaseHdc($changedDc) }
                            $changedBitmap.Save((Join-Path $ArtifactDirectory 'settings-editor-preview-changed-ru-RU.bmp'))
                        }
                        finally { $changedGraphics.Dispose(); $changedBitmap.Dispose() }
                    }
                    if ($locale -eq 'ru-RU' -and $pageIndex -eq 2) {
                        $editors = @($pageAudit.controls | Where-Object { $_.Class -eq 'Scintilla' -and $_.Text -like '<?xml*' })
                        if ($editors.Count -ne 1) { throw 'The XML preview must contain one independent Scintilla control.' }
                        $editor = [IntPtr]::new([Convert]::ToInt64($editors[0].Hwnd.Substring(2), 16))
                        $page = [SettingsGeneralLayoutWindow]::Parent([SettingsGeneralLayoutWindow]::Parent($editor))
                        if ([SettingsGeneralLayoutWindow]::Query($editor, 2140, 0) -ne 1 -or
                            [SettingsGeneralLayoutWindow]::Query($editor, 2154, 0) -lt 6) {
                            throw 'The XML preview is editable or its sample is incomplete.'
                        }
                        if ([SettingsGeneralLayoutWindow]::Query($editor, 2485, 32) -ne 20) {
                            throw 'The XML preview did not receive the newly selected Editor font size.'
                        }
                        [SettingsGeneralLayoutWindow]::SetCheckboxHandle($page, (Get-AuditControlHandle $pageAudit 1027 $page), 1027, $false) # Wrap lines
                        if ([SettingsGeneralLayoutWindow]::Query($editor, 2269, 0) -ne 0) {
                            throw 'The XML preview did not update line wrapping immediately.'
                        }
                        [SettingsGeneralLayoutWindow]::SetCheckboxHandle($page, (Get-AuditControlHandle $pageAudit 1029 $page), 1029, $true)
                        [SettingsGeneralLayoutWindow]::SetCheckboxHandle($page, (Get-AuditControlHandle $pageAudit 1032 $page), 1032, $true)
                        [SettingsGeneralLayoutWindow]::SetCheckboxHandle($page, (Get-AuditControlHandle $pageAudit 1030 $page), 1030, $true)
                        if ([SettingsGeneralLayoutWindow]::Query($editor, 2355, 0) -ne 1 -or
                            [SettingsGeneralLayoutWindow]::Query($editor, 2020, 0) -ne 1 -or
                            [SettingsGeneralLayoutWindow]::Query($editor, 2243, 0) -le 0) {
                            throw 'The XML preview did not update EOL, whitespace, or line numbers immediately.'
                        }
                        [SettingsGeneralLayoutWindow]::SelectComboHandle($page, (Get-AuditControlHandle $pageAudit 1783 $page), 1783, 2) # Custom caret
                        [SettingsGeneralLayoutWindow]::SelectComboHandle($page, (Get-AuditControlHandle $pageAudit 1785 $page), 1785, 4) # 4 logical pixels
                        [SettingsGeneralLayoutWindow]::SelectComboHandle($page, (Get-AuditControlHandle $pageAudit 1787 $page), 1787, 3) # White
                        [SettingsGeneralLayoutWindow]::SelectComboHandle($page, (Get-AuditControlHandle $pageAudit 1790 $page), 1790, 4) # Do not blink
                        if ([SettingsGeneralLayoutWindow]::Query($editor, 2189, 0) -lt 4 -or
                            [SettingsGeneralLayoutWindow]::Query($editor, 2138, 0) -ne 0xFFFFFF -or
                            [SettingsGeneralLayoutWindow]::Query($editor, 2075, 0) -ne 0) {
                            throw 'The XML preview did not apply the custom caret immediately.'
                        }
                        [SettingsGeneralLayoutWindow]::ScrollBottom($page)
                        Start-Sleep -Milliseconds 100
                        $scrolledBitmap = New-Object Drawing.Bitmap(($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top))
                        $scrolledGraphics = [Drawing.Graphics]::FromImage($scrolledBitmap)
                        try {
                            $scrolledDc = $scrolledGraphics.GetHdc()
                            try { [void][SettingsGeneralLayoutWindow]::PrintWindow($settings, $scrolledDc, 0) }
                            finally { $scrolledGraphics.ReleaseHdc($scrolledDc) }
                            $scrolledBitmap.Save((Join-Path $ArtifactDirectory 'settings-source-preview-ru-RU.bmp'))
                        }
                        finally { $scrolledGraphics.Dispose(); $scrolledBitmap.Dispose() }
                        $editorRect = New-Object SettingsGeneralLayoutWindow+Rect
                        [void][SettingsGeneralLayoutWindow]::GetWindowRect($editor, [ref]$editorRect)
                        $editorBitmap = New-Object Drawing.Bitmap(($editorRect.Right - $editorRect.Left), ($editorRect.Bottom - $editorRect.Top))
                        $editorGraphics = [Drawing.Graphics]::FromImage($editorBitmap)
                        try {
                            $editorDc = $editorGraphics.GetHdc()
                            try { [void][SettingsGeneralLayoutWindow]::PrintWindow($editor, $editorDc, 2) }
                            finally { $editorGraphics.ReleaseHdc($editorDc) }
                            $editorBitmap.Save((Join-Path $ArtifactDirectory 'settings-source-scintilla-ru-RU.bmp'))
                        }
                        finally { $editorGraphics.Dispose(); $editorBitmap.Dispose() }
                        Write-Host 'ru-RU XML preview is read-only and applies size, EOL, whitespace, line-number, wrapping, and caret changes.'
                    }
                }
            }
            [SettingsGeneralLayoutWindow]::Close($settings)
            [SettingsGeneralLayoutWindow]::Close($frame)
            [void]$process.WaitForExit(5000)
        }
        finally {
            if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
        }
    }
    Write-Host "Settings layout checks passed for: $($Locales -join ', ')."
    $passed = $true
}
finally {
    # Windows can keep the just-exited executable open briefly (for example,
    # while a file-system scanner inspects it). Keep the isolated runtime until
    # the lock clears, then remove it rather than reporting a layout failure.
    $cleanupAttempts = 60
    for ($cleanupAttempt = 0; $cleanupAttempt -lt $cleanupAttempts; ++$cleanupAttempt) {
        try {
            Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed -ErrorAction Stop
            break
        }
        catch {
            if ($_.Exception.Message -like 'Disposable runtime leaked FBE.exe process*') { throw }
            if ($cleanupAttempt -eq $cleanupAttempts - 1) { throw }
            Start-Sleep -Milliseconds 500
        }
    }
}
