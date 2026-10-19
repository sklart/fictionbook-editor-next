if (IsFbeTestScenario(L"scripting-stage3a") || IsFbeTestScenario(L"scripting-stage3a-missing-schema"))
{
    wchar_t root[32768] = {}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", root, _countof(root));
    const CString script = CString(root) + L"\\stage3.js";
    _Settings.SetScriptsFolder(root);
    bool passed = false; CStringA details = "start";
    auto invoke = [&](LPCWSTR name, const VARIANT* argument = nullptr) {
        CComVariant result, input;
        if (argument) input = *argument;
        const HRESULT error = m_doc->InvokeFunc(name, argument ? &input : nullptr, argument ? 1 : 0, result, true);
        if (result.vt == VT_BSTR) details = CW2A(result.bstrVal, CP_UTF8);
        if (FAILED(error) || result.vt != VT_BSTR || wcscmp(result.bstrVal, L"PASS") != 0) CheckError(E_FAIL);
    };
    try {
        FbeScriptDiagnostics::ScopedDialogSuppression suppressDialogs;
        ::ShowWindow(m_hWnd, SW_SHOWNORMAL); ::SetForegroundWindow(m_hWnd);
        m_doc->RunScript(script);
        invoke(L"resultStage3");
        if (!IsFbeTestScenario(L"scripting-stage3a-missing-schema")) {
        {
            CComPtr<IDispatch> external;
            CheckError(m_doc->m_body.Document()->parentWindow->get_external(&external));
            const wchar_t* methods[] = { L"ValidateCurrentDocument", L"LogMessage", L"GetLastScriptingError", L"ClearLastScriptingError" };
            for (DISPID index = 0; index < 4; ++index) {
                LPOLESTR name = const_cast<LPOLESTR>(methods[index]); DISPID id = DISPID_UNKNOWN;
                CheckError(external->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &id));
                if (id != 143 + index) CheckError(E_FAIL);
            }
            FbeScripts::ScriptExecutionScope context(script);
            CComVariant option(L"{}"); DISPID named = 1; DISPPARAMS parameters = { &option, &named, 1, 1 };
            if (external->Invoke(143, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &parameters, nullptr, nullptr, nullptr) != DISP_E_NONAMEDARGS) CheckError(E_FAIL);
            parameters = { nullptr, nullptr, 0, 0 };
            if (external->Invoke(145, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &parameters, nullptr, nullptr, nullptr) != DISP_E_MEMBERNOTFOUND) CheckError(E_FAIL);
        }
        auto check = [&](bool valid) {
            FbeScripts::ScriptExecutionScope context(script);
            const auto view = m_editor_view_state.Current();
            const bool modified = DocChanged();
            const auto before = m_doc->CaptureUndoSavepoints();
            const auto revision = m_doc->m_body.GetVersionNumber();
            const std::string source = FbeScripts::SourceBytes(m_source);
            const auto anchor = m_source.SendMessage(SCI_GETANCHOR), caret = m_source.SendMessage(SCI_GETCURRENTPOS);
            const auto undo = m_source.SendMessage(SCI_CANUNDO), redo = m_source.SendMessage(SCI_CANREDO), dirty = m_source.SendMessage(SCI_GETMODIFY);
            CComVariant expected(valid);
            invoke(L"checkStage3", &expected);
            const auto after = m_doc->CaptureUndoSavepoints();
            if (view != m_editor_view_state.Current() || modified != DocChanged() || revision != m_doc->m_body.GetVersionNumber() ||
                source != FbeScripts::SourceBytes(m_source) || anchor != m_source.SendMessage(SCI_GETANCHOR) || caret != m_source.SendMessage(SCI_GETCURRENTPOS) ||
                undo != m_source.SendMessage(SCI_CANUNDO) || redo != m_source.SendMessage(SCI_CANREDO) || dirty != m_source.SendMessage(SCI_GETMODIFY) ||
                before.savedVersion != after.savedVersion || before.recoveryVersion != after.recoveryVersion || before.savedHtml != after.savedHtml ||
                before.recoveryHtml != after.recoveryHtml || before.hasSaved != after.hasSaved || before.hasRecovery != after.hasRecovery ||
                before.reconcile != after.reconcile || before.clean != after.clean || before.recoveryClean != after.recoveryClean) {
                details = "validation changed editor state"; CheckError(E_FAIL);
            }
        };
        ShowView(BODY); check(true);
        ShowView(DESC); check(true);
        {
            FbeScripts::ScriptExecutionScope context(script);
            invoke(L"editDescription3");
        }
        check(true);
        SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
        invoke(L"undoneDescription3"); check(true);
        SendMessage(WM_COMMAND, ID_EDIT_REDO, 0);
        invoke(L"redoneDescription3"); check(true);
        ShowView(SOURCE);
        check(true);
        const std::string original = FbeScripts::SourceBytes(m_source);
        const char broken[] = "<broken";
        m_source.SendMessage(SCI_APPENDTEXT, sizeof(broken) - 1, reinterpret_cast<LPARAM>(broken));
        m_source.SendMessage(SCI_SETSEL, 9, 3);
        check(false);
        { FbeScripts::ScriptExecutionScope context(script); invoke(L"positionStage3"); }
        m_source.SendMessage(SCI_UNDO); check(true);
        m_source.SendMessage(SCI_REDO); check(false);
        m_source.SendMessage(SCI_UNDO);
        if (FbeScripts::SourceBytes(m_source) != original) CheckError(E_FAIL);
        const char invalidUtf8[] = { static_cast<char>(0xff), 0 };
        m_source.SendMessage(SCI_APPENDTEXT, 1, reinterpret_cast<LPARAM>(invalidUtf8));
        check(false); m_source.SendMessage(SCI_UNDO); check(true);
        if (m_doc->GetDocumentFileType() == FictionBookFileType::Fb2) {
            const size_t at = original.rfind("</FictionBook>");
            if (at == std::string::npos) CheckError(E_FAIL);
            const char schemaError[] = "<invalid-3a>quote &quot; slash \\ Unicode Я</invalid-3a>";
            m_source.SendMessage(SCI_INSERTTEXT, at, reinterpret_cast<LPARAM>(schemaError));
            check(false);
            m_source.SendMessage(SCI_UNDO); check(true);
        }
        {
            FbeScripts::ScriptExecutionScope context(script);
            FB::Doc* saved = m_doc; m_doc = nullptr;
            try {
                CComVariant result;
                const HRESULT error = saved->InvokeFunc(L"missingStage3", nullptr, 0, result, true);
                m_doc = saved;
                if (FAILED(error) || result.vt != VT_BSTR || wcscmp(result.bstrVal, L"PASS") != 0) CheckError(E_FAIL);
            } catch (...) { m_doc = saved; throw; }
        }
        {
            FbeScripts::ScriptExecutionScope context(script);
            invoke(L"cleanStage3");
        }
        ShowView(BODY);
        DWORD handlesBefore = 0, handlesMiddle = 0, handlesAfter = 0;
        invoke(L"collectStage3");
        ::GetProcessHandleCount(::GetCurrentProcess(), &handlesBefore);
        for (int repeat = 0; repeat < 24; ++repeat) check(true);
        invoke(L"collectStage3");
        ::GetProcessHandleCount(::GetCurrentProcess(), &handlesMiddle);
        for (int repeat = 0; repeat < 24; ++repeat) check(true);
        invoke(L"collectStage3");
        ::GetProcessHandleCount(::GetCurrentProcess(), &handlesAfter);
        CStringA handles; handles.Format("handles=%lu,%lu,%lu\r\n", handlesBefore, handlesMiddle, handlesAfter);
        DWORD countWritten = 0; output.Write(handles, static_cast<DWORD>(handles.GetLength()), &countWritten); output.Flush();
        if (handlesMiddle > handlesBefore + 4 && handlesAfter > handlesMiddle + 4) { details = "validation leaked handles across both batches"; CheckError(E_FAIL); }
        if (StartupTrace::Enabled()) {
            StartupTrace::Flush();
            CString package, error;
            if (!StartupTrace::CreateDiagnosticPackage(package, error)) { details = CW2A(error, CP_UTF8); CheckError(E_FAIL); }
            ::DeleteFileW(package);
        }
        }
        passed = FbeScripts::ExecutionContexts().empty() && FbeScripts::RedrawStates().empty();
    } catch (const _com_error& error) {
        CStringA code; code.Format("; hr=0x%08lX", static_cast<unsigned long>(error.Error())); details += code;
    } catch (...) { details = "native exception"; }
    CStringA report; report.Format("result=%s\r\ntrace=%d\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", StartupTrace::Enabled(), details.GetString());
    DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
    ::PostQuitMessage(passed ? 0 : 1); return 0;
}
