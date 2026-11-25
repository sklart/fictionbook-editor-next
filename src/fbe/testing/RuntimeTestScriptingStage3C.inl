if (IsFbeTestScenario(L"scripting-stage3c-dialogs")||IsFbeTestScenario(L"scripting-stage3c-clipboard")||IsFbeTestScenario(L"scripting-stage3c-clipboard-ole")||IsFbeTestScenario(L"scripting-stage3c-status")) {
    bool passed=false; CStringA details="start";
    try {
        wchar_t root[32768]={}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT",root,_countof(root));
        const CString script=CString(root)+L"\\stage3c.js"; _Settings.SetScriptsFolder(root);
        const bool dirty=DocChanged();
        if (dirty) { details="fixture must start clean"; CheckError(E_FAIL); }
        const GUID setupIdentity=m_doc->ClipboardIdentity();
        const auto setupDocument=m_doc->CreateDOMForAnalysis();
        if(!setupDocument) { details="cannot snapshot document before setup"; CheckError(E_FAIL); }
        const CString setupXml(static_cast<LPCWSTR>(setupDocument->xml));
        FB::Doc::RecoveryBinarySnapshot setupBinaries;
        if(!m_doc->CaptureRecoveryBinaries(setupBinaries)) { details="cannot snapshot binaries before setup"; CheckError(E_FAIL); }
        const auto setupSavepoints=m_doc->CaptureUndoSavepoints();
        CComPtr<IOleUndoManager> undoManager;IServiceProviderPtr services(m_doc->m_body.Document());
        CheckError(services->QueryService(SID_SOleUndoManager,IID_IOleUndoManager,reinterpret_cast<void**>(&undoManager)));
        auto history=[&](bool redo){
            CComPtr<IEnumOleUndoUnits> units;CheckError(redo?undoManager->EnumRedoable(&units):undoManager->EnumUndoable(&units));
            std::vector<CComPtr<IOleUndoUnit>> result;
            for(;;){CComPtr<IOleUndoUnit> unit;const HRESULT hr=units->Next(1,&unit,nullptr);if(hr==S_FALSE)break;CheckError(hr);result.push_back(unit);}return result;
        };
        const auto setupUndo=history(false), setupRedo=history(true);
        FbeScriptDiagnostics::ScopedDialogSuppression suppress;
        m_doc->RunScript(script);
        if(!m_doc || FB::Doc::BinaryOwner(setupIdentity)!=m_doc) { details="script setup changed document identity"; CheckError(E_FAIL); }
        const bool dirtyAfterSetup=DocChanged(); const auto revisionAfterSetup=m_doc->m_body.GetVersionNumber();
        const auto setupAfter=m_doc->CreateDOMForAnalysis(); FB::Doc::RecoveryBinarySnapshot setupBinariesAfter;
        if(FB::Doc::BinaryOwner(setupIdentity)!=m_doc || !setupAfter ||
            setupXml!=CString(static_cast<LPCWSTR>(setupAfter->xml)) || !m_doc->CaptureRecoveryBinaries(setupBinariesAfter) ||
            !FB::Doc::SameRecoveryBinaries(setupBinaries,setupBinariesAfter)) {
            details="script setup changed document identity or saved content"; CheckError(E_FAIL);
        }
        const auto setupAfterSavepoints=m_doc->CaptureUndoSavepoints();
        if(setupUndo!=history(false) || setupRedo!=history(true) ||
            setupSavepoints.savedVersion!=setupAfterSavepoints.savedVersion ||
            setupSavepoints.recoveryVersion!=setupAfterSavepoints.recoveryVersion ||
            setupSavepoints.savedHtml!=setupAfterSavepoints.savedHtml || setupSavepoints.recoveryHtml!=setupAfterSavepoints.recoveryHtml ||
            setupSavepoints.hasSaved!=setupAfterSavepoints.hasSaved || setupSavepoints.hasRecovery!=setupAfterSavepoints.hasRecovery ||
            setupSavepoints.reconcile!=setupAfterSavepoints.reconcile) {
            details="script setup changed history or checkpoint contents"; CheckError(E_FAIL);
        }
        CComPtr<IDispatch> external;CheckError(m_doc->m_body.Document()->parentWindow->get_external(&external));
        const wchar_t* names[]={L"ShowMessage",L"SelectFromList",L"SelectMultipleFromList",L"ChooseColor",L"GetClipboardText",L"SetClipboardText",L"GetClipboardHtml",L"SetClipboardHtml",L"PushStatusBarText",L"PopStatusBarText",L"ClearStatusBarText"};
        for(DISPID i=0;i<11;++i){LPOLESTR name=const_cast<LPOLESTR>(names[i]);DISPID id=DISPID_UNKNOWN;CheckError(external->GetIDsOfNames(IID_NULL,&name,1,LOCALE_USER_DEFAULT,&id));if(id!=153+i){details="DISPID changed";CheckError(E_FAIL);}}
        auto invoke=[&](LPCWSTR name,CComVariant* args=nullptr,int count=0) {
            const auto undo=history(false),redo=history(true);
            CComVariant result; CheckError(m_doc->InvokeFunc(name,args,count,result,true));
            if(result.vt==VT_BSTR) details=CW2A(result.bstrVal,CP_UTF8);
            if(result.vt!=VT_BSTR||wcscmp(result.bstrVal,L"PASS")) CheckError(E_FAIL);
            if(wcscmp(name,L"stage3cNested")&&(undo!=history(false)||redo!=history(true))){details="API changed Undo/Redo history";CheckError(E_FAIL);}
        };
        invoke(L"stage3cResult");
        wchar_t prepareHistory[8]={}; ::GetEnvironmentVariableW(L"FBE_NEXT_STAGE3C_PREPARE_HISTORY",prepareHistory,_countof(prepareHistory));
        if(!wcscmp(prepareHistory,L"1")) {
            if(!IsFbeTestScenario(L"scripting-stage3c-dialogs")){details="history fixture requires dialogs";CheckError(E_INVALIDARG);}
            CComVariant prepared; CheckError(m_doc->InvokeFunc(L"stage3cPrepareHistory",nullptr,0,prepared,true));
            if(prepared.vt!=VT_BSTR||wcscmp(prepared.bstrVal,L"PASS")){details="cannot prepare dialog history";CheckError(E_FAIL);}
            CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
            const auto preparedUndo=history(false),preparedRedo=history(true);
            CStringA state;state.Format("history-prepared undo=%u redo=%u\r\n",static_cast<unsigned>(preparedUndo.size()),static_cast<unsigned>(preparedRedo.size()));
            DWORD written=0;output.Write(state,state.GetLength(),&written);output.Flush();
            if(preparedUndo.empty()||preparedRedo.empty()){details="dialog history fixture is empty";CheckError(E_FAIL);}
        }
        const bool dirtyBeforeDialogs=DocChanged();
        const auto revision=m_doc->m_body.GetVersionNumber();
        const CString xml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
        CStringA initialState; initialState.Format("setup-dirty=%d,%d,%d revision=%ld,%ld\r\n",dirty?1:0,dirtyAfterSetup?1:0,DocChanged()?1:0,revisionAfterSetup,revision); DWORD initialBytes=0; output.Write(initialState,initialState.GetLength(),&initialBytes); output.Flush();
        const char* noopStatus=dirtyAfterSetup ? "script-setup-dirty=FAIL\r\n" : "script-setup-dirty=PASS\r\n";
        DWORD noopBytes=0; output.Write(noopStatus,static_cast<DWORD>(strlen(noopStatus)),&noopBytes); output.Flush();
        if(dirtyAfterSetup) { details="script setup changed dirty state"; CheckError(E_FAIL); }
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
        } else {
          // Capture live form values as well as the serialized XML.
          auto liveState=[&]() {
            MSHTML::IHTMLDocument3Ptr document(m_doc->m_body.Document());
            MSHTML::IHTMLElementPtr body(document->getElementById(L"fbw_body"));
            MSHTML::IHTMLElementPtr description(document->getElementById(L"fbw_desc"));
            if(!body||!description) CheckError(E_FAIL);
            CString state;
            auto append=[&](const _bstr_t& value) {
                CString length; length.Format(L"%u:",value.length()); state+=length;
                state.Append(static_cast<LPCWSTR>(value),static_cast<int>(value.length()));
            };
            append(_bstr_t(body->outerHTML)); append(_bstr_t(description->innerHTML));
            MSHTML::IHTMLElementCollectionPtr controls(MSHTML::IHTMLElement2Ptr(description)->getElementsByTagName(L"*"));
            if(!controls) CheckError(E_FAIL);
            for(long i=0;i<controls->length;++i) {
                IDispatchPtr element(controls->item(_variant_t(i),_variant_t()));
                MSHTML::IHTMLInputElementPtr input(element);
                if(input) { append(_bstr_t(input->value)); append(_bstr_t(input->checked?L"1":L"0")); continue; }
                MSHTML::IHTMLTextAreaElementPtr area(element);
                if(area) { append(_bstr_t(area->value)); continue; }
                MSHTML::IHTMLSelectElementPtr select(element);
                if(select) append(_bstr_t(select->value));
            }
            return state;
          };
          for(int index=0;index<8;++index) {
            const auto revisionBeforeTheme=m_doc->m_body.GetVersionNumber(); const bool dirtyBeforeTheme=DocChanged();
            if(index==2) ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
            if(index==3) ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
            const auto revisionBeforeDialog=m_doc->m_body.GetVersionNumber(); const bool dirtyBeforeDialog=DocChanged();
            CString title; title.Format(L"FBE stage3c %lu %d",::GetCurrentProcessId(),index);
            const HWND priorFocus=::GetFocus(); const DWORD uiThread=::GetCurrentThreadId();
            const auto beforeDocument=m_doc->CreateDOMForAnalysis();
            if(!beforeDocument){details="cannot snapshot document before dialog";CheckError(E_FAIL);}
            const CString beforeXml(static_cast<LPCWSTR>(beforeDocument->xml));
            const bool beforeDirty=DocChanged(); const auto beforeUndo=history(false),beforeRedo=history(true);
            const CString beforeLive=liveState();
            FB::Doc::RecoveryBinarySnapshot beforeBinaries;
            if(!m_doc->CaptureRecoveryBinaries(beforeBinaries)){details="cannot snapshot binaries before dialog";CheckError(E_FAIL);}
            const auto beforeSavepoints=m_doc->CaptureUndoSavepoints();
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
            const auto afterDocument=m_doc->CreateDOMForAnalysis();
            if(!afterDocument){details="cannot snapshot document after dialog";CheckError(E_FAIL);}
            const CString afterXml(static_cast<LPCWSTR>(afterDocument->xml));
            const bool afterDirty=DocChanged(), xmlSame=beforeXml==afterXml;
            const bool undoSame=beforeUndo==history(false),redoSame=beforeRedo==history(true);
            const bool liveSame=beforeLive==liveState();
            FB::Doc::RecoveryBinarySnapshot afterBinaries;
            const bool binarySame=m_doc->CaptureRecoveryBinaries(afterBinaries)&&FB::Doc::SameRecoveryBinaries(beforeBinaries,afterBinaries);
            const auto afterSavepoints=m_doc->CaptureUndoSavepoints();
            const long versionAfterDialog=m_doc->m_body.GetVersionNumber();
            // The test changes to Dark immediately before dialog 3. A deferred
            // theme application may rebase a clean checkpoint to the current
            // MSHTML version; no other version transition is excepted here.
            const bool darkThemeTransition=index==3 && ThemeManager::GetSelectedTheme()==INTERFACE_THEME_DARK;
            const bool savedVersionSame=beforeSavepoints.savedVersion==afterSavepoints.savedVersion ||
                (darkThemeTransition && beforeSavepoints.clean && afterSavepoints.clean && beforeSavepoints.savedVersion==revisionBeforeDialog &&
                 afterSavepoints.savedVersion==versionAfterDialog);
            const bool recoveryVersionSame=beforeSavepoints.recoveryVersion==afterSavepoints.recoveryVersion ||
                (darkThemeTransition && beforeSavepoints.recoveryClean && afterSavepoints.recoveryClean && beforeSavepoints.recoveryVersion==revisionBeforeDialog &&
                 afterSavepoints.recoveryVersion==versionAfterDialog);
            const bool savepointsSame=savedVersionSame&&recoveryVersionSame&&
                beforeSavepoints.savedHtml==afterSavepoints.savedHtml&&beforeSavepoints.recoveryHtml==afterSavepoints.recoveryHtml&&
                beforeSavepoints.hasSaved==afterSavepoints.hasSaved&&beforeSavepoints.hasRecovery==afterSavepoints.hasRecovery&&
                beforeSavepoints.reconcile==afterSavepoints.reconcile&&beforeSavepoints.clean==afterSavepoints.clean&&
                beforeSavepoints.recoveryClean==afterSavepoints.recoveryClean;
            CStringA isolation;isolation.Format("dialog=%d isolation xml=%d live=%d binary=%d savepoints=%d dirty=%d,%d undo=%d redo=%d\r\n",index,xmlSame?1:0,liveSame?1:0,binarySame?1:0,savepointsSame?1:0,beforeDirty?1:0,afterDirty?1:0,undoSame?1:0,redoSame?1:0);
            DWORD isolationBytes=0;output.Write(isolation,isolation.GetLength(),&isolationBytes);output.Flush();
            if(!xmlSame||!liveSame||!binarySame||!savepointsSame||beforeDirty!=afterDirty||!undoSame||!redoSame){details="dialog changed document, save state, Undo or Redo";CheckError(E_FAIL);}
            CStringA transition; transition.Format("dialog=%d revision=%ld,%ld,%ld dirty=%d,%d,%d\r\n",index,revisionBeforeTheme,revisionBeforeDialog,m_doc->m_body.GetVersionNumber(),dirtyBeforeTheme?1:0,dirtyBeforeDialog?1:0,DocChanged()?1:0); DWORD transitionBytes=0; output.Write(transition,transition.GetLength(),&transitionBytes); output.Flush();
            if(!driver.owned||(::IsWindow(priorFocus)&&::GetFocus()!=priorFocus)) { details="dialog owner/focus"; CheckError(E_FAIL); }
            CStringA row; row.Format("dialog=%d PASS\r\n",index); DWORD written=0; output.Write(row,row.GetLength(),&written); output.Flush();
          }
        }
        const bool dirtyAfter=DocChanged();
        const CString xmlAfter(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
        if(dirtyBeforeDialogs!=dirtyAfter||xml!=xmlAfter) {
            int firstDifference=0; const int commonLength=(std::min)(xml.GetLength(),xmlAfter.GetLength());
            while(firstDifference<commonLength && xml[firstDifference]==xmlAfter[firstDifference]) ++firstDifference;
            details.Format("dialog changed document: dirty=%d->%d xml_equal=%d xml_length=%d->%d first_difference=%d revision=%ld->%ld",dirtyBeforeDialogs?1:0,dirtyAfter?1:0,xml==xmlAfter?1:0,xml.GetLength(),xmlAfter.GetLength(),firstDifference,revision,m_doc->m_body.GetVersionNumber());
            CheckError(E_FAIL);
        }
        CStringA versions;versions.Format("revision-before=%ld revision-after=%ld\r\n",revision,m_doc->m_body.GetVersionNumber());
        DWORD versionBytes=0;output.Write(versions,versions.GetLength(),&versionBytes);
        passed=true;
    } catch(const _com_error& error) { CStringA code; code.Format(" hr=0x%08lX",static_cast<unsigned long>(error.Error()));details+=code; }
    catch(...) { details="native exception"; }
    CStringA report; report.Format("result=%s\r\ndetails=%s\r\n",passed?"PASS":"FAIL",details.GetString());
    DWORD written=0;output.Write(report,report.GetLength(),&written);output.Close();::PostQuitMessage(passed?0:1);return 0;
}
