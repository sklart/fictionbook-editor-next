if (IsFbeTestScenario(L"replace-all-runtime"))
{
	CStringA report;
	bool passed = false;
	auto pump = []() {
		const ULONGLONG until = ::GetTickCount64() + 100;
		while (::GetTickCount64() < until) {
			MSG message = {};
			while (::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessageW(&message); }
			::Sleep(1);
		}
	};
	try {
		wchar_t directory[MAX_PATH] = {};
		if (!::GetEnvironmentVariableW(L"FBE_NEXT_TEST_REPLACE_OUTPUT", directory, _countof(directory))) CheckError(E_INVALIDARG);
		const CString beforePath = CString(directory) + L"\\before.fb2";
		const CString afterPath = CString(directory) + L"\\after.fb2";
		wchar_t fault[64] = {};
		::GetEnvironmentVariableW(L"FBE_NEXT_TEST_REPLACE_FAULT", fault, _countof(fault));
		const bool expectedFailure = fault[0] != 0;
		const bool expectedMutation = expectedFailure && wcscmp(fault, L"before-first") != 0;
		ShowView(BODY); m_doc->m_body.SetFocus(); pump();
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		CComPtr<IOleUndoManager> undoManager;
		IServiceProviderPtr provider(document);
		CheckError(provider->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&undoManager)));
		auto historyCount = [&](bool redo) -> int {
			CComPtr<IEnumOleUndoUnits> entries; CheckError(redo ? undoManager->EnumRedoable(&entries) : undoManager->EnumUndoable(&entries));
			int count = 0;
			for (;;) { CComPtr<IOleUndoUnit> unit; const HRESULT hr = entries->Next(1, &unit, NULL); if (hr == S_FALSE) break; CheckError(hr); ++count; }
			return count;
		};
		MSHTML::IHTMLElementPtr body(MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"fbw_body"));
		if (!body || !m_doc->Save(beforePath)) CheckError(E_FAIL);
		const CString beforeHtml(static_cast<LPCWSTR>(body->innerHTML));
		const CString beforeXml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
		const wchar_t* replacement = wcscmp(fault, L"format") == 0 ? L"\\S+\\Q" : L"+";
		if (!m_doc->m_body.ConfigureReplaceAllRuntimeTest(L"\\d", replacement, true)) CheckError(E_FAIL);
		const UINT_PTR timer = ::SetTimer(NULL, 0, 30, [](HWND, UINT, UINT_PTR, DWORD) {
			::EnumThreadWindows(::GetCurrentThreadId(), [](HWND window, LPARAM) -> BOOL {
				wchar_t name[64] = {}; ::GetClassNameW(window, name, _countof(name));
				if ((wcscmp(name, L"#32770") == 0 || wcscmp(name, L"FBEThemedMessageDialog") == 0) && ::GetDlgItem(window, IDYES))
					::PostMessageW(window, WM_COMMAND, IDYES, 0);
				return TRUE;
			}, 0);
		});
		if (!timer) CheckError(E_FAIL);
		CString error; int count = -1;
		try { count = m_doc->m_body.ReplaceAllSearchCore(&error); }
		catch (...) { ::KillTimer(NULL, timer); throw; }
		::KillTimer(NULL, timer); pump();
		const CString afterHtml(static_cast<LPCWSTR>(body->innerHTML));
		const CString afterXml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
		const bool changed = beforeHtml != afterHtml;
		const int undoAvailable = historyCount(false);
		int redoAvailable = 0;
		bool undo = false, redo = false;
		if (changed) {
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
			const CString undoHtml(static_cast<LPCWSTR>(body->innerHTML));
			const CString undoXml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
			redoAvailable = historyCount(true);
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
			const CString redoHtml(static_cast<LPCWSTR>(body->innerHTML));
			const CString redoXml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
			undo = undoHtml == beforeHtml && undoXml == beforeXml;
			redo = redoHtml == afterHtml && redoXml == afterXml;
		}
		const CString warning = FbeLoadRuntimeStringByKey(L"fbe.replace.partial_failure",
			L"Replace All may have partially changed the document. Do not overwrite the original file. Reopen the last saved copy to restore it; unsaved changes will be lost.");
		const bool partialWarning = error.Find(warning) >= 0;
		const bool saved = m_doc->Save(afterPath); pump();
		const bool reopened = saved && LoadFile(afterPath) == OK; pump();
		const CString reopenedXml(reopened ? static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml) : L"");
		const bool xmlMatches = reopened && reopenedXml == afterXml;
		bool savedCopyRecovery = true;
		if (expectedFailure) {
			savedCopyRecovery = LoadFile(beforePath) == OK; pump();
			savedCopyRecovery = savedCopyRecovery && CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == beforeXml;
		}
		report.Format("fault=%S\ncount=%d\nerror-empty=%d\npartial-warning=%d\nchanged=%d\nundo-tested=%d\nundo-available=%d\nredo-available=%d\nundo=%d\nredo=%d\nsaved=%d\nreopened=%d\nxml-matches=%d\nsaved-copy-recovery=%d\n",
			fault, count, error.IsEmpty(), partialWarning, changed, changed, undoAvailable, redoAvailable, undo, redo, saved, reopened, xmlMatches, savedCopyRecovery);
		const bool operation = expectedFailure ? count == -1 && !error.IsEmpty() && partialWarning == expectedMutation && changed == expectedMutation
			: count > 0 && error.IsEmpty() && changed;
		passed = operation && (!changed || (undo && redo && undoAvailable > 0 && redoAvailable > 0)) && saved && reopened && xmlMatches && savedCopyRecovery;
	}
	catch (const _com_error& error) { CStringA line; line.Format("error=0x%08lX\n", static_cast<unsigned long>(error.Error())); report += line; }
	report += passed ? "result=pass\n" : "result=fail\n";
	DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Flush(); output.Close();
	::PostQuitMessage(passed ? 0 : 1); return 0;
}
