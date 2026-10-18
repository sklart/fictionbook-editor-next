// Runs only inside the existing guarded, isolated diagnostic-catalog regression.
namespace {
    bool TestHangCleanup(const CString& directory)
    {
        const CString root=directory+L"Hangs\\"; ::SHCreateDirectoryEx(NULL,root,NULL);
        auto id=[](unsigned int n) { CString value; value.Format(L"AAAA%04X-0000-0000-0000-000000000001",n); return value; };
        auto path=[&](unsigned int n) { return root+L"fbe-hang-"+id(n)+L"-1.txt"; };
        auto content=[](const CString& session) { return L"hang-format=1\r\nsession_id="+session+L"\r\nhang-state=confirmed\r\nhang-duration-ms=30001\r\nhang-confirmed=1\r\n"; };
        auto write=[](const CString& name,const CString& text,bool old=true) {
            const auto bytes=Utf8(text); HANDLE file=::CreateFile(name,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
            if(file==INVALID_HANDLE_VALUE) return false;
            DWORD count=0; bool ok=::WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&count,NULL)!=FALSE && count==bytes.size();
            if(old) { SYSTEMTIME time={}; time.wYear=2000; time.wMonth=1; time.wDay=1; FILETIME ft={}; ::SystemTimeToFileTime(&time,&ft); ok=ok && ::SetFileTime(file,NULL,NULL,&ft)!=FALSE; }
            ::CloseHandle(file); return ok;
        };
        auto exists=[](const CString& name) { return ::GetFileAttributes(name)!=INVALID_FILE_ATTRIBUTES; };
        // 1 good, 2 changed after preview, 3 recent, 4 locked, 5 hard-linked, 6 damaged, 7 active session.
        for(unsigned int n=1;n<=7;++n) if(!write(path(n),content(id(n)),n!=3)) return false;
        const CString current=root+L"fbe-hang-"+StartupTrace::SessionId()+L"-1.txt";
        const CString foreign=root+L"notes.txt", alias=root+L"alias.txt";
        CString active; active.Format(L"PID=%lu\r\n",::GetCurrentProcessId());
        if(!write(current,content(StartupTrace::SessionId())) || !write(foreign,L"FOREIGN_PRIVATE_CONTENT") || !write(path(6),content(id(6))+L"PRIVATE_BOOK=secret\r\n") ||
            !write(directory+L"fbe-session-"+id(7)+L".txt",active) || !::CreateHardLink(alias,path(5),NULL)) return false;
        struct Lock { HANDLE file; ~Lock() { if(file!=INVALID_HANDLE_VALUE) ::CloseHandle(file); } } locked{::CreateFile(path(4),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL)};
        if(locked.file==INVALID_HANDLE_VALUE) return false;
        const auto plan=StartupTrace::PrepareDiagnosticHangCleanup(0); // API cannot lower the 30-day floor.
        if(plan.files!=2 || plan.items.size()!=2 || !plan.bytes || !exists(path(1))) return false;
        if(!write(path(2),content(id(2))+L"hang-duration-ms=40000\r\n") || !write(path(8),content(id(8)))) return false;
        {
            std::atomic_bool cancelled{true}; StartupTrace::DiagnosticTaskScope scope(cancelled);
            if(!StartupTrace::PrepareDiagnosticHangCleanup().items.empty() || StartupTrace::ExecuteDiagnosticHangCleanup(plan).filesDeleted) return false;
        }
        const auto cleaned=StartupTrace::ExecuteDiagnosticHangCleanup(plan);
        if(cleaned.filesDeleted!=1 || cleaned.filesFailed!=1 || cleaned.bytesDeleted!=plan.items[0].bytes || exists(path(1))) return false;
        for(unsigned int n=2;n<=8;++n) if(!exists(path(n))) return false;
        if(!exists(current) || !exists(alias) || !exists(foreign)) return false;
        auto forged=plan; forged.items[0].path=foreign;
        if(StartupTrace::ExecuteDiagnosticHangCleanup(forged).filesDeleted || !exists(foreign)) return false;
        const auto fresh=StartupTrace::PrepareDiagnosticHangCleanup();
        // Lock a previously approved file: execution must not delete it or widen the plan.
        Lock lateLock{::CreateFile(path(8),GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL)};
        if(lateLock.file==INVALID_HANDLE_VALUE) return false;
        const auto second=StartupTrace::ExecuteDiagnosticHangCleanup(fresh);
        ::DeleteFile(directory+L"fbe-session-"+id(7)+L".txt");
        return second.filesDeleted==1 && second.filesFailed==1 && exists(path(8)) && !exists(path(2)) && ReadText(foreign)==L"FOREIGN_PRIVATE_CONTENT";
    }
}
