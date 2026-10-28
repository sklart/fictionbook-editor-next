// Test-only control: the real editor, real Save and the document's OLE history.
if (IsFbeTestScenario(L"split-control-oracle")) {
    DWORD written = 0;
    try {
        wchar_t variant[24] = {}; ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_CONTROL_VARIANT", variant, _countof(variant));
        const bool savepoint = wcscmp(variant, L"savepoint") == 0 || wcscmp(variant, L"savepoint-light") == 0;
        const bool direct = wcscmp(variant, L"savepoint-direct") == 0;
        const bool light = wcscmp(variant, L"a3-light") == 0 || wcscmp(variant, L"savepoint-light") == 0;
        if (!savepoint && !direct && !light && wcscmp(variant, L"a1") && wcscmp(variant, L"a2") && wcscmp(variant, L"a3") && wcscmp(variant, L"a4")) CheckError(E_INVALIDARG);
        ShowView(BODY);
        MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
        MSHTML::IHTMLElementPtr section(document->all->item(L"oracle-section"));
        MSHTML::IHTMLElementPtr body(document->all->item(L"fbw_body"));
        if (!section || !body || !m_doc->Save()) CheckError(E_FAIL);
        MSHTML::IHTMLElementCollectionPtr children(section->children); MSHTML::IHTMLElementPtr paragraph(children ? MSHTML::IHTMLElementPtr(children->item(0)) : MSHTML::IHTMLElementPtr());
        if (!paragraph) CheckError(E_FAIL);
        IServiceProviderPtr service(document); CComPtr<IOleUndoManager> manager;
        if (!service) CheckError(E_NOINTERFACE);
        CheckError(service->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager)));
        auto flat = [](CString value) { value.Replace(L"\r", L""); value.Replace(L"\n", L""); value.Replace(L"\t", L" "); return value; };
        auto dom = [&]() { return CString(static_cast<const wchar_t*>(body->innerHTML)); };
        auto stack = [&](bool redo) { CString result; CComPtr<IEnumOleUndoUnits> list; HRESULT hr = redo ? manager->EnumRedoable(&list) : manager->EnumUndoable(&list); if (FAILED(hr) || !list) return CString(L"unavailable"); long count = 0; for (; count < 128; ++count) { CComPtr<IOleUndoUnit> unit; if (list->Next(1, &unit, nullptr) != S_OK) break; GUID type = {}; LONG id = 0; wchar_t guid[40] = {}; BSTR description = nullptr; const HRESULT typeHr = unit->GetUnitType(&type, &id); const HRESULT descHr = unit->GetDescription(&description); CString entry; if (SUCCEEDED(typeHr) && ::StringFromGUID2(type, guid, _countof(guid))) entry.Format(L"%s:%ld", guid, id); else entry = L"unknown"; if (SUCCEEDED(descHr) && description) { entry += L":"; entry += flat(CString(description)); } ::SysFreeString(description); if (!result.IsEmpty()) result += L"|"; result += entry; } CString prefix; prefix.Format(L"%ld;", count); return prefix + result; };
        auto observe = [&](const char* phase, HRESULT hr) {
            const CString current(dom()); const auto saved = m_doc->CaptureUndoSavepoints(); const long versionBefore = m_doc->m_body.GetVersionNumber(); CString caret(L"unavailable");
            try { MSHTML::IHTMLTxtRangePtr selected(document->selection->createRange()); if (selected) { MSHTML::IHTMLElementPtr parent(selected->parentElement()); MSHTML::IHTMLTxtRangePtr prefix(selected->duplicate()); prefix->moveToElementText(paragraph); prefix->setEndPoint(L"EndToStart", selected); _bstr_t before(prefix->text); _bstr_t text(selected->text); caret.Format(L"parent=%s;offset=%ld;length=%ld", parent ? static_cast<const wchar_t*>(parent->tagName) : L"", static_cast<long>(before.length()), static_cast<long>(text.length())); } } catch (const _com_error&) {}
            MSXML2::IXMLDOMDocument2Ptr book(m_doc->CreateDOMForAnalysis()); CString bookXml(book ? static_cast<const wchar_t*>(book->xml) : L"");
            const CStringA current8(CW2A(flat(current), CP_UTF8)), book8(CW2A(flat(bookXml), CP_UTF8)), undo8(CW2A(flat(stack(false)), CP_UTF8)), redo8(CW2A(flat(stack(true)), CP_UTF8)), caret8(CW2A(flat(caret), CP_UTF8)); CStringA row;
            row.Format("%s\t%s\t%s\t%d\t%d\t%d\t%d\t%ld\t%ld\t%s\t%s\t%s\t%s\t%d\t%d\t0x%08lX\r\n", phase, current8.GetString(), book8.GetString(), !saved.clean ? 1 : 0, m_doc->DocChanged() ? 1 : 0, saved.hasSaved ? 1 : 0, saved.hasSaved && current == saved.savedHtml ? 1 : 0, saved.savedVersion, versionBefore, undo8.GetString(), redo8.GetString(), caret8.GetString(), saved.reconcile ? "enabled" : "disabled", current == dom() ? 1 : 0, versionBefore == m_doc->m_body.GetVersionNumber() ? 1 : 0, static_cast<unsigned long>(hr)); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
        };
        CStringA header("phase\tdom\tfb2_model\tdirty_before\tdirty\thas_saved\tdom_equals_savepoint\tsaved_version\tbody_version\tundo_units\tredo_units\tselection\treconcile\tobserver_dom_unchanged\tobserver_version_unchanged\thresult\r\n"); output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
        const CString initial(dom()); observe("saved", S_OK);
        m_doc->m_body.SetFocus(); m_doc->m_body.GoTo(paragraph, false);
        MSHTML::IHTMLTxtRangePtr range(document->selection->createRange()); MSHTML::IHTMLElementPtr selectedParent(range ? MSHTML::IHTMLElementPtr(range->parentElement()) : MSHTML::IHTMLElementPtr());
        if (!selectedParent || selectedParent != paragraph) CheckError(E_UNEXPECTED);
        if (!light) observe("ready", S_OK);
        HWND focus = ::GetFocus(); if (!focus) CheckError(E_FAIL); ::SendMessage(focus, WM_CHAR, L'Q', 0);
        if (CString(static_cast<const wchar_t*>(paragraph->innerText)) != L"Qtail") CheckError(E_UNEXPECTED);
        const CString afterQ(dom()); if (!light) observe("Q", S_OK);
        if (!savepoint && !direct) {
            if (!wcscmp(variant, L"a2")) { MSG msg = {}; for (int n = 0; n < 100 && ::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE); ++n) { if (msg.message == WM_QUIT) { ::PostQuitMessage(static_cast<int>(msg.wParam)); break; } ::TranslateMessage(&msg); ::DispatchMessageW(&msg); } }
            if (!wcscmp(variant, L"a3") || !wcscmp(variant, L"a3-light")) { range = document->selection->createRange(); range->move(L"character", 1); range->select(); range->move(L"character", -1); range->select(); }
            if (!wcscmp(variant, L"a4")) { range = MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange(); range->moveToElementText(paragraph); if (!range->findText(L"tail", 0, 0)) CheckError(E_FAIL); range->select(); CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_BOLD))); observe("format", S_OK); m_doc->m_body.GoTo(paragraph, false); }
            if (!light) observe("boundary", S_OK); focus = ::GetFocus(); if (!focus) CheckError(E_FAIL); ::SendMessage(focus, WM_CHAR, L'X', 0);
            const CString targetText(static_cast<const wchar_t*>(paragraph->innerText));
            if (targetText != (!wcscmp(variant, L"a4") ? L"XQtail" : L"QXtail")) CheckError(E_UNEXPECTED);
            observe("X", S_OK);
        }
        const CString finalDom(dom()); BOOL handled = FALSE; int undoCount = 0; bool restored = false;
        for (int i = 1; i <= 6; ++i) { HRESULT hr = direct ? static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)) : static_cast<HRESULT>(m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled)); CStringA label; label.Format("undo-%d", i); observe(label, hr); ++undoCount; if (direct) { m_doc->RestoreCleanIfBodyMatches(); observe("reconcile-explicit", S_OK); } if (dom() == initial) { restored = true; break; } }
        for (int i = 1; i <= undoCount; ++i) { HRESULT hr = static_cast<HRESULT>(m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled)); CStringA label; label.Format("redo-%d", i); observe(label, hr); }
        const bool redone = dom() == finalDom;
        if (savepoint || direct) { const HRESULT hr = static_cast<HRESULT>(m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled)); observe("final-undo", hr); }
        CStringA result; result.Format("result\trestored=%d;redone=%d;undo_count=%d;q_changed=%d\r\n", restored, redone, undoCount, afterQ != initial); output.Write(result, static_cast<DWORD>(result.GetLength()), &written); output.Close(); ::PostQuitMessage(0); return 0;
    } catch (const _com_error& error) { CStringA row; row.Format("error\t0x%08lX\r\n", static_cast<unsigned long>(error.Error())); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
}
