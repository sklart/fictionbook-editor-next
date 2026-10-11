<# Opens the real Add Image and Table dialogs in four representative UI locales. #>
[CmdletBinding()]
param(
    [string]$FbeExe,
    [ValidateSet('ru-RU', 'en-US', 'de-DE', 'uk-UA')]
    [string[]]$Locales = @('ru-RU', 'en-US', 'de-DE', 'uk-UA')
)
$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name add-image-layout
$passed = $false
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class AddImageLayoutWindow {
  public delegate bool Callback(IntPtr hwnd, IntPtr data);
  [DllImport("user32.dll")] public static extern bool EnumWindows(Callback callback, IntPtr data);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint process);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder text, int size);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd, int command);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wp, IntPtr lp);
  public static IntPtr Find(int processId, bool dialog) {
    IntPtr found=IntPtr.Zero;
    EnumWindows((h,d)=>{
      uint owner;
      GetWindowThreadProcessId(h,out owner);
      if(owner!=(uint)processId) return true;
      if(dialog) {
        if(GetDlgItem(h,1050)==IntPtr.Zero || !IsWindowVisible(h)) return true;
      } else {
        var name=new StringBuilder(128);
        GetClassName(h,name,name.Capacity);
        if(name.ToString()!="FictionBookEditorFrame") return true;
      }
      found=h;
      return false;
    },IntPtr.Zero);
    return found;
  }
  public static IntPtr FindTable(int processId) {
    IntPtr found=IntPtr.Zero;
    EnumWindows((h,d)=>{
      uint owner;
      GetWindowThreadProcessId(h,out owner);
      var name=new StringBuilder(128);
      GetClassName(h,name,name.Capacity);
      if(owner==(uint)processId && name.ToString()=="#32770" &&
         GetDlgItem(h,1038)!=IntPtr.Zero && GetDlgItem(h,1600)!=IntPtr.Zero && IsWindowVisible(h)) {
        found=h;
        return false;
      }
      return true;
    },IntPtr.Zero);
    return found;
  }
}
'@
try {
    & pwsh -NoProfile -File (Join-Path $PSScriptRoot '..\localization\export-runtime-lang.ps1') -OutputDirectory (Join-Path $isolation.Runtime 'Lang') -Clean | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Runtime localization export failed.' }
    $fixture = (Resolve-Path (Join-Path $PSScriptRoot 'fb2-validate-valid-smoke.fb2')).Path
    $catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $PSScriptRoot '..\..\localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
    $languageIds = @{ 'ru-RU' = 0xFBE002; 'en-US' = 0xFBE001; 'de-DE' = 0xFBE004; 'uk-UA' = 0xFBE003 }
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
                if ($process.HasExited) { throw "$locale FBE exited before opening the document." }
                $frame = [AddImageLayoutWindow]::Find($process.Id, $false)
            }
            if ($frame -eq [IntPtr]::Zero) { throw "$locale main frame missing." }
            [void][AddImageLayoutWindow]::ShowWindow($frame, 5)
            [void][AddImageLayoutWindow]::PostMessage($frame, 0x0111, [IntPtr]32820, [IntPtr]::Zero)
            $dialog = [IntPtr]::Zero
            for ($attempt = 0; $attempt -lt 60 -and $dialog -eq [IntPtr]::Zero; ++$attempt) {
                Start-Sleep -Milliseconds 100
                $dialog = [AddImageLayoutWindow]::Find($process.Id, $true)
            }
            if ($dialog -eq [IntPtr]::Zero) { throw "$locale Add Image dialog missing." }
            $report = Join-Path $isolation.Root "add-image-layout-$locale.json"
            & (Join-Path $PSScriptRoot '..\ui\invoke-ui-layout-audit.ps1') -ProcessId $process.Id -Language $locale -OutputPath $report | Out-Null
            $audit = Get-Content -Raw -Encoding UTF8 -LiteralPath $report | ConvertFrom-Json
            foreach ($issue in $audit.issues) {
                $affected = @($audit.controls | Where-Object Hwnd -eq $issue.hwnd | Select-Object -First 1)
                Write-Verbose "$locale Add Image audit finding: $($issue.kind), hwnd=$($affected.Hwnd), class=$($affected.Class), id=$($affected.Id), ignored=$($issue.ignored)"
            }
            $dialogHandle = '0x' + $dialog.ToInt64().ToString('X')
            $expectedPrompt = $catalog.strings.'fbe.dialog.idd_addimage.text'.translations.$locale
            $prompt = @($audit.controls | Where-Object { $_.Parent -eq $dialogHandle -and $_.Id -eq 188 })
            if ($prompt.Count -ne 1 -or $prompt[0].Text -ne $expectedPrompt) {
                throw "$locale Add Image prompt did not use the selected language."
            }
            foreach ($id in 188, 6, 2, 1050) {
                $control = @($audit.controls | Where-Object { $_.Parent -eq $dialogHandle -and $_.Id -eq $id })
                if ($control.Count -ne 1 -or $control[0].TextMayClip) {
                    throw "$locale Add Image control $id is missing or clips text."
                }
            }
            $handles = @($audit.controls | Where-Object Parent -eq $dialogHandle | ForEach-Object Hwnd)
            $overlaps = @($audit.issues | Where-Object { $_.kind -eq 'possible-overlap' -and -not $_.ignored -and $handles -contains $_.hwnd })
            if ($overlaps.Count) { throw "$locale Add Image controls overlap." }
            Write-Host "$locale Add Image dialog fits."
            # Choosing the clear-image action closes this prompt without opening
            # the subsequent system file picker, so the table command can run.
            [void][AddImageLayoutWindow]::PostMessage($dialog, 0x0111, [IntPtr]6, [IntPtr]::Zero)
            [void][AddImageLayoutWindow]::PostMessage($frame, 0x0111, [IntPtr]32853, [IntPtr]::Zero)
            $table = [IntPtr]::Zero
            for ($attempt = 0; $attempt -lt 60 -and $table -eq [IntPtr]::Zero; ++$attempt) {
                Start-Sleep -Milliseconds 100
                $table = [AddImageLayoutWindow]::FindTable($process.Id)
            }
            if ($table -eq [IntPtr]::Zero) { throw "$locale Table dialog missing." }
            $tableReport = Join-Path $isolation.Root "table-layout-$locale.json"
            & (Join-Path $PSScriptRoot '..\ui\invoke-ui-layout-audit.ps1') -ProcessId $process.Id -Language $locale -OutputPath $tableReport | Out-Null
            $tableAudit = Get-Content -Raw -Encoding UTF8 -LiteralPath $tableReport | ConvertFrom-Json
            foreach ($issue in $tableAudit.issues) {
                $affected = @($tableAudit.controls | Where-Object Hwnd -eq $issue.hwnd | Select-Object -First 1)
                Write-Verbose "$locale Table audit finding: $($issue.kind), hwnd=$($affected.Hwnd), class=$($affected.Class), id=$($affected.Id), ignored=$($issue.ignored)"
            }
            $tableHandle = '0x' + $table.ToInt64().ToString('X')
            foreach ($item in @(
                @{ Id = 1600; Key = 'fbe.dialog.idd_table.rows_label' },
                @{ Id = 1601; Key = 'fbe.dialog.idd_table.columns_label' },
                @{ Id = 1038; Key = 'fbe.dialog.idd_table.header_row' })) {
                $control = @($tableAudit.controls | Where-Object { $_.Parent -eq $tableHandle -and $_.Id -eq $item.Id })
                $expected = $catalog.strings.($item.Key).translations.$locale
                if ($control.Count -ne 1 -or $control[0].Text -ne $expected -or $control[0].TextMayClip) {
                    throw "$locale Table control $($item.Id) is missing, untranslated, or clips text."
                }
            }
            foreach ($id in 1, 2) {
                $control = @($tableAudit.controls | Where-Object { $_.Parent -eq $tableHandle -and $_.Id -eq $id })
                if ($control.Count -ne 1 -or $control[0].TextMayClip) { throw "$locale Table button $id clips text." }
            }
            $tableHandles = @($tableAudit.controls | Where-Object Parent -eq $tableHandle | ForEach-Object Hwnd)
            $tableOverlaps = @($tableAudit.issues | Where-Object { $_.kind -eq 'possible-overlap' -and -not $_.ignored -and $tableHandles -contains $_.hwnd })
            if ($tableOverlaps.Count) { throw "$locale Table controls overlap." }
            $dialogBounds = @($tableAudit.controls | Where-Object Hwnd -eq $tableHandle)[0].Bounds
            foreach ($child in $tableAudit.controls | Where-Object Parent -eq $tableHandle) {
                if ($child.Bounds.Left -lt $dialogBounds.Left -or $child.Bounds.Top -lt $dialogBounds.Top -or
                    $child.Bounds.Right -gt $dialogBounds.Right -or $child.Bounds.Bottom -gt $dialogBounds.Bottom) {
                    throw "$locale Table control $($child.Id) extends beyond the dialog."
                }
            }
            Write-Host "$locale Table dialog fits."
            [void][AddImageLayoutWindow]::PostMessage($table, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            [void][AddImageLayoutWindow]::PostMessage($frame, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            [void]$process.WaitForExit(1000)
        }
        finally {
            if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
        }
    }
    $passed = $true
    Write-Host "Add Image and Table localized layout passed for $($Locales -join '/')."
}
finally {
    for ($attempt = 0; $attempt -lt 5; ++$attempt) {
        try {
            Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed -ErrorAction Stop
            break
        }
        catch {
            if ($attempt -eq 4) { throw }
            Start-Sleep -Milliseconds 400
        }
    }
}
