    // Included in CMainFrame's guarded runtime dispatcher; production thresholds stay 15/30 s.
    if(IsFbeTestScenario(L"watchdog-runtime"))
    {
        auto pump=[](DWORD milliseconds) {
            const ULONGLONG deadline=::GetTickCount64()+milliseconds;
            while(::GetTickCount64()<deadline) {
                MSG message={}; while(::PeekMessage(&message,NULL,0,0,PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessage(&message); }
                ::MsgWaitForMultipleObjectsEx(0,NULL,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            }
        };
        const CString session=StartupTrace::SessionId(), directory=StartupTrace::CurrentLogDirectory()+L"\\Hangs\\";
        auto reports=[directory]() {
            std::vector<CString> files; WIN32_FIND_DATA data={}; HANDLE search=::FindFirstFile(directory+L"fbe-hang-*.txt",&data);
            if(search!=INVALID_HANDLE_VALUE) { do { if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) files.push_back(directory+data.cFileName); } while(::FindNextFile(search,&data)); ::FindClose(search); }
            return files;
        };
        auto read=[](const CString& path) {
            HANDLE file=::CreateFile(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
            if(file==INVALID_HANDLE_VALUE) return CString();
            const DWORD size=::GetFileSize(file,NULL); CString text;
            if(size<256*1024) { std::vector<char> bytes(static_cast<size_t>(size)+1,0); DWORD count=0;
                if(::ReadFile(file,bytes.data(),size,&count,NULL) && count==size) text=CString(CA2W(bytes.data(),CP_UTF8)); }
            ::CloseHandle(file); return text;
        };
#include "RuntimeTestWatchdogLifecycle.inl"
        FbeDiagnostics::WatchdogState deterministic; FbeDiagnostics::WatchdogOptions production;
        deterministic.Request(100); // Probes before explicit UI readiness remain suppressed.
        bool transitions=production.suspectMs==15000 && production.confirmMs==30000 && deterministic.Advance(60000,production)==0;
        deterministic.Ready(); deterministic.Request(100);
        transitions=transitions && deterministic.Advance(15100,production)==0 && deterministic.Advance(15101,production)==1 && deterministic.Advance(30101,production)==2 && deterministic.Advance(90000,production)==0;
        transitions=transitions && deterministic.Reply(); deterministic.Request(90001); deterministic.Reset(); transitions=transitions && deterministic.Advance(999999,production)==0;
        transitions=transitions && FbeDiagnostics::WatchdogState::ClockDiscontinuity(1100,1000,30100,1000) &&
            FbeDiagnostics::WatchdogState::ClockDiscontinuity(31000,1000,31000,1000) && !FbeDiagnostics::WatchdogState::ClockDiscontinuity(2000,1000,2000,1000);
        ShowView(BODY);
        MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document()); MSHTML::IHTMLElementPtr body(document ? document->body : NULL);
        if(!body) { output.Close(); ::PostQuitMessage(1); return 0; }
        body->insertAdjacentHTML(L"beforeEnd",L"<p>PRIVATE_BOOK unsaved watchdog fixture.</p>");
        const CString before(static_cast<LPCWSTR>(body->innerHTML)); const bool dirtyBefore=m_doc->DocChanged(); FB::Doc* const owned=m_doc;
        OLECMD beforeCommands[]={{IDM_UNDO,0},{IDM_REDO,0}}; m_doc->m_body.QueryStatus(beforeCommands,_countof(beforeCommands));
        { StartupTrace::ScopedOperation operation(StartupTrace::Operation::Save); operation.Succeeded(); }
        m_ui_watchdog.Stop();
        const bool earlyStarted=m_ui_watchdog.Start(m_hWnd);
        ::Sleep(900); // UI is ready but cannot answer the very first posted heartbeat.
        bool early=earlyStarted && m_ui_watchdog.Replies()==0 && m_ui_watchdog.Posts()==1 && m_ui_watchdog.Confirmations()==1 && reports().size()==1;
        if(early) { const CString text=read(reports()[0]); early=StartupTrace::ValidateHangReport(text,session) && text.Find(L"hang-state=confirmed")>=0 && text.Find(L"hang-state=recovered")<0 && text.Find(L"hang-last-reply-ms=0")>=0; }
        pump(250);
        early=early && m_ui_watchdog.Recoveries()==1 && reports().size()==1 && read(reports()[0]).Find(L"hang-state=recovered")>=0;
        m_ui_watchdog.Stop(); m_ui_watchdog.Start(m_hWnd);
        pump(500);
        const bool normal=m_ui_watchdog.Running() && m_ui_watchdog.Replies()>=3 && m_ui_watchdog.Suspicions()==0 && reports().size()==1 && !StartupTrace::Enabled();
        { StartupTrace::ScopedOperation operation(StartupTrace::Operation::Save); operation.Succeeded(); }
        StartupTrace::HResult(L"watchdog-test",L"WD901",E_FAIL,L"PRIVATE_USER C:\\secret\\book.fb2 <p>PRIVATE_BOOK</p>");
        StartupTrace::Event(L"document",L"WD902",L"PRIVATE_USER C:\\secret\\book.fb2");
        bool independent=true, noDuplicates=true;
        for(unsigned int incident=1;incident<=2;++incident) {
            std::atomic_bool blocking{true}, observed{false}; HANDLE finished=::CreateEvent(NULL,TRUE,FALSE,NULL);
            const DWORD postsBefore=m_ui_watchdog.Posts();
            std::thread observer([&,incident,finished]() {
                const ULONGLONG limit=::GetTickCount64()+2000;
                while(::GetTickCount64()<limit && ::WaitForSingleObject(finished,20)==WAIT_TIMEOUT) {
                    const auto files=reports();
                    if(files.size()==incident+1) for(const auto& file:files) {
                        const CString text=read(file);
                        if(blocking.load() && text.Find(L"hang-state=confirmed")>=0 && text.Find(L"hang-state=recovered")<0 && text.Find(L"stats-in-progress=script")>=0 &&
                            text.Find(L"active-editor-mode=body")>=0 && text.Find(L"hang-last-hresult=0x80004005")>=0 && StartupTrace::ValidateHangReport(text,session)) observed.store(true);
                    }
                }
            });
            {
                StartupTrace::ScopedOperation operation(StartupTrace::Operation::Script);
                // This blocks the real UI while retaining the existing trace critical section.
                StartupTrace::TestHoldTraceLockForWatchdog(900); operation.Succeeded();
            }
            blocking.store(false); ::SetEvent(finished); observer.join(); ::CloseHandle(finished);
            independent=independent && observed.load();
            noDuplicates=noDuplicates && reports().size()==incident+1 && m_ui_watchdog.Confirmations()==incident && m_ui_watchdog.Posts()-postsBefore<=1;
            if(incident==2) {
                BOOL handled=TRUE; OnWatchdogPower(WM_POWERBROADCAST,PBT_APMSUSPEND,0,handled); ::Sleep(200);
                OnWatchdogPower(WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC,0,handled);
            }
            pump(300);
        }
        const bool recovered=m_ui_watchdog.Recoveries()==2;
        bool completeReports=reports().size()==3, recoveredAcrossPause=false;
        for(const auto& file:reports()) {
            const CString text=read(file);
            recoveredAcrossPause=recoveredAcrossPause || text.Find(L"hang-monitoring-interrupted=1")>=0;
            completeReports=completeReports && StartupTrace::ValidateHangReport(text,session) && text.Find(L"hang-state=recovered")>=0 && text.Find(L"hang-state=confirmed")>=0 && (text.Find(L"stats-last-completed=save")>=0 || text.Find(L"stats-last-completed=script")>=0) &&
                text.Find(L"PRIVATE_BOOK")<0 && text.Find(L"PRIVATE_USER")<0 && text.Find(L"C:\\secret")<0;
        }
        BOOL powerHandled=TRUE; OnWatchdogPower(WM_POWERBROADCAST,PBT_APMSUSPEND,0,powerHandled); ::Sleep(650);
        OnWatchdogPower(WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC,0,powerHandled); pump(400);
        const bool suspend=m_ui_watchdog.Confirmations()==2 && m_ui_watchdog.Suspicions()==2;
        const DWORD error=m_ui_watchdog.LastError(); const ULONGLONG stopping=::GetTickCount64(); m_ui_watchdog.Stop(); const ULONGLONG stopMs=::GetTickCount64()-stopping;
        ::Sleep(400); const bool stopped=!m_ui_watchdog.Running() && reports().size()==3 && stopMs<1000;
        OLECMD afterCommands[]={{IDM_UNDO,0},{IDM_REDO,0}}; m_doc->m_body.QueryStatus(afterCommands,_countof(afterCommands));
        const bool documentPreserved=m_doc==owned && dirtyBefore && m_doc->DocChanged() && before==CString(static_cast<LPCWSTR>(body->innerHTML)) && beforeCommands[0].cmdf==afterCommands[0].cmdf && beforeCommands[1].cmdf==afterCommands[1].cmdf;
        CString safe=reports().empty() ? CString() : read(reports()[0]);
        CString unsafe=safe+L"PRIVATE_BOOK=C:\\secret\\book.fb2\r\n";
        const bool privacy=!StartupTrace::ValidateHangReport(unsafe,session) && !StartupTrace::ValidateHangReport(safe,L"11111111-1111-1111-1111-111111111111");
        const CString corrupted=directory+L"fbe-hang-"+session+L"-4294967295.txt";
        { CAtlFile bad; if(SUCCEEDED(bad.Create(corrupted,GENERIC_WRITE,FILE_SHARE_READ,CREATE_NEW))) { CStringA bytes(CW2A(unsafe,CP_UTF8)); bad.Write(bytes,static_cast<DWORD>(bytes.GetLength())); } }
        StartupTrace::DiagnosticSession selected; bool catalog=false;
        for(const auto& item:m_diagnostic_commands.Sessions()) if(item.id==session) { selected=item; catalog=item.hasHangReports; }
        std::shared_ptr<const StartupTrace::DiagnosticPackageSnapshot> checked; CString packageError;
        bool zip=m_diagnostic_commands.PreparePackage(selected,{},checked,packageError); unsigned int included=0, rejected=0;
        if(checked) for(const auto& component:checked->Components()) {
            if(component.name.Left(5)==L"hang/") { if(component.status==L"included") ++included; if(component.status==L"privacy-rejected" && component.bytes.empty()) ++rejected; }
            if(component.status==L"included") { CString name(component.name); name.Replace(L"/",L"_"); CAtlFile preview;
                if(FAILED(preview.Create(AU::_ARGS.source_memory_benchmark_path+L"."+name+L".preview",GENERIC_WRITE,FILE_SHARE_READ,CREATE_NEW))) zip=false;
                else if(!component.bytes.empty() && FAILED(preview.Write(component.bytes.data(),static_cast<DWORD>(component.bytes.size())))) zip=false;
            }
        }
        zip=zip && checked && included==3 && rejected==1 && StartupTrace::SaveDiagnosticPackage(*checked,AU::_ARGS.source_memory_benchmark_path+L".zip",packageError);
        const bool restarted=m_ui_watchdog.Start(m_hWnd); pump(150);
        const bool shutdownRunning=restarted && m_ui_watchdog.Running() && m_ui_watchdog.Replies()>0;
        const bool passed=lifecycle && directoryFailure && restart && workerException && concurrent && early && recoveredAcrossPause && shutdownRunning && transitions && normal && independent && noDuplicates && recovered && completeReports && suspend && stopped && documentPreserved && privacy && catalog && zip && error==0;
        CStringA result; result.Format("directory_win32=%lu\nblocked_win32=%lu\nlifecycle=%d\ndirectory_failure=%d\nrestart_after_failure=%d\nworker_exception=%d\nconcurrent_lifecycle=%d\nfirst_heartbeat=%d\ntransitions=%d\nnormal=%d\nreport_while_ui_and_trace_locked=%d\nno_duplicates=%d\nrecovered=%d\nreports_complete=%d\npause_recovery=%d\nsuspend_resume=%d\nstop=%d\nstop_ms=%llu\nshutdown_running=%d\ndocument_preserved=%d\nprivacy=%d\ncatalog=%d\nzip=%d\nwrite_error=%lu\npassed=%d\n",directoryCode,blockedCode,lifecycle,directoryFailure,restart,workerException,concurrent,early,transitions,normal,independent,noDuplicates,recovered,completeReports,recoveredAcrossPause,suspend,stopped,stopMs,shutdownRunning,documentPreserved,privacy,catalog,zip,error,passed);
        DWORD written=0; output.Write(result,static_cast<DWORD>(result.GetLength()),&written); output.Flush(); output.Close();
        PostMessage(WM_CLOSE); if(!passed) ::PostQuitMessage(1); return 0;
    }
