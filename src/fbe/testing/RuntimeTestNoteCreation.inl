if(IsFbeTestScenario(L"note-creation-runtime"))
{
    const CString reportPath = CString(DeploymentContext::DiagnosticsDirectory().c_str()) + L"note-creation-report.txt";
    CString results;
    bool passed = true;
    int completed = 0;
    wchar_t stageBuffer[MAX_PATH] = {};
    const DWORD stageLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_NOTES_STAGE_PATH", stageBuffer, _countof(stageBuffer));
    const CString stagePath = stageLength > 0 && stageLength < _countof(stageBuffer) ? CString(stageBuffer) : reportPath + L".stage";
    CString currentScenario = L"<startup>";
    CString currentStage = L"initialize";
    auto markStage = [&](LPCWSTR stage) {
        currentStage = stage;
        CStringA scenarioUtf8(CW2A(currentScenario, CP_UTF8));
        CStringA entry;
        entry.Format("scenario=%s\nstage=%s\ntick=%llu\n", static_cast<LPCSTR>(scenarioUtf8),
            static_cast<LPCSTR>(CW2A(currentStage, CP_UTF8)), ::GetTickCount64());
        if(!WritePortableStateTestText(stagePath, entry)) _com_issue_error(E_FAIL);
    };
    try
    {
        markStage(L"initialize");
        wchar_t path[MAX_PATH] = {};
        const DWORD length = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_NOTES_PATH", path, _countof(path));
        if(!m_doc || length == 0 || length >= _countof(path) || DeploymentContext::CurrentMode() != DeploymentContext::Mode::Portable)
            _com_issue_error(E_INVALIDARG);
        auto pump = []() {
            const ULONGLONG until = ::GetTickCount64() + 30;
            do {
                MSG message = {};
                while(::PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessage(&message); }
                ::Sleep(1);
            } while(::GetTickCount64() < until);
        };
        ::ShowWindow(m_hWnd, SW_SHOWNORMAL);
        ::SetForegroundWindow(m_hWnd);
        m_doc->m_body.SetFocus();
        markStage(L"load-script");
        m_doc->RunScript(path);
        MSHTML::IHTMLWindow2Ptr window = m_doc->m_body.Document()->parentWindow;
        auto execute = [&](const CString& code) {
            window->execScript(_bstr_t(code), _bstr_t(L"JScript"));
            return m_status_state.EffectiveMainText(false, false, CString());
        };
        markStage(L"count-cases");
        const int count = _wtoi(execute(L"window.__notesCount();"));
        if(count < 1 || count > 160) _com_issue_error(E_INVALIDARG);
        IServiceProviderPtr service(m_doc->m_body.Document());
        CComPtr<IOleUndoManager> manager;
        if(service) service->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager));
        if(!manager) _com_issue_error(E_NOINTERFACE);
        auto unitCount = [&]() {
            CComPtr<IEnumOleUndoUnits> units;
            if(FAILED(manager->EnumUndoable(&units)) || !units) return -1;
            int total = 0;
            for(;;)
            {
                CComPtr<IOleUndoUnit> unit;
                const HRESULT next = units->Next(1, &unit, NULL);
                if(next == S_FALSE) return total;
                if(next != S_OK) return -1;
                ++total;
            }
        };
        auto firstIdentity = [&]() {
            CComPtr<IEnumOleUndoUnits> units;
            CComPtr<IOleUndoUnit> unit;
            CComPtr<IUnknown> identity;
            if(SUCCEEDED(manager->EnumUndoable(&units)) && units && units->Next(1, &unit, NULL) == S_OK && unit)
                unit->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity));
            return identity;
        };
        for(int index = 0; index < count; ++index)
        {
            CString caseCode; caseCode.Format(L"window.__notesCaseName(%d);", index);
            currentScenario = execute(caseCode);
            if(currentScenario.IsEmpty()) _com_issue_error(E_FAIL);
            markStage(L"setup");
            CString setup; setup.Format(L"window.__notesSetup(%d);", index);
            ::SetForegroundWindow(m_hWnd);
            m_doc->m_body.SetFocus();
            const bool refusal = execute(setup) == L"refusal";
            pump();
            markStage(L"reset-history");
            if(FAILED(manager->DiscardFrom(NULL))) _com_issue_error(E_FAIL);
            CComPtr<IUnknown> sentinel;
            if(refusal)
            {
                markStage(L"seed-history");
                execute(L"window.__notesSeedHistory();");
                sentinel = firstIdentity();
                if(!sentinel || unitCount() != 1) _com_issue_error(E_FAIL);
            }
            DWORD before = 0, after = 0;
            const HRESULT beforeState = manager->GetOpenParentState(&before);
            markStage(L"run");
            const bool behavior = execute(L"window.__notesRun();") == L"1";
            const HRESULT afterState = manager->GetOpenParentState(&after);
            const bool closed = beforeState == afterState && before == after;
            const CString name = execute(L"window.__notesName();");
            const CString detail = execute(L"window.__notesDetails();");
            bool undo = false, redo = false;
            if(refusal) undo = redo = unitCount() == 1 && firstIdentity().p == sentinel.p;
            else
            {
                const bool single = unitCount() == 1;
                m_doc->m_body.SetFocus();
                markStage(L"undo");
                SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_UNDO, 0), 0); pump();
                undo = single && execute(L"window.__notesSnapshot('before');") == L"1" && unitCount() == 0;
                m_doc->m_body.SetFocus();
                markStage(L"redo");
                SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_REDO, 0), 0); pump();
                redo = single && execute(L"window.__notesSnapshot('after');") == L"1";
            }
            markStage(L"record-result");
            const bool correct = behavior && closed && undo && redo;
            results.AppendFormat(L"%s=%d\n%s-behavior=%d\n%s-closed=%d\n%s-undo=%d\n%s-redo=%d\n%s-detail=%s\n",
                static_cast<LPCWSTR>(name), correct, static_cast<LPCWSTR>(name), behavior,
                static_cast<LPCWSTR>(name), closed, static_cast<LPCWSTR>(name), undo,
                static_cast<LPCWSTR>(name), redo, static_cast<LPCWSTR>(name), static_cast<LPCWSTR>(detail));
            passed = passed && correct;
            ++completed;
        }
    }
    catch(const _com_error& error) {
        passed = false;
        results.AppendFormat(L"failed-scenario=%s\nfailed-stage=%s\nnative-error=0x%08lx\n",
            static_cast<LPCWSTR>(currentScenario), static_cast<LPCWSTR>(currentStage), static_cast<unsigned long>(error.Error()));
    }
    CStringA output;
    output.Format("fixtures=%d\n%sresult=%s\n", completed, static_cast<LPCSTR>(CW2A(results, CP_UTF8)), passed && completed > 0 ? "pass" : "fail");
    WritePortableStateTestText(reportPath, output);
    PostMessage(WM_CLOSE);
    return;
}
