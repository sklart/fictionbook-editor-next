// Included in DiagnosticPackage.cpp's private namespace.
    std::vector<CString> DiagnosticDirectories()
    {
        std::vector<CString> directories={CString(DeploymentContext::DiagnosticsDirectory().c_str())};
        if(DeploymentContext::CurrentMode()!=DeploymentContext::Mode::Portable) {
            wchar_t temp[MAX_PATH]={}; if(::GetTempPath(_countof(temp),temp)) directories.push_back(CString(temp)+L"FBE Next Diagnostics\\");
        }
        for(auto& path:directories) { path.TrimRight(L"\\/"); path+=L"\\"; }
        if(directories.size()==2 && directories[0].CompareNoCase(directories[1])==0) directories.pop_back();
        return directories;
    }
    bool CrashFileName(const CString& name,DWORD& pid)
    {
        pid=0;
        if(name.GetLength()<42 || name.Left(14)!=L"FBENext-crash-" || name.Right(4)!=L".txt" || name[22]!=L'-' || name[29]!=L'-' || name.Mid(33,4)!=L"-pid") return false;
        for(int i=14;i<33;++i) if(i!=22 && i!=29 && (name[i]<L'0' || name[i]>L'9')) return false;
        for(int i=37;i<name.GetLength()-4;++i) {
            if(name[i]<L'0' || name[i]>L'9' || pid>(MAXDWORD-static_cast<DWORD>(name[i]-L'0'))/10) return false;
            pid=pid*10+static_cast<DWORD>(name[i]-L'0');
        }
        return pid!=0;
    }
    ULONGLONG FileTicks(const FILETIME& time) { return (static_cast<ULONGLONG>(time.dwHighDateTime)<<32)|time.dwLowDateTime; }
    CString CrashFingerprint(const BY_HANDLE_FILE_INFORMATION& info,HANDLE file=INVALID_HANDLE_VALUE)
    {
        CString value; value.Format(L"%08lX-%08lX%08lX-%08lX%08lX-%08lX%08lX",info.dwVolumeSerialNumber,info.nFileIndexHigh,info.nFileIndexLow,info.ftLastWriteTime.dwHighDateTime,info.ftLastWriteTime.dwLowDateTime,info.nFileSizeHigh,info.nFileSizeLow);
        FILE_BASIC_INFO basic={};
        if(file!=INVALID_HANDLE_VALUE && ::GetFileInformationByHandleEx(file,FileBasicInfo,&basic,sizeof(basic)) && basic.ChangeTime.QuadPart>0) {
            CString change; change.Format(L"-%016llX",static_cast<ULONGLONG>(basic.ChangeTime.QuadPart)); value+=change;
        }
        return value;
    }
    CString ReadCrashHeader(HANDLE file)
    {
        std::vector<BYTE> bytes(8192); DWORD count=0;
        if(!::ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&count,NULL)) return CString();
        bytes.resize(count);
        // Discard an incomplete last record, including any partial UTF-8 character.
        if(count==8192) {
            if(bytes[0]==0xff && bytes[1]==0xfe) {
                while(bytes.size()>2 && !(bytes[bytes.size()-2]==10 && bytes.back()==0)) { bytes.pop_back(); bytes.pop_back(); }
            } else { while(!bytes.empty() && bytes.back()!=10) bytes.pop_back(); }
        }
        return Decode(bytes);
    }
    struct CrashIndex {
        std::vector<StartupTrace::DiagnosticCrashReport> reports;
        std::map<std::wstring,std::vector<size_t>> bySession;
        std::map<DWORD,std::vector<size_t>> legacyByPid;
    };
    CrashIndex BuildCrashIndex(const CString& directory,StartupTrace::DiagnosticCatalogMetrics* metrics)
    {
        CrashIndex index; const CString root=directory+L"Crashes\\";
        if(metrics) ++metrics->crashDirectories;
        WIN32_FILE_ATTRIBUTE_DATA rootInfo={};
        if(!::GetFileAttributesEx(root,GetFileExInfoStandard,&rootInfo) || (rootInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return index;
        WIN32_FIND_DATA find={}; HANDLE search=::FindFirstFile(root+L"FBENext-crash-*.txt",&find);
        if(search==INVALID_HANDLE_VALUE) return index;
        do {
            if(TaskCancelled()) break;
            DWORD pid=0; if((find.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || !CrashFileName(find.cFileName,pid)) continue;
            StartupTrace::DiagnosticCrashReport report; report.path=root+find.cFileName; report.processId=pid;
            HANDLE file=::CreateFile(report.path,GENERIC_READ|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
            if(file==INVALID_HANDLE_VALUE) continue; // Cannot safely fingerprint an inaccessible report.
            BY_HANDLE_FILE_INFORMATION before={},after={};
            bool valid=::GetFileInformationByHandle(file,&before)!=FALSE && !(before.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
            const CString beforeFingerprint=valid ? CrashFingerprint(before,file) : CString();
            CString header; if(valid) { if(metrics) ++metrics->crashHeaders; header=ReadCrashHeader(file); }
            valid=valid && ::GetFileInformationByHandle(file,&after)!=FALSE;
            report.fingerprint=valid ? CrashFingerprint(after,file) : CString(); ::CloseHandle(file); if(!valid) continue; report.modifiedTicks=FileTicks(after.ftLastWriteTime);
            report.bytes=(static_cast<ULONGLONG>(after.nFileSizeHigh)<<32)|after.nFileSizeLow;
            // A changing/invalid header remains reviewable, but cannot bind a package to a session.
            if(beforeFingerprint==report.fingerprint) report.sessionId=Field(header,L"session_id");
            const size_t number=index.reports.size(); index.reports.push_back(report);
            if(ValidSessionId(report.sessionId)) index.bySession[std::wstring(report.sessionId)].push_back(number);
            else if(report.sessionId.IsEmpty()) index.legacyByPid[pid].push_back(number);
        } while(::FindNextFile(search,&find)); ::FindClose(search);
        for(auto& item:index.legacyByPid) std::sort(item.second.begin(),item.second.end(),[&](size_t a,size_t b){return index.reports[a].modifiedTicks<index.reports[b].modifiedTicks;});
        return index;
    }
    CString FindCrashTextForSession(const StartupTrace::DiagnosticSession& session,const CrashIndex& index,bool* ambiguous)
    {
        if(ambiguous) *ambiguous=false;
        if(!session.legacy) {
            const auto found=index.bySession.find(std::wstring(session.id));
            if(found==index.bySession.end()) return CString();
            if(ambiguous) *ambiguous=found->second.size()>1;
            return found->second.size()==1 ? index.reports[found->second[0]].path : CString();
        }
        const auto found=index.legacyByPid.find(session.processId); if(found==index.legacyByPid.end()) return CString();
        WIN32_FILE_ATTRIBUTE_DATA trace={}; if(!::GetFileAttributesEx(session.logPath,GetFileExInfoStandard,&trace)) return CString();
        const ULONGLONG ticks=FileTicks(trace.ftLastWriteTime), span=30ULL*60*10000000;
        const ULONGLONG first=ticks>span ? ticks-span : 0, last=ticks>ULLONG_MAX-span ? ULLONG_MAX : ticks+span;
        auto match=std::lower_bound(found->second.begin(),found->second.end(),first,[&](size_t i,ULONGLONG t){return index.reports[i].modifiedTicks<t;});
        if(match==found->second.end() || index.reports[*match].modifiedTicks>last) return CString();
        const CString path=index.reports[*match].path;
        if(++match!=found->second.end() && index.reports[*match].modifiedTicks<=last) { if(ambiguous) *ambiguous=true; return CString(); }
        return path;
    }
