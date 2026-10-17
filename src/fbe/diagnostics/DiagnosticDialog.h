#pragma once
#include "../StartupTrace.h"
#include "../ThemeManager.h"
#include "../UiMetrics.h"
#include "DiagnosticCommandService.h"
#include "../../common/DeploymentContext.h"
#include <commdlg.h>

// A single native dialog owns the immutable snapshot from preview through save.
class DiagnosticDialog
{
    enum { Sessions=IDC_DIAGNOSTIC_SESSIONS, Components, PreviewText, Status, Toggle, OpenLog, OpenFolder, Cleanup, Preview, Save, State, Close, SessionLabel, ComponentLabel };
    HWND window = NULL;
    HFONT font = NULL;
    bool cleanupPending = false;
    FbeDiagnostics::DiagnosticCommandService& commands;
    std::vector<StartupTrace::DiagnosticSession> sessions;
    std::vector<CString> excluded;
    std::shared_ptr<const StartupTrace::DiagnosticPackageSnapshot> snapshot;
    static CString Text(const wchar_t* key, const wchar_t* fallback) { return FbeLoadRuntimeStringByKey(CString(L"fbe.diagnostics.") + key, fallback); }
    void StatusText(const CString& text) { ::SetDlgItemText(window, Status, text); }
    int Selected() const { return static_cast<int>(::SendDlgItemMessage(window, Sessions, CB_GETCURSEL, 0, 0)); }
    void ShowPreview()
    {
        const int selected = static_cast<int>(::SendDlgItemMessage(window, Components, LB_GETCURSEL, 0, 0));
        CString text;
        if (snapshot && selected >= 0 && static_cast<size_t>(selected) < snapshot->Components().size()) {
            const auto& component = snapshot->Components()[selected];
            if (!component.bytes.empty()) {
                const int length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(component.bytes.data()), static_cast<int>(component.bytes.size()), NULL, 0);
                if (length) { ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(component.bytes.data()), static_cast<int>(component.bytes.size()), text.GetBuffer(length), length); text.ReleaseBuffer(length); }
            } else text = Text(L"component_unavailable", L"Component omitted or unavailable; see the manifest.");
        }
        ::SetDlgItemText(window, PreviewText, text);
    }
    void Prepare()
    {
        ::EnableWindow(::GetDlgItem(window, Save), FALSE);
        snapshot.reset(); const int index = Selected(); if (index < 0 || static_cast<size_t>(index) >= sessions.size()) return;
        CString error; const bool valid = commands.PreparePackage(sessions[index], excluded, snapshot, error);
        ::SendDlgItemMessage(window, Components, LB_RESETCONTENT, 0, 0);
        ULONGLONG size = 0; unsigned int omitted = 0; int extent=0;
        HDC listDC=::GetDC(::GetDlgItem(window,Components));
        HGDIOBJ oldFont=listDC ? ::SelectObject(listDC,reinterpret_cast<HFONT>(::SendDlgItemMessage(window,Components,WM_GETFONT,0,0))) : NULL;
        if (snapshot) for (const auto& component : snapshot->Components()) {
            CString row, state = Text(component.status, component.status);
            row.Format(L"%s  |  %llu  |  %s", (LPCWSTR)component.name, static_cast<ULONGLONG>(component.bytes.size()), (LPCWSTR)state);
            ::SendDlgItemMessage(window, Components, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.GetString())); size += component.bytes.size();
            if (component.status != L"included") ++omitted;
            SIZE measured={}; if(listDC && ::GetTextExtentPoint32(listDC,row,row.GetLength(),&measured)) extent=(std::max)(extent,static_cast<int>(measured.cx)+16);
        }
        if(listDC) { if(oldFont) ::SelectObject(listDC,oldFont); ::ReleaseDC(::GetDlgItem(window,Components),listDC); }
        ::SendDlgItemMessage(window,Components,LB_SETHORIZONTALEXTENT,extent,0);
        CString status; status.Format(Text(L"summary", L"Verified bytes / omitted: %llu / %u"), size, omitted);
        StatusText(valid ? status : Text(L"blocked", L"Saving blocked: mandatory privacy check failed or size limit exceeded."));
        ::EnableWindow(::GetDlgItem(window, Save), valid);
        ::SendDlgItemMessage(window, Components, LB_SETCURSEL, 0, 0); ShowPreview();
    }
    static ULONGLONG TimeTicks(const CString& value) {
        if(value.GetLength()<19) return 0;
        SYSTEMTIME t={}; t.wYear=static_cast<WORD>(_wtoi(value.Mid(0,4))); t.wMonth=static_cast<WORD>(_wtoi(value.Mid(5,2))); t.wDay=static_cast<WORD>(_wtoi(value.Mid(8,2)));
        t.wHour=static_cast<WORD>(_wtoi(value.Mid(11,2))); t.wMinute=static_cast<WORD>(_wtoi(value.Mid(14,2))); t.wSecond=static_cast<WORD>(_wtoi(value.Mid(17,2)));
        FILETIME ft={}; if(!::SystemTimeToFileTime(&t,&ft)) return 0;
        return (static_cast<ULONGLONG>(ft.dwHighDateTime)<<32)|ft.dwLowDateTime;
    }
    void RefreshSessions()
    {
        sessions = commands.Sessions();
        ::SendDlgItemMessage(window, Sessions, CB_RESETCONTENT, 0, 0);
        int selected = 0; bool selectedCrash = false;
        for (size_t i = 0; i < sessions.size(); ++i) {
            const auto& session = sessions[i]; CString row;
            CString outcome = Text(session.outcome, session.outcome), duration=Text(L"unknown",L"Unknown");
            const ULONGLONG start=TimeTicks(session.started), end=TimeTicks(session.ended);
            if(start && end>=start) duration.Format(Text(L"duration",L"%llu s"),(end-start)/10000000ULL);
            row.Format(L"%s%s | %s | %s | PID %lu | %s | %s%s", session.outcome == L"crash" ? L"* " : L"", (LPCWSTR)session.started, (LPCWSTR)duration,
                (LPCWSTR)session.version, session.processId, (LPCWSTR)outcome, (LPCWSTR)Text(session.logPath.IsEmpty() ? L"no_log" : L"has_log", session.logPath.IsEmpty() ? L"No trace" : L"Trace"),
                (LPCWSTR)(session.legacy ? CString(L" | ")+Text(L"legacy",L"Legacy: incomplete matching") : CString()));
            ::SendDlgItemMessage(window, Sessions, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.GetString()));
            if (session.id == StartupTrace::SessionId() && !selectedCrash) selected = static_cast<int>(i);
            if (!selectedCrash && session.outcome == L"crash") { selected = static_cast<int>(i); selectedCrash = true; }
        }
        ::SendDlgItemMessage(window, Sessions, CB_SETCURSEL, selected, 0); excluded.clear(); Prepare();
    }
    void SavePackage()
    {
        if (!snapshot || !snapshot->Valid()) return;
        wchar_t path[MAX_PATH] = L"FBE-Diagnostics.zip";
        const CString title = Text(L"save", L"Save package");
        wchar_t filterBuffer[] = L"ZIP (*.zip)\0*.zip\0\0";
        OPENFILENAME file = {}; file.lStructSize = sizeof(file); file.hwndOwner = window; file.lpstrFile = path; file.nMaxFile = _countof(path);
        file.lpstrFilter = filterBuffer; file.lpstrTitle = title; file.lpstrDefExt = L"zip"; file.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!::GetSaveFileName(&file)) return;
        CString error;
        if (commands.SavePackage(*snapshot, path, error)) StatusText(Text(L"saved", L"Package saved.") + L" " + path);
        else StatusText(Text(L"write_failed", L"Could not save. Choose a new filename and check free space and permissions."));
    }
    void OpenSelectedLog()
    {
        const int index = Selected(); if (index < 0 || static_cast<size_t>(index) >= sessions.size()) return;
        const auto& session = sessions[index]; const CString path = !session.logPath.IsEmpty() ? session.logPath : (!session.crashPath.IsEmpty() ? session.crashPath : session.metadataPath);
        if (path.IsEmpty() || reinterpret_cast<INT_PTR>(::ShellExecute(window, L"open", path, NULL, NULL, SW_SHOWNORMAL)) <= 32) StatusText(Text(L"open_failed", L"Could not open the selected file."));
    }
    void ApplyFont(UINT dpi) {
        HFONT next = UiMetrics::CreateDialogFontForDpi(dpi); if(!next) return;
        ::SendMessage(window,WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);
        ::EnumChildWindows(window,[](HWND child,LPARAM value)->BOOL { ::SendMessage(child,WM_SETFONT,static_cast<WPARAM>(value),TRUE); return TRUE; },reinterpret_cast<LPARAM>(next));
        if(font) ::DeleteObject(font); font=next;
    }
    void Init()
    {
        ::SetWindowText(window, Text(L"title", L"FBE Next diagnostics"));
        const struct { int id; const wchar_t* key; const wchar_t* fallback; } labels[] = {
            {Toggle,L"toggle",L"Toggle trace"},{OpenLog,L"open",L"Open selected file"},{OpenFolder,L"folder",L"Open folder"},
            {Cleanup,L"cleanup",L"Clear old logs"},{Preview,L"preview",L"Preview / refresh"},{Save,L"save",L"Save package"},{Close,L"close",L"Close"},
            {SessionLabel,L"sessions",L"Diagnostic sessions"},{ComponentLabel,L"components",L"Package components (bytes); double-click to exclude/include"}
        };
        for (const auto& label : labels) ::SetDlgItemText(window, label.id, Text(label.key, label.fallback));
        CString source=StartupTrace::TraceSource(); if(source==L"setting") source=Text(L"setting",L"Setting"); else if(source==L"normal") source=Text(L"source_none",L"None");
        CString state; state.Format(Text(L"state", L"Mode: %s; source: %s; write error: %lu\r\nSession: %s\r\nFolder: %s"),
            (LPCWSTR)Text(StartupTrace::Enabled() ? L"extended" : L"normal_mode", StartupTrace::Enabled() ? L"Extended trace" : L"Normal"),
            (LPCWSTR)source, StartupTrace::LastWriteError(), (LPCWSTR)StartupTrace::SessionId(), (LPCWSTR)StartupTrace::CurrentLogDirectory());
        ::SetDlgItemText(window, State, state);
        ApplyFont(UiMetrics::DpiForWindow(window)); ThemeManager::ApplyToWindow(window); RefreshSessions();
        const CString ini = CString(DeploymentContext::SettingsDirectory().c_str()) + L"diagnostics-window.ini";
        const int width = ::GetPrivateProfileInt(L"Window", L"Width", 0, ini), height = ::GetPrivateProfileInt(L"Window", L"Height", 0, ini);
        if (width >= 640 && width <= 16384 && height >= 500 && height <= 16384) ::SetWindowPos(window, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
        const int x = ::GetPrivateProfileInt(L"Window", L"X", -32000, ini), y = ::GetPrivateProfileInt(L"Window", L"Y", -32000, ini);
        RECT candidate = { x,y,x + (width ? width : 800), y + (height ? height : 600) };
        if (x != -32000 && ::MonitorFromRect(&candidate, MONITOR_DEFAULTTONULL)) ::SetWindowPos(window, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    void Layout()
    {
        // Use dialog units, so the system dialog font/DPI governs every control.
        RECT client = {}; ::GetClientRect(window, &client); RECT unit = {0,0,4,8}; ::MapDialogRect(window, &unit);
        const int width = ::MulDiv(client.right, 4, unit.right), height = ::MulDiv(client.bottom, 8, unit.bottom);
        auto move = [&](int id, int x, int y, int w, int h) { RECT r={x,y,x+w,y+h}; ::MapDialogRect(window,&r); ::MoveWindow(::GetDlgItem(window,id),r.left,r.top,r.right-r.left,r.bottom-r.top,TRUE); };
        move(State,8,8,width-16,40); move(SessionLabel,8,76,width-16,10); move(Sessions,8,88,width-16,150);
        move(ComponentLabel,8,110,width-16,10); move(Components,8,122,width-16,65);
        move(PreviewText,8,193,width-16,height-255); move(Status,8,height-57,width-16,25);
        move(Preview,8,height-28,110,18); move(Save,width-200,height-28,110,18); move(Close,width-82,height-28,74,18);
    }
    void CloseDialog()
    {
        RECT r = {}; ::GetWindowRect(window, &r); const CString ini = CString(DeploymentContext::SettingsDirectory().c_str()) + L"diagnostics-window.ini";
        const struct { const wchar_t* key; int value; } values[] = {{L"X",r.left},{L"Y",r.top},{L"Width",r.right-r.left},{L"Height",r.bottom-r.top}};
        for (const auto& value : values) { CString text; text.Format(L"%d",value.value); ::WritePrivateProfileString(L"Window",value.key,text,ini); }
        ::EndDialog(window, IDCANCEL);
    }
    static INT_PTR CALLBACK Proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
    {
        auto self = reinterpret_cast<DiagnosticDialog*>(::GetWindowLongPtr(hwnd, DWLP_USER));
        if (message == WM_INITDIALOG) { self = reinterpret_cast<DiagnosticDialog*>(lp); self->window = hwnd; ::SetWindowLongPtr(hwnd,DWLP_USER,lp); self->Init(); self->Layout(); return TRUE; }
        if (!self) return FALSE;
        if (message == WM_SIZE) { self->Layout(); return TRUE; }
        if (message == WM_GETMINMAXINFO) { auto info = reinterpret_cast<MINMAXINFO*>(lp); RECT minimum={0,0,480,350}; ::MapDialogRect(hwnd,&minimum); info->ptMinTrackSize.x=minimum.right; info->ptMinTrackSize.y=minimum.bottom; return TRUE; }
        if (message == 0x02E0) { self->ApplyFont(HIWORD(wp)); auto r=reinterpret_cast<RECT*>(lp); ::SetWindowPos(hwnd,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER); self->Layout(); return TRUE; }
        if (message == WM_FBE_THEMECHANGED || message == WM_SETTINGCHANGE) { ThemeManager::ApplyToWindow(hwnd); return TRUE; }
        if (message == WM_CLOSE) { self->CloseDialog(); return TRUE; }
        if (message == WM_COMMAND) {
            const int id=LOWORD(wp), code=HIWORD(wp);
            if(id!=Cleanup) self->cleanupPending=false;
            if (id == Close || id == IDCANCEL) self->CloseDialog();
            else if (id == Sessions && code == CBN_SELCHANGE) { self->excluded.clear(); self->Prepare(); }
            else if (id == Components && code == LBN_SELCHANGE) self->ShowPreview();
            else if (id == Components && code == LBN_DBLCLK && self->snapshot) {
                const int i=static_cast<int>(::SendDlgItemMessage(hwnd,Components,LB_GETCURSEL,0,0));
                if (i>=0 && static_cast<size_t>(i)<self->snapshot->Components().size()) {
                    const auto item=self->snapshot->Components()[i];
                    if (item.optional) { auto it=std::find(self->excluded.begin(),self->excluded.end(),item.name); if(it==self->excluded.end()) self->excluded.push_back(item.name); else self->excluded.erase(it); self->Prepare(); }
                }
            }
            else if (id == Preview) self->Prepare();
            else if (id == Save) self->SavePackage();
            else if (id == Toggle) {
                const bool ok=StartupTrace::SetEnabledForNextLaunch(!StartupTrace::IsEnabledByStoredNextLaunchPreference());
                self->StatusText(Text(ok ? L"restart" : L"write_failed",ok ? L"Setting changed; restart required. FBE_NEXT_TRACE can still enable tracing." : L"Could not save setting."));
            }
            else if (id == OpenLog) self->OpenSelectedLog();
            else if (id == OpenFolder) ::ShellExecute(hwnd,L"open",StartupTrace::CurrentLogDirectory(),NULL,NULL,SW_SHOWNORMAL);
            else if (id == Cleanup) {
                if(!self->cleanupPending) {
                    const auto estimate=StartupTrace::CleanupDiagnosticSessions(0,true); CString text;
                    text.Format(Text(L"cleanup_confirm",L"Files / bytes to delete: %u / %llu. Click Clear old logs again to confirm."),estimate.filesDeleted,estimate.bytesDeleted);
                    self->StatusText(text); self->cleanupPending=true; return TRUE;
                }
                self->cleanupPending=false;
                const auto result=self->commands.ClearOldLogSessions(); CString text;
                text.Format(Text(L"cleaned",L"Deleted files / failed / bytes freed: %u / %u / %llu"),result.filesDeleted,result.filesFailed,result.bytesDeleted);
                self->RefreshSessions(); self->StatusText(text);
            }
            return TRUE;
        }
        return FALSE;
    }
public:
    explicit DiagnosticDialog(FbeDiagnostics::DiagnosticCommandService& service) : commands(service) {}
    ~DiagnosticDialog() { if(font) ::DeleteObject(font); }
    void Show(HWND owner) { ::DialogBoxParam(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDD_DIAGNOSTICS), owner, Proc, reinterpret_cast<LPARAM>(this)); }
};
