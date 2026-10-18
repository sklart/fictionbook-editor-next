#pragma once
#include "../StartupTrace.h"
#include "../ThemeManager.h"
#include "../UiMetrics.h"
#include "DiagnosticCommandService.h"
#include "../../common/DeploymentContext.h"
#include <commdlg.h>
#include <thread>
#include <functional>

// A single native dialog owns the immutable snapshot from preview through save.
class DiagnosticDialog
{
    enum { Sessions=IDC_DIAGNOSTIC_SESSIONS, Components, PreviewText, Status, Toggle, OpenLog, OpenFolder, Cleanup, Preview, Save, State, Close, SessionLabel, ComponentLabel };
    enum { CrashLabel=IDC_DIAGNOSTIC_CRASH_LABEL, Crashes=IDC_DIAGNOSTIC_CRASH_REPORTS, ReviewCrash=IDC_DIAGNOSTIC_REVIEW_CRASH, ClearCrashes=IDC_DIAGNOSTIC_CLEAR_CRASHES };
    enum class Work { Catalog, Prepare, Save, Preview, Estimate, Cleanup, ReviewCrash, EstimateCrashes, ClearCrashes };
    struct Job {
        explicit Job(Work kind) : work(kind) {}
        Work work;
        std::atomic_bool cancelled{false}, done{false};
        bool success=false;
        size_t page=0, pages=1;
        CString error, path, preview;
        std::vector<StartupTrace::DiagnosticSession> sessions;
        std::shared_ptr<const StartupTrace::DiagnosticPackageSnapshot> snapshot;
        StartupTrace::DiagnosticLogCleanupResult cleanup;
        std::vector<StartupTrace::DiagnosticCrashReport> crashes;
        StartupTrace::DiagnosticCrashCleanupPlan crashPlan;
    };
    std::shared_ptr<Job> job;
    std::thread worker;
    size_t previewPage=0, previewPages=1;
    bool closing=false, catalogReady=false;
    CString cleanupSummary;
    HWND window = NULL;
    HFONT font = NULL;
    bool cleanupPending = false, reviewPending=false;
    std::shared_ptr<const StartupTrace::DiagnosticCrashCleanupPlan> crashPlan;
    std::vector<StartupTrace::DiagnosticCrashReport> crashes;
    std::vector<StartupTrace::DiagnosticSession> sessions;
    std::vector<CString> excluded;
    std::shared_ptr<const StartupTrace::DiagnosticPackageSnapshot> snapshot;
    static CString Text(const wchar_t* key, const wchar_t* fallback) { return FbeLoadRuntimeStringByKey(CString(L"fbe.diagnostics.") + key, fallback); }
    void StatusText(const CString& text) { ::SetDlgItemText(window, Status, text); }
    int Selected() const { return static_cast<int>(::SendDlgItemMessage(window, Sessions, CB_GETCURSEL, 0, 0)); }
    void Busy(bool value)
    {
        const int controls[]={Sessions,Crashes,ReviewCrash,ClearCrashes,Components,Preview,Save,Toggle,OpenLog,OpenFolder,Cleanup,IDC_DIAGNOSTIC_PREVIOUS_PAGE,IDC_DIAGNOSTIC_NEXT_PAGE};
        for(int id : controls)
            ::EnableWindow(::GetDlgItem(window,id),!value && (id!=Save || (snapshot && snapshot->Valid())));
        ::ShowWindow(::GetDlgItem(window,IDC_DIAGNOSTIC_CANCEL),value ? SW_SHOW : SW_HIDE);
        ::ShowWindow(::GetDlgItem(window,IDC_DIAGNOSTIC_PROGRESS),value ? SW_SHOW : SW_HIDE);
        ::SendDlgItemMessage(window,IDC_DIAGNOSTIC_PROGRESS,PBM_SETMARQUEE,value,30);
        if(!value) {
            ::EnableWindow(::GetDlgItem(window,IDC_DIAGNOSTIC_PREVIOUS_PAGE),snapshot && previewPage>0);
            ::EnableWindow(::GetDlgItem(window,IDC_DIAGNOSTIC_NEXT_PAGE),snapshot && previewPage+1<previewPages);
        }
        Layout();
    }
    void Begin(Work work, const std::function<void(Job&)>& action)
    {
        if(job) return;
        auto pending=std::make_shared<Job>(work); job=pending; Busy(true);
        StatusText(Text(work==Work::Save ? L"saving" : L"working",work==Work::Save ? L"Saving verified package..." : L"Preparing diagnostics..."));
        try {
            // The worker owns its data and never holds a dialog/service pointer or HWND.
            worker=std::thread([pending,action]() {
                StartupTrace::DiagnosticTaskScope cancellation(pending->cancelled);
                try { action(*pending); } catch(...) { pending->success=false; }
                pending->done.store(true,std::memory_order_release);
            });
        } catch(...) { job.reset(); Busy(false); StatusText(Text(L"write_failed",L"Could not start diagnostic operation.")); }
    }
    static void DecodePreview(const std::vector<BYTE>& bytes, size_t requestedPage, Job& result)
    {
        result.pages=(std::max)(static_cast<size_t>(1),(bytes.size()+65535)/65536);
        result.page=(std::min)(requestedPage,result.pages-1);
        size_t start=result.page*65536, end=(std::min)(bytes.size(),start+65536);
        // Split before UTF-8 continuation bytes, so adjacent pages concatenate exactly.
        while(start>0 && start<bytes.size() && (bytes[start]&0xc0)==0x80) --start;
        while(end<bytes.size() && end>start && (bytes[end]&0xc0)==0x80) --end;
        const int count=static_cast<int>(end-start);
        if(count) {
            const char* text=reinterpret_cast<const char*>(bytes.data()+start);
            const int chars=::MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,count,NULL,0);
            if(chars) { ::MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,count,result.preview.GetBuffer(chars),chars); result.preview.ReleaseBuffer(chars); }
        }
        result.success=true;
    }
    void ShowPreview(bool resetPage=true)
    {
        const int index=static_cast<int>(::SendDlgItemMessage(window,Components,LB_GETCURSEL,0,0));
        if(!snapshot || index<0 || static_cast<size_t>(index)>=snapshot->Components().size()) return;
        ::SetDlgItemText(window,PreviewText,L"");
        if(resetPage) previewPage=0;
        const size_t page=previewPage;
        const auto checked=snapshot;
        Begin(Work::Preview,[checked,index,page](Job& result) {
            DecodePreview(checked->Components()[index].bytes,page,result);
            result.snapshot=checked;
        });
    }
    void CompleteJob()
    {
        if(!job || !job->done.load(std::memory_order_acquire)) return;
        if(worker.joinable()) worker.join(); // done is published after all I/O.
        const auto result=job; job.reset(); Busy(false);
        if(result->work==Work::ReviewCrash && result->success)
            for(auto& report:crashes) if(report.path==result->path) report.reviewed=true;
        if(closing) { CloseDialog(); return; }
        if(result->cancelled.load() && !((result->work==Work::Save || result->work==Work::ReviewCrash || result->work==Work::ClearCrashes || result->work==Work::Cleanup) && result->success)) { StatusText(Text(L"cancelled",L"Operation cancelled.")); return; }
        if(result->work==Work::Catalog && result->success) { catalogReady=true; sessions=result->sessions; crashes=result->crashes; ApplyCrashes(); ApplySessions(); }
        else if(result->work==Work::Prepare) {
            snapshot=result->snapshot; ApplyPrepared(result->success);
        }
        else if(result->work==Work::Preview && result->success) {
            previewPage=result->page; previewPages=result->pages; Busy(false);
            ::SetDlgItemText(window,PreviewText,result->preview.IsEmpty() ? Text(L"component_unavailable",L"Component omitted or unavailable; see the manifest.") : result->preview);
            if(!snapshot->Valid()) StatusText(Text(L"blocked",L"Saving blocked: mandatory privacy check failed or size limit exceeded."));
            else if(result->pages>1) {
                CString status; status.Format(Text(L"preview_page",L"Preview page %u / %u; ZIP contains all verified bytes."),static_cast<unsigned int>(result->page+1),static_cast<unsigned int>(result->pages)); StatusText(status);
            }
            else StatusText(Text(L"ready",L"Verified snapshot ready.")+L" "+cleanupSummary);
            const int index=static_cast<int>(::SendDlgItemMessage(window,Components,LB_GETCURSEL,0,0)), selected=Selected();
            if(index>=0 && snapshot && static_cast<size_t>(index)<snapshot->Components().size() && selected>=0 && static_cast<size_t>(selected)<sessions.size()) {
                const auto& component=snapshot->Components()[index];
                if(component.name==L"crash/latest-crash-report.txt" && component.status==L"included")
                    if(StartupTrace::MarkDiagnosticCrashReviewed(sessions[selected])) {
                        for(auto& report:crashes) if(report.path==sessions[selected].crashPath && report.fingerprint==sessions[selected].crashFingerprint) report.reviewed=true;
                        ApplyCrashes();
                    }
            }
        }
        else if(result->work==Work::ReviewCrash) {
            if(result->success) ApplyCrashes();
            StatusText(Text(result->success ? L"review_done" : L"blocked",result->success ? L"Marked reviewed. Report bytes were preserved." : L"Report changed or could not be marked; refresh diagnostics."));
        }
        else if(result->work==Work::EstimateCrashes && result->success) {
            crashPlan=std::make_shared<const StartupTrace::DiagnosticCrashCleanupPlan>(result->crashPlan);
            CString text; text.Format(Text(L"crash_cleanup_confirm",L"Reviewed crashes older than 30 days: %u files / %llu bytes. Click Clear old crashes again to delete."),crashPlan->files,crashPlan->bytes); StatusText(text);
        }
        else if(result->work==Work::ClearCrashes) {
            cleanupSummary.Format(Text(L"cleaned",L"Deleted files / failed / bytes freed: %u / %u / %llu"),result->cleanup.filesDeleted,result->cleanup.filesFailed,result->cleanup.bytesDeleted);
            crashPlan.reset(); RefreshSessions();
        }
        else if(result->work==Work::Save) StatusText(result->success ? Text(L"saved",L"Package saved.")+L" "+result->path : Text(L"write_failed",L"Could not save. Choose a new filename and check free space and permissions."));
        else if(result->work==Work::Estimate) {
            CString text; text.Format(Text(L"cleanup_confirm",L"Files / bytes to delete: %u / %llu. Click Clear old logs again to confirm."),result->cleanup.filesDeleted,result->cleanup.bytesDeleted);
            cleanupPending=true; StatusText(text);
        }
        else if(result->work==Work::Cleanup) {
            cleanupSummary.Format(Text(L"cleaned",L"Deleted files / failed / bytes freed: %u / %u / %llu"),result->cleanup.filesDeleted,result->cleanup.filesFailed,result->cleanup.bytesDeleted);
            snapshot.reset(); RefreshSessions();
        }
        else StatusText(Text(L"blocked",L"Diagnostic operation failed."));
    }
    void Prepare()
    {
        snapshot.reset(); previewPage=0; previewPages=1; ::SetDlgItemText(window,PreviewText,L"");
        ::SendDlgItemMessage(window,Components,LB_RESETCONTENT,0,0);
        const int index=Selected(); if(index<0 || static_cast<size_t>(index)>=sessions.size()) { if(sessions.empty()) RefreshSessions(); else Busy(false); return; }
        const auto session=sessions[index]; const auto exclusions=excluded;
        Begin(Work::Prepare,[session,exclusions](Job& result) {
            FbeDiagnostics::DiagnosticCommandService commands;
            result.success=commands.PreparePackage(session,exclusions,result.snapshot,result.error);
        });
    }
    void ApplyPrepared(bool valid)
    {
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
        int preview=0; const int session=Selected();
        if(snapshot && session>=0 && static_cast<size_t>(session)<sessions.size() && !StartupTrace::IsDiagnosticCrashReviewed(sessions[session]))
            for(size_t i=0;i<snapshot->Components().size();++i)
                if(snapshot->Components()[i].name==L"crash/latest-crash-report.txt" && snapshot->Components()[i].status==L"included") preview=static_cast<int>(i);
        ::SendDlgItemMessage(window, Components, LB_SETCURSEL, preview, 0); ShowPreview();
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
        snapshot.reset();
        Begin(Work::Catalog,[](Job& result) {
            FbeDiagnostics::DiagnosticCommandService commands;
            result.sessions=commands.Sessions(); result.crashes=StartupTrace::EnumerateDiagnosticCrashReports(); result.success=true;
        });
    }
    void ApplyCrashes()
    {
        const int previous=static_cast<int>(::SendDlgItemMessage(window,Crashes,CB_GETCURSEL,0,0));
        ::SendDlgItemMessage(window,Crashes,CB_RESETCONTENT,0,0);
        int selected=previous>=0 && static_cast<size_t>(previous)<crashes.size() ? previous : 0;
        bool unread=false;
        for(size_t i=0;i<crashes.size();++i) {
            const auto& report=crashes[i]; const int slash=report.path.ReverseFind(L'\\');
            CString row=Text(report.reviewed ? L"reviewed" : L"unread",report.reviewed ? L"Reviewed" : L"New")+L" | "+report.path.Mid(slash+1);
            ::SendDlgItemMessage(window,Crashes,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(row.GetString()));
            if(!unread && !report.reviewed) { selected=static_cast<int>(i); unread=true; }
        }
        ::SendDlgItemMessage(window,Crashes,CB_SETCURSEL,selected,0);
    }
    void ReviewSelectedCrash()
    {
        const int index=static_cast<int>(::SendDlgItemMessage(window,Crashes,CB_GETCURSEL,0,0));
        if(index<0 || static_cast<size_t>(index)>=crashes.size()) return;
        if(!reviewPending) { reviewPending=true; StatusText(Text(L"review_confirm",L"Click Mark reviewed again to acknowledge this file, including an unreadable preview. Its contents will not be exported.")); return; }
        reviewPending=false; const auto report=crashes[index];
        Begin(Work::ReviewCrash,[report](Job& result) { result.path=report.path; result.success=StartupTrace::MarkDiagnosticCrashReportReviewed(report); });
    }
    void ApplySessions()
    {
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
            if (!selectedCrash && session.outcome == L"crash" && !StartupTrace::IsDiagnosticCrashReviewed(session)) { selected = static_cast<int>(i); selectedCrash = true; }
        }
        ::SendDlgItemMessage(window, Sessions, CB_SETCURSEL, selected, 0); excluded.clear(); Prepare();
    }
    void SavePackage()
    {
        if (!snapshot || !snapshot->Valid()) return;
        wchar_t path[MAX_PATH] = {};
        const CString suggested=L"FBE-Diagnostics-"+StartupTrace::SessionId()+L".zip";
        wcsncpy_s(path,suggested,_TRUNCATE);
        const CString title = Text(L"save", L"Save package");
        wchar_t filterBuffer[] = L"ZIP (*.zip)\0*.zip\0\0";
        OPENFILENAME file = {}; file.lStructSize = sizeof(file); file.hwndOwner = window; file.lpstrFile = path; file.nMaxFile = _countof(path);
        file.lpstrFilter = filterBuffer; file.lpstrTitle = title; file.lpstrDefExt = L"zip"; file.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!::GetSaveFileName(&file)) return;
        const auto checked=snapshot; const CString requested(path);
        Begin(Work::Save,[checked,requested](Job& result) {
            FbeDiagnostics::DiagnosticCommandService commands;
            result.success=commands.SavePackageUnique(*checked,requested,result.path,result.error);
        });
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
    void ApplyLabels()
    {
        ::SetWindowText(window, Text(L"title", L"FBE Next diagnostics"));
        const struct { int id; const wchar_t* key; const wchar_t* fallback; } labels[] = {
            {Toggle,L"toggle",L"Toggle trace"},{OpenLog,L"open",L"Open selected file"},{OpenFolder,L"folder",L"Open folder"},
            {Cleanup,L"cleanup",L"Clear old logs"},{Preview,L"preview",L"Preview / refresh"},{Save,L"save",L"Save package"},{Close,L"close",L"Close"},
            {CrashLabel,L"crash_reports",L"Crash reports (including damaged reports)"},{ReviewCrash,L"review_crash",L"Mark reviewed"},{ClearCrashes,L"clear_crashes",L"Clear old crashes"},
            {SessionLabel,L"sessions",L"Diagnostic sessions"},{ComponentLabel,L"components",L"Package components (bytes); double-click to exclude/include"}
        };
        ::SetDlgItemText(window,IDC_DIAGNOSTIC_PREVIOUS_PAGE,Text(L"previous_page",L"< Back"));
        ::SetDlgItemText(window,IDC_DIAGNOSTIC_NEXT_PAGE,Text(L"next_page",L"Next >"));
        for (const auto& label : labels) ::SetDlgItemText(window, label.id, Text(label.key, label.fallback));
        ::SetDlgItemText(window,IDC_DIAGNOSTIC_CANCEL,Text(L"cancel",L"Cancel"));
    }
    int ButtonWidthUnits(int id, int minimum) const
    {
        HWND child=::GetDlgItem(window,id); wchar_t label[256]={}; ::GetWindowText(child,label,_countof(label));
        HDC dc=::GetDC(child); if(!dc) return minimum;
        HGDIOBJ old=::SelectObject(dc,reinterpret_cast<HFONT>(::SendMessage(child,WM_GETFONT,0,0)));
        SIZE text={}; ::GetTextExtentPoint32(dc,label,static_cast<int>(wcslen(label)),&text);
        ::SelectObject(dc,old); ::ReleaseDC(child,dc); RECT unit={0,0,4,8}; ::MapDialogRect(window,&unit);
        return (std::max)(minimum,static_cast<int>((text.cx*4+unit.right-1)/unit.right)+12);
    }
    SIZE MinimumSize() const
    {
        int top=0; for(int id : {Toggle,OpenLog,OpenFolder,Cleanup}) top=(std::max)(top,ButtonWidthUnits(id,0));
        const int bottom=ButtonWidthUnits(Preview,110)+ButtonWidthUnits(IDC_DIAGNOSTIC_PREVIOUS_PAGE,50)+ButtonWidthUnits(IDC_DIAGNOSTIC_NEXT_PAGE,50)+ButtonWidthUnits(Save,110)+ButtonWidthUnits(Close,74)+48;
        const int crashRow=ButtonWidthUnits(ReviewCrash,100)+ButtonWidthUnits(ClearCrashes,120)+230;
        RECT bounds={0,0,(std::max)(crashRow,(std::max)(540,(std::max)(top*4+34,bottom))),380}; ::MapDialogRect(window,&bounds);
        ::AdjustWindowRectEx(&bounds,static_cast<DWORD>(::GetWindowLongPtr(window,GWL_STYLE)),FALSE,static_cast<DWORD>(::GetWindowLongPtr(window,GWL_EXSTYLE)));
        SIZE size={bounds.right-bounds.left,bounds.bottom-bounds.top}; return size;
    }
    void Init()
    {
        ApplyLabels();
        CString source=StartupTrace::TraceSource(); if(source==L"setting") source=Text(L"setting",L"Setting"); else if(source==L"normal") source=Text(L"source_none",L"None");
        CString state; state.Format(Text(L"state", L"Mode: %s; source: %s; write error: %lu\r\nSession: %s\r\nFolder: %s"),
            (LPCWSTR)Text(StartupTrace::Enabled() ? L"extended" : L"normal_mode", StartupTrace::Enabled() ? L"Extended trace" : L"Normal"),
            (LPCWSTR)source, StartupTrace::LastWriteError(), (LPCWSTR)StartupTrace::SessionId(), (LPCWSTR)StartupTrace::CurrentLogDirectory());
        ::SetDlgItemText(window, State, state);
        ::SendDlgItemMessage(window,PreviewText,EM_SETLIMITTEXT,65536,0);
        ApplyFont(UiMetrics::DpiForWindow(window)); ThemeManager::ApplyToWindow(window);
        ::SetDlgItemText(window,IDC_DIAGNOSTIC_CANCEL,Text(L"cancel",L"Cancel"));
        ::SetTimer(window,1,100,NULL); RefreshSessions();
        const CString ini = CString(DeploymentContext::SettingsDirectory().c_str()) + L"diagnostics-window.ini";
        const int width = ::GetPrivateProfileInt(L"Window", L"Width", 0, ini), height = ::GetPrivateProfileInt(L"Window", L"Height", 0, ini);
        if (width >= 640 && width <= 16384 && height >= 500 && height <= 16384) {
            const SIZE minimum=MinimumSize();
            ::SetWindowPos(window,NULL,0,0,(std::max)(width,static_cast<int>(minimum.cx)),(std::max)(height,static_cast<int>(minimum.cy)),SWP_NOMOVE|SWP_NOZORDER);
        }
        // Also honor localized button widths on the very first open (no saved size).
        RECT current={}; ::GetWindowRect(window,&current); const SIZE minimum=MinimumSize();
        if(current.right-current.left<minimum.cx || current.bottom-current.top<minimum.cy)
            ::SetWindowPos(window,NULL,0,0,(std::max)(current.right-current.left,minimum.cx),(std::max)(current.bottom-current.top,minimum.cy),SWP_NOMOVE|SWP_NOZORDER);
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
        move(State,8,8,width-16,40);
        const int button=(width-34)/4;
        move(Toggle,8,52,button,18); move(OpenLog,14+button,52,button,18);
        move(OpenFolder,20+2*button,52,button,18); move(Cleanup,26+3*button,52,button,18); move(SessionLabel,8,76,width-16,10); move(Sessions,8,88,width-16,150);
        const int review=ButtonWidthUnits(ReviewCrash,100), clear=ButtonWidthUnits(ClearCrashes,120);
        move(CrashLabel,8,110,width-16,10); move(Crashes,8,122,width-review-clear-28,150);
        move(ReviewCrash,width-clear-review-14,122,review,18); move(ClearCrashes,width-clear-8,122,clear,18);
        move(ComponentLabel,8,146,width-16,10); move(Components,8,158,width-16,65);
        move(PreviewText,8,229,width-16,(std::max)(20,height-291));
        move(Status,8,height-57,job ? width-220 : width-16,25);
        move(IDC_DIAGNOSTIC_PROGRESS,width-204,height-49,100,8);
        move(IDC_DIAGNOSTIC_CANCEL,width-98,height-57,90,18);
        const int preview=ButtonWidthUnits(Preview,110), back=ButtonWidthUnits(IDC_DIAGNOSTIC_PREVIOUS_PAGE,50), next=ButtonWidthUnits(IDC_DIAGNOSTIC_NEXT_PAGE,50);
        const int save=ButtonWidthUnits(Save,110), close=ButtonWidthUnits(Close,74);
        move(Preview,8,height-28,preview,18);
        move(IDC_DIAGNOSTIC_PREVIOUS_PAGE,16+preview,height-28,back,18); move(IDC_DIAGNOSTIC_NEXT_PAGE,24+preview+back,height-28,next,18);
        move(Save,width-16-close-save,height-28,save,18); move(Close,width-8-close,height-28,close,18);
    }
    void CloseDialog()
    {
        if(job) {
            closing=true; job->cancelled.store(true); cleanupPending=false;
            ::EnableWindow(::GetDlgItem(window,IDC_DIAGNOSTIC_CANCEL),FALSE);
            StatusText(Text(L"cancelling",L"Cancelling diagnostic operation...")); return;
        }
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
        if(message==WM_TIMER && wp==1) { self->CompleteJob(); self->TestTick(); return TRUE; }
        if(message==WM_DESTROY) { ::KillTimer(hwnd,1); if(self->job) self->job->cancelled.store(true); return TRUE; }
        if (message == WM_SIZE && wp!=SIZE_MINIMIZED) { self->Layout(); return TRUE; }
        if (message == WM_GETMINMAXINFO) { auto info=reinterpret_cast<MINMAXINFO*>(lp); const SIZE size=self->MinimumSize();
            info->ptMinTrackSize.x=size.cx; info->ptMinTrackSize.y=size.cy; return TRUE; }
        if (message == 0x02E0) { self->ApplyFont(HIWORD(wp)); auto r=reinterpret_cast<RECT*>(lp); ::SetWindowPos(hwnd,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER); self->Layout(); return TRUE; }
        if (message == WM_FBE_THEMECHANGED || message == WM_SETTINGCHANGE) { ThemeManager::ApplyToWindow(hwnd); return TRUE; }
        if (message == WM_CLOSE) { self->CloseDialog(); return TRUE; }
        if (message == WM_COMMAND) {
            const int id=LOWORD(wp), code=HIWORD(wp);
            if(id==IDC_DIAGNOSTIC_CANCEL && self->job) { self->job->cancelled.store(true); self->StatusText(Text(L"cancelling",L"Cancelling diagnostic operation...")); return TRUE; }
            if(self->job && id!=Close && id!=IDCANCEL) return TRUE;
            if(id!=Cleanup) self->cleanupPending=false;
            if(id!=ReviewCrash) self->reviewPending=false;
            if(id!=ClearCrashes) self->crashPlan.reset();
            if (id == Close || id == IDCANCEL) self->CloseDialog();
            else if(id==ReviewCrash) self->ReviewSelectedCrash();
            else if(id==ClearCrashes) {
                if(!self->crashPlan) self->Begin(Work::EstimateCrashes,[](Job& result) { result.crashPlan=StartupTrace::PrepareDiagnosticCrashCleanup(); result.success=true; });
                else { const auto plan=self->crashPlan; self->crashPlan.reset(); self->Begin(Work::ClearCrashes,[plan](Job& result) { result.cleanup=StartupTrace::ExecuteDiagnosticCrashCleanup(*plan); result.success=true; }); }
            }
            else if (id == Sessions && code == CBN_SELCHANGE) { self->excluded.clear(); self->Prepare(); }
            else if (id == Components && code == LBN_SELCHANGE) self->ShowPreview();
            else if (id == Components && code == LBN_DBLCLK && self->snapshot) {
                const int i=static_cast<int>(::SendDlgItemMessage(hwnd,Components,LB_GETCURSEL,0,0));
                if (i>=0 && static_cast<size_t>(i)<self->snapshot->Components().size()) {
                    const auto& item=self->snapshot->Components()[i];
                    if (item.optional) { auto it=std::find(self->excluded.begin(),self->excluded.end(),item.name); if(it==self->excluded.end()) self->excluded.push_back(item.name); else self->excluded.erase(it); self->Prepare(); }
                }
            }
            else if(id==IDC_DIAGNOSTIC_PREVIOUS_PAGE && self->previewPage>0) { --self->previewPage; self->ShowPreview(false); }
            else if(id==IDC_DIAGNOSTIC_NEXT_PAGE && self->previewPage+1<self->previewPages) { ++self->previewPage; self->ShowPreview(false); }
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
                    self->Begin(Work::Estimate,[](Job& result) { result.cleanup=StartupTrace::CleanupDiagnosticSessions(0,true); result.success=true; });
                } else {
                    self->cleanupPending=false;
                    self->Begin(Work::Cleanup,[](Job& result) { FbeDiagnostics::DiagnosticCommandService commands; result.cleanup=commands.ClearOldLogSessions(); result.success=true; });
                }
            }
            return TRUE;
        }
        return FALSE;
    }
#include "../testing/DiagnosticDialogTest.inl"
public:
    explicit DiagnosticDialog(FbeDiagnostics::DiagnosticCommandService&) {}
    bool CatalogReady() const { return catalogReady; }
    bool HasUnreviewedCrashes() const { return std::any_of(crashes.begin(),crashes.end(),[](const StartupTrace::DiagnosticCrashReport& report){return !report.reviewed;}); }
    ~DiagnosticDialog() {
        // Normal close waits through the message loop. This guard also covers an
        // externally destroyed modal/WM_QUIT before StartupTrace is shut down.
        if(job) job->cancelled.store(true);
        if(worker.joinable()) worker.join();
        if(font) ::DeleteObject(font);
    }
    void Show(HWND owner) { ::DialogBoxParam(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDD_DIAGNOSTICS), owner, Proc, reinterpret_cast<LPARAM>(this)); }
};
