if (IsFbeTestScenario(L"nbsp-input-runtime"))
{
	CStringA report;
	auto pump = []() {
		const ULONGLONG until = ::GetTickCount64() + 100;
		while (::GetTickCount64() < until) {
			MSG message = {};
			while (::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
				::TranslateMessage(&message); ::DispatchMessageW(&message);
			}
			::Sleep(1);
		}
	};
	auto codepoints = [](const CString& text) {
		CStringA result;
		for (int i = 0; i < text.GetLength(); ++i) {
			CStringA part; part.Format("%s%04X", i ? "," : "", static_cast<unsigned>(text[i])); result += part;
		}
		return result;
	};
	bool passed = false;
	try {
		wchar_t target[MAX_PATH] = {};
		if (!::GetEnvironmentVariableW(L"FBE_NEXT_TEST_NBSP_OUTPUT", target, _countof(target))) CheckError(E_INVALIDARG);
		ShowView(BODY); m_doc->m_body.SetFocus(); pump();
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		auto paragraph = [&]() -> MSHTML::IHTMLElementPtr {
			return MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"nbsp-probe");
		};
		auto state = [&]() -> CString {
			MSHTML::IHTMLElementPtr p = paragraph();
			if (!p) CheckError(E_FAIL);
			return CString(static_cast<LPCWSTR>(p->outerHTML));
		};
		auto text = [&]() -> CString {
			MSHTML::IHTMLElementPtr p = paragraph();
			if (!p) CheckError(E_FAIL);
			CString result;
			for (MSHTML::IHTMLDOMNodePtr child = MSHTML::IHTMLDOMNodePtr(p)->firstChild; child; child = child->nextSibling) {
				if (child->nodeType != 3) CheckError(E_FAIL);
				result += static_cast<LPCWSTR>(_bstr_t(child->nodeValue));
			}
			return result;
		};
		CComPtr<IOleUndoManager> undoManager;
		IServiceProviderPtr provider(document);
		CheckError(provider->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&undoManager)));
		auto historyCount = [&](bool redo) -> int {
			CComPtr<IEnumOleUndoUnits> entries; CheckError(redo ? undoManager->EnumRedoable(&entries) : undoManager->EnumUndoable(&entries));
			int count = 0;
			for (;;) { CComPtr<IOleUndoUnit> unit; const HRESULT hr = entries->Next(1, &unit, NULL); if (hr == S_FALSE) break; CheckError(hr); ++count; }
			return count;
		};
		MSHTML::IHTMLElementPtr p = paragraph();
		if (!p || text() != L"word1word2") CheckError(E_FAIL);
		const CString original = state(); const int initialUndo = historyCount(false);
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
		range->moveToElementText(p); range->collapse(VARIANT_TRUE); range->move(L"character", 5); range->select(); pump();
		const HWND focus = ::GetFocus();
		const bool focusInside = focus && (focus == m_doc->m_body.m_hWnd || ::IsChild(m_doc->m_body.m_hWnd, focus));
		if (!focusInside) CheckError(E_FAIL);
		::SendMessageW(focus, WM_CHAR, L' ', 1); pump();
		const CString first = state(), firstText = text(); const int firstUndo = historyCount(false);
		::SendMessageW(focus, WM_CHAR, L' ', 1); pump();
		const CString second = state(), secondText = text(); const int secondUndo = historyCount(false);
		m_doc->m_body.SetFocus(); pump();
		const HWND undoFocus = ::GetFocus();
		const bool routedFocus = undoFocus == m_view.GetActiveWnd() || ::IsChild(m_view.GetActiveWnd(), undoFocus);
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
		const CString undo1 = state(), undo1Text = text(); const int afterUndo = historyCount(false), afterUndoRedo = historyCount(true);
		int steps = 1; CString undo2, undo2Text;
		if (undo1 != original) { SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump(); undo2 = state(); undo2Text = text(); ++steps; }
		const bool restored = (steps == 1 ? undo1 : undo2) == original;
		SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
		const CString redo1 = state(), redo1Text = text(); CString redo2, redo2Text;
		if (steps == 2) { SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump(); redo2 = state(); redo2Text = text(); }
		const bool replay = steps == 1 ? redo1 == second : undo1 == first && redo1 == first && redo2 == second;
		const CString xml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
		const bool validText = firstText == L"word1 word2" && secondText.GetLength() == 12 &&
			secondText.Left(5) == L"word1" && secondText.Right(5) == L"word2" &&
			(secondText[5] == L' ' || secondText[5] == L'\x00a0' || secondText[5] == _Settings.GetNBSPChar()[0]) &&
			(secondText[6] == L' ' || secondText[6] == L'\x00a0' || secondText[6] == _Settings.GetNBSPChar()[0]);
		const bool saved = m_doc->Save(target); pump();
		const bool reopened = saved && LoadFile(target) == OK; pump();
		const CString reopenedXml(reopened ? static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml) : L"");
		const bool xmlMatches = reopened && xml == reopenedXml;
		report.Format("mode=%s\nfocus=%d\nundo-focus-routed=%d\nundo-count-before=%d\nundo-count-first=%d\nundo-count-second=%d\nfirst-text=%s\nsecond-text=%s\nundo-steps=%d\nrestored=%d\nreplay=%d\nvalid-text=%d\nsaved=%d\nreopened=%d\nxml-matches=%d\n",
			static_cast<LPCSTR>(codepoints(_Settings.GetNBSPChar())), focusInside, routedFocus, initialUndo, firstUndo, secondUndo, static_cast<LPCSTR>(codepoints(firstText)),
			static_cast<LPCSTR>(codepoints(secondText)), steps, restored, replay, validText, saved, reopened, xmlMatches);
		CStringA history; history.Format("undo1-text=%s\nundo2-text=%s\nredo1-text=%s\nredo2-text=%s\n",
			static_cast<LPCSTR>(codepoints(undo1Text)), static_cast<LPCSTR>(codepoints(undo2Text)),
			static_cast<LPCSTR>(codepoints(redo1Text)), static_cast<LPCSTR>(codepoints(redo2Text))); report += history;
		CStringA counts; counts.Format("undo-count-after-undo=%d\nredo-count-after-undo=%d\n", afterUndo, afterUndoRedo); report += counts;
		passed = focusInside && restored && replay && validText && saved && reopened && xmlMatches;
	}
	catch (const _com_error& error) { CStringA line; line.Format("error=0x%08lX\n", static_cast<unsigned long>(error.Error())); report += line; }
	report += passed ? "result=pass\n" : "result=fail\n";
	DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Flush(); output.Close();
	::PostQuitMessage(passed ? 0 : 1); return 0;
}
