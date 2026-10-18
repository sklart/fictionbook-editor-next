// Included inside DiagnosticDialog; reachable only through the guarded runtime scenario.
    CString testFailure;
    bool damagedReview=false;
    std::shared_ptr<const StartupTrace::DiagnosticPackageSnapshot> testSnapshot;
    bool testMode=false, testPassed=true;
    unsigned int testPhase=0, heartbeat=0;
    bool cancelledReady=false, closeDeferred=false;
    std::shared_ptr<std::atomic_bool> testReviewGate;
    std::shared_ptr<Job> testReviewJob;
    StartupTrace::DiagnosticSession testReviewSession;
    ATL::CHandle testReviewLock;
    bool automaticReview=false, reviewWorkerThread=false, staleRejected=false, reviewWriteFailed=false;
    unsigned int reviewHeartbeat=0;
    std::shared_ptr<Job> closingJob;
    bool CheckLayout()
    {
        const InterfaceTheme originalTheme=ThemeManager::GetSelectedTheme();
        RECT original={}; ::GetWindowRect(window,&original);
        bool valid=true;
        std::vector<BYTE> bytes;
        const BYTE pattern[]={0x41,0xd0,0x91,0xe2,0x82,0xac,0xf0,0x9f,0xa7,0xaa};
        for(unsigned int i=0;i<20000;++i) bytes.insert(bytes.end(),pattern,pattern+_countof(pattern));
        CString combined;
        for(size_t page=0;page<(bytes.size()+65535)/65536;++page) { Job decoded(Work::Preview); DecodePreview(bytes,page,decoded); combined+=decoded.preview; }
        const int size=::WideCharToMultiByte(CP_UTF8,0,combined,combined.GetLength(),NULL,0,NULL,NULL);
        std::vector<BYTE> roundtrip(static_cast<size_t>(size));
        ::WideCharToMultiByte(CP_UTF8,0,combined,combined.GetLength(),reinterpret_cast<char*>(roundtrip.data()),size,NULL,NULL);
        valid=roundtrip==bytes; if(!valid) testFailure=L"UTF-8 preview page roundtrip failed";
        for(InterfaceTheme theme : {INTERFACE_THEME_LIGHT,INTERFACE_THEME_DARK}) {
            ThemeManager::SetSelectedTheme(theme); ThemeManager::ApplyToWindow(window);
            valid=valid && ThemeManager::GetSelectedTheme()==theme;
        }
        wchar_t previousLocale[32]={}; ::GetEnvironmentVariable(L"FBE_NEXT_UI_LOCALE",previousLocale,_countof(previousLocale));
        for(const wchar_t* locale : {L"en-US",L"ru-RU",L"uk-UA",L"de-DE",L"fr-FR",L"es-ES",L"it-IT",L"pl-PL",L"pt-PT",L"nl-NL",L"cs-CZ",L"bg-BG"}) {
        ::SetEnvironmentVariable(L"FBE_NEXT_UI_LOCALE",locale); FbeResetRuntimeLocalization(); ApplyLabels();
        for(UINT dpi : {96U,144U,192U}) {
            ApplyFont(dpi);
            const SIZE minimum=MinimumSize();
            for(int extra : {0,240}) {
                ::SetWindowPos(window,NULL,0,0,minimum.cx+extra,minimum.cy+extra,SWP_NOMOVE|SWP_NOZORDER);
                Layout(); RECT client={}; ::GetClientRect(window,&client);
                const int controls[]={State,Toggle,OpenLog,OpenFolder,Cleanup,SessionLabel,Sessions,CrashLabel,Crashes,ReviewCrash,ClearCrashes,ClearHangs,ComponentLabel,Components,PreviewText,Status,Preview,Save,Close};
                for(int id : controls) {
                    RECT control={}; HWND child=::GetDlgItem(window,id); ::GetWindowRect(child,&control);
                    ::MapWindowPoints(NULL,window,reinterpret_cast<POINT*>(&control),2);
                    if(testFailure.IsEmpty() && !(control.left>=0 && control.top>=0 && control.right<=client.right && control.bottom<=client.bottom && control.right>control.left && control.bottom>control.top)) testFailure.Format(L"bounds locale=%s dpi=%u id=%d rect=%ld,%ld,%ld,%ld client=%ld,%ld",locale,dpi,id,control.left,control.top,control.right,control.bottom,client.right,client.bottom);
                    valid=valid && control.left>=0 && control.top>=0 && control.right<=client.right && control.bottom<=client.bottom && control.right>control.left && control.bottom>control.top;
                }
                const int buttons[]={ReviewCrash,ClearCrashes,ClearHangs,Toggle,OpenLog,OpenFolder,Cleanup,Preview,Save,Close,IDC_DIAGNOSTIC_CANCEL,IDC_DIAGNOSTIC_PREVIOUS_PAGE,IDC_DIAGNOSTIC_NEXT_PAGE};
                for(int id : buttons) {
                    HWND child=::GetDlgItem(window,id); wchar_t label[256]={}; ::GetWindowText(child,label,_countof(label));
                    HDC dc=::GetDC(child); HGDIOBJ old=::SelectObject(dc,reinterpret_cast<HFONT>(::SendMessage(child,WM_GETFONT,0,0)));
                    SIZE text={}; ::GetTextExtentPoint32(dc,label,static_cast<int>(wcslen(label)),&text);
                    ::SelectObject(dc,old); ::ReleaseDC(child,dc); RECT area={}; ::GetClientRect(child,&area);
                    if(testFailure.IsEmpty() && text.cx+::MulDiv(12,static_cast<int>(dpi),96)>area.right) testFailure.Format(L"caption locale=%s dpi=%u id=%d text=%ld area=%ld",locale,dpi,id,text.cx,area.right);
                    valid=valid && text.cx+::MulDiv(12,static_cast<int>(dpi),96)<=area.right;
                }
                RECT left={},right={}; ::GetWindowRect(::GetDlgItem(window,Preview),&left); ::GetWindowRect(::GetDlgItem(window,Save),&right);
                valid=valid && left.right<right.left;
            }
        }
        }
        ::SetEnvironmentVariable(L"FBE_NEXT_UI_LOCALE",previousLocale[0] ? previousLocale : NULL); FbeResetRuntimeLocalization(); ApplyLabels();
        ThemeManager::SetSelectedTheme(originalTheme); ApplyFont(UiMetrics::DpiForWindow(window));
        ::SetWindowPos(window,NULL,original.left,original.top,original.right-original.left,original.bottom-original.top,SWP_NOZORDER); Layout();
        return valid;
    }
    void TestTick()
    {
        if(!testMode) return;
        if(testPhase==0 && job && job->work==Work::ReviewPreview) {
            ++reviewHeartbeat;
            if(reviewHeartbeat==3) {
                testReviewJob=job;
                wchar_t shown[128]={}; ::GetDlgItemText(window,PreviewText,shown,_countof(shown));
                testPassed=testPassed && shown[0]!=0 && ::IsWindowVisible(window);
                testReviewGate->store(true);
            }
        } else if(testPhase==0 && !job) {
            automaticReview=testReviewJob && testReviewJob->success && reviewHeartbeat>=3;
            reviewWorkerThread=testReviewJob && testReviewJob->threadId!=::GetCurrentThreadId();
            for(const auto& session:sessions) if(session.id==L"DDDD0001-0000-0000-0000-000000000001") {
                testReviewSession=session; automaticReview=automaticReview && session.crashReviewed;
            }
            testPassed=testPassed && CheckLayout() && snapshot && snapshot->Valid();
            testPassed=testPassed && automaticReview && reviewWorkerThread && !testReviewSession.crashPath.IsEmpty();
            testSnapshot=snapshot;
            int damaged=-1;
            for(size_t i=0;i<crashes.size();++i) if(crashes[i].path.Find(L"001-pid4294967290")>=0 && !crashes[i].reviewed) damaged=static_cast<int>(i);
            if(crashes.size()!=2 || damaged<0) testPassed=false;
            ::SendDlgItemMessage(window,Crashes,CB_SETCURSEL,damaged,0);
            ReviewSelectedCrash(); testPassed=testPassed && reviewPending && !job;
            ReviewSelectedCrash(); testPhase=10;
        } else if(testPhase==10 && !job) {
            damagedReview=crashes.size()==2 && !HasUnreviewedCrashes() && snapshot==testSnapshot && snapshot && snapshot->Valid();
            testPassed=testPassed && damagedReview;
            ::SendMessage(window,WM_COMMAND,ClearHangs,0); testPhase=11;
        } else if(testPhase==11 && !job) {
            testPassed=testPassed && hangPlan && hangPlan->files==0 && snapshot==testSnapshot;
            ::SendMessage(window,WM_COMMAND,ClearHangs,0); testPhase=12;
        } else if(testPhase==12 && !job) {
            testPassed=testPassed && !hangPlan && snapshot && snapshot->Valid();
            testSnapshot=snapshot;
            for(auto& session:sessions) if(session.crashPath==testReviewSession.crashPath) session.crashReviewed=false;
            for(auto& report:crashes) if(report.path==testReviewSession.crashPath) report.reviewed=false;
            auto stale=testReviewSession; stale.crashFingerprint+=L"-stale";
            testReviewGate=std::make_shared<std::atomic_bool>(true);
            ReviewPreview(stale); testReviewJob=job; testPhase=20;
        } else if(testPhase==20 && !job) {
            staleRejected=testReviewJob && !testReviewJob->success && snapshot==testSnapshot;
            for(const auto& session:sessions) if(session.crashPath==testReviewSession.crashPath) staleRejected=staleRejected && !session.crashReviewed;
            testPassed=testPassed && staleRejected;
            const CString ini=CString(DeploymentContext::SettingsDirectory().c_str())+L"diagnostics-reviewed.ini";
            const int slash=testReviewSession.crashPath.ReverseFind(L'\\');
            testPassed=testPassed && ::WritePrivateProfileString(L"Reviewed",testReviewSession.crashPath.Mid(slash+1),NULL,ini)!=FALSE;
            const HANDLE lock=::CreateFile(ini,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
            if(lock!=INVALID_HANDLE_VALUE) testReviewLock.Attach(lock); else testPassed=false;
            ReviewPreview(testReviewSession); testReviewJob=job; testPhase=21;
        } else if(testPhase==21 && !job) {
            reviewWriteFailed=testReviewJob && !testReviewJob->success && snapshot==testSnapshot;
            for(const auto& session:sessions) if(session.crashPath==testReviewSession.crashPath) reviewWriteFailed=reviewWriteFailed && !session.crashReviewed;
            testPassed=testPassed && reviewWriteFailed; testReviewLock.Close();
            testReviewGate=std::make_shared<std::atomic_bool>(false);
            ReviewPreview(testReviewSession); testReviewJob=job; testPhase=1;
        } else if(testPhase==1 && job) {
            ++heartbeat;
            if(heartbeat==3) { ::SendMessage(window,WM_COMMAND,IDC_DIAGNOSTIC_CANCEL,0); testPhase=2; }
        } else if(testPhase==2 && !job) {
            cancelledReady=snapshot && snapshot->Valid() && snapshot==testSnapshot && ::IsWindowEnabled(::GetDlgItem(window,Save)) &&
                testReviewJob && testReviewJob->cancelled.load() && !testReviewJob->success;
            testPhase=3;
            ReviewPreview(testReviewSession);
            closingJob=job; ::SendMessage(window,WM_CLOSE,0,0);
            closeDeferred=job && closing && ::IsWindow(window);
        }
    }
public:
    CStringA RuntimeTestDetails() const {
        CStringA result(CW2A(testFailure,CP_UTF8)), state; state.Format("\ndamaged_review=%d\nautomatic_review=%d\nreview_worker_thread=%d\nreview_stale_rejected=%d\nreview_write_failed=%d\nphase=%u\nheartbeat=%u\ncancel_ready=%d\nclose_deferred=%d\n",damagedReview,automaticReview,reviewWorkerThread,staleRejected,reviewWriteFailed,testPhase,heartbeat,cancelledReady,closeDeferred); return result+state;
    }
    bool RunRuntimeTest(HWND owner)
    {
        wchar_t guard[2]={};
        if(::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE",guard,2)!=1 || guard[0]!=L'1') return false;
        testMode=true; testReviewGate=std::make_shared<std::atomic_bool>(false); Show(owner);
        return testPassed && testPhase==3 && heartbeat>=3 && cancelledReady && closeDeferred && closingJob && closingJob->done.load(std::memory_order_acquire) &&
            closingJob->cancelled.load() && !closingJob->success && !StartupTrace::IsDiagnosticCrashReviewed(testReviewSession);
    }
private:
