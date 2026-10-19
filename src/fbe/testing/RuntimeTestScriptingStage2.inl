if (IsFbeTestScenario(L"scripting-stage2"))
{
    wchar_t root[32768] = {}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", root, _countof(root));
    _Settings.SetScriptsFolder(root);
    bool passed = false; CStringA details;
    auto invoke = [&](LPCWSTR function, LPCWSTR expected) {
        CComVariant result; const HRESULT hr = m_doc->InvokeFunc(function, NULL, 0, result, true);
        if (result.vt == VT_BSTR) details = CW2A(result.bstrVal, CP_UTF8);
        return SUCCEEDED(hr) && result.vt == VT_BSTR && wcscmp(result.bstrVal, expected) == 0;
    };
    try {
        FbeScriptDiagnostics::ScopedDialogSuppression suppressDialogs;
        ::ShowWindow(m_hWnd, SW_SHOWNORMAL); ::SetForegroundWindow(m_hWnd);
        ShowView(BODY); m_doc->m_body.SetFocus();
        m_doc->RunScript(CString(root) + L"\\stage2.js");
        passed = invoke(L"stage2Result", L"PASS") && ::IsWindowEnabled(m_hWnd) && FbeScripts::ExecutionContexts().empty() && FbeScripts::RedrawStates().empty();
        if (passed) {
            SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
            passed = invoke(L"stage2Paragraph", L"selected words");
            SendMessage(WM_COMMAND, ID_EDIT_REDO, 0);
            passed = passed && invoke(L"stage2Paragraph", L"batch two");
        }
        if (passed) {
            ShowView(SOURCE);
            FbeScripts::ScriptExecutionScope context(CString(root) + L"\\stage2.js");
            passed = invoke(L"stage2Source", L"PASS");
            const char whitespace[] = " ";
            if (passed) {
                m_source.SendMessage(SCI_APPENDTEXT, 1, reinterpret_cast<LPARAM>(whitespace));
                m_source.SendMessage(SCI_APPENDTEXT, 1, reinterpret_cast<LPARAM>(whitespace));
                passed = invoke(L"stage2SourceEnd", L"PASS");
            }
            m_source.SendMessage(SCI_UNDO);
            passed = passed && invoke(L"stage2SourceUndone", L"PASS");
            m_source.SendMessage(SCI_REDO);
            passed = passed && invoke(L"stage2SourceRedone", L"PASS");
        }
        if (passed) {
            ShowView(BODY);
            FbeScripts::ScriptExecutionScope context(CString(root) + L"\\stage2.js");
            passed = invoke(L"stage2Replaced", L"PASS");
        }
        if (passed) {
            FbeScripts::ScriptExecutionScope context(CString(root) + L"\\stage2.js");
            passed = invoke(L"stage2ProgressBegin", L"PASS");
            if (passed) {
                bool cancelled = false;
                for (int attempt = 0; attempt < 1500 && !cancelled; ++attempt) {
                    struct Probe { HWND owner; CAtlFile* output; bool describe; } probe = { m_hWnd, &output, attempt == 500 };
                    ::EnumWindows([](HWND window, LPARAM data) -> BOOL {
                        auto* probe = reinterpret_cast<Probe*>(data);
                        DWORD process = 0; ::GetWindowThreadProcessId(window, &process);
                        wchar_t title[128] = {}; ::GetWindowTextW(window, title, _countof(title));
                        if (wcscmp(title, L"FBE stage2 cancellation") == 0) {
                            if (probe->describe) {
                                CStringA row; row.Format("progress_pid=%lu owner=%p expected=%p cancel=%p\r\n", process, ::GetAncestor(window, GA_ROOTOWNER), probe->owner, ::GetDlgItem(window, IDCANCEL));
                                DWORD written = 0; probe->output->Write(row, row.GetLength(), &written); probe->output->Flush();
                            }
                            if (::GetAncestor(window, GA_ROOTOWNER) != probe->owner) return TRUE;
                            HWND cancel = ::GetDlgItem(window, IDCANCEL);
                            if (cancel) ::PostMessageW(window, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), reinterpret_cast<LPARAM>(cancel));
                            else ::PostMessageW(window, WM_CLOSE, 0, 0);
                        }
                        return TRUE;
                    }, reinterpret_cast<LPARAM>(&probe));
                    cancelled = invoke(L"stage2ProgressCancelled", L"PASS");
                    if (!cancelled) ::Sleep(10);
                }
                passed = cancelled && invoke(L"stage2ProgressEnd", L"PASS");
            }
        }
        passed = passed && FbeScripts::ExecutionContexts().empty() && FbeScripts::RedrawStates().empty();
    } catch (const _com_error& error) { passed = false; details.Format("HRESULT=0x%08lX", static_cast<unsigned long>(error.Error())); }
    catch (...) { passed = false; details = "native exception"; }
    try {
        CComVariant temporary;
        if (SUCCEEDED(m_doc->InvokeFunc(L"stage2TemporaryFile", NULL, 0, temporary, true)) && temporary.vt == VT_BSTR) {
            wchar_t directory[32768] = {}; const DWORD size = ::GetTempPathW(_countof(directory), directory);
            const CString path(temporary.bstrVal);
            if (size && size < _countof(directory) && path.Left(size).CompareNoCase(directory) == 0 && path.Find(L"fbe-{") == static_cast<int>(size)) ::DeleteFileW(path);
        }
    } catch (const _com_error&) { passed = false; }
    CStringA report; report.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString());
    DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
    ::PostQuitMessage(passed ? 0 : 1); return 0;
}
