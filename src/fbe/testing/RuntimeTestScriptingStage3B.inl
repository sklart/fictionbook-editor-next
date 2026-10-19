if (IsFbeTestScenario(L"scripting-stage3b"))
{
    wchar_t root[32768] = {}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", root, _countof(root));
    const CString script = CString(root) + L"\\stage3b.js";
    _Settings.SetScriptsFolder(root);
    bool passed = false; CStringA details = "start";
    auto invoke = [&](LPCWSTR name) {
        CComVariant result;
        CheckError(m_doc->InvokeFunc(name, nullptr, 0, result, true));
        if (result.vt == VT_BSTR) details = CW2A(result.bstrVal, CP_UTF8);
        if (result.vt != VT_BSTR || wcscmp(result.bstrVal, L"PASS")) CheckError(E_FAIL);
    };
    try {
        FbeScriptDiagnostics::ScopedDialogSuppression suppressDialogs;
        m_doc->RunScript(script); invoke(L"resultBinary");
        for (const auto view : {BODY, DESC}) {
            ShowView(view);
            FbeScripts::ScriptExecutionScope context(script);
            const bool dirty = DocChanged();
            const auto revision = m_doc->m_body.GetVersionNumber();
            const auto before = m_doc->CaptureUndoSavepoints();
            const CString xml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
            invoke(L"readBinary");
            const auto after = m_doc->CaptureUndoSavepoints();
            if (dirty != DocChanged() || revision != m_doc->m_body.GetVersionNumber() || xml != CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) ||
                before.savedVersion != after.savedVersion || before.savedHtml != after.savedHtml || before.recoveryVersion != after.recoveryVersion || before.recoveryHtml != after.recoveryHtml) {
                details = "reading changed document or savepoints"; CheckError(E_FAIL);
            }
        }
        {
            const CString path = CString(root) + L"\\export\\_png";
            CHandle locked(::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (locked == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
            FbeScripts::ScriptExecutionScope context(script);
            invoke(L"lockedBinary");
        }
        ShowView(SOURCE);
        const auto source = FbeScripts::SourceBytes(m_source);
        const auto caret = m_source.SendMessage(SCI_GETCURRENTPOS), anchor = m_source.SendMessage(SCI_GETANCHOR);
        { FbeScripts::ScriptExecutionScope context(script); invoke(L"sourceBinary"); }
        if (source != FbeScripts::SourceBytes(m_source) || caret != m_source.SendMessage(SCI_GETCURRENTPOS) || anchor != m_source.SendMessage(SCI_GETANCHOR) || !IsSourceActive()) CheckError(E_FAIL);
        passed = true;
    } catch (const _com_error& error) {
        CStringA code; code.Format("; hr=0x%08lX", static_cast<unsigned long>(error.Error())); details += code;
    } catch (...) { details = "native exception"; }
    CStringA report; report.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString());
    DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
    ::PostQuitMessage(passed ? 0 : 1); return 0;
}

