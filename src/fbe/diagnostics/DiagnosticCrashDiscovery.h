#pragma once
#include "../StartupTrace.h"
#include <thread>

// One launch scan; the UI only starts/polls it. No persistent watcher or HWND in worker.
class DiagnosticCrashDiscovery {
    std::atomic_bool cancelled{false}, done{false};
    std::thread worker;
    bool unread=false;
    StartupTrace::DiagnosticCatalogMetrics metrics;
    DWORD threadId=0;
    ULONGLONG launchMs=0, scanMs=0;
public:
    ~DiagnosticCrashDiscovery() { Stop(); }
    bool Start(bool retainLogs=false) {
        if(worker.joinable()) return false;
        cancelled.store(false); done.store(false); unread=false;
        const ULONGLONG start=::GetTickCount64();
        try { worker=std::thread([this,retainLogs]() {
            StartupTrace::DiagnosticTaskScope scope(cancelled);
            threadId=::GetCurrentThreadId(); const ULONGLONG begin=::GetTickCount64();
            try { for(const auto& report:StartupTrace::EnumerateDiagnosticCrashReports(&metrics)) if(!report.reviewed) { unread=true; break; } } catch(...) { }
            scanMs=::GetTickCount64()-begin;
            // Preserve the existing ten-session log retention, outside startup/UI I/O.
            // It has its own indexed catalog pass; crash TXT/DMP are never auto-deleted.
            if(retainLogs && !cancelled.load()) { try { StartupTrace::CleanupDiagnosticSessions(10); } catch(...) { } }
            done.store(true,std::memory_order_release);
        }); } catch(...) { return false; }
        launchMs=::GetTickCount64()-start; return true;
    }
    bool Poll(bool& result) {
        if(!done.load(std::memory_order_acquire)) return false;
        if(worker.joinable()) worker.join(); result=unread; return true;
    }
    void Stop() { cancelled.store(true); if(worker.joinable()) worker.join(); }
    const StartupTrace::DiagnosticCatalogMetrics& Metrics() const { return metrics; } // Only after Poll.
    DWORD ThreadId() const { return threadId; }
    ULONGLONG LaunchMilliseconds() const { return launchMs; }
    ULONGLONG ScanMilliseconds() const { return scanMs; }
};
