__declspec(noinline) LRESULT CMainFrame::RunBinaryScriptingCompatibilityRuntime(CAtlFile& output)
{
    bool passed = false; CStringA details = "start", rows = "case\tid\tsha256\tsize\tmetadata\tdirty\r\n";
    unsigned completed = 0;
    wchar_t rootBuffer[32768] = {}; ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", rootBuffer, _countof(rootBuffer));
    const CString root(rootBuffer);
    auto require = [&](bool ok, const char* label) { if (!ok) { details = label; CheckError(E_FAIL); } };
    auto bytesOf = [&](const _variant_t& value) {
        std::vector<BYTE> bytes;
        if (value.vt == VT_NULL || value.vt == VT_EMPTY) return bytes;
        require(value.vt == (VT_ARRAY | VT_UI1) && value.parray && ::SafeArrayGetDim(value.parray) == 1, "legacy bytes type");
        LONG lower = 0, upper = -1; CheckError(::SafeArrayGetLBound(value.parray, 1, &lower)); CheckError(::SafeArrayGetUBound(value.parray, 1, &upper));
        const LONGLONG size = static_cast<LONGLONG>(upper) - lower + 1;
        require(size >= 0 && size <= INT_MAX, "legacy bytes bounds");
        FbeScripts::Binary::Bytes access(value.parray);
        if (size) bytes.assign(access.data, access.data + static_cast<size_t>(size));
        return bytes;
    };
    auto hashOf = [&](const std::vector<BYTE>& bytes) {
        const BYTE empty = 0; return FCCrypt::Get_SHA256(bytes.empty() ? &empty : bytes.data(), static_cast<int>(bytes.size()));
    };
    auto fileBytes = [&](const CString& name) {
        CAtlFile input; CheckError(input.Create(root + L"\\" + name, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING));
        ULONGLONG size = 0; CheckError(input.GetSize(size)); require(size <= INT_MAX, "fixture size");
        std::vector<BYTE> bytes(static_cast<size_t>(size)); DWORD read = 0;
        if (size) { CheckError(input.Read(bytes.data(), static_cast<DWORD>(size), read)); require(read == size, "fixture read"); }
        return bytes;
    };
    auto history = [&]() {
        CComPtr<IOleUndoManager> manager; IServiceProviderPtr services(m_doc->m_body.Document());
        CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager)));
        std::vector<CComPtr<IOleUndoUnit>> result;
        for (bool redo : {false, true}) {
            CComPtr<IEnumOleUndoUnits> entries; CheckError(redo ? manager->EnumRedoable(&entries) : manager->EnumUndoable(&entries));
            for (;;) { CComPtr<IOleUndoUnit> unit; HRESULT hr = entries->Next(1, &unit, nullptr); if (hr == S_FALSE) break; CheckError(hr); result.push_back(unit); }
            result.push_back(nullptr);
        }
        return result;
    };
    auto snapshot = [&](const CString& label, const CString& id, const CString& expectedFile, bool absent) {
        require(m_doc != nullptr, "missing active document");
        CComPtr<IDispatch> external; CheckError(m_doc->m_body.Document()->parentWindow->get_external(&external));
        CComDispatchDriver api(external); CComVariant argument(id), metadata;
        const bool dirty = DocChanged(); const auto undo = history();
        const HRESULT infoError = api.Invoke1(L"GetBinaryInfo", &argument, &metadata);
        _variant_t data; m_doc->GetBinary(L"#" + id, data);
        if (absent) { require(FAILED(infoError) && (data.vt == VT_EMPTY || data.vt == VT_NULL), "deleted/old ID observable"); return; }
        const auto actual = bytesOf(data), expected = fileBytes(expectedFile);
        require(actual == expected, "native binary bytes stale");
        // Independent old JS read path, not the native metadata implementation.
        CComDispatchDriver script(m_doc->m_body.Document()->Script); _variant_t legacy;
        CheckError(script.Invoke1(L"GetImageData", &argument, &legacy));
        require(bytesOf(legacy) == expected, "legacy/native byte disagreement");
        require(SUCCEEDED(infoError) || expected.empty(), "metadata request failed");
        if (SUCCEEDED(infoError)) require(metadata.vt == VT_BSTR, "metadata return type");
        require(dirty == DocChanged() && undo == history(), "cache lookup changed dirty/history");
        CStringA row; row.Format("%s\t%s\t%s\t%u\t%s\t%d\r\n", CW2A(label, CP_UTF8).m_psz, CW2A(id, CP_UTF8).m_psz,
            CW2A(hashOf(actual), CP_UTF8).m_psz, static_cast<unsigned>(actual.size()),
            SUCCEEDED(infoError) ? CW2A(metadata.bstrVal, CP_UTF8).m_psz : "null", dirty ? 1 : 0);
        rows += row;
    };
    try {
        CHandle finished(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!finished) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        std::atomic<int> dialogAnswer{IDOK};
        struct DialogCloser {
            HANDLE finished; std::thread worker;
            ~DialogCloser() { ::SetEvent(finished); worker.join(); }
        } closeDialogs{finished, std::thread([&]() {
            while (::WaitForSingleObject(finished, 10) == WAIT_TIMEOUT)
                ::EnumWindows([](HWND window, LPARAM context) -> BOOL {
                    DWORD process = 0; ::GetWindowThreadProcessId(window, &process); if (process != ::GetCurrentProcessId()) return TRUE;
                    wchar_t type[64] = {}; ::GetClassNameW(window, type, _countof(type));
                    if (::IsWindowVisible(window) && (wcscmp(type, L"#32770") == 0 || wcscmp(type, L"FBEThemedMessageDialog") == 0))
                        { const int answer = reinterpret_cast<std::atomic<int>*>(context)->load(); if (::GetDlgItem(window, answer)) ::PostMessageW(window, WM_COMMAND, answer, 0); else if (::GetDlgItem(window, IDOK)) ::PostMessageW(window, WM_COMMAND, IDOK, 0); else if (::GetDlgItem(window, IDCANCEL) && !::GetDlgItem(window, IDYES) && !::GetDlgItem(window, IDNO)) ::PostMessageW(window, WM_COMMAND, IDCANCEL, 0); }
                    return TRUE;
                }, reinterpret_cast<LPARAM>(&dialogAnswer));
        })};
        auto manifest = U::CreateDocument(true); require(manifest->load(_variant_t(root + L"\\suite.xml")) != VARIANT_FALSE, "suite manifest");
        auto steps = manifest->selectNodes(L"/suite/step");
        auto saveBeforeLifecycle = [&]() {
            if (m_doc && m_doc->m_namevalid && DocChanged()) {
                require(m_doc->Save(root + L"\\lifecycle-current.fb2"), "save before lifecycle");
                CommitSuccessfulSave();
            }
        };
        for (long index = 0; index < steps->length; ++index) {
            MSXML2::IXMLDOMElementPtr step(steps->item[index]);
            auto attr = [&](LPCWSTR name) { const _variant_t value(step->getAttribute(name)); return value.vt == VT_BSTR ? CString(value.bstrVal) : CString(); };
            const CString descriptionTheme = attr(L"descriptionTheme");
            if(!descriptionTheme.IsEmpty()) {
                require(descriptionTheme == L"light" || descriptionTheme == L"dark", "description theme fixture");
                ThemeManager::SetSelectedTheme(descriptionTheme == L"dark" ? INTERFACE_THEME_DARK : INTERFACE_THEME_LIGHT);
                m_doc->ApplyThemeAppearance();
                MSHTML::IHTMLElementPtr description = MSHTML::IHTMLDocument3Ptr(m_doc->m_body.Document())->getElementById(L"fbw_desc");
                const COLORREF background = ThemeManager::ControlColor(), inputBackground = ThemeManager::WindowColor(), foreground = ThemeManager::TextColor();
                CString cssBackground, cssInputBackground, cssForeground;
                cssBackground.Format(L"rgb(%d,%d,%d)", GetRValue(background), GetGValue(background), GetBValue(background));
                cssInputBackground.Format(L"rgb(%d,%d,%d)", GetRValue(inputBackground), GetGValue(inputBackground), GetBValue(inputBackground));
                cssForeground.Format(L"rgb(%d,%d,%d)", GetRValue(foreground), GetGValue(foreground), GetBValue(foreground));
                description->setAttribute(L"data-description-theme-background", _variant_t(cssBackground), 0);
                description->setAttribute(L"data-description-theme-input-background", _variant_t(cssInputBackground), 0);
                description->setAttribute(L"data-description-theme-foreground", _variant_t(cssForeground), 0);
            }
            const CString label = attr(L"name"), command = attr(L"command"), id = attr(L"id"), script = attr(L"script");
            details = CW2A(label, CP_UTF8);
            { CAtlFile progress; CheckError(progress.Create(root + L"\\progress.txt", GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS)); CheckError(progress.Write(details, static_cast<DWORD>(details.GetLength()))); }
            dialogAnswer = IDNO; // Explicitly discard only unsaved empty New documents during lifecycle commands.
            if (command == L"undo" || command == L"redo") CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(command == L"undo" ? IDM_UNDO : IDM_REDO)));
            else if (command == L"open") { saveBeforeLifecycle(); require(LoadFile(root + L"\\" + attr(L"open")) == OK, "open lifecycle"); }
            else if (command == L"new") { saveBeforeLifecycle(); BOOL handled = FALSE; OnFileNew(0, 0, nullptr, handled); }
            else if (command == L"close") { saveBeforeLifecycle(); BOOL handled = FALSE; OnFileClose(0, 0, nullptr, handled); }
            else if (command == L"visual-source") { ShowView(SOURCE); require(IsSourceActive(), "source view"); ShowView(BODY); require(!IsSourceActive(), "visual view"); }
            else if (command == L"source-mutate") {
                ShowView(SOURCE); std::string source = FbeScripts::SourceBytes(m_source);
                const size_t identity = source.find("id=\"image1.png\""); require(identity != std::string::npos, "source binary identity");
                const size_t begin = source.find('>', identity), end = source.find("</binary>", begin);
                require(begin != std::string::npos && end != std::string::npos, "source binary text");
                source.replace(begin + 1, end - begin - 1, std::string(CW2A(attr(L"base64"), CP_UTF8)));
                m_source.SendMessage(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(source.c_str())); ShowView(BODY);
                require(!IsSourceActive(), "source mutation apply");
            }
            dialogAnswer = attr(L"corpus").IsEmpty() ? IDOK : IDYES;
            _Settings.SetScriptsFolder(root);
            if (!script.IsEmpty()) {
                const bool dirty = DocChanged();
                m_doc->RunScript(root + L"\\" + script);
                if (attr(L"corpus").IsEmpty()) {
                    CComVariant result; CheckError(m_doc->InvokeFunc(L"CompatResult", nullptr, 0, result, true));
                    if (result.vt != VT_BSTR || wcscmp(result.bstrVal, L"PASS")) { if (result.vt == VT_BSTR) details += ": " + CStringA(CW2A(result.bstrVal, CP_UTF8)); CheckError(E_FAIL); }
                }
                if (attr(L"rawDirty") == L"unchanged") require(dirty == DocChanged(), "raw expando dirty semantics changed");
            }
            if (!attr(L"reader").IsEmpty()) {
                m_doc->RunScript(root + L"\\" + attr(L"reader")); CComVariant result;
                CheckError(m_doc->InvokeFunc(L"CompatResult", nullptr, 0, result, true)); require(result.vt == VT_BSTR && !wcscmp(result.bstrVal, L"PASS"), "script A to script B");
            }
            if (!id.IsEmpty()) snapshot(label, id, attr(L"expected"), attr(L"absent") == L"true");
            if (attr(L"readOnly100") == L"true") {
                const auto before = history(); const bool dirty = DocChanged();
                for (int repeat = 0; repeat < 100; ++repeat) snapshot(label, id, attr(L"expected"), false);
                require(before == history() && dirty == DocChanged(), "100 reads changed history/dirty");
            }
            if (!attr(L"save").IsEmpty()) { require(m_doc->Save(root + L"\\" + attr(L"save")), "legacy save"); CommitSuccessfulSave(); }
            ++completed;
        }
        CAtlFile states; CheckError(states.Create(root + L"\\states.tsv", GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS));
        CheckError(states.Write(rows, static_cast<DWORD>(rows.GetLength())));
        passed = true;
    } catch (const _com_error& error) { CStringA hr; hr.Format("; hr=0x%08lX", static_cast<unsigned long>(error.Error())); details += hr; }
    catch (...) { details += ": native exception"; }
    CStringA report; report.Format("result=%s\r\ncompleted=%u\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", completed, details.GetString());
    DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
    ::PostQuitMessage(passed ? 0 : 1); return 0;
}
