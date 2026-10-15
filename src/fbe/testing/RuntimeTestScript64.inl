if(IsFbeTestScenario(L"script64-runtime"))
{
    const CString script64Report = CString(DeploymentContext::DiagnosticsDirectory().c_str()) + L"script64-report.txt";
    wchar_t probePath[MAX_PATH] = {};
    const DWORD probeLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SCRIPT64_PATH", probePath, _countof(probePath));
    const bool parserNegative = Script64ParserSelfTest();
    bool ran = false, undone = false, redone = false, single = false;
    CString results;
    try
    {
        if(m_doc && probeLength > 0 && probeLength < _countof(probePath) &&
            DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable)
        {
            auto pump = []() {
                const ULONGLONG until = ::GetTickCount64() + 100;
                while(::GetTickCount64() < until)
                {
                    MSG message = {};
                    while(::PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
                    {
                        ::TranslateMessage(&message);
                        ::DispatchMessage(&message);
                    }
                    ::Sleep(1);
                }
            };
            ::ShowWindow(m_hWnd, SW_SHOWNORMAL);
            ::SetForegroundWindow(m_hWnd);
            m_doc->m_body.SetFocus();
            m_doc->RunScript(probePath);
            pump();
            IServiceProviderPtr service(m_doc->m_body.Document());
            CComPtr<IOleUndoManager> manager;
            if(service) service->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager));
            // Discard fixture construction and all previous cases before Run.
            if(!manager || FAILED(manager->DiscardFrom(NULL))) _com_issue_error(E_FAIL);
            MSHTML::IHTMLWindow2Ptr window = m_doc->m_body.Document()->parentWindow;
            window->execScript(_bstr_t(L"window.__script64Ordinary();"), _bstr_t(L"JScript"));
            results = m_status_state.EffectiveMainText(false, false, CString());
            ran = Script64ResultIs(results, L"ordinary", L"1");
            pump();
            BSTR description = NULL;
            const HRESULT undoState = manager->GetLastUndoDescription(&description);
            const bool named = SUCCEEDED(undoState) && description && CString(description) == L"Замена символов по списку из буфера обмена";
            results.AppendFormat(L"\nundo-state=0x%08lx\nundo-description=%s\n", static_cast<unsigned long>(undoState), description ? description : L"");
            if(description) ::SysFreeString(description);
            CComPtr<IEnumOleUndoUnits> units;
            if(SUCCEEDED(manager->EnumUndoable(&units)) && units)
            {
                CComPtr<IOleUndoUnit> first, second;
                single = named && units->Next(1, &first, NULL) == S_OK && units->Next(1, &second, NULL) == S_FALSE;
            }
            auto htmlMatches = [&](const wchar_t* snapshot) {
                CString code;
                code.Format(L"window.external.SetStatusBarText(document.getElementById('fbw_body').innerHTML===window.%s?'1':'0');", snapshot);
                window->execScript(_bstr_t(code), _bstr_t(L"JScript"));
                return m_status_state.EffectiveMainText(false, false, CString()) == L"1";
            };
            m_doc->m_body.SetFocus();
            SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_UNDO, 0), 0);
            pump();
            undone = single && htmlMatches(L"__script64Before");
            // No older operation is allowed to supply a successful Undo.
            CComPtr<IEnumOleUndoUnits> remaining;
            CComPtr<IOleUndoUnit> previous;
            single = single && SUCCEEDED(manager->EnumUndoable(&remaining)) && remaining && remaining->Next(1, &previous, NULL) == S_FALSE;
            m_doc->m_body.SetFocus();
            SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_REDO, 0), 0);
            pump();
            redone = single && htmlMatches(L"__script64After");
        }
    }
    catch(const _com_error& error)
    {
        results.AppendFormat(L"\nnative-error=0x%08lx\n", static_cast<unsigned long>(error.Error()));
    }
    const bool passed = parserNegative && ran && undone && redone && single && Script64CasesPass(results);
    CStringA output;
    output.Format("phase=script64\nharness-ran=%d\nparser-negative=%d\nundo=%d\nredo=%d\nundo-single=%d\n%s\nresult=%s\n", ran, parserNegative, undone, redone, single,
        static_cast<LPCSTR>(CW2A(results, CP_UTF8)), passed ? "pass" : "fail");
    WritePortableStateTestText(script64Report, output);
    PostMessage(WM_CLOSE);
    return;
}
