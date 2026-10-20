if (IsFbeTestScenario(L"scripting-stage3b"))
{
    wchar_t root[32768] = {}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", root, _countof(root));
    const CString script = CString(root) + L"\\stage3b.js";
    _Settings.SetScriptsFolder(root);
    bool passed = false; CStringA details = "start", action;
    auto invoke = [&](LPCWSTR name) {
        CComVariant result;
        CheckError(m_doc->InvokeFunc(name, nullptr, 0, result, true));
        if (result.vt == VT_BSTR) details = CW2A(result.bstrVal, CP_UTF8);
        if (result.vt != VT_BSTR || wcscmp(result.bstrVal, L"PASS")) { details = action + ": " + details; CheckError(E_FAIL); }
    };
    try {
        FbeScriptDiagnostics::ScopedDialogSuppression suppressDialogs;
        m_doc->RunScript(script); invoke(L"resultBinary");
        {
            CComPtr<IDispatch> external; CheckError(m_doc->m_body.Document()->parentWindow->get_external(&external));
            const wchar_t* names[] = {L"GetBinaryInfo", L"GenerateUniqueBinaryId", L"AddBinaryFromFile", L"ReplaceBinaryFromFile", L"RemoveBinary", L"SaveAllBinaries"};
            for (DISPID index = 0; index < 6; ++index) {
                LPOLESTR name = const_cast<LPOLESTR>(names[index]); DISPID id = DISPID_UNKNOWN;
                CheckError(external->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &id));
                if (id != 147 + index) { details = "binary DISPID changed"; CheckError(E_FAIL); }
            }
        }
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
            CAtlFile original; CheckError(original.Create(path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS));
            CheckError(original.Write("KEEP", 4)); original.Close();
            CHandle locked(::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (locked == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
            FbeScripts::ScriptExecutionScope context(script);
            invoke(L"lockedBinary");
            char kept[4] = {}; DWORD read = 0;
            if (!::ReadFile(locked, kept, sizeof(kept), &read, nullptr) || read != sizeof(kept) || memcmp(kept, "KEEP", 4)) { details = "failed export lost existing file"; CheckError(E_FAIL); }
        }
        {
            const CString path = CString(root) + L"\\export\\_CON";
            if (!::DeleteFileW(path) || !::CreateDirectoryW(path, nullptr)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
            { FbeScripts::ScriptExecutionScope context(script); invoke(L"directoryBinary"); }
            if (!::RemoveDirectoryW(path)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        }
        ShowView(BODY);
        {
            FbeScripts::ScriptExecutionScope context(script);
            static std::vector<CString> reads;
            struct RestoreObserver {
                decltype(FbeScripts::Binary::readObserver) previous = FbeScripts::Binary::readObserver;
                ~RestoreObserver() { FbeScripts::Binary::readObserver = previous; }
            } restoreObserver;
            auto probe = [&](LPCWSTR name, size_t count, LPCWSTR target = nullptr) {
                reads.clear();
                FbeScripts::Binary::readObserver = [](const CString& id) { reads.push_back(id); };
                invoke(name);
                FbeScripts::Binary::readObserver = nullptr;
                if (reads.size() != count || (target && std::any_of(reads.begin(), reads.end(), [&](const auto& id) { return id != target; }))) {
                    details = "unexpected base64data reads: "; details += CW2A(name, CP_UTF8); CheckError(E_FAIL);
                }
            };
            const size_t count = FbeScripts::Binary::Objects(m_doc->m_body.Document()).size();
            probe(L"probeIdBinary", 0); probe(L"probeInfoBinary", 1, L"png"); probe(L"probeMissingBinary", 0);
            for (const auto name : {L"probeAddBinary", L"probeReplaceBinary", L"probeRemoveBinary"}) {
                probe(name, name == CString(L"probeAddBinary") ? 0U : 1U,
                    name == CString(L"probeRemoveBinary") ? L"CON" : L"png");
                CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
            }
            probe(L"probeExportBinary", count);
            std::set<std::wstring> exported;
            for (const auto& id : reads) exported.insert(std::wstring(id));
            if (exported.size() != count) { details = "export read an object more than once"; CheckError(E_FAIL); }
            probe(L"probeSkipBinary", 0); probe(L"probeFailBinary", 0);
            const auto catalog = FbeScripts::Binary::Objects(m_doc->m_body.Document());
            const auto png = std::find_if(catalog.begin(), catalog.end(), [](const auto& object) { return object.id == L"png"; });
            if (png == catalog.end()) CheckError(E_FAIL);
            FbeScripts::Binary::LoadedObject original; FbeScripts::Binary::Load(*png, original);
            CComDispatchDriver element(png->element);
            auto corrupt = [&](CComVariant& value) {
                CheckError(element.PutPropertyByName(L"base64data", &value));
                try {
                    probe(L"probeIdBinary", 0); probe(L"probeBrokenBinary", 1, L"png");
                    probe(L"probeSkipBinary", 0); probe(L"probeFailBinary", 0); probe(L"probePartialBinary", count);
                } catch (...) { element.PutPropertyByName(L"base64data", &original.data); throw; }
                CheckError(element.PutPropertyByName(L"base64data", &original.data));
            };
            CComVariant notArray(42L); corrupt(notArray);
            CComVariant matrix; matrix.vt = VT_ARRAY | VT_UI1;
            SAFEARRAYBOUND bounds[2] = {{2, 0}, {2, 0}};
            matrix.parray = ::SafeArrayCreate(VT_UI1, 2, bounds);
            if (!matrix.parray) CheckError(E_OUTOFMEMORY);
            corrupt(matrix);
        }
        {
            FbeScripts::ScriptExecutionScope context(script);
            CComPtr<IOleUndoManager> manager; IServiceProviderPtr services(m_doc->m_body.Document());
            CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager)));
            auto snapshot = [&]() { return CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)); };
            auto history = [&](bool redo) {
                CComPtr<IEnumOleUndoUnits> list; CheckError(redo ? manager->EnumRedoable(&list) : manager->EnumUndoable(&list));
                std::vector<CComPtr<IOleUndoUnit>> units;
                for (;;) { CComPtr<IOleUndoUnit> unit; const HRESULT error = list->Next(1, &unit, nullptr); if (error == S_FALSE) break; CheckError(error); units.push_back(unit); }
                return units;
            };
            const auto initial = snapshot(); const bool dirty = DocChanged();
            invoke(L"rejectedBinaries");
            if (initial != snapshot() || dirty != DocChanged()) { details = "rejected edit changed data"; CheckError(E_FAIL); }
            const auto undo = history(false), redo = history(true);
            invoke(L"sameBinary");
            if (undo != history(false) || redo != history(true) || initial != snapshot()) { details = "identical replacement changed history"; CheckError(E_FAIL); }
            invoke(L"seedBinaryHistory");
            auto cycle = [&](LPCWSTR name) {
                action = CW2A(name, CP_UTF8); action += (m_editor_view_state.Current() == DESC) ? "/description" : "/design";
                invoke(L"prepareBinarySelection");
                const auto before = snapshot(); const bool wasDirty = DocChanged(); const auto prior = history(false);
                details = CW2A(name, CP_UTF8); invoke(name);
                invoke(L"afterBinarySelection");
                const auto after = snapshot();
                if (before == after || !DocChanged()) { details = "edit did not change document/dirty"; CheckError(E_FAIL); }
                CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
                if (before != snapshot() || wasDirty != DocChanged() || prior != history(false)) { details = "binary Undo failed"; CheckError(E_FAIL); }
                invoke(L"undoBinarySelection");
                CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO)));
                if (after != snapshot() || !DocChanged()) { details = "binary Redo failed"; CheckError(E_FAIL); }
                invoke(L"redoBinarySelection");
                CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
                if (before != snapshot()) { details = "second binary Undo failed"; CheckError(E_FAIL); }
            };
            for (const auto view : {BODY, DESC}) {
                ShowView(view);
                for (const auto name : {L"addBinary", L"replaceBinary", L"removeBinary", L"removeFreeBinary", L"largeBinary", L"unicodeBinary", L"namedBinary", L"jpegBinary", L"pdfBinary", L"replaceNonimageBinary"}) cycle(name);
            }
            for (const auto function : {L"faultAddBinary", L"faultReplaceBinary", L"faultRemoveBinary"}) {
                for (const auto point : {L"binary-api-update", L"binary-api-oom", L"binary-api-close", L"binary-api-add"}) {
                    const auto before = snapshot(); const bool wasDirty = DocChanged(); const auto oldUndo = history(false), oldRedo = history(true);
                    ::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", point); invoke(function);
                    DWORD open = 0;
                    if (before != snapshot() || wasDirty != DocChanged() || oldUndo != history(false) || oldRedo != history(true) || manager->GetOpenParentState(&open) != S_FALSE) {
                        details = "failed binary edit did not compensate"; CheckError(E_FAIL);
                    }
                }
            }
            {
                CHandle locked(::CreateFileW(CString(root) + L"\\input.png", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
                if (locked == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
                const auto before = snapshot(); invoke(L"lockedInputBinary");
                if (before != snapshot()) { details = "locked input changed book"; CheckError(E_FAIL); }
            }
            invoke(L"addBinary");
            for (const auto redoFailure : {false, true}) {
                if (redoFailure) CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
                for (const auto point : {L"binary-api-update", L"binary-api-oom", L"binary-api-close", L"binary-api-add"}) {
                    const auto before = snapshot(); const bool wasDirty = DocChanged(); const auto oldUndo = history(false), oldRedo = history(true);
                    ::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", point);
                    const HRESULT error = static_cast<HRESULT>(m_doc->m_body.ExecCommand(redoFailure ? IDM_REDO : IDM_UNDO));
                    if (SUCCEEDED(error) || before != snapshot() || wasDirty != DocChanged() || oldUndo != history(false) || oldRedo != history(true)) {
                        details = "failed binary Undo/Redo did not compensate"; CheckError(E_FAIL);
                    }
                }
            }
            CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
            if (initial != snapshot() || dirty != DocChanged()) { details = "preceding binary history was lost"; CheckError(E_FAIL); }
            CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO)));
            CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
            invoke(L"unknownBinaryReferences");
            if (initial != snapshot()) { details = "unknown reference test changed XML"; CheckError(E_FAIL); }
        }
        ShowView(SOURCE);
        const auto source = FbeScripts::SourceBytes(m_source);
        const auto caret = m_source.SendMessage(SCI_GETCURRENTPOS), anchor = m_source.SendMessage(SCI_GETANCHOR);
        { FbeScripts::ScriptExecutionScope context(script); invoke(L"sourceBinary"); }
        if (source != FbeScripts::SourceBytes(m_source) || caret != m_source.SendMessage(SCI_GETCURRENTPOS) || anchor != m_source.SendMessage(SCI_GETANCHOR) || !IsSourceActive()) CheckError(E_FAIL);
        ShowView(BODY);
        { FbeScripts::ScriptExecutionScope context(script); invoke(L"persistBinaries"); }
        // Full payload snapshots belong only to this Save/Reopen assertion,
        // never to the production metadata catalog.
        auto snapshotBinaries = [&]() {
            std::vector<FbeScripts::Binary::LoadedObject> snapshot;
            for (const auto& object : FbeScripts::Binary::Objects(m_doc->m_body.Document())) {
                FbeScripts::Binary::LoadedObject loaded; FbeScripts::Binary::Load(object, loaded);
                snapshot.push_back(std::move(loaded));
            }
            return snapshot;
        };
        const auto expected = snapshotBinaries();
        auto references = [&]() {
            auto dom = m_doc->CreateDOMForAnalysis();
            auto nodes = dom->selectNodes(L"//*[local-name()='image']/@*[local-name()='href']");
            std::vector<std::wstring> values;
            for (long index = 0; index < nodes->length; ++index) values.emplace_back(static_cast<LPCWSTR>(nodes->item[index]->text));
            for (const auto& cover : FbeScripts::Binary::Covers(m_doc->m_body.Document())) values.emplace_back(static_cast<LPCWSTR>(cover.value));
            return values;
        };
        const auto expectedReferences = references();
        const CString filename = m_doc->m_filename;
        if (!m_doc->Save()) { details = "binary save failed"; CheckError(E_FAIL); }
        CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
        if (!DocChanged()) { details = "Undo after save must be dirty"; CheckError(E_FAIL); }
        CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO)));
        if (DocChanged()) { details = "Redo to saved binary state must be clean"; CheckError(E_FAIL); }
        if (LoadFile(filename) != OK) { details = "binary reopen failed"; CheckError(E_FAIL); }
        if (expectedReferences != references()) { details = "binary references/covers after reopening"; CheckError(E_FAIL); }
        const auto restored = snapshotBinaries();
        if (expected.size() != restored.size()) { details = "binary count after reopening"; CheckError(E_FAIL); }
        for (const auto& object : expected) {
            auto found = std::find_if(restored.begin(), restored.end(), [&](const auto& item) { return item.id == object.id; });
            if (found == restored.end() || found->mime != object.mime || found->size != object.size) { details = "binary metadata after reopening"; CheckError(E_FAIL); }
            FbeScripts::Binary::Bytes first(object.data.parray), second(found->data.parray);
            if (memcmp(first.data, second.data, object.size)) { details = "binary bytes after reopening"; CheckError(E_FAIL); }
        }
        m_doc->RunScript(script); invoke(L"resultBinary");
        { FbeScripts::ScriptExecutionScope context(script); invoke(L"exportedMutations"); }
        passed = true;
    } catch (const _com_error& error) {
        CStringA code; code.Format("; hr=0x%08lX", static_cast<unsigned long>(error.Error())); details += code;
    } catch (...) { details = "native exception"; }
    CStringA report; report.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString());
    DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
    ::PostQuitMessage(passed ? 0 : 1); return 0;
}
