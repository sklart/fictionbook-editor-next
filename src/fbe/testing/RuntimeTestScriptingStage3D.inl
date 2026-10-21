if (IsFbeTestScenario(L"scripting-stage3d")) {
    bool passed = false;CStringA details = "start";
    try {
        std::vector<wchar_t> root(32768);::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", root.data(), static_cast<DWORD>(root.size()));
        const CString script = CString(root.data()) + L"\\stage3d.js";_Settings.SetScriptsFolder(root.data());
        FbeScriptDiagnostics::ScopedDialogSuppression suppress;
        m_doc->RunScript(script);
        {
            CComPtr<IDispatch> external;CheckError(m_doc->m_body.Document()->parentWindow->get_external(&external));
            const wchar_t* methods[] = {L"SaveCurrentDocument",L"SaveCurrentDocumentAs",L"SaveDocumentCopy",L"SerializeCurrentDocument",L"SaveSerializedDocumentCopy"};
            for (DISPID i = 0; i < 5; ++i) {LPOLESTR name = const_cast<LPOLESTR>(methods[i]);DISPID id = DISPID_UNKNOWN;
                CheckError(external->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &id));if (id != 164+i) CheckError(E_FAIL);}
            DISPPARAMS parameters = {};CComVariant result;
            if (external->Invoke(167, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &parameters, &result, nullptr, nullptr) != E_UNEXPECTED) CheckError(E_FAIL);
        }
        FbeScripts::ScriptExecutionScope context(script);
        auto invoke = [&](LPCWSTR name) {
            details = CW2A(name, CP_UTF8);
            CStringA phase = CStringA("phase=") + details + "\r\n";DWORD phaseWritten = 0;
            output.Write(phase, phase.GetLength(), &phaseWritten);output.Flush();
            CComVariant result;CheckError(m_doc->InvokeFunc(name, nullptr, 0, result, true));
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
        const auto initialLocation = m_document_session.Location();
        MSHTML::IHTMLElementPtr editable(m_doc->m_body.Document()->all->item(L"probe"));
        if (editable) editable->innerText = L"UNSAVED_BODY";
        MSHTML::IHTMLInputElementPtr titleControl(m_doc->m_body.Document()->all->item(L"tiTitle"));
        if (initialLocation.containerKind != DocumentContainerKind::Rar) titleControl->value = L"API3D Edited";
        const CString alias = CString(root.data()) + L"\\original-alias" + (m_doc->GetDocumentFileType() == FictionBookFileType::Fbd ? L".fbd" : L".fb2");
        if (!::CreateHardLinkW(alias, initialLocation.storagePath, NULL)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        const auto version = m_doc->m_body.GetVersionNumber();const bool dirty = DocChanged();
        const auto savepoints = m_doc->CaptureUndoSavepoints();
        std::vector<wchar_t> workingDirectory(32768);::GetCurrentDirectoryW(static_cast<DWORD>(workingDirectory.size()), workingDirectory.data());
        const CString html = static_cast<LPCWSTR>(m_doc->m_body.Document()->body->outerHTML);
        invoke(L"blockedUndo3d");
        const auto undo = history(false), redo = history(true);
        const bool named = m_doc->m_namevalid;
        m_doc->m_namevalid = false;
        try {invoke(L"saveFailure3d");} catch (...) {m_doc->m_namevalid = named;throw;}
        m_doc->m_namevalid = named;
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
        const auto copySavepoints = m_doc->CaptureUndoSavepoints();
        std::vector<wchar_t> afterDirectory(32768);::GetCurrentDirectoryW(static_cast<DWORD>(afterDirectory.size()), afterDirectory.data());
        if (wcscmp(workingDirectory.data(), afterDirectory.data()) || savepoints.savedVersion != copySavepoints.savedVersion ||
            savepoints.recoveryVersion != copySavepoints.recoveryVersion || savepoints.savedHtml != copySavepoints.savedHtml ||
            savepoints.recoveryHtml != copySavepoints.recoveryHtml || m_document_session.Location().storagePath != initialLocation.storagePath ||
            m_document_session.Location().entryPath != initialLocation.entryPath) {details = "copy changed checkpoints/location/CWD";CheckError(E_FAIL);}
        if (editable) {
            ::SetEnvironmentVariableW(L"FBE_NEXT_FAULT_INJECT", L"drop-serialized-row-during-analysis");
            try {invoke(L"snapshotFailure3d");} catch (...) {::SetEnvironmentVariableW(L"FBE_NEXT_FAULT_INJECT", nullptr);throw;}
            ::SetEnvironmentVariableW(L"FBE_NEXT_FAULT_INJECT", nullptr);
            if (m_doc->IsSerializationUnsafe() || version != m_doc->m_body.GetVersionNumber() || html != CString(static_cast<LPCWSTR>(m_doc->m_body.Document()->body->outerHTML))) CheckError(E_FAIL);
        }
        ShowView(DESC);
        invoke(L"prepareDescription3d");
        const bool descriptionDirty = DocChanged();const HWND descriptionFocus = ::GetFocus();
        const auto descriptionUndo = history(false), descriptionRedo = history(true);
        const auto descriptionVersion = m_doc->m_body.GetVersionNumber();
        invoke(L"description3d");
        if (descriptionUndo != history(false) || descriptionRedo != history(true) || descriptionVersion != m_doc->m_body.GetVersionNumber() ||
            descriptionDirty != DocChanged() || descriptionFocus != ::GetFocus()) CheckError(E_FAIL);
        ShowView(SOURCE);
        const std::string source = FbeScripts::SourceBytes(m_source);
        std::string changed = source;
        size_t title = changed.find("<book-title>");
        if (title == std::string::npos) CheckError(E_FAIL);title += strlen("<book-title>");
        const size_t titleEnd = changed.find("</book-title>", title);
        if (titleEnd == std::string::npos) CheckError(E_FAIL);changed.replace(title, titleEnd-title, "SourceCurrent");
        m_source.SendMessage(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(changed.c_str()));
        m_source.SendMessage(SCI_SETSEL, 13, 23);
        invoke(L"source3d");
        if (!IsSourceActive() || FbeScripts::SourceBytes(m_source) != changed || m_source.SendMessage(SCI_GETANCHOR) != 13 ||
            m_source.SendMessage(SCI_GETCURRENTPOS) != 23 || !m_source.SendMessage(SCI_GETMODIFY)) {details = "Source changed during export";CheckError(E_FAIL);}
        m_source.SendMessage(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("<invalid>"));
        invoke(L"invalidSource3d");
        if (FbeScripts::SourceBytes(m_source) != "<invalid>") CheckError(E_FAIL);
        std::string badBinary = source;
        const size_t binary = badBinary.find("<binary");
        if (binary != std::string::npos) {
            const size_t content = badBinary.find('>', binary);
            if (content == std::string::npos) CheckError(E_FAIL);
            badBinary.insert(content + 1, "!");
            m_source.SendMessage(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(badBinary.c_str()));
            invoke(L"invalidSource3d");
        }
        if (initialLocation.containerKind == DocumentContainerKind::None && m_doc->GetDocumentFileType() == FictionBookFileType::Fb2) {
            std::string large = source;const size_t paragraph = large.find("ordinary paragraph");
            if (paragraph == std::string::npos) CheckError(E_FAIL);
            large.replace(paragraph, strlen("ordinary paragraph"), 4U * 1024 * 1024, 'x');
            m_source.SendMessage(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(large.c_str()));invoke(L"large3d");
            if (FbeScripts::SourceBytes(m_source) != large || !IsSourceActive()) CheckError(E_FAIL);
        }
        m_source.SendMessage(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(source.c_str()));
        m_source.SendMessage(SCI_SETSAVEPOINT);ShowView(BODY);
        const auto beforeSaveUndo = history(false), beforeSaveRedo = history(true);
        invoke(L"saves3d");
        if (DocChanged() || m_doc->m_filename.Find(L"saveas.") < 0 ||
            m_document_session.Location().storagePath != m_doc->m_filename ||
            beforeSaveUndo != history(false) || beforeSaveRedo != history(true)) {details = "Save state/history";CheckError(E_FAIL);}
        const CString savedPath = m_doc->m_filename;
        HANDLE locked = ::CreateFileW(savedPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (locked == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        try {invoke(L"saveFailure3d");} catch (...) {::CloseHandle(locked);throw;}
        ::CloseHandle(locked);
        if (m_doc->m_filename != savedPath || beforeSaveUndo != history(false) || beforeSaveRedo != history(true)) CheckError(E_FAIL);
        const CString target = CString(root.data()) + L"\\locked-target" + (m_doc->GetDocumentFileType() == FictionBookFileType::Fbd ? L".fbd" : L".fb2");
        HANDLE sentinel = ::CreateFileW(target, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (sentinel == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        DWORD count = 0;const char marker[] = "sentinel";
        if (!::WriteFile(sentinel, marker, sizeof(marker), &count, NULL)) {::CloseHandle(sentinel);CheckError(E_FAIL);}
        try {invoke(L"replaceFailure3d");} catch (...) {::CloseHandle(sentinel);throw;}
        ::SetFilePointer(sentinel, 0, NULL, FILE_BEGIN);char kept[sizeof(marker)] = {};
        const BOOL read = ::ReadFile(sentinel, kept, sizeof(kept), &count, NULL);::CloseHandle(sentinel);
        if (!read || count != sizeof(marker) || memcmp(kept, marker, sizeof(marker))) CheckError(E_FAIL);
        // A real external modification must be rejected, with editor history intact.
        HANDLE external = ::CreateFileW(savedPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (external == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        const char change[] = "<!-- external -->";
        const wchar_t wideChange[] = L"<!-- external -->";
        const bool utf16 = m_doc->m_encoding.CompareNoCase(L"utf-16") == 0;
        const BOOL appended = ::WriteFile(external, utf16 ? static_cast<const void*>(wideChange) : static_cast<const void*>(change),
            static_cast<DWORD>(utf16 ? sizeof(wideChange)-sizeof(wchar_t) : sizeof(change)-1), &count, NULL);::CloseHandle(external);
        if (!appended) CheckError(E_FAIL);invoke(L"saveFailure3d");
        if (m_doc->m_filename != savedPath || beforeSaveUndo != history(false) || beforeSaveRedo != history(true)) CheckError(E_FAIL);
        // Reopen the independent copy in FBE after all original-state checks.
        details = "copy reopen/binary data";
        const CString copyPath = CString(root.data()) + L"\\copy" +
            (m_doc->GetDocumentFileType() == FictionBookFileType::Fbd ? L".fbd" : L".fb2");
        auto stored = U::CreateDocument(false);
        if (stored->load(_variant_t(copyPath)) != VARIANT_TRUE) CheckError(E_FAIL);
        auto payloads = [](MSXML2::IXMLDOMDocument2Ptr dom) {
            std::vector<std::pair<std::wstring, std::vector<std::uint8_t>>> result;
            auto nodes = dom->selectNodes(L"//*[local-name()='binary']");
            for (long i = 0; i < nodes->length; ++i) {
                std::vector<std::uint8_t> bytes;
                if (!FbeFb2Binary::DecodeBase64(static_cast<LPCWSTR>(nodes->item[i]->text), bytes)) CheckError(E_FAIL);
                result.emplace_back(static_cast<LPCWSTR>(nodes->item[i]->attributes->getNamedItem(L"id")->text), std::move(bytes));
            }
            std::sort(result.begin(), result.end());return result;
        };
        auto references = [](MSXML2::IXMLDOMDocument2Ptr dom) {
            std::vector<std::wstring> result;
            auto nodes = dom->selectNodes(L"//*[local-name()='image']/@*[local-name()='href']");
            for (long i = 0; i < nodes->length; ++i) result.emplace_back(static_cast<LPCWSTR>(nodes->item[i]->text));
            std::sort(result.begin(), result.end());return result;
        };
        const auto expectedPayloads = payloads(stored);const auto expectedReferences = references(stored);
        if (LoadFile(copyPath) != OK) CheckError(E_FAIL);
        CString snapshotError;auto reopened = m_doc->CreateScriptingSnapshot(m_doc->m_encoding, snapshotError);
        if (!reopened || payloads(reopened) != expectedPayloads || references(reopened) != expectedReferences) CheckError(E_FAIL);
        passed = true;details = "read-only snapshots, copies, current Source and invalid Source PASS";
    } catch (const _com_error& error) {CStringA code;code.Format(" hr=0x%08lX", static_cast<unsigned long>(error.Error()));details += code;}
    catch (...) {details = "native exception";}
    CStringA report;report.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString());
    DWORD written = 0;output.Write(report, report.GetLength(), &written);output.Close();::PostQuitMessage(passed ? 0 : 1);return 0;
}
