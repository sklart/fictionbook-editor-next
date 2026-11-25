param([Parameter(Mandatory)][string]$RuntimeDirectory)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'Run only on a disposable GitHub-hosted Windows runner.'
}
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class BaselineWindow {
    public delegate bool EnumProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc callback, IntPtr data);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string text);
    [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr window, uint message, IntPtr wp, IntPtr lp, uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("oleacc.dll")] public static extern int ObjectFromLresult(IntPtr result, ref Guid iid, IntPtr wp, [MarshalAs(UnmanagedType.Interface)] out object obj);
    public static object[] Documents(IntPtr root) {
        var found = new List<object>();
        EnumChildWindows(root, (window, ignored) => {
            var name = new StringBuilder(256);
            GetClassName(window, name, name.Capacity);
            if (name.ToString() != "Internet Explorer_Server") return true;
            IntPtr result;
            if (SendMessageTimeout(window, RegisterWindowMessage("WM_HTML_GETOBJECT"), IntPtr.Zero, IntPtr.Zero, 2, 1000, out result) == IntPtr.Zero) return true;
            var iid = new Guid("332C4425-26CB-11D0-B483-00C04FD90119");
            object document;
            var status = ObjectFromLresult(result, ref iid, IntPtr.Zero, out document);
            Console.WriteLine("HTML document result: 0x" + status.ToString("X8"));
            if (status == 0 && document != null) found.Add(document);
            return true;
        }, IntPtr.Zero);
        return found.ToArray();
    }
}
'@
$root = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$exe = Join-Path $root 'FBE.exe'
if ((Get-FileHash -LiteralPath $exe).Hash -ne 'D159E6709C39787D205C9D447226B63EE04D7488C480D8D5EF6F598D6AB50759') {
    throw 'The baseline executable does not match the 3.0.11 release.'
}
$process = Start-Process -FilePath $exe -WorkingDirectory $root -ArgumentList @('--portable', (Join-Path $root 'blank.fb2')) -PassThru
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(90)
    do { Start-Sleep -Milliseconds 500; $process.Refresh() } until ($process.MainWindowHandle -ne [IntPtr]::Zero -or $process.HasExited -or [DateTime]::UtcNow -ge $deadline)
    if ($process.HasExited -or $process.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Editor window did not start.' }
    Start-Sleep -Seconds 15
    $process.Refresh()
    if ($process.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Main window is unavailable after startup.' }
    Write-Host "Editor window title: $($process.MainWindowTitle)"
    [void][BaselineWindow]::SendMessage($process.MainWindowHandle, 0x111, [IntPtr]32809, [IntPtr]::Zero)
    $field = $null
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 500
        $documents = [BaselineWindow]::Documents($process.MainWindowHandle)
        Write-Host "Documents found: $($documents.Count)"
        foreach ($document in $documents) {
            try { $field = $document.all.item('tiTitle'); if ($field) { break } } catch { Write-Host "Document access: $($_.Exception.Message)" }
        }
    } until ($field -or [DateTime]::UtcNow -ge $deadline)
    if (-not $field) { throw 'Description title input was not located.' }
    $before = [string]$field.value
    [System.Windows.Forms.Clipboard]::SetText('Baseline clipboard test')
    [void][BaselineWindow]::SetForegroundWindow($process.MainWindowHandle)
    $field.focus()
    $field.select()
    [System.Windows.Forms.SendKeys]::SendWait('^v')
    Start-Sleep -Seconds 3
    $after = [string]$field.value
    Write-Host "Baseline title before: $before"
    Write-Host "Baseline title after: $after"
    Write-Host "Baseline insertion matched: $($after -ceq 'Baseline clipboard test')"
    Write-Host "Baseline clipboard preserved: $([System.Windows.Forms.Clipboard]::GetText() -ceq 'Baseline clipboard test')"
    foreach ($id in @('tiKwd', 'first', 'last', 'nick', 'home', 'piName', 'piPub', 'piISBN')) {
        try {
            $input = $document.all.item($id)
            if (-not $input -or $input.length) { Write-Host "Field $id unavailable or ambiguous"; continue }
            $input.focus()
            $input.select()
            [System.Windows.Forms.SendKeys]::SendWait('^v')
            Start-Sleep -Milliseconds 500
            Write-Host "Field $id value: $($input.value)"
        } catch { Write-Host "Field $id error: $($_.Exception.Message)" }
    }
    $htmlData = New-Object System.Windows.Forms.DataObject
    $htmlData.SetData([System.Windows.Forms.DataFormats]::UnicodeText, 'Formatted sample')
    $prefix = '<html><body><!--StartFragment-->'
    $fragment = '<b>Formatted sample</b>'
    $suffix = '<!--EndFragment--></body></html>'
    $template = "Version:0.9`r`nStartHTML:{0:D10}`r`nEndHTML:{1:D10}`r`nStartFragment:{2:D10}`r`nEndFragment:{3:D10}`r`n"
    $headerLength = ($template -f 0, 0, 0, 0).Length
    $startFragment = $headerLength + $prefix.Length
    $endFragment = $startFragment + $fragment.Length
    $header = $template -f $headerLength, ($endFragment + $suffix.Length), $startFragment, $endFragment
    $htmlData.SetData('HTML Format', $header + $prefix + $fragment + $suffix)
    [System.Windows.Forms.Clipboard]::SetDataObject($htmlData, $true)
    $field.focus(); $field.select()
    [System.Windows.Forms.SendKeys]::SendWait('^v')
    Start-Sleep -Seconds 2
    Write-Host "Formatted clipboard title: $($field.value)"
    [System.Windows.Forms.Clipboard]::SetText('Menu paste sample')
    $field.focus(); $field.select()
    [void][BaselineWindow]::PostMessage($process.MainWindowHandle, 0x111, [IntPtr]0xE125, [IntPtr]::Zero)
    Start-Sleep -Seconds 3
    Write-Host "Menu paste title: $($field.value)"
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
