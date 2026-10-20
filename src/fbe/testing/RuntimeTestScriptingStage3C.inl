if (IsFbeTestScenario(L"scripting-stage3c-dialogs")||IsFbeTestScenario(L"scripting-stage3c-clipboard")||IsFbeTestScenario(L"scripting-stage3c-clipboard-ole")||IsFbeTestScenario(L"scripting-stage3c-status")) {
    bool passed=false; CStringA details="start";
    try {
        wchar_t root[32768]={}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT",root,_countof(root));
        const CString script=CString(root)+L"\\stage3c.js"; _Settings.SetScriptsFolder(root);
        const bool dirty=DocChanged();
        FbeScriptDiagnostics::ScopedDialogSuppression suppress;
        m_doc->RunScript(script);
        CComPtr<IDispatch> external;CheckError(m_doc->m_body.Document()->parentWindow->get_external(&external));
        const wchar_t* names[]={L"ShowMessage",L"SelectFromList",L"SelectMultipleFromList",L"ChooseColor",L"GetClipboardText",L"SetClipboardText",L"GetClipboardHtml",L"SetClipboardHtml",L"PushStatusBarText",L"PopStatusBarText",L"ClearStatusBarText"};
        for(DISPID i=0;i<11;++i){LPOLESTR name=const_cast<LPOLESTR>(names[i]);DISPID id=DISPID_UNKNOWN;CheckError(external->GetIDsOfNames(IID_NULL,&name,1,LOCALE_USER_DEFAULT,&id));if(id!=153+i){details="DISPID changed";CheckError(E_FAIL);}}
        CComPtr<IOleUndoManager> undoManager;IServiceProviderPtr services(m_doc->m_body.Document());
        CheckError(services->QueryService(SID_SOleUndoManager,IID_IOleUndoManager,reinterpret_cast<void**>(&undoManager)));
        auto history=[&](bool redo){
            CComPtr<IEnumOleUndoUnits> units;CheckError(redo?undoManager->EnumRedoable(&units):undoManager->EnumUndoable(&units));
            std::vector<CComPtr<IOleUndoUnit>> result;
            for(;;){CComPtr<IOleUndoUnit> unit;const HRESULT hr=units->Next(1,&unit,nullptr);if(hr==S_FALSE)break;CheckError(hr);result.push_back(unit);}return result;
        };
        auto invoke=[&](LPCWSTR name,CComVariant* args=nullptr,int count=0) {
            const auto undo=history(false),redo=history(true);
            CComVariant result; CheckError(m_doc->InvokeFunc(name,args,count,result,true));
            if(result.vt==VT_BSTR) details=CW2A(result.bstrVal,CP_UTF8);
            if(result.vt!=VT_BSTR||wcscmp(result.bstrVal,L"PASS")) CheckError(E_FAIL);
            if(wcscmp(name,L"stage3cNested")&&(undo!=history(false)||redo!=history(true))){details="API changed Undo/Redo history";CheckError(E_FAIL);}
        };
        invoke(L"stage3cResult");
        const auto revision=m_doc->m_body.GetVersionNumber();
        const CString xml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
        const InterfaceTheme initialTheme=ThemeManager::GetSelectedTheme();
        struct RestoreTheme { InterfaceTheme value; ~RestoreTheme(){ThemeManager::SetSelectedTheme(value);} } restoreTheme={initialTheme};
        FbeScripts::ScriptExecutionScope context(script);
        if(IsFbeTestScenario(L"scripting-stage3c-status")) {
            auto pane=[&](int index) {
                const size_t length=LOWORD(::SendMessageW(m_status,SB_GETTEXTLENGTHW,index,0));
                std::vector<wchar_t> text(length+1);::SendMessageW(m_status,SB_GETTEXTW,index,reinterpret_cast<LPARAM>(text.data()));return CString(text.data());
            };
            auto expect=[&](LPCWSTR value) {if(pane(0)!=value){details="incorrect status pane";CheckError(E_FAIL);}};
            const int parts=m_status.GetParts(0,nullptr);std::vector<CString> neighbors;
            for(int i=1;i<parts;++i)neighbors.push_back(pane(i));
            m_status_state.SetContext(L"context-current");
            m_status_state.SetTransient(L"system-current",::GetTickCount());RefreshStatusMainPane();
            {
                FbeScripts::ScriptExecutionScope parent(script);
                invoke(L"stage3cPushA");expect(L"outer");invoke(L"stage3cPushB");expect(L"inner");
                invoke(L"stage3cBadPop");expect(L"inner");invoke(L"stage3cPopB");expect(L"outer");
                const size_t depth=FbeScripts::ExecutionContexts().size();invoke(L"stage3cNested");expect(L"outer");
                if(depth!=FbeScripts::ExecutionContexts().size()){details="nested context leaked";CheckError(E_FAIL);}
                {
                    FbeScripts::ScriptExecutionScope child(script);invoke(L"stage3cClear");expect(L"outer");
                }
                m_incsearch=1;m_is_str=L"probe";RefreshStatusMainPane();
                if(pane(0).Find(L"probe")<0||pane(0)==L"outer"){details="incremental search priority";CheckError(E_FAIL);}
                m_incsearch=0;RefreshStatusMainPane();expect(L"outer");
                invoke(L"stage3cLegacy");expect(L"outer");invoke(L"stage3cPopA");expect(L"legacy-current");
                invoke(L"stage3cPushA");
            }
            expect(L"legacy-current");
            if(FbeScripts::Status::Active(m_hWnd)){details="completed script retained status";CheckError(E_FAIL);}
            m_status_state.ClearTransientIfExpired(::GetTickCount()+6000);RefreshStatusMainPane();expect(L"context-current");
            for(int i=1;i<parts;++i)if(pane(i)!=neighbors[static_cast<size_t>(i-1)]){details="neighboring status pane changed";CheckError(E_FAIL);}
            details="LIFO, nested cleanup, JScript/COM exceptions, live system status and neighboring panes PASS";
        } else if(IsFbeTestScenario(L"scripting-stage3c-clipboard-ole")) {
            // Defense in depth: never publish into a developer/interactive clipboard.
            wchar_t github[16]={},runner[32]={},account[256]={},station[256]={};DWORD bytes=0;
            ::GetEnvironmentVariableW(L"GITHUB_ACTIONS",github,_countof(github));
            ::GetEnvironmentVariableW(L"RUNNER_ENVIRONMENT",runner,_countof(runner));
            ::GetEnvironmentVariableW(L"FBE_NEXT_CLIPBOARD_CI_ACCOUNT",account,_countof(account));
            if(wcscmp(github,L"true")||wcscmp(runner,L"github-hosted")||!account[0]||
                !::GetUserObjectInformationW(::GetProcessWindowStation(),UOI_NAME,station,sizeof(station),&bytes)||
                !_wcsicmp(station,L"WinSta0")||FbeScripts::Clipboard::testStorage) {
                details="real clipboard requires private CI logon/window station";CheckError(E_ACCESSDENIED);
            }
            struct ClearOleClipboard {~ClearOleClipboard(){::OleSetClipboard(nullptr);}} cleanup;
            invoke(L"stage3cOleClipboard");
            {
                CComPtr<IDataObject> object;CheckError(::OleGetClipboard(&object));
                FORMATETC marker={static_cast<CLIPFORMAT>(::RegisterClipboardFormatW(L"FBE Next Internal HTML v2")),nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
                FbeScripts::Clipboard::Medium markerData;
                const HRESULT markerResult=object->GetData(&marker,&markerData.value);
                // The system OLE wrapper may use another failure HRESULT for
                // unsupported formats. Verify absence, not one error code.
                const BOOL markerAvailable=::IsClipboardFormatAvailable(marker.cfFormat);
                if(SUCCEEDED(markerResult)||markerAvailable){details.Format("OLE internal marker: available=%d GetData=0x%08lX",markerAvailable,static_cast<unsigned long>(markerResult));CheckError(E_FAIL);}
                FORMATETC format={FbeScripts::Clipboard::HtmlFormat(),nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
                FbeScripts::Clipboard::Medium medium;CheckError(object->GetData(&format,&medium.value));
                FbeScripts::Clipboard::Locked locked(medium.value.hGlobal);
                const size_t size=::GlobalSize(medium.value.hGlobal);
                const char* data=static_cast<const char*>(locked.data);
                if(!size||size>FbeScripts::Clipboard::MaximumBytes||!memchr(data,0,size))CheckError(E_FAIL);
                const CStringA raw(data);
                auto offset=[&](LPCSTR key) {
                    const int found=raw.Find(key);if(found<0)CheckError(E_FAIL);
                    const int start=found+static_cast<int>(strlen(key));
                    if(start+12>raw.GetLength()||raw.Mid(start+10,2)!="\r\n")CheckError(E_FAIL);
                    size_t result=0;
                    for(int i=0;i<10;++i){const char digit=raw[start+i];if(digit<'0'||digit>'9')CheckError(E_FAIL);result=result*10+static_cast<size_t>(digit-'0');}
                    return result;
                };
                const size_t start=offset("StartHTML:"),end=offset("EndHTML:"),first=offset("StartFragment:"),last=offset("EndFragment:");
                const CStringA expected(CW2A(L"<p><strong>Кириллица 😀</strong></p>",CP_UTF8));
                if(!(start<first&&first<last&&last<end&&end==static_cast<size_t>(raw.GetLength()))||
                    last-first!=static_cast<size_t>(expected.GetLength())||memcmp(data+first,expected.GetString(),last-first)||
                    raw.Mid(static_cast<int>(start),static_cast<int>(first-start))!="<html><body><!--StartFragment-->"||
                    raw.Mid(static_cast<int>(last))!="<!--EndFragment--></body></html>") {
                    details="CF_HTML UTF-8 byte offsets";CheckError(E_FAIL);
                }
            } // ReleaseStgMedium, GlobalUnlock and IDataObject::Release before clearing OLE.
            CString paste;
            if(!FbeClipboard::HtmlPasteSanitizer::ReadClipboardFragment(m_hWnd,paste)||
                paste!=L"<p><strong>Кириллица 😀</strong></p>"){details="normal Paste decoder rejected OLE HTML";CheckError(E_FAIL);}
            CheckError(::OleSetClipboard(nullptr));
            {
                FbeScripts::Clipboard::Open opened(m_hWnd);
                if(::IsClipboardFormatAvailable(CF_UNICODETEXT)||::IsClipboardFormatAvailable(FbeScripts::Clipboard::HtmlFormat())) {
                    details="OLE formats retained after cleanup";CheckError(E_FAIL);
                }
            }
            details="real OLE/JScript Unicode and sanitized HTML roundtrip, UTF-8 offsets, Paste decoder and resource cleanup PASS";
        } else if(IsFbeTestScenario(L"scripting-stage3c-clipboard")) {
            FbeScripts::Clipboard::TestStorage storage;
            struct ResetClipboardStorage { ~ResetClipboardStorage(){FbeScripts::Clipboard::testStorage=nullptr;} } resetStorage;
            FbeScripts::Clipboard::testStorage=&storage;
            invoke(L"stage3cClipboard");
            for(const auto view:{DESC,SOURCE}){ShowView(view);invoke(L"stage3cClipboard");}ShowView(BODY);
            FORMATETC marker={static_cast<CLIPFORMAT>(::RegisterClipboardFormatW(L"FBE Next Internal HTML v2")),nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
            if(storage.object->QueryGetData(&marker)!=DV_E_FORMATETC) {details="external HTML marked as internal";CheckError(E_FAIL);}
            storage.failure=CLIPBRD_E_CANT_OPEN;invoke(L"stage3cBusy");storage.failure=S_OK;
            for(const auto* malformed:{"Version:1.0\r\nStartFragment:0\r\nEndFragment:5\r\n", "Version:1.0\r\nStartHTML:100\r\nEndHTML:99999999\r\nStartFragment:100\r\nEndFragment:99999999\r\n"}) {
                std::vector<FbeScripts::Clipboard::Format> formats;
                formats.push_back({FbeScripts::Clipboard::HtmlFormat(),std::vector<BYTE>(malformed,malformed+strlen(malformed)+1)});
                storage.object.Attach(new FbeScripts::Clipboard::DataObject(std::move(formats)));invoke(L"stage3cBadHtml");
            }
            std::vector<FbeScripts::Clipboard::Format> badText={{CF_UNICODETEXT,{0x41,0x41}}};
            storage.object.Release();storage.object.Attach(new FbeScripts::Clipboard::DataObject(std::move(badText)));invoke(L"stage3cBadText");
            FbeScripts::Clipboard::testStorage=nullptr;
            CHandle ready(::CreateEventW(nullptr,TRUE,FALSE,nullptr)),release(::CreateEventW(nullptr,TRUE,FALSE,nullptr));
            if(!ready||!release)CheckError(E_OUTOFMEMORY);
            LONG opened=0;
            std::thread holder([&](){if(::OpenClipboard(nullptr)){::InterlockedExchange(&opened,1);::SetEvent(ready);::WaitForSingleObject(release,5000);::CloseClipboard();}else ::SetEvent(ready);});
            ::WaitForSingleObject(ready,2000);HRESULT failure=E_FAIL;
            if(::InterlockedCompareExchange(&opened,0,0)) {try{invoke(L"stage3cBusy");failure=S_OK;}catch(const _com_error& error){failure=error.Error();}}
            ::SetEvent(release);holder.join();CheckError(failure);
            details="isolated OLE formats and real busy access PASS; user clipboard unchanged";
        } else for(int index=0;index<8;++index) {
            if(index==2) ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
            if(index==3) ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
            CString title; title.Format(L"FBE stage3c %lu %d",::GetCurrentProcessId(),index);
            const HWND priorFocus=::GetFocus(); const DWORD uiThread=::GetCurrentThreadId();
            struct Driver { DWORD thread; HWND owner,dialog=nullptr,button=nullptr; bool owned=false; } driver={uiThread,m_hWnd};
            std::thread input([&]() {
                for(int retry=0;retry<400&&!driver.button;++retry) {
                    ::EnumThreadWindows(driver.thread,[](HWND window,LPARAM data)->BOOL {
                        auto& state=*reinterpret_cast<Driver*>(data);
                        if(window!=state.owner&&::IsWindowVisible(window)&&::GetAncestor(window,GA_ROOTOWNER)==state.owner) state.dialog=window;
                        return TRUE;
                    },reinterpret_cast<LPARAM>(&driver));
                    const int id=index==0?IDNO:(index==1||index==5||index==7)?IDCANCEL:IDOK;
                    if(driver.dialog&&!::IsWindowEnabled(driver.owner)) {
                        driver.button=::GetDlgItem(driver.dialog,id); driver.owned=driver.button!=nullptr;
                        if(index>=2&&index<=5){wchar_t caption[256]={};::GetWindowTextW(driver.dialog,caption,_countof(caption));driver.owned=driver.owned&&title==caption;}
                    }
                    if(!driver.button) ::Sleep(5);
                }
                if(driver.button&&(index==2||index==5)) ::PostMessageW(driver.dialog,WM_KEYDOWN,index==2?VK_RETURN:VK_ESCAPE,0);
                else if(driver.button) ::PostMessageW(driver.dialog,WM_COMMAND,MAKEWPARAM(::GetDlgCtrlID(driver.button),BN_CLICKED),reinterpret_cast<LPARAM>(driver.button));
                else if(driver.dialog) ::PostMessageW(driver.dialog,WM_CLOSE,0,0);
            });
            CComVariant args[2]; args[0]=index; args[1]=title;
            HRESULT failure=S_OK;
            try { invoke(L"stage3cDialog",args,2); } catch(const _com_error& error) { failure=error.Error(); }
            input.join(); CheckError(failure);
            if(!driver.owned||(::IsWindow(priorFocus)&&::GetFocus()!=priorFocus)) { details="dialog owner/focus"; CheckError(E_FAIL); }
            CStringA row; row.Format("dialog=%d PASS\r\n",index); DWORD written=0; output.Write(row,row.GetLength(),&written); output.Flush();
        }
        if(dirty!=DocChanged()||xml!=CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml))) { details="dialog changed document"; CheckError(E_FAIL); }
        CStringA versions;versions.Format("revision-before=%ld revision-after=%ld\r\n",revision,m_doc->m_body.GetVersionNumber());
        DWORD versionBytes=0;output.Write(versions,versions.GetLength(),&versionBytes);
        passed=true;
    } catch(const _com_error& error) { CStringA code; code.Format(" hr=0x%08lX",static_cast<unsigned long>(error.Error()));details+=code; }
    catch(...) { details="native exception"; }
    CStringA report; report.Format("result=%s\r\ndetails=%s\r\n",passed?"PASS":"FAIL",details.GetString());
    DWORD written=0;output.Write(report,report.GetLength(),&written);output.Close();::PostQuitMessage(passed?0:1);return 0;
}
