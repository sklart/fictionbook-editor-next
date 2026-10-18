// Manual-only cleanup. The immutable preview records identities, never report contents.
namespace {
    struct HangCleanupHandle : CrashHandle {
        HANDLE parent=INVALID_HANDLE_VALUE, root=INVALID_HANDLE_VALUE;
        ~HangCleanupHandle() { if(root!=INVALID_HANDLE_VALUE) ::CloseHandle(root); if(parent!=INVALID_HANDLE_VALUE) ::CloseHandle(parent); }
        bool LockDirectories(const CString& path) {
            const CString folder=path.Left(path.ReverseFind(L'\\'));
            const CString diagnostics=folder.Left(folder.ReverseFind(L'\\'));
            const auto open=[](const CString& name,HANDLE& handle) {
                handle=::CreateFile(name,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
                BY_HANDLE_FILE_INFORMATION info={};
                return handle!=INVALID_HANDLE_VALUE && ::GetFileInformationByHandle(handle,&info) && (info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && !(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT);
            };
            // Hold both directory identities across validation and deletion; no junction/rename swap.
            return open(diagnostics,parent) && open(folder,root);
        }
    };
    bool AllowedHangPath(const CString& path,const CString& session)
    {
        CString id; const CString name=Leaf(path);
        if(!HangFileName(name,id) || id!=session || id==StartupTrace::SessionId()) return false;
        for(const auto& directory:DiagnosticDirectories()) {
            if(path.CompareNoCase(directory+L"Hangs\\"+name)!=0 || !SafeCrashRoot(directory) || !SafeCrashRoot(directory+L"Hangs")) continue;
            // An active session is protected even between its individual hang episodes.
            const CString metadata=directory+L"fbe-session-"+id+L".txt";
            const DWORD attrs=::GetFileAttributes(metadata);
            if(attrs!=INVALID_FILE_ATTRIBUTES) {
                if(attrs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) return false;
                const CString pid=Field(ReadText(metadata),L"PID");
                if(!NumericText(pid,L"") || _wtoi64(pid)<=0 || _wtoi64(pid)>MAXDWORD || CrashProcessActive(static_cast<DWORD>(_wtoi64(pid)))) return false;
            } else if(::GetLastError()!=ERROR_FILE_NOT_FOUND) return false;
            return true;
        }
        return false;
    }
    bool LockHangCleanup(const StartupTrace::DiagnosticHangCleanupItem& item,ULONGLONG cutoff,HangCleanupHandle& file)
    {
        if(!file.LockDirectories(item.path) || !AllowedHangPath(item.path,item.sessionId) || !file.Open(item.path) || !OldCrashFile(file.info,cutoff) || file.Bytes()>256*1024 ||
            (!item.fingerprint.IsEmpty() && item.fingerprint!=file.fingerprint)) return false;
        std::vector<BYTE> bytes(static_cast<size_t>(file.Bytes())); DWORD count=0;
        if(!::ReadFile(file.file,bytes.data(),static_cast<DWORD>(bytes.size()),&count,NULL) || count!=bytes.size() || TaskCancelled()) return false;
        // Validate every record from the same exclusively held file that will be deleted.
        return !ContainsUnsafeContent(bytes) && StartupTrace::ValidateHangReport(Decode(bytes),item.sessionId);
    }
    ULONGLONG HangCleanupCutoff(unsigned int days)
    {
        FILETIME now={}; ::GetSystemTimeAsFileTime(&now);
        const ULONGLONG ticks=FileTicks(now), age=static_cast<ULONGLONG>((std::max)(days,30U))*86400*10000000;
        return ticks>age ? ticks-age : 0;
    }
}
StartupTrace::DiagnosticHangCleanupPlan StartupTrace::PrepareDiagnosticHangCleanup(unsigned int days)
{
    DiagnosticHangCleanupPlan plan; plan.cutoffTicks=HangCleanupCutoff(days);
    for(const auto& directory:DiagnosticDirectories()) for(const auto& report:HangFiles(directory)) {
        if(TaskCancelled()) return DiagnosticHangCleanupPlan();
        DiagnosticHangCleanupItem item; item.path=report.path; item.sessionId=report.session;
        HangCleanupHandle file; if(!LockHangCleanup(item,plan.cutoffTicks,file)) continue;
        item.fingerprint=file.fingerprint; item.bytes=file.Bytes();
        plan.items.push_back(item); ++plan.files; plan.bytes+=item.bytes;
    }
    return plan;
}
StartupTrace::DiagnosticLogCleanupResult StartupTrace::ExecuteDiagnosticHangCleanup(const DiagnosticHangCleanupPlan& plan)
{
    DiagnosticLogCleanupResult result; const ULONGLONG cutoff=(std::min)(plan.cutoffTicks,HangCleanupCutoff(30));
    for(const auto& item:plan.items) {
        if(TaskCancelled()) break;
        HangCleanupHandle file;
        if(item.fingerprint.IsEmpty() || !LockHangCleanup(item,cutoff,file) || item.bytes!=file.Bytes()) { ++result.filesFailed; ++result.sessionsFailed; continue; }
        if(TaskCancelled()) break;
        ++result.sessionsFound;
        FILE_DISPOSITION_INFO disposition={TRUE};
        if(::SetFileInformationByHandle(file.file,FileDispositionInfo,&disposition,sizeof(disposition))) {
            ++result.filesDeleted; ++result.sessionsFullyDeleted; result.bytesDeleted+=file.Bytes();
        } else { ++result.filesFailed; ++result.sessionsFailed; result.lastError=::GetLastError(); }
    }
    return result;
}
