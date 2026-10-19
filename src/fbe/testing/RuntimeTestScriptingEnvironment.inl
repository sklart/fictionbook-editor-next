if (IsFbeTestScenario(L"scripting-environment"))
{
    wchar_t root[32768] = {}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", root, _countof(root));
    _Settings.SetScriptsFolder(root);
    bool passed = true; CStringA details;
    try {
        FbeScriptDiagnostics::ScopedDialogSuppression suppressDialogs;
        m_doc->RunScript(CString(root) + L"\\outer.js");
        CComVariant setup;
        passed = SUCCEEDED(m_doc->InvokeFunc(L"apiStage1Results", NULL, 0, setup, true)) &&
            setup.vt == VT_BSTR && wcscmp(setup.bstrVal, L"PASS") == 0;
        if (!passed) {
            if (setup.vt == VT_BSTR) details = CW2A(setup.bstrVal, CP_UTF8);
            CStringA error;
            error.Format("; settings_error=0x%08lX; settings_operation=%s", static_cast<unsigned long>(FbeScripts::SettingsStore::LastFailure()),
                static_cast<LPCSTR>(CW2A(FbeScripts::SettingsStore::LastFailureOperation(), CP_UTF8)));
            details += error;
        }
        auto check = [&](LPCWSTR expected) {
            CComVariant result;
            const bool correct = SUCCEEDED(m_doc->InvokeFunc(L"apiStage1EnvironmentValue", NULL, 0, result, true)) &&
                result.vt == VT_BSTR && wcscmp(result.bstrVal, expected) == 0;
            if (!correct) { details = "environment expected "; details += CW2A(expected, CP_UTF8); if (result.vt == VT_BSTR) { details += " actual "; details += CW2A(result.bstrVal, CP_UTF8); } }
            return correct;
        };
        auto pump = []() {
            const ULONGLONG until = ::GetTickCount64() + 100;
            while (::GetTickCount64() < until) { MSG message = {}; while (::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessageW(&message); } ::Sleep(1); }
        };
        auto paragraph = [&]() {
            MSHTML::IHTMLDocument3Ptr document(m_doc->m_body.Document());
            MSHTML::IHTMLElement2Ptr body(document->getElementById(L"fbw_body"));
            auto paragraphs = body->getElementsByTagName(L"P");
            return MSHTML::IHTMLElementPtr(paragraphs->item(_variant_t(0L), _variant_t()));
        };
        ::ShowWindow(m_hWnd, SW_SHOWNORMAL); ::SetForegroundWindow(m_hWnd);
        ShowView(BODY); m_doc->m_body.SetFocus();
        paragraph()->innerText = L"alpha beta"; pump(); m_doc->MarkSavePoint();
        if (passed) passed = check(L"design:false");
        const wchar_t* files[] = {
            L"Scripts\\01_Регистр\\01_В верхний регистр.js",
            L"Scripts\\01_Регистр\\02_В нижний регистр.js",
            L"Scripts\\01_Регистр\\05_Капитализация выделения.js"
        };
        const wchar_t* expected[] = { L"ALPHA BETA", L"alpha beta", L"Alpha Beta" };
        for (int index = 0; passed && index < 3; ++index) {
            MSHTML::IHTMLBodyElementPtr body(m_doc->m_body.Document()->body);
            auto selection = body->createTextRange(); selection->moveToElementText(paragraph()); selection->select();
            m_doc->RunScript(U::GetProgDirFile(files[index])); pump();
            _bstr_t text(paragraph()->innerText);
            passed = wcscmp(static_cast<LPCWSTR>(text), expected[index]) == 0 && FbeScripts::ExecutionContexts().empty() && check(L"design:true");
            if (passed && index == 0) {
                SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
                _bstr_t undone(paragraph()->innerText); passed = wcscmp(static_cast<LPCWSTR>(undone), L"alpha beta") == 0;
                SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
                _bstr_t redone(paragraph()->innerText); passed = passed && wcscmp(static_cast<LPCWSTR>(redone), L"ALPHA BETA") == 0;
            }
            if (!passed && details.IsEmpty()) details.Format("legacy script/undo/redo %d", index);
        }
        if (passed) {
            ShowView(DESC);
            MSHTML::IHTMLDocument3Ptr document(m_doc->m_body.Document());
            MSHTML::IHTMLInputElementPtr title(document->getElementById(L"tiTitle"));
            MSHTML::IHTMLElement2Ptr element(title); element->focus(); pump();
            _bstr_t before(title->value); m_doc->MarkSavePoint();
            passed = check(L"description:false");
            title->value = L"API description edit"; pump();
            passed = passed && check(L"description:true");
            title->value = before; m_doc->MarkSavePoint();
            ShowView(SOURCE); passed = passed && check(L"source:false");
            const char whitespace[] = " "; m_source.SendMessage(SCI_APPENDTEXT, 1, reinterpret_cast<LPARAM>(whitespace));
            passed = passed && check(L"source:true");
            m_source.SendMessage(SCI_UNDO); passed = passed && check(L"source:false");
            ShowView(BODY); passed = passed && check(L"design:false");
        }
    } catch (const _com_error& error) { passed = false; details.Format("HRESULT=0x%08lX", static_cast<unsigned long>(error.Error())); }
    CStringA report; report.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString());
    DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
    ::PostQuitMessage(passed ? 0 : 1); return 0;
}
