#include "../stdafx.h"
#include "UiWatchdog.h"
#include "../CrashHandler.h"
#include <algorithm>
#include <system_error>
#include <new>

namespace {
    std::atomic<DWORD> nextSequence{0};
    DWORD NewSequence() { DWORD value=nextSequence.fetch_add(1)+1; if(!value) value=nextSequence.fetch_add(1)+1; return value; }
    ULONGLONG AwakeMilliseconds() { ULONGLONG ticks=0; return ::QueryUnbiasedInterruptTime(&ticks) ? ticks/10000 : ::GetTickCount64(); }
    const wchar_t* OperationName(StartupTrace::Operation operation) {
        const wchar_t* names[]={L"none",L"load",L"save",L"validate",L"script",L"view-switch"};
        const size_t n=static_cast<size_t>(operation); return n<_countof(names) ? names[n] : names[0];
    }
    const wchar_t* ModeName(StartupTrace::EditorMode mode) {
        const wchar_t* names[]={L"unknown",L"body",L"description",L"source"};
        const size_t n=static_cast<size_t>(mode); return n<_countof(names) ? names[n] : names[0];
    }
    CString TelemetryText(const StartupTrace::HangTelemetry& sample) {
        CString text; const ULONGLONG now=::GetTickCount64();
        text.Format(L"operation-statistics-source=watchdog-snapshot\r\nactive-editor-mode=%s\r\nstats-dropped=%llu\r\nstats-last-completed=%s\r\nstats-last-result=%s\r\nstats-last-duration-ms=%llu\r\nstats-in-progress=%s\r\nstats-in-progress-ms=%llu\r\n",
            ModeName(sample.mode),sample.dropped,OperationName(sample.lastCompleted),sample.lastCompleted==StartupTrace::Operation::None ? L"unknown" : (sample.lastSucceeded ? L"success" : L"failure"),sample.lastDuration,
            OperationName(sample.inProgress),sample.inProgressSince && now>=sample.inProgressSince ? now-sample.inProgressSince : 0);
        for(size_t i=1;i<static_cast<size_t>(StartupTrace::Operation::Count);++i) {
            const auto& c=sample.operations[i]; const wchar_t* name=OperationName(static_cast<StartupTrace::Operation>(i));
            text.AppendFormat(L"stats-%s-started=%llu\r\nstats-%s-completed=%llu\r\nstats-%s-failed=%llu\r\nstats-%s-active=%llu\r\nstats-%s-total-ms=%llu\r\nstats-%s-last-ms=%llu\r\nstats-%s-max-ms=%llu\r\n",
                name,c.started,name,c.completed,name,c.failed,name,c.active,name,c.totalMs,name,c.lastMs,name,c.maxMs);
        }
        const auto code=[](const wchar_t* value) { return value[0] ? value : L"unknown"; };
        text.AppendFormat(L"hang-last-event-code=%s\r\nhang-last-document-code=%s\r\nhang-last-script-code=%s\r\nhang-last-script-failure-code=%s\r\nhang-last-hresult-code=%s\r\nhang-hresult-available=%u\r\nhang-last-hresult=0x%08lX\r\nhang-failure-available=%u\r\nhang-last-failure-hresult=0x%08lX\r\n",
            code(sample.eventCode),code(sample.documentCode),code(sample.scriptCode),code(sample.scriptFailureCode),code(sample.hresultCode),sample.hasHresult ? 1U:0U,sample.hresult,sample.hasFailure ? 1U:0U,sample.failureHresult);
        return text;
    }
    DWORD DirectoryError(const CString& directory) {
        const DWORD attributes=::GetFileAttributes(directory);
        if(attributes==INVALID_FILE_ATTRIBUTES) return ::GetLastError();
        if(!(attributes&FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
        return (attributes&FILE_ATTRIBUTE_REPARSE_POINT) ? ERROR_ACCESS_DENIED : ERROR_SUCCESS;
    }
}
FbeDiagnostics::WatchdogOptions FbeDiagnostics::WatchdogOptions::CurrentProcess()
{
    WatchdogOptions options; wchar_t guard[2]={}, scenario[64]={}, shortMode[2]={};
    if(::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE",guard,2)==1 && guard[0]==L'1' &&
        ::GetEnvironmentVariable(L"FBE_NEXT_TEST_WATCHDOG_SHORT",shortMode,2)==1 && shortMode[0]==L'1' &&
        ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SCENARIO",scenario,_countof(scenario)) && wcscmp(scenario,L"watchdog-runtime")==0)
        { options.intervalMs=25; options.suspectMs=150; options.confirmMs=300; }
    return options;
}
bool FbeDiagnostics::UiWatchdog::Start(HWND window)
{
    std::lock_guard<std::mutex> lock(lifecycleMutex);
    if(state.load(std::memory_order_acquire)!=State::Stopped) return false;
    StopLocked(); // Reap a completed worker before reusing its event/HWND.
    if(!::IsWindow(window)) { writeError.store(ERROR_INVALID_WINDOW_HANDLE); return false; }
    stopEvent=::CreateEvent(NULL,TRUE,FALSE,NULL);
    if(!stopEvent) { writeError.store(::GetLastError()); return false; }
    ownerWindow=window;
    stopping.store(false); paused.store(false); posted.store(0); acknowledged.store(0); lastReply.store(0);
    posts.store(0); replies.store(0); suspicions.store(0); confirmations.store(0); recoveries.store(0);
    try {
        const CString session=StartupTrace::SessionId(), directory=StartupTrace::CurrentLogDirectory(), environment=StartupTrace::HangEnvironmentReport();
        const DWORD uiThread=::GetWindowThreadProcessId(window,NULL);
        if(!uiThread) { writeError.store(ERROR_INVALID_WINDOW_HANDLE); StopLocked(); return false; }
        const auto options=WatchdogOptions::CurrentProcess();
        state.store(State::Starting,std::memory_order_release);
        worker=std::thread([this,window,uiThread,session,directory,environment,options]() {
            try { Run(window,uiThread,session,directory,environment,options); }
            catch(const std::bad_alloc&) { writeError.store(ERROR_NOT_ENOUGH_MEMORY); }
            catch(...) { writeError.store(ERROR_UNHANDLED_EXCEPTION); }
            // Publish only after Run's report handles and every error have been finalized.
            state.store(State::Stopped,std::memory_order_release);
        });
    } catch(const std::bad_alloc&) { writeError.store(ERROR_NOT_ENOUGH_MEMORY); StopLocked(); return false; }
    catch(const std::system_error&) { writeError.store(ERROR_NO_SYSTEM_RESOURCES); StopLocked(); return false; }
    catch(...) { writeError.store(ERROR_UNHANDLED_EXCEPTION); StopLocked(); return false; }
    return true; // Accepted asynchronous start; Running becomes true only after initialization.
}
void FbeDiagnostics::UiWatchdog::Stop()
{
    std::lock_guard<std::mutex> lock(lifecycleMutex);
    StopLocked();
}
void FbeDiagnostics::UiWatchdog::StopLocked()
{
    stopping.store(true); if(stopEvent) ::SetEvent(stopEvent);
    if(worker.joinable()) worker.join();
    if(stopEvent) { ::CloseHandle(stopEvent); stopEvent=NULL; }
    // Remove our one outstanding probe before a possible restart, on the owning UI thread.
    if(ownerWindow && ::GetWindowThreadProcessId(ownerWindow,NULL)==::GetCurrentThreadId()) {
        MSG stale={}; ::PeekMessage(&stale,ownerWindow,WatchdogHeartbeatMessage,WatchdogHeartbeatMessage,PM_REMOVE);
    }
    ownerWindow=NULL;
    state.store(State::Stopped,std::memory_order_release);
}
void FbeDiagnostics::UiWatchdog::Acknowledge(DWORD sequence) noexcept
{
    if(sequence && sequence==posted.load(std::memory_order_acquire)) {
        lastReply.store(AwakeMilliseconds()); acknowledged.store(sequence,std::memory_order_release); replies.fetch_add(1);
    }
}
void FbeDiagnostics::UiWatchdog::Run(HWND window,DWORD uiThread,const CString& session,const CString& directory,const CString& environment,WatchdogOptions options)
{
    CString root(directory); root.TrimRight(L"\\/"); const CString parent=root; root+=L"\\Hangs\\";
    DWORD error=DirectoryError(parent); if(error) { writeError.store(error); return; }
    CString folder(root); folder.TrimRight(L"\\/");
    error=DirectoryError(folder);
    if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND) {
        if(!::CreateDirectory(folder,NULL)) {
            error=::GetLastError(); if(error!=ERROR_ALREADY_EXISTS) { writeError.store(error); return; }
        }
        error=DirectoryError(folder);
    }
    if(error) { writeError.store(error); return; }
    HANDLE access=::CreateFile(folder,FILE_ADD_FILE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(access==INVALID_HANDLE_VALUE) { writeError.store(::GetLastError()); return; }
    BY_HANDLE_FILE_INFORMATION info={};
    const bool inspected=::GetFileInformationByHandle(access,&info)!=FALSE;
    error=inspected ? (!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) ? ERROR_DIRECTORY : ((info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) ? ERROR_ACCESS_DENIED : ERROR_SUCCESS)) : ::GetLastError();
    ::CloseHandle(access); if(error) { writeError.store(error); return; }
    if(stopping.load()) return;
    state.store(State::Running,std::memory_order_release);
    // Deterministic exception coverage, gated by the existing three test-only short-mode guards.
    wchar_t fault[16]={};
    if(options.intervalMs==25 && ::GetEnvironmentVariable(L"FBE_NEXT_TEST_WATCHDOG_FAULT",fault,_countof(fault)) && wcscmp(fault,L"exception")==0) throw ERROR_UNHANDLED_EXCEPTION;
    struct ReportFile { HANDLE handle=INVALID_HANDLE_VALUE; ~ReportFile() { Close(); } void Close() { if(handle!=INVALID_HANDLE_VALUE) { ::CloseHandle(handle); handle=INVALID_HANDLE_VALUE; } } } report;
    // Start is called after UI readiness; the first unanswered probe is monitored too.
    WatchdogState episode; episode.Ready(); DWORD tracked=0, observedEpoch=epoch.load(), incident=0;
    ULONGLONG previousAwake=AwakeMilliseconds(), previousWall=::GetTickCount64();
    StartupTrace::HangTelemetry cached; bool haveTelemetry=false, monitoringInterrupted=false; ULONGLONG excludedMs=0;
    auto record=[&](const wchar_t* status,bool confirmed,ULONGLONG now) {
        if(stopping.load() || paused.load() || CrashDiagnostics::HandlingCrash() || ::IsDebuggerPresent()) return;
        StartupTrace::HangTelemetry latest; const bool fresh=StartupTrace::TryGetHangTelemetry(latest);
        if(fresh) { cached=latest; haveTelemetry=true; }
        SYSTEMTIME utc={}; ::GetSystemTime(&utc); const ULONGLONG wall=::GetTickCount64();
        CString text; text.Format(L"hang-format=1\r\nsession_id=%s\r\nhang-incident=%lu\r\nhang-state=%s\r\nhang-confirmed=%u\r\nhang-utc=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\r\nhang-duration-ms=%llu\r\nhang-ui-thread-id=%lu\r\nhang-worker-thread-id=%lu\r\nhang-ui-state=%s\r\nhang-clock=unbiased\r\nhang-request-ms=%llu\r\nhang-last-reply-ms=%llu\r\nhang-heartbeat-sequence=%lu\r\nhang-heartbeat-acknowledged=%lu\r\nhang-telemetry-available=%u\r\nhang-telemetry-fresh=%u\r\nhang-telemetry-age-ms=%llu\r\nhang-threshold-suspect-ms=%lu\r\nhang-threshold-confirm-ms=%lu\r\nhang-dump=not-requested\r\n",
            (LPCWSTR)session,incident,status,confirmed ? 1U:0U,utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds,
            now>=episode.requestedAt ? (now-episode.requestedAt)-(std::min)(excludedMs,now-episode.requestedAt):0,uiThread,::GetCurrentThreadId(),wcscmp(status,L"recovered")==0 ? L"responding" : L"heartbeat-timeout",episode.requestedAt,lastReply.load(),posted.load(),acknowledged.load(),haveTelemetry ? 1U:0U,fresh ? 1U:0U,
            haveTelemetry && wall>=cached.sampledAt ? wall-cached.sampledAt:0,options.suspectMs,options.confirmMs);
        text.AppendFormat(L"hang-monitoring-interrupted=%u\r\nhang-monitor-paused-ms=%llu\r\n",monitoringInterrupted ? 1U:0U,excludedMs);
        text+=TelemetryText(cached); text+=environment;
        // Same typed schema is checked again when building a package. No trace/crash locks or UI calls.
        if(!StartupTrace::ValidateHangReport(text,session)) { writeError.store(ERROR_INVALID_DATA); return; }
        if(report.handle==INVALID_HANDLE_VALUE) {
            CString path; path.Format(L"%sfbe-hang-%s-%lu.txt",(LPCWSTR)root,(LPCWSTR)session,incident);
            report.handle=::CreateFile(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
            if(report.handle==INVALID_HANDLE_VALUE) { writeError.store(::GetLastError()); return; }
        }
        const CStringA bytes(CW2A(text,CP_UTF8)); DWORD written=0;
        if(!::WriteFile(report.handle,bytes,static_cast<DWORD>(bytes.GetLength()),&written,NULL)) writeError.store(::GetLastError());
        else if(written!=static_cast<DWORD>(bytes.GetLength())) writeError.store(ERROR_WRITE_FAULT);
        else if(!::FlushFileBuffers(report.handle)) writeError.store(::GetLastError());
    };
    while(!stopping.load()) {
        const DWORD waited=::WaitForSingleObject(stopEvent,options.intervalMs);
        if(waited==WAIT_OBJECT_0) break;
        if(waited!=WAIT_TIMEOUT) { writeError.store(waited==WAIT_FAILED ? ::GetLastError() : ERROR_GEN_FAILURE); return; }
        if(stopping.load()) break;
        const ULONGLONG now=AwakeMilliseconds(), wall=::GetTickCount64(); const DWORD currentEpoch=epoch.load();
        const bool discontinuity=WatchdogState::ClockDiscontinuity(now,previousAwake,wall,previousWall);
        const ULONGLONG delta=now>=previousAwake ? now-previousAwake : 0;
        previousAwake=now; previousWall=wall;
        const auto resetMonitoring=[&]() {
            if(episode.suspected) {
                // Keep an already reported episode until its UI reply. Suppression must
                // not lose recovery or create another report for the same continuous stall.
                monitoringInterrupted=true; excludedMs+=delta;
            } else {
                const bool ready=episode.armed; episode.Reset(); episode.armed=ready;
                tracked=0; report.Close(); excludedMs=0; monitoringInterrupted=false;
            }
            observedEpoch=currentEpoch;
        };
        if(paused.load() || ::IsDebuggerPresent() || CrashDiagnostics::HandlingCrash()) { resetMonitoring(); continue; }
        if(discontinuity || observedEpoch!=currentEpoch) resetMonitoring();
        StartupTrace::HangTelemetry latest; if(StartupTrace::TryGetHangTelemetry(latest)) { cached=latest; haveTelemetry=true; }
        const DWORD pending=posted.load(), reply=acknowledged.load(std::memory_order_acquire);
        if(pending && pending==reply && tracked==pending) {
            const bool wasConfirmed=episode.confirmed;
            if(episode.suspected) { record(L"recovered",wasConfirmed,lastReply.load()); recoveries.fetch_add(1); }
            episode.Reply(); report.Close(); tracked=0; monitoringInterrupted=false; excludedMs=0;
        }
        if(!pending || pending==reply) {
            const DWORD sequence=NewSequence(); posted.store(sequence,std::memory_order_release);
            if(stopping.load()) break;
            if(::PostMessage(window,WatchdogHeartbeatMessage,sequence,0)) { posts.fetch_add(1); tracked=sequence; episode.Request(now); }
            else { writeError.store(::GetLastError()); posted.store(reply); episode.Reset(); episode.Ready(); tracked=0; }
        } else {
            if(tracked!=pending) { tracked=pending; episode.Request(now); }
            const unsigned int events=episode.Advance(now>=excludedMs ? now-excludedMs:0,options);
            if(events&1) { incident=NewSequence(); suspicions.fetch_add(1); record(L"suspected",false,now); }
            // A future optional dump collector must require a separate explicit,
            // revocable user consent. This path only writes typed technical text.
            if(events&2) { confirmations.fetch_add(1); record(L"confirmed",true,now); }
        }
    }
}
