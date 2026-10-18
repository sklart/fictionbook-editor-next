// Included by DiagnosticPackage.cpp. No file content is exported by review/cleanup.
namespace {
    bool SafeCrashRoot(const CString& directory)
    {
        WIN32_FILE_ATTRIBUTE_DATA info={};
        return ::GetFileAttributesEx(directory,GetFileExInfoStandard,&info)!=FALSE && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
    }
    bool AllowedCrashPath(const CString& path)
    {
        DWORD pid=0; const CString name=Leaf(path); if(!CrashFileName(name,pid)) return false;
        for(const auto& directory:DiagnosticDirectories())
            if(path.CompareNoCase(directory+L"Crashes\\"+name)==0 && SafeCrashRoot(directory) && SafeCrashRoot(directory+L"Crashes")) return true;
        return false;
    }
    bool ReviewedFingerprint(const CString& path,const CString& fingerprint)
    {
        return StoredReviewMatches(path,fingerprint);
    }
    bool CrashProcessActive(DWORD pid)
    {
        if(pid==::GetCurrentProcessId()) return true;
        HANDLE process=::OpenProcess(SYNCHRONIZE,FALSE,pid);
        if(!process) return ::GetLastError()!=ERROR_INVALID_PARAMETER; // Unknown/denied is not proof of exit.
        const DWORD state=::WaitForSingleObject(process,0); ::CloseHandle(process); return state!=WAIT_OBJECT_0;
    }
    struct CrashHandle {
        HANDLE file=INVALID_HANDLE_VALUE;
        BY_HANDLE_FILE_INFORMATION info={};
        CString fingerprint;
        ~CrashHandle() { if(file!=INVALID_HANDLE_VALUE) ::CloseHandle(file); }
        bool Open(const CString& path) {
            file=::CreateFile(path,GENERIC_READ|FILE_READ_ATTRIBUTES|DELETE,0,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
            const bool valid=file!=INVALID_HANDLE_VALUE && ::GetFileInformationByHandle(file,&info)!=FALSE && !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) && info.nNumberOfLinks==1;
            if(valid) fingerprint=CrashFingerprint(info,file); return valid;
        }
        ULONGLONG Bytes() const { return (static_cast<ULONGLONG>(info.nFileSizeHigh)<<32)|info.nFileSizeLow; }
    };
    bool OldCrashFile(const BY_HANDLE_FILE_INFORMATION& info,ULONGLONG cutoff) { return FileTicks(info.ftLastWriteTime)<cutoff; }
    bool LockCleanupPair(const StartupTrace::DiagnosticCrashReport& report,ULONGLONG cutoff,CrashHandle& text,CrashHandle& dump,bool& haveDump)
    {
        if(!AllowedCrashPath(report.path) || !ReviewedFingerprint(report.path,report.fingerprint) || CrashProcessActive(report.processId)) return false;
        if(!text.Open(report.path) || text.fingerprint!=report.fingerprint || !OldCrashFile(text.info,cutoff)) return false;
        const CString header=ReadCrashHeader(text.file), id=Field(header,L"session_id");
        if(id==StartupTrace::SessionId()) return false;
        const CString dumpPath=report.path.Left(report.path.GetLength()-4)+L".dmp";
        const DWORD attributes=::GetFileAttributes(dumpPath);
        haveDump=attributes!=INVALID_FILE_ATTRIBUTES;
        if(!haveDump && ::GetLastError()!=ERROR_FILE_NOT_FOUND) return false;
        return !haveDump || (dump.Open(dumpPath) && OldCrashFile(dump.info,cutoff));
    }
}
std::vector<StartupTrace::DiagnosticCrashReport> StartupTrace::EnumerateDiagnosticCrashReports(DiagnosticCatalogMetrics* metrics)
{
    if(metrics) *metrics=DiagnosticCatalogMetrics();
    std::vector<DiagnosticCrashReport> result;
    for(const auto& directory:DiagnosticDirectories()) {
        if(TaskCancelled()) return {};
        auto index=BuildCrashIndex(directory,metrics);
        for(auto& report:index.reports) {
            report.reviewed=ReviewedFingerprint(report.path,report.fingerprint);
            result.push_back(report);
        }
    }
    if(TaskCancelled()) return {};
    std::sort(result.begin(),result.end(),[](const DiagnosticCrashReport& a,const DiagnosticCrashReport& b){return a.modifiedTicks>b.modifiedTicks;});
    return result;
}
bool StartupTrace::MarkDiagnosticCrashReportReviewed(const DiagnosticCrashReport& report)
{
    DWORD pid=0; if(TaskCancelled() || !AllowedCrashPath(report.path) || !CrashFileName(Leaf(report.path),pid)) return false;
    DiagnosticSession session; session.crashPath=report.path; session.crashFingerprint=report.fingerprint;
    return MarkDiagnosticCrashReviewed(session);
}
StartupTrace::DiagnosticCrashCleanupPlan StartupTrace::PrepareDiagnosticCrashCleanup(unsigned int days)
{
    DiagnosticCrashCleanupPlan plan;
    FILETIME now={}; ::GetSystemTimeAsFileTime(&now);
    // At least 30 days: no caller may turn this into a new/current-report cleanup.
    const ULONGLONG age=static_cast<ULONGLONG>((std::max)(days,30U))*86400*10000000;
    const ULONGLONG ticks=FileTicks(now); plan.cutoffTicks=ticks>age ? ticks-age : 0;
    for(const auto& report:EnumerateDiagnosticCrashReports()) {
        if(TaskCancelled()) return DiagnosticCrashCleanupPlan();
        if(!report.reviewed || report.sessionId==SessionId()) continue;
        CrashHandle text,dump; bool haveDump=false;
        if(!LockCleanupPair(report,plan.cutoffTicks,text,dump,haveDump)) continue;
        DiagnosticCrashCleanupItem item; item.report=report;
        if(haveDump) { item.dumpFingerprint=dump.fingerprint; item.dumpBytes=dump.Bytes(); }
        plan.items.push_back(item); plan.files+=haveDump ? 2U : 1U; plan.bytes+=text.Bytes()+item.dumpBytes;
    }
    return plan;
}
StartupTrace::DiagnosticLogCleanupResult StartupTrace::ExecuteDiagnosticCrashCleanup(const DiagnosticCrashCleanupPlan& plan)
{
    DiagnosticLogCleanupResult result;
    FILETIME now={}; ::GetSystemTimeAsFileTime(&now); const ULONGLONG current=FileTicks(now), minimumAge=30ULL*86400*10000000;
    const ULONGLONG cutoff=(std::min)(plan.cutoffTicks,current>minimumAge ? current-minimumAge : 0);
    for(const auto& item:plan.items) {
        if(TaskCancelled()) break;
        DWORD pid=0;
        if(!CrashFileName(Leaf(item.report.path),pid) || pid!=item.report.processId) continue;
        CrashHandle text,dump; bool haveDump=false;
        if(!LockCleanupPair(item.report,cutoff,text,dump,haveDump) ||
            haveDump!=!item.dumpFingerprint.IsEmpty() || (haveDump && dump.fingerprint!=item.dumpFingerprint)) {
            ++result.sessionsFailed; ++result.filesFailed; continue;
        }
        ++result.sessionsFound; bool failed=false, deleted=false;
        // Delete by the verified exclusive handle (Win7 FileDispositionInfo), never by a reopened path.
        const auto remove=[&](CrashHandle& handle) {
            FILE_DISPOSITION_INFO disposition={TRUE};
            if(::SetFileInformationByHandle(handle.file,FileDispositionInfo,&disposition,sizeof(disposition))) {
                ++result.filesDeleted; result.bytesDeleted+=handle.Bytes(); deleted=true; return true;
            }
            ++result.filesFailed; result.lastError=::GetLastError(); failed=true; return false;
        };
        if(!haveDump || remove(dump)) remove(text);
        if(failed && deleted) ++result.sessionsPartiallyDeleted;
        else if(failed) ++result.sessionsFailed;
        else ++result.sessionsFullyDeleted;
    }
    return result;
}
