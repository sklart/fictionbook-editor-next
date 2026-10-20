if (IsFbeTestScenario(L"scripting-stage3c-dialogs")) {
    bool passed=false; CStringA details="start";
    try {
        wchar_t root[32768]={}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT",root,_countof(root));
        const CString script=CString(root)+L"\\stage3c.js"; _Settings.SetScriptsFolder(root);
        const bool dirty=DocChanged();
        FbeScriptDiagnostics::ScopedDialogSuppression suppress;
        m_doc->RunScript(script);
        auto invoke=[&](LPCWSTR name,CComVariant* args=nullptr,int count=0) {
            CComVariant result; CheckError(m_doc->InvokeFunc(name,args,count,result,true));
            if(result.vt==VT_BSTR) details=CW2A(result.bstrVal,CP_UTF8);
            if(result.vt!=VT_BSTR||wcscmp(result.bstrVal,L"PASS")) CheckError(E_FAIL);
        };
        invoke(L"stage3cResult");
        const auto revision=m_doc->m_body.GetVersionNumber();
        const CString xml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
        const InterfaceTheme initialTheme=ThemeManager::GetSelectedTheme();
        struct RestoreTheme { InterfaceTheme value; ~RestoreTheme(){ThemeManager::SetSelectedTheme(value);} } restoreTheme={initialTheme};
        FbeScripts::ScriptExecutionScope context(script);
        for(int index=0;index<8;++index) {
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
                    }
                    if(!driver.button) ::Sleep(5);
                }
                if(driver.button) ::PostMessageW(driver.dialog,WM_COMMAND,MAKEWPARAM(::GetDlgCtrlID(driver.button),BN_CLICKED),reinterpret_cast<LPARAM>(driver.button));
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
