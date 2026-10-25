__declspec(noinline) LRESULT CMainFrame::RunBinaryStoreRuntime(CAtlFile& output)
{
    bool passed = false; CStringA details = "start", metrics;
    wchar_t root[32768] = {}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", root, _countof(root));
    auto require = [&](bool condition, const char* message) { if (!condition) { details = message; CheckError(E_FAIL); } };
    auto compare = [&]() {
        m_doc->ReconcileBinaryStore();
        const auto objects = FbeScripts::Binary::Objects(m_doc->m_body.Document());
        require(m_doc->m_binaryStore.Count() == objects.size(), "DOM/store count");
        for (const auto& object : objects) {
            m_doc->BinaryInfo(object.id);
            const auto* metadata = m_doc->m_binaryStore.Find(object.id);
            FbeScripts::Binary::LoadedObject payload; FbeScripts::Binary::Load(object, payload);
            require(metadata && metadata->contentType == object.mime && metadata->byteSize == payload.size, "DOM/store metadata");
        }
    };
    try {
        ShowView(BODY);
        require(m_doc->m_binaryStore.Count() == 100 && !m_doc->m_binaryStore.Dirty(), "PutBinaries registration");
        require(m_doc->m_binaryStore.Find(L"png")->dimensionsKnown && m_doc->m_binaryStore.Find(L"png")->sizeKnown,
            "initial UI metadata not reused");
        compare(); require(m_doc->m_binaryStore.Count() == 100, "initial 100 entries");
        {
            IDispatch* node = m_doc->m_binaryStore.Element(L"payload2");
            require(FB::Doc::BinaryOwner(m_doc->ClipboardIdentity()) == m_doc, "private bridge document identity");
            CComPtr<IDispatch> external; CheckError(m_doc->m_body.Document()->parentWindow->get_external(&external));
            CComDispatchDriver bridge(external);
            CComVariant args[2] = {CComVariant(node), CComVariant(L"register")};
            details = "private bridge register"; CheckError(bridge.InvokeN(L"_BinaryStoreChanged", args, 2));
        }
        m_doc->m_binaryStore.Invalidate();
        const auto before = m_doc->m_binaryStore.Statistics();
        const ULONGLONG started = ::GetTickCount64();
        for (int index = 0; index < 100; ++index) {
            m_doc->BinaryInfo(L"png"); m_doc->BinaryInfo(L"jpeg");
        }
        for (int index = 0; index < 500; ++index) m_doc->BinaryInfo(L"payload2");
        const auto after = m_doc->m_binaryStore.Statistics();
        require(after.reconciliations - before.reconciliations == 1, "repeated lookup performed DOM rebuild");
        require(after.sizeCalculations - before.sizeCalculations == 3, "size not computed once per generation");
        require(after.dimensionsCalculations - before.dimensionsCalculations == 2, "PNG/JPEG decoded more than once");
        require(after.dimensionsHits - before.dimensionsHits >= 198, "missing dimensions cache hits");
        metrics.Format("lookup_elapsed_ms=%llu\r\nsize_calculations=%llu\r\ndimensions_calculations=%llu\r\ndimensions_cache_hits=%llu\r\nsize_cache_hits=%llu\r\nreconciliations=%llu\r\n",
            ::GetTickCount64() - started, after.sizeCalculations - before.sizeCalculations,
            after.dimensionsCalculations - before.dimensionsCalculations, after.dimensionsHits - before.dimensionsHits,
            after.sizeHits - before.sizeHits, after.reconciliations - before.reconciliations);
        const auto* png = m_doc->m_binaryStore.Find(L"png");
        require(png && png->dimensionsKnown && png->width == 3 && png->height == 2, "PNG dimensions");
        const auto* jpeg = m_doc->m_binaryStore.Find(L"jpeg");
        require(jpeg && jpeg->dimensionsKnown && jpeg->width == 3 && jpeg->height == 2, "JPEG dimensions");
        _Settings.SetScriptsFolder(root);
        m_doc->RunScript(CString(root) + L"\\cache-mutate.js");
        const auto beforeMutationRefresh = m_doc->m_binaryStore.Statistics();
        for (int index = 0; index < 100; ++index) m_doc->BinaryInfo(L"png");
        const auto afterMutationRefresh = m_doc->m_binaryStore.Statistics();
        require(afterMutationRefresh.dimensionsCalculations - beforeMutationRefresh.dimensionsCalculations == 1 &&
            afterMutationRefresh.dimensionsHits - beforeMutationRefresh.dimensionsHits == 99, "mutation decode/cache boundary");
        require(m_doc->m_binaryStore.Find(L"png")->width == 5 && m_doc->m_binaryStore.Find(L"png")->height == 4, "mutation dimensions stale");
        metrics.AppendFormat("mutation_requests=100\r\nmutation_decodes=%llu\r\nmutation_cache_hits=%llu\r\n",
            afterMutationRefresh.dimensionsCalculations - beforeMutationRefresh.dimensionsCalculations,
            afterMutationRefresh.dimensionsHits - beforeMutationRefresh.dimensionsHits);
        m_doc->RunScript(CString(root) + L"\\cache-restore.js");
        // Exercise the existing reference-aware rename algorithm, retaining the
        // store metadata generation without touching or copying the payload.
        MSHTML::IHTMLElementPtr node(m_doc->m_binaryStore.Element(L"payload3"));
        MSHTML::IHTMLElementCollectionPtr fields(node->all);
        MSHTML::IHTMLInputElementPtr input(fields->item(L"id"));
        input->value = L"renamed";
        CComDispatchDriver script(m_doc->m_body.Document()->Script); CComVariant argument(input);
        CheckError(script.Invoke1(L"OnBinaryChange", &argument));
        require(!m_doc->m_binaryStore.Contains(L"payload3") && m_doc->m_binaryStore.Contains(L"renamed"), "rename index");
        compare();
        FbeScripts::ScriptExecutionScope context(CString(root) + L"\\legacy.js");
        auto cycle = [&](DISPID method, const std::vector<CComVariant>& args) {
            CComVariant result; FbeScripts::Binary::Mutate(m_doc, method, args, result); compare();
            CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO))); compare();
            CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO))); compare();
        };
        cycle(149, {CComVariant(CString(root) + L"\\replace.png"), CComVariant(L"added")});
        require(m_doc->m_binaryStore.Contains(L"added"), "add/redo index");
        cycle(150, {CComVariant(L"png"), CComVariant(CString(root) + L"\\replace.png")});
        require(m_doc->m_binaryStore.Find(L"png")->width == 5, "replace/redo dimensions");
        cycle(151, {CComVariant(L"payload99")});
        require(!m_doc->m_binaryStore.Contains(L"payload99"), "remove/redo index");
        // Real RunScript executes all three legacy writes without a native hook.
        _Settings.SetScriptsFolder(root);
        FbeScriptDiagnostics::ScopedDialogSuppression suppressDialogs;
        m_doc->RunScript(CString(root) + L"\\legacy.js");
        compare();
        require(!m_doc->m_binaryStore.Contains(L"payload2") && m_doc->m_binaryStore.Contains(L"legacy-renamed"), "legacy rename reconciliation");
        const auto* legacy = m_doc->m_binaryStore.Find(L"legacy-renamed");
        require(legacy && legacy->contentType == L"application/x-legacy" && legacy->byteSize == 17, "legacy data/type reconciliation");
        {
            // The real legacy error handler shows a message box. Close only
            // this isolated test process's UI-thread dialogs, as in stage 1.
            CHandle finished(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
            if (!finished) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
            const DWORD uiThread = ::GetCurrentThreadId();
            struct CloseExpectedError {
                HANDLE finished;
                std::thread worker;
                ~CloseExpectedError() { ::SetEvent(finished); worker.join(); }
            } closeError{finished, std::thread([&]() {
                while (::WaitForSingleObject(finished, 10) == WAIT_TIMEOUT)
                    ::EnumThreadWindows(uiThread, [](HWND window, LPARAM) -> BOOL {
                        wchar_t name[64] = {}; ::GetClassNameW(window, name, _countof(name));
                        if (::IsWindowVisible(window) && (wcscmp(name, L"#32770") == 0 || wcscmp(name, L"FBEThemedMessageDialog") == 0))
                            ::PostMessageW(window, WM_CLOSE, 0, 0);
                        return TRUE;
                    }, 0);
            })};
            m_doc->RunScript(CString(root) + L"\\legacy-error.js");
        }
        compare();
        require(m_doc->m_binaryStore.Find(L"legacy-renamed")->contentType == L"application/x-legacy-error", "legacy exception reconciliation");
        const CString saved = CString(root) + L"\\saved.fb2";
        require(m_doc->Save(saved), "save after legacy mutation");
        const GUID previousIdentity = m_doc->ClipboardIdentity();
        require(LoadFile(saved) == OK, "reopen after legacy mutation"); compare();
        require(FB::Doc::BinaryOwner(previousIdentity) == nullptr, "replaced document identity remains live");
        require(m_doc->m_binaryStore.Count() == 100 && !m_doc->m_binaryStore.Contains(L"payload2"), "reopen lifecycle");
        require(m_doc->m_binaryStore.Find(L"legacy-renamed")->byteSize == 17, "reopen legacy bytes");
        m_doc->TraceBinaryStore();
        PROCESS_MEMORY_COUNTERS memory = {}; memory.cb = sizeof(memory);
        require(::GetProcessMemoryInfo(::GetCurrentProcess(), &memory, sizeof(memory)) != FALSE, "process peak memory unavailable");
        metrics.AppendFormat("peak_private_bytes=%llu\r\npeak_working_set_bytes=%llu\r\n",
            static_cast<unsigned long long>(memory.PeakPagefileUsage), static_cast<unsigned long long>(memory.PeakWorkingSetSize));
        passed = true;
    } catch (const _com_error& error) {
        CStringA code; code.Format("; hr=0x%08lX", static_cast<unsigned long>(error.Error())); details += code;
    } catch (...) { details = "native exception"; }
    CStringA report; report.Format("result=%s\r\ndetails=%s\r\n%s", passed ? "PASS" : "FAIL", details.GetString(), metrics.GetString());
    DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
    ::PostQuitMessage(passed ? 0 : 1); return 0;
}
