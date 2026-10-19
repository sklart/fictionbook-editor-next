if (IsFbeTestScenario(L"scripting-dialogs"))
{
    wchar_t root[32768] = {}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", root, _countof(root));
    _Settings.SetScriptsFolder(root);
    const bool modified = DocChanged(); const auto view = m_editor_view_state.Current();
    bool passed = true; CStringA details;
    try {
        FbeScriptDiagnostics::ScopedDialogSuppression suppressDialogs;
        m_doc->RunScript(CString(root) + L"\\outer.js");
        CComVariant state;
        passed = SUCCEEDED(m_doc->InvokeFunc(L"apiStage1Results", NULL, 0, state, true)) &&
            state.vt == VT_BSTR && wcscmp(state.bstrVal, L"PASS") == 0;
        if (state.vt == VT_BSTR) details = CW2A(state.bstrVal, CP_UTF8);
        if (!passed) {
            CStringA error;
            error.Format("; settings_error=0x%08lX; settings_operation=%s", static_cast<unsigned long>(FbeScripts::SettingsStore::LastFailure()),
                static_cast<LPCSTR>(CW2A(FbeScripts::SettingsStore::LastFailureOperation(), CP_UTF8)));
            details += error;
        }
        for (int index = 0; passed && index < 7; ++index) {
            CString caption; caption.Format(L"FBE API45 %lu case %d", ::GetCurrentProcessId(), index);
            bool found = false, owned = false, focused = false, positioned = false;
            std::thread driver([&]() {
                HWND dialog = NULL, button = NULL;
                const UINT buttonId = index == 4 || index == 6 ? IDCANCEL : (index == 5 ? IDYES : IDOK);
                for (int retry = 0; retry < 600 && !(button && focused); ++retry) {
                    dialog = ::FindWindowW(L"#32770", caption);
                    DWORD process = 0; if (dialog) ::GetWindowThreadProcessId(dialog, &process);
                    if (process == ::GetCurrentProcessId()) {
                        HWND candidate = ::GetDlgItem(dialog, buttonId);
                        if (candidate && ::IsWindowVisible(candidate) && ::IsWindowEnabled(candidate)) button = candidate;
                        GUITHREADINFO gui = {}; gui.cbSize = sizeof(gui);
                        if (::GetGUIThreadInfo(::GetWindowThreadProcessId(dialog, NULL), &gui))
                            focused = gui.hwndFocus && (gui.hwndFocus == dialog || ::IsChild(dialog, gui.hwndFocus));
                    }
                    if (!(button && focused)) ::Sleep(10);
                }
                if (!dialog || !button) { if (dialog) ::PostMessageW(dialog, WM_CLOSE, 0, 0); return; }
                found = true;
                owned = ::GetAncestor(dialog, GA_ROOTOWNER) == m_hWnd && !::IsWindowEnabled(m_hWnd);
                if (index == 5) ::SetWindowTextW(::GetDlgItem(dialog, IDC_INPUT), L"");
                positioned = ::MonitorFromWindow(dialog, MONITOR_DEFAULTTONEAREST) == ::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
                ::SendMessageW(button, BM_CLICK, 0, 0);
            });
            CComVariant parameters[2]; parameters[1] = index; parameters[0] = static_cast<LPCWSTR>(caption);
            CComVariant result;
            const HRESULT hr = m_doc->InvokeFunc(L"apiStage1DialogCase", parameters, 2, result, true);
            driver.join();
            const bool correct = SUCCEEDED(hr) && result.vt == VT_BSTR && wcscmp(result.bstrVal, L"PASS") == 0;
            if (result.vt == VT_BSTR) details = CW2A(result.bstrVal, CP_UTF8);
            passed = correct && found && owned && focused && positioned;
            if (index == 2) passed = passed && ::GetFileAttributesW(CString(root) + L"\\новый результат.txt") == INVALID_FILE_ATTRIBUTES;
            CStringA row; row.Format("case=%d correct=%d found=%d owner=%d focus=%d monitor=%d\r\n", index, correct, found, owned, focused, positioned);
            DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
        }
        passed = passed && DocChanged() == modified && m_editor_view_state.Current() == view && FbeScripts::ExecutionContexts().empty();
    } catch (...) { passed = false; details = "native exception"; }
    CStringA report; report.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString());
    DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
    ::PostQuitMessage(passed ? 0 : 1); return 0;
}
