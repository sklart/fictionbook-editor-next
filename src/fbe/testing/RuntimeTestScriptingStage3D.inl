if (IsFbeTestScenario(L"scripting-stage3d")) {
    bool passed = false;CStringA details = "start";
    try {
        wchar_t root[32768] = {};::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", root, _countof(root));
        const CString script = CString(root) + L"\\stage3d.js";_Settings.SetScriptsFolder(root);
        FbeScriptDiagnostics::ScopedDialogSuppression suppress;
        m_doc->RunScript(script);
        FbeScripts::ScriptExecutionScope context(script);
        auto invoke = [&](LPCWSTR name) {
            details = CW2A(name, CP_UTF8);CComVariant result;CheckError(m_doc->InvokeFunc(name, nullptr, 0, result, true));
            if (result.vt != VT_BSTR || wcscmp(result.bstrVal, L"PASS")) {
                if (result.vt == VT_BSTR) details += CStringA(": ") + CW2A(result.bstrVal, CP_UTF8);
                CheckError(E_FAIL);
            }
        };
        CComPtr<IOleUndoManager> manager;IServiceProviderPtr services(m_doc->m_body.Document());
        CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager)));
        auto history = [&](bool redo) {
            CComPtr<IEnumOleUndoUnits> units;CheckError(redo ? manager->EnumRedoable(&units) : manager->EnumUndoable(&units));
            std::vector<CComPtr<IOleUndoUnit>> result;
            for (;;) {CComPtr<IOleUndoUnit> unit;HRESULT hr = units->Next(1, &unit, nullptr);if (hr == S_FALSE) break;CheckError(hr);result.push_back(unit);}return result;
        };
        const CString path = m_doc->m_filename, encoding = m_doc->m_encoding;
        const auto version = m_doc->m_body.GetVersionNumber();const bool dirty = DocChanged();
        const CString html = static_cast<LPCWSTR>(m_doc->m_body.Document()->body->outerHTML);
        const auto undo = history(false), redo = history(true);
        invoke(L"copies3d");
        if (path != m_doc->m_filename || encoding != m_doc->m_encoding || dirty != DocChanged() ||
            version != m_doc->m_body.GetVersionNumber() || html != CString(static_cast<LPCWSTR>(m_doc->m_body.Document()->body->outerHTML)) ||
            undo != history(false) || redo != history(true)) {
            details.Format("copy state: path=%d encoding=%d dirty=%d version=%d html=%d undo=%d redo=%d",
                path == m_doc->m_filename, encoding == m_doc->m_encoding, dirty == DocChanged(), version == m_doc->m_body.GetVersionNumber(),
                html == CString(static_cast<LPCWSTR>(m_doc->m_body.Document()->body->outerHTML)), undo == history(false), redo == history(true));
            const CString after = static_cast<LPCWSTR>(m_doc->m_body.Document()->body->outerHTML);
            int difference = 0;while (difference < min(html.GetLength(), after.GetLength()) && html[difference] == after[difference]) ++difference;
            CStringA position;position.Format(" first-difference=%d before-length=%d after-length=%d", difference, html.GetLength(), after.GetLength());details += position;
            // Synthetic fixture only; report an attribute name, never book text.
            if (after.Mid(difference, 8) == L"selected") details += " implicit-selected-attribute";
            if (after.Mid(difference, 5) == L"value") details += " implicit-value-attribute";
            CheckError(E_FAIL);
        }
        invoke(L"blocked3d");
        ShowView(SOURCE);
        const std::string source = FbeScripts::SourceBytes(m_source);
        std::string changed = source;const size_t title = changed.find("API3D");
        if (title == std::string::npos) CheckError(E_FAIL);changed.replace(title, 5, "SourceCurrent");
        m_source.SendMessage(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(changed.c_str()));
        m_source.SendMessage(SCI_SETSEL, 13, 23);
        invoke(L"source3d");
        if (!IsSourceActive() || FbeScripts::SourceBytes(m_source) != changed || m_source.SendMessage(SCI_GETANCHOR) != 13 ||
            m_source.SendMessage(SCI_GETCURRENTPOS) != 23 || !m_source.SendMessage(SCI_GETMODIFY)) {details = "Source changed during export";CheckError(E_FAIL);}
        m_source.SendMessage(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("<invalid>"));
        invoke(L"invalidSource3d");
        if (FbeScripts::SourceBytes(m_source) != "<invalid>") CheckError(E_FAIL);
        passed = true;details = "read-only snapshots, copies, current Source and invalid Source PASS";
    } catch (const _com_error& error) {CStringA code;code.Format(" hr=0x%08lX", static_cast<unsigned long>(error.Error()));details += code;}
    catch (...) {details = "native exception";}
    CStringA report;report.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString());
    DWORD written = 0;output.Write(report, report.GetLength(), &written);output.Close();::PostQuitMessage(passed ? 0 : 1);return 0;
}
