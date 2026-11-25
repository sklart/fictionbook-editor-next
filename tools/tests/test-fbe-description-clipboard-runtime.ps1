param([Parameter(Mandatory)][string]$RuntimeDirectory, [switch]$ExpectFixed, [string]$ExpectedExecutableSha256)
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
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr data);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string text);
    [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr window, uint message, IntPtr wp, IntPtr lp, uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
    [DllImport("user32.dll")] static extern uint GetMenuState(IntPtr menu, uint item, uint flags);
    [DllImport("user32.dll")] static extern int GetMenuItemCount(IntPtr menu);
    [DllImport("user32.dll")] static extern uint GetMenuItemID(IntPtr menu, int position);
    [DllImport("user32.dll")] static extern bool GetMenuItemRect(IntPtr window, IntPtr menu, uint item, out RECT rect);
    [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
    [DllImport("user32.dll")] static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
    [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll")] static extern IntPtr VirtualAllocEx(IntPtr process, IntPtr address, UIntPtr size, uint allocation, uint protection);
    [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size, uint type);
    [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] buffer, int size, out IntPtr read);
    [StructLayout(LayoutKind.Sequential)] struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("oleacc.dll")] public static extern int ObjectFromLresult(IntPtr result, ref Guid iid, IntPtr wp, [MarshalAs(UnmanagedType.Interface)] out object obj);
    public static void OpenKeyboardContextMenu() {
        keybd_event(0x10, 0, 0, UIntPtr.Zero); keybd_event(0x79, 0, 0, UIntPtr.Zero);
        keybd_event(0x79, 0, 2, UIntPtr.Zero); keybd_event(0x10, 0, 2, UIntPtr.Zero);
    }
    public static void ClickPasteToolbar(IntPtr root, uint pid, int command) {
        IntPtr toolbar = IntPtr.Zero;
        int index = -1;
        EnumChildWindows(root, (window, ignored) => {
            var cls = new StringBuilder(256); GetClassName(window, cls, cls.Capacity);
            if (cls.ToString() != "ToolbarWindow32") return true;
            int found = unchecked((int)SendMessage(window, 0x419, (IntPtr)command, IntPtr.Zero).ToInt64());
            if (found < 0) return true;
            toolbar = window; index = found; return false;
        }, IntPtr.Zero);
        if (toolbar == IntPtr.Zero || SendMessage(toolbar, 0x409, (IntPtr)command, IntPtr.Zero) == IntPtr.Zero)
            throw new InvalidOperationException("Paste toolbar button missing or disabled.");
        IntPtr process = OpenProcess(0x438, false, pid);
        if (process == IntPtr.Zero) throw new InvalidOperationException("Cannot inspect toolbar process.");
        IntPtr address = IntPtr.Zero;
        try {
            address = VirtualAllocEx(process, IntPtr.Zero, (UIntPtr)16, 0x3000, 4);
            if (address == IntPtr.Zero || SendMessage(toolbar, 0x41d, (IntPtr)index, address) == IntPtr.Zero)
                throw new InvalidOperationException("Cannot read paste button rectangle.");
            byte[] data = new byte[16]; IntPtr count;
            if (!ReadProcessMemory(process, address, data, data.Length, out count) || count.ToInt32() != data.Length)
                throw new InvalidOperationException("Cannot retrieve paste button rectangle.");
            int x = (BitConverter.ToInt32(data, 0) + BitConverter.ToInt32(data, 8)) / 2;
            int y = (BitConverter.ToInt32(data, 4) + BitConverter.ToInt32(data, 12)) / 2;
            IntPtr point = (IntPtr)((y << 16) | (x & 0xffff));
            SendMessage(toolbar, 0x201, (IntPtr)1, point);
            SendMessage(toolbar, 0x202, IntPtr.Zero, point);
        } finally {
            if (address != IntPtr.Zero) VirtualFreeEx(process, address, UIntPtr.Zero, 0x8000);
            CloseHandle(process);
        }
    }
    public static bool ClickContextPaste(uint pid, uint command) {
        IntPtr popup = IntPtr.Zero;
        while ((popup = FindWindowEx(IntPtr.Zero, popup, "#32768", null)) != IntPtr.Zero) {
            uint owner; GetWindowThreadProcessId(popup, out owner);
            if (owner != pid) continue;
            IntPtr menu = SendMessage(popup, 0x1e1, IntPtr.Zero, IntPtr.Zero);
            uint state = GetMenuState(menu, command, 0);
            if (state == 0xffffffff || (state & 3) != 0) throw new InvalidOperationException("Context paste is missing or disabled.");
            int count = GetMenuItemCount(menu);
            for (int i = 0; i < count; ++i) {
                if (GetMenuItemID(menu, i) != command) continue;
                RECT rect;
                if (!GetMenuItemRect(popup, menu, (uint)i, out rect)) throw new InvalidOperationException("Cannot locate context paste.");
                SetCursorPos((rect.Left + rect.Right) / 2, (rect.Top + rect.Bottom) / 2);
                mouse_event(2, 0, 0, 0, UIntPtr.Zero); mouse_event(4, 0, 0, 0, UIntPtr.Zero);
                return true;
            }
        }
        return false;
    }
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
    public static string[] WindowTexts(uint pid) {
        var texts = new List<string>();
        EnumWindows((window, ignored) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            if (owner != pid) return true;
            EnumProc collect = (child, unused) => {
                var caption = new StringBuilder(2048);
                GetWindowText(child, caption, caption.Capacity);
                if (caption.Length > 0) texts.Add(caption.ToString());
                return true;
            };
            collect(window, IntPtr.Zero);
            EnumChildWindows(window, collect, IntPtr.Zero);
            return true;
        }, IntPtr.Zero);
        return texts.ToArray();
    }
}
'@
$root = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$exe = Join-Path $root 'FBE.exe'
$executableHash = (Get-FileHash -LiteralPath $exe).Hash
Write-Host "Editor SHA-256: $executableHash"
if ($ExpectedExecutableSha256 -and $executableHash -ne $ExpectedExecutableSha256) { throw 'Executable hash does not match the requested artifact.' }
if (-not $ExpectFixed -and $executableHash -ne 'D159E6709C39787D205C9D447226B63EE04D7488C480D8D5EF6F598D6AB50759') {
    throw 'The baseline executable does not match the 3.0.11 release.'
}
$fixture = Join-Path $root 'blank.fb2'
if ($ExpectFixed) {
    $fixture = Join-Path $root 'description-paste-fixture.fb2'
    $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Original</first-name><last-name>Author</last-name></author><book-title>Original title</book-title><lang>en</lang></title-info><document-info><author><nickname>Tester</nickname></author><date value="2026-01-01">2026-01-01</date><id>original-id</id><version>1.0</version></document-info><publish-info><book-name>Original</book-name><publisher>Publisher</publisher><isbn>123</isbn></publish-info><custom-info info-type="test">unchanged</custom-info></description><body><section><p>Body remains unchanged.</p></section></body><binary id="attachment" content-type="application/octet-stream">AQIDBA==</binary></FictionBook>'
    [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
    $settings = New-Object System.Xml.XmlReaderSettings
    $settings.XmlResolver = New-Object System.Xml.XmlUrlResolver
    $settings.Schemas.XmlResolver = New-Object System.Xml.XmlUrlResolver
    $null = $settings.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0', (Join-Path $root 'FictionBook.xsd'))
    $settings.ValidationType = [System.Xml.ValidationType]::Schema
    $reader = [System.Xml.XmlReader]::Create($fixture, $settings)
    try { while ($reader.Read()) {} } finally { $reader.Dispose() }
}
$env:FBE_NEXT_TRACE = '1'
$process = Start-Process -FilePath $exe -WorkingDirectory $root -ArgumentList @('--portable', $fixture) -PassThru
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
    $bodyBefore = [string]$document.all.item('fbw_body').innerHTML
    $binariesBefore = [string]$document.all.item('binobj').innerHTML
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
    if ($after -cne 'Baseline clipboard test') { throw 'Plain text insertion failed.' }
    foreach ($id in @('tiKwd', 'first', 'last', 'nick', 'home', 'piName', 'piPub', 'piISBN')) {
        try {
            $input = $document.all.item($id)
            $tag = try { [string]$input.tagName } catch { '' }
            if ($input -and -not $tag) { $input = $input.item(0) }
            if (-not $input) { throw "Field $id unavailable." }
            $expected = if ($id -eq 'home') { 'https://example.org/' } else { 'Baseline clipboard test' }
            [System.Windows.Forms.Clipboard]::SetText($expected)
            $input.focus()
            $input.select()
            [System.Windows.Forms.SendKeys]::SendWait('^v')
            Start-Sleep -Milliseconds 500
            Write-Host "Field $id value: $($input.value)"
            if ($ExpectFixed -and [string]$input.value -cne $expected) { throw "Insertion failed for $id." }
        } catch {
            if ($ExpectFixed) { throw }
            Write-Host "Field $id error: $($_.Exception.Message)"
        }
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
    $windows = [BaselineWindow]::WindowTexts([uint32]$process.Id)
    Write-Host ("Windows after formatted paste: " + ($windows -join ' | '))
    $trace = Get-ChildItem -LiteralPath $root -Filter 'fbe-*.log' -Recurse -File -ErrorAction SilentlyContinue
    Write-Host "Trace files: $($trace.Count)"
    $pasteDiagnostics = @()
    foreach ($file in $trace) {
        Write-Host "Trace path: $($file.FullName)"
        Get-Content -LiteralPath $file.FullName -Tail 80 | ForEach-Object { Write-Host $_ }
        $pasteDiagnostics += @(Select-String -LiteralPath $file.FullName -Pattern 'CP201|CP202|CP206|CP3[0-9][0-9]|CP4[0-9][0-9]' | ForEach-Object { $_.Line })
    }
    $pasteDiagnostics | ForEach-Object { Write-Host $_ }
    if ($ExpectFixed) {
        if ($windows -contains 'COM Error' -or [string]$field.value -cne 'Formatted sample') {
            throw 'Formatted clipboard text was not inserted into the title field.'
        }
    } elseif ($windows -notcontains 'COM Error' -or -not ($windows -match '80004005')) {
        throw 'The baseline did not show the reported COM error after formatted paste.'
    }
    if ($ExpectFixed -and ($pasteDiagnostics -match 'code=CP20[0126]')) {
        throw 'Paste diagnostic recorded a clipboard error.'
    }
    if (-not $ExpectFixed -and (-not ($pasteDiagnostics -match 'CP200.*0x80004005.*CP309') -or
        -not ($pasteDiagnostics -match 'CP201.*0x80004005.*CP313'))) {
        throw 'The baseline trace did not contain the expected CP309 and CP313 failure stages.'
    }
    [System.Windows.Forms.Clipboard]::SetText('Menu paste sample')
    $field.focus(); $field.select()
    [void][BaselineWindow]::PostMessage($process.MainWindowHandle, 0x111, [IntPtr]0xE125, [IntPtr]::Zero)
    Start-Sleep -Seconds 3
    Write-Host "Menu paste title: $($field.value)"
    Write-Host ("Windows after menu paste: " + ([BaselineWindow]::WindowTexts([uint32]$process.Id) -join ' | '))
    if ($ExpectFixed -and [string]$field.value -cne 'Menu paste sample') {
        throw 'Menu paste did not update the title field.'
    }
    if ($ExpectFixed) {
        $sequence = $document.all.item('tiSeq').getElementsByTagName('INPUT').item(0)
        if (-not $sequence) { throw 'Series name field was not found.' }
        [System.Windows.Forms.Clipboard]::SetText('Series clipboard test')
        $sequence.focus(); $sequence.select()
        [System.Windows.Forms.SendKeys]::SendWait('^v')
        Start-Sleep -Seconds 1
        if ([string]$sequence.value -cne 'Series clipboard test') { throw 'Series name paste failed.' }
        [System.Windows.Forms.Clipboard]::SetText('Plain command sample')
        $field.focus(); $field.select()
        [void][BaselineWindow]::PostMessage($process.MainWindowHandle, 0x111, [IntPtr]32933, [IntPtr]::Zero)
        Start-Sleep -Seconds 1
        if ([string]$field.value -cne 'Plain command sample') { throw 'Paste as plain text command failed.' }
        [System.Windows.Forms.Clipboard]::SetText('Toolbar paste sample')
        $field.focus(); $field.select()
        Start-Sleep -Seconds 2
        [BaselineWindow]::ClickPasteToolbar($process.MainWindowHandle, [uint32]$process.Id, 0xE125)
        Start-Sleep -Seconds 1
        if ([string]$field.value -cne 'Toolbar paste sample') { throw 'Toolbar paste failed.' }
        [System.Windows.Forms.Clipboard]::SetText('Context paste sample')
        $field.focus(); $field.select()
        [BaselineWindow]::OpenKeyboardContextMenu()
        $contextClicked = $false
        for ($attempt = 0; $attempt -lt 20 -and -not $contextClicked; ++$attempt) {
            Start-Sleep -Milliseconds 250
            $contextClicked = [BaselineWindow]::ClickContextPaste([uint32]$process.Id, 0xE125)
        }
        if (-not $contextClicked) { throw 'Description context menu did not open.' }
        Start-Sleep -Seconds 1
        if ([string]$field.value -cne 'Context paste sample') { throw 'Context menu paste failed.' }
        $keyword = $document.all.item('tiKwd')
        $keyword.focus(); $keyword.select()
        [System.Windows.Forms.SendKeys]::SendWait('^c')
        Start-Sleep -Milliseconds 500
        $copiedField = [System.Windows.Forms.Clipboard]::GetText()
        if ($copiedField -cne 'Baseline clipboard test') { throw 'Copy from metadata did not reach the clipboard.' }
        $field.focus(); $field.select()
        [System.Windows.Forms.SendKeys]::SendWait('^v')
        Start-Sleep -Seconds 1
        if ([string]$field.value -cne $copiedField) { throw 'Pasting text copied from metadata failed.' }
        [void][BaselineWindow]::SendMessage($process.MainWindowHandle, 0x111, [IntPtr]32810, [IntPtr]::Zero)
        Start-Sleep -Seconds 1
        $paragraph = $document.all.item('fbw_body').getElementsByTagName('P').item(0)
        if (-not $paragraph) { throw 'Body paragraph was not found.' }
        $bodyRange = $document.body.createTextRange()
        $bodyRange.moveToElementText($paragraph)
        $bodyRange.select()
        [void][BaselineWindow]::PostMessage($process.MainWindowHandle, 0x111, [IntPtr]0xE122, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 500
        $copiedBody = [System.Windows.Forms.Clipboard]::GetText().Trim()
        if ($copiedBody -cne 'Body remains unchanged.') { throw "Copy from the book body did not reach the clipboard: '$copiedBody'." }
        [void][BaselineWindow]::SendMessage($process.MainWindowHandle, 0x111, [IntPtr]32809, [IntPtr]::Zero)
        Start-Sleep -Seconds 1
        $field.focus(); $field.select()
        [System.Windows.Forms.SendKeys]::SendWait('^v')
        Start-Sleep -Seconds 1
        if ([string]$field.value -cne $copiedBody) { throw 'Pasting text copied from the book body failed.' }
        $previousTitle = [string]$field.value
        $unicode = ([string][char]0x041f) + [char]0x0440 + [char]0x0438 + [char]0x0432 + [char]0x0435 + [char]0x0442 + [char]0x00a0 + [char]0x03a9
        [System.Windows.Forms.Clipboard]::SetText($unicode)
        $field.focus(); $field.select()
        [System.Windows.Forms.SendKeys]::SendWait('+{INSERT}')
        Start-Sleep -Seconds 1
        if ([string]$field.value -cne $unicode) { throw 'Unicode or NBSP was changed during Shift+Insert.' }
        if ([System.Windows.Forms.Clipboard]::GetText() -cne $unicode) { throw 'Paste changed the clipboard.' }
        [System.Windows.Forms.SendKeys]::SendWait('^z')
        Start-Sleep -Seconds 1
        if ([string]$field.value -cne $previousTitle) { throw 'Undo did not restore the previous title.' }
        [System.Windows.Forms.Clipboard]::Clear()
        [System.Windows.Forms.SendKeys]::SendWait('^v')
        Start-Sleep -Milliseconds 500
        if ([string]$field.value -cne $previousTitle) { throw 'Empty clipboard changed the title after Undo.' }
        [System.Windows.Forms.SendKeys]::SendWait('^y')
        Start-Sleep -Seconds 1
        if ([string]$field.value -cne $unicode) { throw 'Redo did not restore the pasted title.' }
        $field.readOnly = $true
        try {
            [System.Windows.Forms.Clipboard]::SetText('must not be inserted')
            $field.focus(); $field.select()
            [void][BaselineWindow]::PostMessage($process.MainWindowHandle, 0x111, [IntPtr]0xE125, [IntPtr]::Zero)
            Start-Sleep -Seconds 1
            if ([string]$field.value -cne $unicode) { throw 'Read-only field was changed.' }
        } finally { $field.readOnly = $false }
        $range = $field.createTextRange()
        $null = $range.moveStart('character', 1)
        $null = $range.moveEnd('character', -5)
        $range.select()
        [System.Windows.Forms.Clipboard]::SetText('XX')
        [System.Windows.Forms.SendKeys]::SendWait('^v')
        Start-Sleep -Seconds 1
        $expectedSelection = $unicode.Substring(0, 1) + 'XX' + $unicode.Substring(3)
        if ([string]$field.value -cne $expectedSelection) { throw 'Selected text was not replaced at the caret.' }
        if ([string]$document.activeElement.id -cne 'tiTitle') { throw 'Paste moved focus away from the title.' }
        [System.Windows.Forms.Clipboard]::Clear()
        $field.focus(); $field.select()
        [System.Windows.Forms.SendKeys]::SendWait('^v')
        Start-Sleep -Seconds 1
        if ([string]$field.value -cne $expectedSelection) { throw 'Empty clipboard changed the title.' }
        $bitmap = New-Object System.Drawing.Bitmap(1, 1)
        try { [System.Windows.Forms.Clipboard]::SetImage($bitmap) } finally { $bitmap.Dispose() }
        [System.Windows.Forms.SendKeys]::SendWait('^v')
        Start-Sleep -Seconds 1
        if ([string]$field.value -cne $expectedSelection) { throw 'Image-only clipboard changed the title.' }
        [System.Windows.Forms.Clipboard]::SetText(('A' * 400))
        $field.focus(); $field.select()
        [System.Windows.Forms.SendKeys]::SendWait('^v')
        Start-Sleep -Seconds 1
        if (([string]$field.value).Length -ne 256) { throw 'The title maximum length was not respected.' }
        $textarea = $document.all.item('stylesheetId')
        if (-not $textarea) { throw 'Multiline field not found.' }
        $multiline = "first line`r`nsecond line"
        [System.Windows.Forms.Clipboard]::SetText($multiline)
        $textarea.focus(); $textarea.select()
        [System.Windows.Forms.SendKeys]::SendWait('^v')
        Start-Sleep -Seconds 1
        if (([string]$textarea.value).Replace("`r`n", "`n") -cne $multiline.Replace("`r`n", "`n")) {
            throw 'Multiline text was not preserved.'
        }
        if ([string]$document.all.item('fbw_body').innerHTML -cne $bodyBefore) { throw 'Metadata paste changed the book body.' }
        if ([string]$document.all.item('binobj').innerHTML -cne $binariesBefore) { throw 'Metadata paste changed binary nodes.' }
        if ([BaselineWindow]::WindowTexts([uint32]$process.Id) -contains 'COM Error') { throw 'COM error appeared during metadata paste.' }
        Write-Host 'Additional fields, Unicode, NBSP, Shift+Insert, maxlength and multiline paste: PASS.'
        Write-Host 'Book body and binary DOM unchanged: PASS.'
        foreach ($viewCommand in @(127, 32810, 32809)) {
            [void][BaselineWindow]::SendMessage($process.MainWindowHandle, 0x111, [IntPtr]$viewCommand, [IntPtr]::Zero)
            Start-Sleep -Milliseconds 500
        }
        foreach ($current in [BaselineWindow]::Documents($process.MainWindowHandle)) {
            try {
                $updatedField = $current.all.item('tiTitle')
                if ($updatedField) { $document = $current; $field = $updatedField; break }
            } catch {}
        }
        if ([string]$field.value -cne ('A' * 256)) { throw 'View switching changed the title.' }
        Write-Host 'Description, Source and Body transitions: PASS.'
        $fileBeforeSave = (Get-FileHash -LiteralPath $fixture).Hash
        [void][BaselineWindow]::PostMessage($process.MainWindowHandle, 0x111, [IntPtr]0xE103, [IntPtr]::Zero)
        $saveDeadline = [DateTime]::UtcNow.AddSeconds(15)
        do {
            Start-Sleep -Milliseconds 500
            $fileAfterSave = (Get-FileHash -LiteralPath $fixture).Hash
        } until ($fileAfterSave -ne $fileBeforeSave -or [DateTime]::UtcNow -ge $saveDeadline)
        if ($fileAfterSave -eq $fileBeforeSave) {
            throw ("Save did not update the fixture. Windows: " + ([BaselineWindow]::WindowTexts([uint32]$process.Id) -join ' | '))
        }
        [xml]$saved = [IO.File]::ReadAllText($fixture, [Text.Encoding]::UTF8)
        $manager = New-Object System.Xml.XmlNamespaceManager($saved.NameTable)
        $manager.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
        $savedTitle = $saved.SelectSingleNode('/fb:FictionBook/fb:description/fb:title-info/fb:book-title', $manager).InnerText
        if ($savedTitle -cne ('A' * 256)) { throw "Saved title was lost. Length: $($savedTitle.Length); value: $savedTitle" }
        if ($saved.SelectSingleNode('/fb:FictionBook/fb:binary[@id="attachment"]', $manager).InnerText -cne 'AQIDBA==') { throw 'Saved binary was changed.' }
        if ($saved.SelectSingleNode('/fb:FictionBook/fb:body/fb:section/fb:p', $manager).InnerText -cne 'Body remains unchanged.') { throw 'Saved book body was changed.' }
        if ($saved.SelectSingleNode('/fb:FictionBook/fb:description/fb:custom-info[@info-type="test"]', $manager).InnerText -cne 'unchanged') { throw 'Unrelated metadata was changed.' }
        $reader = [System.Xml.XmlReader]::Create($fixture, $settings)
        try { while ($reader.Read()) {} } finally { $reader.Dispose() }
        Write-Host 'Saved FB2, title, body, binary and XSD: PASS.'
        [void]$process.CloseMainWindow()
        if (-not $process.WaitForExit(15000)) { throw 'Saved document did not close cleanly.' }
        $process = Start-Process -FilePath $exe -WorkingDirectory $root -ArgumentList @('--portable', $fixture) -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds(90)
        do { Start-Sleep -Milliseconds 500; $process.Refresh() } until ($process.MainWindowTitle -like '*description-paste-fixture.fb2*' -or $process.HasExited -or [DateTime]::UtcNow -ge $deadline)
        if ($process.HasExited -or $process.MainWindowTitle -notlike '*description-paste-fixture.fb2*') { throw 'Saved document could not be reopened.' }
        [void][BaselineWindow]::SendMessage($process.MainWindowHandle, 0x111, [IntPtr]32809, [IntPtr]::Zero)
        $reopened = $null
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        do {
            Start-Sleep -Milliseconds 500
            foreach ($doc in [BaselineWindow]::Documents($process.MainWindowHandle)) {
                try { $reopened = $doc.all.item('tiTitle'); if ($reopened) { break } } catch {}
            }
        } until ($reopened -or [DateTime]::UtcNow -ge $deadline)
        if (-not $reopened -or [string]$reopened.value -cne ('A' * 256)) { throw 'Reopened title was lost.' }
        Write-Host 'Save and reopen: PASS.'
    }
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
