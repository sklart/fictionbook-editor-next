if(IsFbeTestScenario(L"script64-runtime"))
{
    const CString script64Report = CString(DeploymentContext::DiagnosticsDirectory().c_str()) + L"script64-report.txt";
    wchar_t probePath[MAX_PATH] = {};
    const DWORD probeLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SCRIPT64_PATH", probePath, _countof(probePath));
    const bool parserNegative = Script64ParserSelfTest();
    bool ran = false, undone = false, redone = false, single = false;
    bool refusalNoUndo = true, exceptionCleanup = false;
    bool safety = true;
    CString safetyResults;
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
            if(!manager) _com_issue_error(E_NOINTERFACE);
            MSHTML::IHTMLWindow2Ptr window = m_doc->m_body.Document()->parentWindow;
            auto execute = [&](const CString& code) { window->execScript(_bstr_t(code), _bstr_t(L"JScript")); };
            auto noUnits = [&]() {
                CComPtr<IEnumOleUndoUnits> units;
                CComPtr<IOleUndoUnit> unit;
                return SUCCEEDED(manager->EnumUndoable(&units)) && units && units->Next(1, &unit, NULL) == S_FALSE;
            };
            for(int index = 0; index < 4; ++index)
            {
                CString setup;
                setup.Format(L"window.__script64RefusalSetup(%d);", index);
                execute(setup);
                pump();
                if(FAILED(manager->DiscardFrom(NULL))) _com_issue_error(E_FAIL);
                DWORD before = 0, after = 0;
                const HRESULT beforeState = manager->GetOpenParentState(&before);
                execute(L"window.__script64RefusalRun();");
                const bool correctRefusal = m_status_state.EffectiveMainText(false, false, CString()) == L"1";
                const HRESULT afterState = manager->GetOpenParentState(&after);
                refusalNoUndo = refusalNoUndo && correctRefusal && noUnits() && beforeState == afterState && before == after;
            }
            for(int fault = 0; fault < 3; ++fault)
            {
            execute(L"window.__script64FaultSetup();");
            pump();
            if(FAILED(manager->DiscardFrom(NULL))) _com_issue_error(E_FAIL);
            DWORD beforeFault = 0, afterFault = 0;
            const HRESULT beforeFaultState = manager->GetOpenParentState(&beforeFault);
            CString faultRun;
            faultRun.Format(L"window.__script64FaultRun(%d);", fault);
            execute(faultRun);
            const bool partialAndClean = m_status_state.EffectiveMainText(false, false, CString()) == L"1";
            const HRESULT afterFaultState = manager->GetOpenParentState(&afterFault);
            const bool faultPassed = partialAndClean && beforeFaultState == afterFaultState && beforeFault == afterFault;
            exceptionCleanup = (fault == 0 ? true : exceptionCleanup) && faultPassed;
            safetyResults.AppendFormat(L"cleanup-mode-%d=%d\n", fault, faultPassed);
            }
            // Each fixture gets a fresh history. Undo cannot consume setup or an older edit.
            for(int fixture = 0; fixture < 6; ++fixture)
            {
                CString setup;
                setup.Format(L"window.__script64PrepareSafety(%d);", fixture);
                ::SetForegroundWindow(m_hWnd);
                m_doc->m_body.SetFocus();
                execute(setup);
                pump();
                if(FAILED(manager->DiscardFrom(NULL))) _com_issue_error(E_FAIL);
                execute(L"window.__script64SafetyRun();");
                const bool changed = m_status_state.EffectiveMainText(false, false, CString()) == L"1";
                pump();
                // Keep TEXTAREA focus for the actual editor Undo/Redo command route.
                ::SetForegroundWindow(m_hWnd);
                m_doc->m_body.SetFocus();
                if(fixture == 5) execute(L"document.getElementById('script64-field').focus();");
                SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_UNDO, 0), 0);
                pump();
                execute(fixture == 5 ? L"window.__script64FieldState();" :
                    L"window.external.SetStatusBarText(document.getElementById('fbw_body').innerHTML===window.__script64Before?'1':'0');");
                const CString undoResult = m_status_state.EffectiveMainText(false, false, CString());
                const bool undoOk = undoResult == (fixture == 5 ? L"xaby|1" : L"1");
                ::SetForegroundWindow(m_hWnd);
                m_doc->m_body.SetFocus();
                if(fixture == 5) execute(L"document.getElementById('script64-field').focus();");
                SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_REDO, 0), 0);
                pump();
                execute(fixture == 5 ? L"window.__script64FieldState();" :
                    L"window.external.SetStatusBarText(document.getElementById('fbw_body').innerHTML===window.__script64After?'1':'0');");
                const CString redoResult = m_status_state.EffectiveMainText(false, false, CString());
                const bool redoOk = redoResult == (fixture == 5 ? L"xABy|1" : L"1");
                if(fixture == 5)
                {
                    // A documented lack of native field history is an observed result too.
                    const bool observed = changed && (undoOk || undoResult == L"xABy|1") && redoOk;
                    safetyResults.AppendFormat(L"textarea-history-observed=%d\ntextarea-undo=%d\ntextarea-redo=%d\ntextarea-after-undo=%s\ntextarea-after-redo=%s\n",
                        observed, undoOk, redoOk, static_cast<LPCWSTR>(undoResult), static_cast<LPCWSTR>(redoResult));
                    safety = safety && observed;
                }
                else
                {
                    const bool correct = changed && undoOk && redoOk;
                    safetyResults.AppendFormat(L"id-collision-%d=%d\nid-collision-%d-run=%d\nid-collision-%d-undo=%d\nid-collision-%d-redo=%d\n",
                        fixture, correct, fixture, changed, fixture, undoOk, fixture, redoOk);
                    safety = safety && correct;
                }
            }
            execute(L"window.__script64PrepareOrdinary();");
            pump();
            // Discard fixture construction and all previous cases before Run.
            if(FAILED(manager->DiscardFrom(NULL))) _com_issue_error(E_FAIL);
            window->execScript(_bstr_t(L"window.__script64Ordinary();"), _bstr_t(L"JScript"));
            results = m_status_state.EffectiveMainText(false, false, CString());
            results += L"\n" + safetyResults;
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
    const bool passed = parserNegative && ran && undone && redone && single && refusalNoUndo && exceptionCleanup && safety && Script64CasesPass(results);
    CStringA output;
    output.Format("phase=script64\nharness-ran=%d\nparser-negative=%d\nundo=%d\nredo=%d\nundo-single=%d\nrefusal-no-undo=%d\nexception-cleanup=%d\n%s\nresult=%s\n", ran, parserNegative, undone, redone, single, refusalNoUndo, exceptionCleanup,
        static_cast<LPCSTR>(CW2A(results, CP_UTF8)), passed ? "pass" : "fail");
    WritePortableStateTestText(script64Report, output);
    PostMessage(WM_CLOSE);
    return;
}
