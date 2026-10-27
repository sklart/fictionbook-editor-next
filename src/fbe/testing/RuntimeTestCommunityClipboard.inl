if (IsFbeTestScenario(L"community-native-copy-paste"))
{
	ShowView(BODY);
	wchar_t caseBuffer[64] = {}, roleBuffer[32] = {}, readyBuffer[MAX_PATH] = {}, doneBuffer[MAX_PATH] = {};
	::GetEnvironmentVariableW(L"FBE_NEXT_COPY_CASE", caseBuffer, _countof(caseBuffer));
	::GetEnvironmentVariableW(L"FBE_NEXT_COPY_ROLE", roleBuffer, _countof(roleBuffer));
	::GetEnvironmentVariableW(L"FBE_NEXT_COPY_READY", readyBuffer, _countof(readyBuffer));
	::GetEnvironmentVariableW(L"FBE_NEXT_COPY_DONE", doneBuffer, _countof(doneBuffer));
	const CString testCase(caseBuffer), role(roleBuffer);
	CStringA report;
	bool passed = true;
	auto check = [&](const char* label, bool value) { report.AppendFormat("%s=%d\n", label, value ? 1 : 0); passed = value && passed; DWORD written = 0; output.Seek(0, FILE_BEGIN); output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); };
	CComPtr<IDataObject> previousClipboard;
	const bool restoreClipboard = role != L"target" && SUCCEEDED(::OleGetClipboard(&previousClipboard));
	auto pump = [&](DWORD milliseconds) {
		const ULONGLONG until = ::GetTickCount64() + milliseconds;
		do {
			MSG message = {};
			if (::PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessage(&message); }
			else ::MsgWaitForMultipleObjects(0, NULL, FALSE, 5, QS_ALLINPUT);
		} while (::GetTickCount64() < until);
	};
	try {
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		auto element = [&](LPCWSTR id) { return MSHTML::IHTMLElementPtr(MSHTML::IHTMLDocument3Ptr(document)->getElementById(id)); };
		auto html = [&]() { return CString(static_cast<LPCWSTR>(element(L"fbw_body")->innerHTML)); };
		auto text = [&]() { return CString(static_cast<LPCWSTR>(element(L"fbw_body")->innerText)); };
		auto count = [](const CString& haystack, const CString& needle) {
			int result = 0, at = 0;
			while (!needle.IsEmpty() && (at = haystack.Find(needle, at)) >= 0) { ++result; at += needle.GetLength(); }
			return result;
		};
		auto select = [&](LPCWSTR id, int offset, int length) {
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
			range->moveToElementText(element(id));
			if (length >= 0) { range->collapse(VARIANT_TRUE); range->move(L"character", offset); range->moveEnd(L"character", length); }
			range->select(); return range;
		};
		m_doc->MarkSavePoint(); m_doc->MarkDocCP();
		StartupTrace::HResult(L"clipboard", L"CP299", E_FAIL, L"stage=CP301; operation-stage=CP402");
		StartupTrace::CrashTraceSnapshot diagnosticProbe = {};
		StartupTrace::TryGetCrashTraceSnapshot(diagnosticProbe);
		check("safe_failure_metadata", wcsstr(diagnosticProbe.lastHResultFailure,L"0x80004005") && wcsstr(diagnosticProbe.lastHResultFailure,L"stage=CP301") && wcsstr(diagnosticProbe.lastHResultFailure,L"operation-stage=CP402"));
		StartupTrace::CrashTraceSnapshot beforeErrors = {};
		StartupTrace::TryGetCrashTraceSnapshot(beforeErrors);
		const bool cut = testCase == L"within" || testCase == L"between";
		CString expected;
		const CString beforeCopy(html());
		if (role == L"target") expected = L"\x041A\x041E\x041F\x0418\x042F";
		else {
			m_doc->m_body.SetFocus();
			const bool rich = testCase == L"formatted";
			const bool whole = rich || testCase == L"paragraph";
			MSHTML::IHTMLTxtRangePtr range = select(rich ? L"copy-rich" : L"copy-src",
				(testCase == L"char" || testCase == L"repeat" || testCase == L"within") ? 6 : 0,
				whole ? -1 : ((testCase == L"char" || testCase == L"repeat" || testCase == L"within") ? 1 : 5));
			if (whole) {
				// Copy a complete ordinary paragraph. ID collision rejection has its
				// own sanitizer/runtime tests; it is not a COM paste failure.
				element(rich ? L"copy-rich" : L"copy-src")->removeAttribute(L"id", 0);
				m_doc->MarkSavePoint(); m_doc->MarkDocCP();
			}
			expected = whole ? CString(static_cast<LPCWSTR>(range->text)) : CString(static_cast<LPCWSTR>(range->text));
			BOOL handled = FALSE;
			if (cut) m_doc->m_body.OnCut(0, ID_EDIT_CUT, 0, handled);
			else m_doc->m_body.OnCopy(0, ID_EDIT_COPY, 0, handled);
			CString plain;
			check("native_copy_text", FbeClipboard::HtmlPasteSanitizer::ReadPlainText(m_hWnd, plain) && plain.TrimRight(L"\r\n") == expected.TrimRight(L"\r\n"));
			check("native_internal_marker", FbeClipboard::HtmlPasteSanitizer::HasInternalCopyMarker(m_hWnd));
			check("copy_nonempty", !expected.IsEmpty());
		}
		if (role == L"source") {
			CAtlFile ready; CheckError(ready.Create(readyBuffer, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS)); ready.Close();
			const ULONGLONG deadline = ::GetTickCount64() + 60000;
			while (::GetFileAttributesW(doneBuffer) == INVALID_FILE_ATTRIBUTES && ::GetTickCount64() < deadline) pump(10);
			check("cross_document_consumed", ::GetFileAttributesW(doneBuffer) != INVALID_FILE_ATTRIBUTES);
			check("source_unchanged", html() == beforeCopy && !m_doc->DocChanged());
		} else {
			const int repeats = testCase == L"repeat" ? 4 : 1;
			for (int repeat = 0; repeat < repeats; ++repeat) {
				LPCWSTR target = testCase == L"within" ? L"copy-src" : (testCase == L"section" ? L"paste-other" : L"paste-target");
				m_doc->m_body.SetFocus();
				MSHTML::IHTMLTxtRangePtr caret = select(target, 0, -1); caret->collapse(VARIANT_FALSE); caret->select();
				const CString before(html()); const int previousCount = count(text(), expected);
				const bool dirtyBefore = m_doc->DocChanged();
				CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_PASTE)));
				pump(250);
				const CString after(html());
				check("native_paste_text", count(text(), expected) == previousCount + 1);
				check("dirty_after_paste", m_doc->DocChanged());
				if (after == before) _com_issue_error(E_ABORT);
				CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
				check("undo_exact", html() == before && m_doc->DocChanged() == dirtyBefore);
				CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO)));
				check("redo_exact", html() == after);
			}
			if (testCase == L"formatted") {
				const CString markup(html());
				check("formatting_and_note", markup.Find(L"<STRONG>") >= 0 && count(text(), L"\x0436\x0438\x0440\x043D\x044B\x0439") == 2 && element(L"note1"));
			}
			const CString expectedAfter(text()), filename(m_doc->m_filename);
			check("saved", m_doc->Save());
			check("reopened", LoadFile(filename) == OK);
			document = m_doc->m_body.Document();
			check("reopen_text", text() == expectedAfter);
		}
		StartupTrace::CrashTraceSnapshot afterErrors = {};
		StartupTrace::TryGetCrashTraceSnapshot(afterErrors);
		check("no_new_com_error", wcscmp(beforeErrors.lastHResultFailure, afterErrors.lastHResultFailure) == 0);
	} catch (const _com_error& error) { report.AppendFormat("hresult=0x%08lX\n", static_cast<unsigned long>(error.Error())); passed = false; }
	catch (const std::bad_alloc&) { report += "hresult=0x8007000E\n"; passed = false; }
	if (restoreClipboard) {
		HRESULT restored=E_FAIL,flushed=S_OK;
		for(int attempt=0;attempt<10;++attempt) {
			restored=::OleSetClipboard(previousClipboard);
			if(SUCCEEDED(restored)) break;
			pump(20);
		}
		if(SUCCEEDED(restored) && previousClipboard) {
			for(int attempt=0;attempt<10;++attempt) {
				flushed=::OleFlushClipboard();if(SUCCEEDED(flushed)) break;pump(20);
			}
		}
		if(FAILED(restored) || FAILED(flushed)) report.AppendFormat("clipboard_restore_hresult=0x%08lX\nclipboard_flush_hresult=0x%08lX\n",static_cast<unsigned long>(restored),static_cast<unsigned long>(flushed));
		check("clipboard_restored", SUCCEEDED(restored) && SUCCEEDED(flushed));
	}
	report.AppendFormat("result=%s\n", passed ? "PASS" : "FAIL");
	DWORD written = 0; output.Seek(0,FILE_BEGIN); output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
	::PostQuitMessage(passed ? 0 : 1); return 0;
}
