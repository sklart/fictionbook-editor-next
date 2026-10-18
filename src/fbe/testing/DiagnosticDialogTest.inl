// Included inside DiagnosticDialog; reachable only through the guarded runtime scenario.
    CString testFailure;
    bool testMode=false, testPassed=true;
    unsigned int testPhase=0, heartbeat=0;
    bool cancelledReady=false, closeDeferred=false;
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
                for(int id : {State,Toggle,OpenLog,OpenFolder,Cleanup,SessionLabel,Sessions,ComponentLabel,Components,PreviewText,Status,Preview,Save,Close}) {
                    RECT control={}; HWND child=::GetDlgItem(window,id); ::GetWindowRect(child,&control);
                    ::MapWindowPoints(NULL,window,reinterpret_cast<POINT*>(&control),2);
                    if(testFailure.IsEmpty() && !(control.left>=0 && control.top>=0 && control.right<=client.right && control.bottom<=client.bottom && control.right>control.left && control.bottom>control.top)) testFailure.Format(L"bounds locale=%s dpi=%u id=%d rect=%ld,%ld,%ld,%ld client=%ld,%ld",locale,dpi,id,control.left,control.top,control.right,control.bottom,client.right,client.bottom);
                    valid=valid && control.left>=0 && control.top>=0 && control.right<=client.right && control.bottom<=client.bottom && control.right>control.left && control.bottom>control.top;
                }
                const int buttons[]={Toggle,OpenLog,OpenFolder,Cleanup,Preview,Save,Close,IDC_DIAGNOSTIC_CANCEL,IDC_DIAGNOSTIC_PREVIOUS_PAGE,IDC_DIAGNOSTIC_NEXT_PAGE};
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
        if(testPhase==0 && !job) {
            testPassed=CheckLayout() && snapshot && snapshot->Valid();
            testPhase=1;
            Begin(Work::Prepare,[](Job& result) {
                for(unsigned int tick=0; tick<200 && !result.cancelled.load(); ++tick) ::Sleep(10);
            });
        } else if(testPhase==1 && job) {
            ++heartbeat;
            if(heartbeat==3) { ::SendMessage(window,WM_COMMAND,IDC_DIAGNOSTIC_CANCEL,0); testPhase=2; }
        } else if(testPhase==2 && !job) {
            cancelledReady=snapshot && snapshot->Valid() && ::IsWindowEnabled(::GetDlgItem(window,Save));
            testPhase=3;
            Begin(Work::Prepare,[](Job& result) {
                for(unsigned int tick=0; tick<200 && !result.cancelled.load(); ++tick) ::Sleep(10);
            });
            closingJob=job; ::SendMessage(window,WM_CLOSE,0,0);
            closeDeferred=job && closing && ::IsWindow(window);
        }
    }
public:
    CStringA RuntimeTestDetails() const {
        CStringA result(CW2A(testFailure,CP_UTF8)), state; state.Format("\nphase=%u\nheartbeat=%u\ncancel_ready=%d\nclose_deferred=%d\n",testPhase,heartbeat,cancelledReady,closeDeferred); return result+state;
    }
    bool RunRuntimeTest(HWND owner)
    {
        wchar_t guard[2]={};
        if(::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE",guard,2)!=1 || guard[0]!=L'1') return false;
        testMode=true; Show(owner);
        return testPassed && testPhase==3 && heartbeat>=3 && cancelledReady && closeDeferred && closingJob && closingJob->done.load(std::memory_order_acquire);
    }
private:
