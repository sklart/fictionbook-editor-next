if (IsFbeTestScenario(L"scripting-stage2"))
{
    wchar_t root[32768] = {}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", root, _countof(root));
    _Settings.SetScriptsFolder(root);
    bool passed = false; CStringA details;
    struct RedrawProbe {
        HWND window; unsigned disabled = 0, enabled = 0;
        static LRESULT CALLBACK Observe(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR data) {
            auto* probe = reinterpret_cast<RedrawProbe*>(data);
            if (message == WM_SETREDRAW) { if (wparam) ++probe->enabled; else ++probe->disabled; }
            return ::DefSubclassProc(window, message, wparam, lparam);
        }
        explicit RedrawProbe(HWND target) : window(target) {
            if (!::SetWindowSubclass(window, Observe, 46, reinterpret_cast<DWORD_PTR>(this))) CheckError(E_FAIL);
        }
        ~RedrawProbe() { ::RemoveWindowSubclass(window, Observe, 46); }
        bool Balanced() const { return disabled == 1 && enabled == 1 && ::IsWindowVisible(window); }
    };
    auto invoke = [&](LPCWSTR function, LPCWSTR expected) {
        CComVariant result; const HRESULT hr = m_doc->InvokeFunc(function, NULL, 0, result, true);
        if (result.vt == VT_BSTR) details = CW2A(result.bstrVal, CP_UTF8);
        return SUCCEEDED(hr) && result.vt == VT_BSTR && wcscmp(result.bstrVal, expected) == 0;
    };
    try {
        FbeScriptDiagnostics::ScopedDialogSuppression suppressDialogs;
        ::ShowWindow(m_hWnd, SW_SHOWNORMAL); ::SetForegroundWindow(m_hWnd);
        ShowView(BODY); m_doc->m_body.SetFocus();
        {
            const CString destination = CString(root) + L"\\native-commit-destination.bin";
            const CString temporary = CString(root) + L"\\native-commit-source.bin";
            auto writeByte = [](LPCWSTR path, char value) {
                CHandle file(::CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL));
                DWORD written = 0;
                if (file == INVALID_HANDLE_VALUE || !::WriteFile(file, &value, 1, &written, NULL) || written != 1) CheckError(STG_E_WRITEFAULT);
                if (!::FlushFileBuffers(file)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
            };
            writeByte(destination, 'a'); writeByte(temporary, 'b');
            CHandle locked(::CreateFileW(destination, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (locked == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
            unsigned denied = 0;
            CHandle attempted(::CreateEventW(NULL, TRUE, FALSE, NULL));
            if (!attempted) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
            std::thread release([&]() { ::WaitForSingleObject(attempted, 1000); ::Sleep(25); locked.Close(); });
            try {
                FbeScripts::RetryScriptFileCommit([&]() {
                    if (!::MoveFileExW(temporary, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                        const DWORD error = ::GetLastError(); ++denied; ::SetEvent(attempted); CheckError(HRESULT_FROM_WIN32(error));
                    }
                });
            } catch (...) { ::SetEvent(attempted); release.join(); throw; }
            ::SetEvent(attempted);
            release.join();
            CHandle saved(::CreateFileW(destination, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            char value = 0; DWORD read = 0;
            if (!denied || saved == INVALID_HANDLE_VALUE || !::ReadFile(saved, &value, 1, &read, NULL) || read != 1 || value != 'b' ||
                ::GetFileAttributesW(temporary) != INVALID_FILE_ATTRIBUTES) CheckError(E_UNEXPECTED);
        }
        {
            CComPtr<IDispatch> external;
            CheckError(m_doc->m_body.Document()->parentWindow->get_external(&external));
            CComQIPtr<IExternalHelper> legacy(external);
            CComBSTR uuid;
            if (!legacy || FAILED(legacy->GetUUID(&uuid)) || !uuid.Length()) CheckError(E_NOINTERFACE);
            const struct { LPCWSTR name; DISPID id; } contracts[] = {
                { L"GetUUID", 8 }, { L"GetScriptingApiVersion", 100 }, { L"CreateSelectionBookmark", 119 },
                { L"BeginDocumentUpdate", 125 }, { L"ReadTextFile", 131 }, { L"WriteTextFile", 132 },
                { L"GetTempFileName", 141 }, { L"WriteTextFileEx", 142 }
            };
            for (const auto& contract : contracts) {
                LPOLESTR name = const_cast<LPOLESTR>(contract.name); DISPID id = DISPID_UNKNOWN;
                CheckError(external->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &id));
                if (id != contract.id) CheckError(E_UNEXPECTED);
            }
        }
        {
            RedrawProbe redraw(m_doc->m_body);
            m_doc->RunScript(CString(root) + L"\\stage2.js");
            if (!redraw.Balanced()) CheckError(E_UNEXPECTED);
        }
        passed = invoke(L"stage2Result", L"PASS") && ::IsWindowEnabled(m_hWnd) && FbeScripts::ExecutionContexts().empty() && FbeScripts::RedrawStates().empty();
        if (passed) {
            SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
            passed = invoke(L"stage2Paragraph", L"selected words");
            SendMessage(WM_COMMAND, ID_EDIT_REDO, 0);
            passed = passed && invoke(L"stage2Paragraph", L"batch two");
        }
        if (passed) {
            ShowView(DESC);
            FbeScripts::ScriptExecutionScope context(CString(root) + L"\\stage2.js");
            RedrawProbe redraw(m_doc->m_body);
            passed = invoke(L"stage2Description", L"PASS");
            SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
            passed = passed && invoke(L"stage2DescriptionUndo", L"PASS");
            SendMessage(WM_COMMAND, ID_EDIT_REDO, 0);
            passed = passed && invoke(L"stage2DescriptionRedo", L"PASS") && FbeScripts::RedrawStates().empty() && redraw.Balanced();
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
            if (passed) {
                const auto start = m_source.SendMessage(SCI_GETLENGTH);
                std::string large(4 * 1024 * 1024, ' ');
                const char unicode[] = "\xd0\xaf\xf0\x9f\x98\x80<&";
                large += unicode;
                m_source.SendMessage(SCI_BEGINUNDOACTION);
                m_source.SendMessage(SCI_APPENDTEXT, large.size(), reinterpret_cast<LPARAM>(large.data()));
                m_source.SendMessage(SCI_ENDUNDOACTION);
                m_source.SendMessage(SCI_SETSEL, start + static_cast<sptr_t>(large.size()), start + 4 * 1024 * 1024);
                struct Reads { unsigned full = 0, ranges = 0; } reads;
                const auto observer = [](HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR data) -> LRESULT {
                    auto* state = reinterpret_cast<Reads*>(data);
                    if (message == SCI_GETTEXT) ++state->full;
                    if (message == SCI_GETTEXTRANGE) ++state->ranges;
                    return ::DefSubclassProc(window, message, wparam, lparam);
                };
                if (!::SetWindowSubclass(m_source, observer, 45, reinterpret_cast<DWORD_PTR>(&reads))) CheckError(E_FAIL);
                passed = invoke(L"stage2SourceSelection", L"PASS");
                ::RemoveWindowSubclass(m_source, observer, 45);
                passed = passed && reads.full == 0 && reads.ranges == 3;
                if (!passed && reads.full) details = "selection copied full XML";
                passed = passed && invoke(L"stage2SourceMark", L"PASS");
                m_source.SendMessage(SCI_SETSEL, 0, 0);
                passed = passed && invoke(L"stage2SourceRestore", L"PASS");
                passed = passed && m_source.SendMessage(SCI_GETANCHOR) == start + static_cast<sptr_t>(large.size()) &&
                    m_source.SendMessage(SCI_GETCURRENTPOS) == start + 4 * 1024 * 1024;
                const char space[] = " ";
                m_source.SendMessage(SCI_INSERTTEXT, 0, reinterpret_cast<LPARAM>(space));
                passed = passed && invoke(L"stage2SourceInvalidated", L"PASS");
                m_source.SendMessage(SCI_UNDO);
                m_source.SendMessage(SCI_UNDO);
            }
        }
        if (passed) {
            ShowView(BODY);
            FbeScripts::ScriptExecutionScope context(CString(root) + L"\\stage2.js");
            passed = invoke(L"stage2Replaced", L"PASS");
        }
        if (passed) {
            if (!InitializeScripts()) CheckError(E_FAIL);
            UINT command = 0;
            for (int index = 0; index < m_scripts.Menu().Count(); ++index)
                if (m_scripts.Menu().Item(index).relativePath.CompareNoCase(L"entry.js") == 0)
                    command = ID_SCRIPT_BASE + m_scripts.Menu().Item(index).commandId;
            if (!command) CheckError(E_FAIL);
            SendMessage(WM_COMMAND, command, 0);
            passed = invoke(L"stage2EntryCount", L"1");
            ACCEL binding = { FVIRTKEY, VK_F2, ID_LAST_SCRIPT };
            const HACCEL accelerator = ::CreateAcceleratorTableW(&binding, 1);
            if (!accelerator) CheckError(E_OUTOFMEMORY);
            MSG key = {}; key.hwnd = m_doc->m_body; key.message = WM_KEYDOWN; key.wParam = VK_F2; key.lParam = 1;
            const bool translated = ::TranslateAcceleratorW(m_hWnd, accelerator, &key) != 0;
            ::DestroyAcceleratorTable(accelerator);
            passed = passed && translated && invoke(L"stage2EntryCount", L"2") && FbeScripts::ExecutionContexts().empty();
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
        for (int index = 0; index < 8; ++index) {
            CComVariant temporary;
            if (FAILED(m_doc->InvokeFunc(L"stage2TemporaryFile", NULL, 0, temporary, true)) || temporary.vt != VT_BSTR || !::SysStringLen(temporary.bstrVal)) break;
            wchar_t directory[32768] = {}; const DWORD size = ::GetTempPathW(_countof(directory), directory);
            const CString path(temporary.bstrVal);
            if (size && size < _countof(directory) && path.Left(size).CompareNoCase(directory) == 0 && path.Mid(size).Find(L'\\') < 0) ::DeleteFileW(path);
        }
    } catch (const _com_error&) { passed = false; }
    CStringA report; report.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString());
    DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
    ::PostQuitMessage(passed ? 0 : 1); return 0;
}
