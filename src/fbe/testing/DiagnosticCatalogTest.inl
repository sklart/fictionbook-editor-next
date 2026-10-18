// Guarded headless production-path regression; all fixtures stay in the isolated test settings.
int StartupTrace::RunDiagnosticCatalogTest()
{
    wchar_t output[32768]={};
    if(DeploymentContext::CurrentMode()!=DeploymentContext::Mode::Installed || DeploymentContext::TestSettingsDirectory().empty() ||
        !::GetEnvironmentVariable(L"FBE_NEXT_TEST_PACKAGE_DIRECTORY",output,_countof(output))) return 90;
    const CString directory(DeploymentContext::DiagnosticsDirectory().c_str()), root=directory+L"Crashes\\";
    ::SHCreateDirectoryEx(NULL,root,NULL);
    auto write=[](const CString& path,const CString& text,bool old=false) {
        const auto bytes=Utf8(text); HANDLE file=::CreateFile(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file==INVALID_HANDLE_VALUE) return false;
        DWORD written=0; bool ok=::WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,NULL)!=FALSE && written==bytes.size();
        if(old) { SYSTEMTIME time={}; time.wYear=2000; time.wMonth=1; time.wDay=1; FILETIME ft={}; ::SystemTimeToFileTime(&time,&ft); ok=ok && ::SetFileTime(file,NULL,NULL,&ft)!=FALSE; }
        ::CloseHandle(file); return ok;
    };
    auto path=[&](unsigned int n,DWORD pid=4294967290UL) { CString name; name.Format(L"FBENext-crash-20000101-000000-%03u-pid%lu.txt",n,pid); return root+name; };
    auto reportAt=[](const CString& file) { for(const auto& item:EnumerateDiagnosticCrashReports()) if(item.path==file) return item; return DiagnosticCrashReport(); };
    auto exists=[](const CString& file) { return ::GetFileAttributes(file)!=INVALID_FILE_ATTRIBUTES; };
    wchar_t verify[2]={};
    if(::GetEnvironmentVariable(L"FBE_NEXT_TEST_CATALOG_VERIFY",verify,2)==1 && verify[0]==L'1') {
        const auto unchanged=reportAt(path(1)), changed=reportAt(path(0));
        return unchanged.reviewed && !changed.reviewed && ReadText(path(0)).Find(L"PRIVATE_BOOK")>=0 ? 0 : 28;
    }
    for(unsigned int i=0;i<1000;++i) {
        CString id; id.Format(L"%08x-0000-0000-0000-000000000001",i);
        if(!write(directory+L"fbe-session-"+id+L".txt",L"session_id="+id+L"\r\nPID=4294967290\r\nstarted=2000-01-01T00:00:00\r\noutcome=normal\r\n")) return 1;
        if(!write(path(i),L"session_id="+id+L"\r\nException code: 0xC0000005\r\n")) return 2;
    }
    DiagnosticCatalogMetrics catalogMetrics;
    const ULONGLONG begin=::GetTickCount64(); const auto sessions=EnumerateDiagnosticSessions(&catalogMetrics); const ULONGLONG catalogMs=::GetTickCount64()-begin;
    if(sessions.size()!=1001 || catalogMetrics.crashHeaders!=1000 || catalogMetrics.metadataReads!=1001) return 3;
    for(const auto& session:sessions) if(session.id!=SessionId() && session.crashPath.IsEmpty()) return 4;
    auto discover=[](DiagnosticCrashDiscovery& scan,bool& unread) {
        if(!scan.Start()) return false;
        const ULONGLONG start=::GetTickCount64();
        while(!scan.Poll(unread)) { if(::GetTickCount64()-start>15000) return false; ::Sleep(1); }
        return scan.ThreadId()!=::GetCurrentThreadId() && scan.Metrics().metadataReads==0 && scan.LaunchMilliseconds()<500;
    };
    bool unread=false; DiagnosticCrashDiscovery scan;
    if(!discover(scan,unread) || !unread || scan.Metrics().crashHeaders!=1000) return 5;
    // Damage never bypasses package privacy. Acknowledgement only stores identity in a separate INI.
    const CString damaged=path(0); const CString unsafe=L"PRIVATE_BOOK C:\\secret\\name\r\nsession_id=broken\r\n";
    if(!write(damaged,unsafe)) return 6;
    auto damagedReport=reportAt(damaged);
    if(damagedReport.reviewed || !MarkDiagnosticCrashReportReviewed(damagedReport) || ReadText(damaged)!=unsafe || !reportAt(damaged).reviewed) return 7;
    // Compatibility with the pre-change-time reviewed.ini fingerprint.
    if(damagedReport.fingerprint.GetLength()==76) {
        if(!::WritePrivateProfileString(L"Reviewed",Leaf(damaged),damagedReport.fingerprint.Left(59),CrashReviewIni()) || !reportAt(damaged).reviewed) return 29;
    }
    if(!write(damaged,unsafe+L"changed")) return 8;
    if(MarkDiagnosticCrashReportReviewed(damagedReport) || reportAt(damaged).reviewed) return 9;
    // All reports acknowledged: next background discovery must be quiet; replacement is new again.
    for(const auto& report:EnumerateDiagnosticCrashReports()) if(!MarkDiagnosticCrashReportReviewed(report)) return 10;
    DiagnosticCrashDiscovery quiet; if(!discover(quiet,unread) || unread) return 11;
    if(!write(damaged,unsafe+L"replacement")) return 12;
    DiagnosticCrashDiscovery changed; if(!discover(changed,unread) || !unread) return 13;
    // Independent cleanup fixtures: all indexed bulk reports are recent and must survive.
    const CString good=path(0,4294967286UL), stale=path(1,4294967286UL), unreviewed=path(2,4294967286UL), locked=path(3,4294967286UL), current=path(4,4294967286UL), active=path(5,::GetCurrentProcessId()), hard=path(6,4294967286UL), lockedText=path(7,4294967286UL);
    auto dump=[](const CString& text) { return text.Left(text.GetLength()-4)+L".dmp"; };
    for(const auto& text:{good,stale,unreviewed,locked,current,active,hard,lockedText}) {
        const CString header=text==current ? L"session_id="+SessionId()+L"\r\n" : unsafe;
        if(!write(text,header,true) || !write(dump(text),L"PRIVATE_DUMP",true)) return 14;
        if(text!=unreviewed && !MarkDiagnosticCrashReportReviewed(reportAt(text))) return 15;
    }
    const CString foreign=root+L"notes.txt", orphan=root+L"FBENext-crash-20000101-000000-999-pid4294967287.dmp", alias=root+L"hard-link.txt";
    if(!write(foreign,unsafe,true) || !write(orphan,unsafe,true) || !::CreateHardLink(alias,hard,NULL)) return 16;
    HANDLE lock=::CreateFile(dump(locked),GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL), textLock=::CreateFile(lockedText,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(lock==INVALID_HANDLE_VALUE || textLock==INVALID_HANDLE_VALUE) return 17;
    const auto plan=PrepareDiagnosticCrashCleanup();
    if(plan.items.size()!=2 || plan.files!=4 || !plan.bytes) { ::CloseHandle(lock); ::CloseHandle(textLock); return 18; }
    if(!write(dump(stale),L"CHANGED_DUMP",true)) { ::CloseHandle(lock); ::CloseHandle(textLock); return 19; }
    // A new eligible file created after preview is outside the approved plan.
    const CString late=path(8,4294967286UL); if(!write(late,unsafe,true) || !MarkDiagnosticCrashReportReviewed(reportAt(late))) return 20;
    {
        std::atomic_bool cancelled{true}; DiagnosticTaskScope scope(cancelled);
        if(!PrepareDiagnosticCrashCleanup().items.empty() || ExecuteDiagnosticCrashCleanup(plan).filesDeleted!=0) return 21;
    }
    const auto cleaned=ExecuteDiagnosticCrashCleanup(plan); ::CloseHandle(lock); ::CloseHandle(textLock);
    if(cleaned.filesDeleted!=2 || cleaned.filesFailed!=1 || exists(good) || exists(dump(good)) || !exists(stale) || !exists(dump(stale))) return 22;
    for(const auto& file:{unreviewed,locked,current,active,hard,lockedText,late,foreign,orphan,alias,path(999)}) if(!exists(file)) return 23;
    // Forged paths and changed TXT identity cannot widen a previously approved plan.
    auto forged=plan; forged.items[0].report.path=foreign;
    if(ExecuteDiagnosticCrashCleanup(forged).filesDeleted!=0 || !exists(foreign)) return 24;
    const auto fresh=PrepareDiagnosticCrashCleanup();
    if(!write(late,unsafe+L"replacement",true)) return 25;
    const auto after=ExecuteDiagnosticCrashCleanup(fresh);
    if(!exists(late) || after.filesFailed==0) return 26;
    if(!TestHangCleanup(directory)) return 31;
    CString evidence; evidence.Format(L"PASS\r\nsessions=%u\r\ncrashes=1000\r\nmetadata_reads=%llu\r\ncrash_headers=%llu\r\ncatalog_ms=%llu\r\nlaunch_ms=%llu\r\nbackground_scan_ms=%llu\r\nstartup_metadata_reads=%llu\r\ndamaged_review=1\r\nbackground_notifications=1\r\ncleanup_guards=1\r\nhang_cleanup_guards=1\r\n",static_cast<unsigned int>(sessions.size()),catalogMetrics.metadataReads,catalogMetrics.crashHeaders,catalogMs,scan.LaunchMilliseconds(),scan.ScanMilliseconds(),scan.Metrics().metadataReads);
    ::SHCreateDirectoryEx(NULL,output,NULL);
    return write(CString(output)+L"\\catalog.txt",evidence) ? 0 : 27;
}
