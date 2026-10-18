#pragma once
#include "../StartupTrace.h"
#include <thread>

namespace FbeDiagnostics {
constexpr UINT WatchdogHeartbeatMessage=WM_APP+0x270;
struct WatchdogOptions {
    DWORD intervalMs=1000, suspectMs=15000, confirmMs=30000;
    static WatchdogOptions CurrentProcess();
};
// Pure transition logic is also exercised with deterministic clocks in runtime tests.
struct WatchdogState {
    bool armed=false, waiting=false, suspected=false, confirmed=false;
    ULONGLONG requestedAt=0;
    static bool ClockDiscontinuity(ULONGLONG awake,ULONGLONG previousAwake,ULONGLONG wall,ULONGLONG previousWall) {
        return awake<previousAwake || wall<previousWall || awake-previousAwake>5000 || wall-previousWall>(awake-previousAwake)+2000;
    }
    void Reset() { *this=WatchdogState(); }
    void Ready() { armed=true; }
    void Request(ULONGLONG now) { waiting=true; requestedAt=now; }
    bool Reply() { const bool recovered=suspected; waiting=false; suspected=confirmed=false; armed=true; return recovered; }
    unsigned int Advance(ULONGLONG now,const WatchdogOptions& options) {
        unsigned int events=0;
        if(!armed || !waiting || now<requestedAt) return events;
        const ULONGLONG elapsed=now-requestedAt;
        if(!suspected && elapsed>options.suspectMs) { suspected=true; events|=1; }
        if(!confirmed && elapsed>options.confirmMs) { confirmed=true; events|=2; }
        return events;
    }
};
class UiWatchdog {
    std::thread worker;
    HANDLE stopEvent=NULL;
    HWND ownerWindow=NULL;
    std::atomic_bool stopping{false}, paused{false};
    std::atomic<DWORD> epoch{0}, posted{0}, acknowledged{0};
    std::atomic<ULONGLONG> lastReply{0};
    std::atomic<DWORD> posts{0}, replies{0}, suspicions{0}, confirmations{0}, recoveries{0}, writeError{0};
    void Run(HWND window,DWORD uiThread,const CString& session,const CString& directory,const CString& environment,WatchdogOptions options);
public:
    ~UiWatchdog() { Stop(); }
    // Call only after the UI initialization/readiness boundary (OnPostCreate).
    bool Start(HWND window);
    void Stop();
    void Pause() noexcept { paused.store(true); epoch.fetch_add(1); }
    void Resume() noexcept { epoch.fetch_add(1); paused.store(false); }
    void Acknowledge(DWORD sequence) noexcept;
    bool Running() const { return worker.joinable(); }
    DWORD Posts() const { return posts.load(); }
    DWORD Replies() const { return replies.load(); }
    DWORD Suspicions() const { return suspicions.load(); }
    DWORD Confirmations() const { return confirmations.load(); }
    DWORD Recoveries() const { return recoveries.load(); }
    DWORD LastError() const { return writeError.load(); }
};
}
