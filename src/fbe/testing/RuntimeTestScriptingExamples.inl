// Executes the handbook scripts with production document/file/settings APIs.
// Only interactive responses are scripted; Clipboard storage is isolated.
if (IsFbeTestScenario(L"scripting-doc-examples")) {
    bool passed = false;CStringA details = "start";
    try {
        std::vector<wchar_t> root(32768);
        ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", root.data(), static_cast<DWORD>(root.size()));
        const CString script = CString(root.data()) + L"\\examples.js";
        _Settings.SetScriptsFolder(root.data());
        FbeScriptDiagnostics::ScopedDialogSuppression suppress;
        FbeScripts::Clipboard::TestStorage clipboard;
        struct RestoreClipboardStorage {
            FbeScripts::Clipboard::TestStorage* previous;
            ~RestoreClipboardStorage() { FbeScripts::Clipboard::testStorage = previous; }
        } restoreClipboard = {FbeScripts::Clipboard::testStorage};
        FbeScripts::Clipboard::testStorage = &clipboard;
        m_doc->RunScript(script);
        CComPtr<IOleUndoManager> manager;IServiceProviderPtr services(m_doc->m_body.Document());
        CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager)));
        auto history = [&](bool redo) {
            CComPtr<IEnumOleUndoUnits> units;CheckError(redo ? manager->EnumRedoable(&units) : manager->EnumUndoable(&units));
            std::vector<CComPtr<IOleUndoUnit>> result;
            for (;;) {CComPtr<IOleUndoUnit> unit;const HRESULT hr = units->Next(1, &unit, nullptr);
                if (hr == S_FALSE) break;CheckError(hr);result.push_back(unit);}return result;
        };
        auto invoke = [&](LPCWSTR name, bool changes = false) {
            details = CW2A(name, CP_UTF8);
            FbeScripts::ScriptExecutionScope context(script);
            const auto undo = history(false), redo = history(true);
            const bool dirty = DocChanged();const auto version = m_doc->m_body.GetVersionNumber();
            const CString html = static_cast<LPCWSTR>(m_doc->m_body.Document()->body->outerHTML);
            const auto source = FbeScripts::SourceBytes(m_source);
            const LRESULT anchor = m_source.SendMessage(SCI_GETANCHOR), caret = m_source.SendMessage(SCI_GETCURRENTPOS);
            CComVariant result;CheckError(m_doc->InvokeFunc(name, nullptr, 0, result, true));
            if (result.vt != VT_BSTR || wcscmp(result.bstrVal, L"PASS")) {
                if (result.vt == VT_BSTR) details += CStringA(": ") + CW2A(result.bstrVal, CP_UTF8);
                CheckError(E_FAIL);
            }
            if (FbeScripts::Status::Active(m_hWnd) || !FbeScripts::RedrawStates().empty()) CheckError(E_FAIL);
            if (changes) {
                if (history(false).size() != undo.size() + 1) {details += ": expected one Undo";CheckError(E_FAIL);}
                MSHTML::IHTMLElementPtr probe(m_doc->m_body.Document()->all->item(L"probe"));
                const CString changed = static_cast<LPCWSTR>(probe->outerHTML);
                CheckError(manager->UndoTo(nullptr));
                if (changed == CString(static_cast<LPCWSTR>(probe->outerHTML))) CheckError(E_FAIL);
                CheckError(manager->RedoTo(nullptr));
                if (changed != CString(static_cast<LPCWSTR>(probe->outerHTML))) CheckError(E_FAIL);
                CheckError(manager->UndoTo(nullptr));
            } else if (undo != history(false) || redo != history(true) || dirty != DocChanged() ||
                version != m_doc->m_body.GetVersionNumber() || html != CString(static_cast<LPCWSTR>(m_doc->m_body.Document()->body->outerHTML)) ||
                source != FbeScripts::SourceBytes(m_source) || anchor != m_source.SendMessage(SCI_GETANCHOR) ||
                caret != m_source.SendMessage(SCI_GETCURRENTPOS)) {details += ": read-only state changed";CheckError(E_FAIL);}
            CStringA row = details + "=PASS\r\n";DWORD written = 0;
            output.Write(row, row.GetLength(), &written);output.Flush();
        };
        invoke(L"backupCancel");invoke(L"backupConflict");invoke(L"backupSuccess", true);
        invoke(L"preferencesCancel");invoke(L"preferencesSuccess", true);invoke(L"preferencesReopen");
        invoke(L"binaryCancel");invoke(L"binaryMissing");invoke(L"binarySuccess");
        invoke(L"prepareSelection");
        invoke(L"clipboardCancel");invoke(L"clipboardSuccess");
        clipboard.failure = E_ACCESSDENIED;invoke(L"clipboardFailure");clipboard.failure = S_OK;
        ShowView(SOURCE);
        std::string source = FbeScripts::SourceBytes(m_source);
        const size_t title = source.find("ExampleBook");if (title == std::string::npos) CheckError(E_FAIL);
        source.replace(title, strlen("ExampleBook"), "UnsavedExample");
        m_source.SendMessage(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(source.c_str()));m_source.SendMessage(SCI_SETSEL, 13, 23);
        invoke(L"sourceCancel");invoke(L"sourceConflict");invoke(L"sourceSuccess");
        m_source.SendMessage(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("<invalid>"));invoke(L"sourceInvalid");
        passed = true;details = "E-K examples: production APIs, scripted UI, isolated Clipboard, Undo/Redo and refusal paths";
    } catch (const _com_error& error) {CStringA code;code.Format(" hr=0x%08lX", static_cast<unsigned long>(error.Error()));details += code;}
    catch (...) {details += ": native exception";}
    CStringA report;report.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString());
    DWORD written = 0;output.Write(report, report.GetLength(), &written);output.Close();::PostQuitMessage(passed ? 0 : 1);return 0;
}
