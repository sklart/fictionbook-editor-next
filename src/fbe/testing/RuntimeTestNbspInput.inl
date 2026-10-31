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
	const char* stage = "setup";
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
			MSHTML::IHTMLDOMNodePtr root(p), child(root->firstChild);
			while (child && child != root) {
				if (child->nodeType == 3) result += static_cast<LPCWSTR>(_bstr_t(child->nodeValue));
				if (child->firstChild) child = child->firstChild;
				else {
					while (child && child != root && !child->nextSibling) child = child->parentNode;
					if (child && child != root) child = child->nextSibling;
				}
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
		stage = "initial-document";
		MSHTML::IHTMLElementPtr p = paragraph();
		if (!p || text() != L"word1word2") CheckError(E_FAIL);
		stage = "prior-element";
		MSHTML::IHTMLElementPtr prior(MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"nbsp-prior"));
		if (!prior) CheckError(E_FAIL);
		const CString priorOriginal(static_cast<LPCWSTR>(prior->outerHTML));
		MSHTML::IHTMLTxtRangePtr priorRange(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
		priorRange->moveToElementText(prior); priorRange->collapse(VARIANT_TRUE);
		priorRange->move(L"character", 2); priorRange->moveEnd(L"character", 1); priorRange->select(); pump();
		m_doc->m_body.BeginUndoUnit(L"prior test edit");
		try { priorRange->text = L"I"; }
		catch (...) { m_doc->m_body.EndUndoUnit(); throw; }
		m_doc->m_body.EndUndoUnit(); pump();
		stage = "prior-input";
		const CString priorEdited(static_cast<LPCWSTR>(prior->outerHTML));
		if (priorOriginal == priorEdited) CheckError(E_FAIL);
		const CString original = state(); const int initialUndo = historyCount(false);
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
		m_doc->m_body.SetFocus();
		range->moveToElementText(p); range->collapse(VARIANT_TRUE); range->move(L"character", 5); range->select(); pump();
		const HWND focus = ::GetFocus();
		const bool focusInside = focus && (focus == m_doc->m_body.m_hWnd || ::IsChild(m_doc->m_body.m_hWnd, focus));
		if (!focusInside) CheckError(E_FAIL);
		stage = "first-space";
		::SendMessageW(focus, WM_CHAR, L' ', 1); pump();
		const CString first = state(), firstText = text(); const int firstUndo = historyCount(false);
		stage = "second-space";
		::SendMessageW(focus, WM_CHAR, L' ', 1); pump();
		const CString second = state(), secondText = text(); const int secondUndo = historyCount(false);
		MSHTML::IHTMLTxtRangePtr caretAfter(document->selection->createRange());
		MSHTML::IHTMLTxtRangePtr prefix(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
		prefix->moveToElementText(paragraph()); prefix->setEndPoint(L"EndToStart", caretAfter);
		const bool caretPosition = caretAfter->compareEndPoints(L"StartToEnd", caretAfter) == 0 &&
			CString(static_cast<LPCWSTR>(prefix->text)).GetLength() == 7;
		stage = "undo-redo";
		m_doc->m_body.SetFocus(); pump();
		const HWND undoFocus = ::GetFocus();
		const bool routedFocus = undoFocus == m_view.GetActiveWnd() || ::IsChild(m_view.GetActiveWnd(), undoFocus);
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
		const CString undo1 = state(), undo1Text = text(); const int afterUndo = historyCount(false), afterUndoRedo = historyCount(true);
		int steps = 1; CString undo2, undo2Text;
		if (undo1 != original) { SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump(); undo2 = state(); undo2Text = text(); ++steps; }
		const bool restored = (steps == 1 ? undo1 : undo2) == original;
		bool priorHistory = CString(static_cast<LPCWSTR>(prior->outerHTML)) == priorEdited;
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
		priorHistory = priorHistory && CString(static_cast<LPCWSTR>(prior->outerHTML)) == priorOriginal && state() == original;
		SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
		priorHistory = priorHistory && CString(static_cast<LPCWSTR>(prior->outerHTML)) == priorEdited && state() == original;
		SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
		const CString redo1 = state(), redo1Text = text(); CString redo2, redo2Text;
		if (steps == 2) { SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump(); redo2 = state(); redo2Text = text(); }
		const bool replay = steps == 1 ? redo1 == second : undo1 == first && redo1 == first && redo2 == second;
		// A space typed over a selection must follow the ordinary editor route,
		// not the special handling for a collapsed caret after a space.
		stage = "selection-input";
		MSHTML::IHTMLTxtRangePtr selected(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
		selected->moveToElementText(paragraph()); selected->collapse(VARIANT_TRUE);
		selected->move(L"character", 8);
		selected->moveEnd(L"character", 1); selected->select(); pump();
		const HWND selectionFocus = ::GetFocus();
		if (!selectionFocus) CheckError(E_FAIL);
		::SendMessageW(selectionFocus, WM_CHAR, L' ', 1); pump();
		const CString selectedText = text();
		const bool selectionInput = selectedText.GetLength() == secondText.GetLength() &&
			selectedText.Left(8) == secondText.Left(8) && selectedText[8] == L' ' && selectedText.Mid(9) == secondText.Mid(9);
		m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
		const bool selectionUndo = state() == second;
		bool thirdInput = false, thirdUndo = false, thirdRedo = false, thirdTested = false;
		if (_Settings.GetNBSPChar() != L"\u00a0") {
			thirdTested = true;
			stage = "third-space";
			MSHTML::IHTMLTxtRangePtr thirdRange(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
			thirdRange->moveToElementText(paragraph()); thirdRange->collapse(VARIANT_TRUE);
			thirdRange->move(L"character", 7); thirdRange->select(); pump();
			::SendMessageW(::GetFocus(), WM_CHAR, L' ', 1); pump();
			const CString third = state(), thirdText = text();
			thirdInput = thirdText.GetLength() == 13 && thirdText.Left(5) == L"word1" && thirdText.Right(5) == L"word2" &&
				thirdText[5] == _Settings.GetNBSPChar()[0] && thirdText[6] == _Settings.GetNBSPChar()[0] && thirdText[7] == L' ';
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump(); thirdUndo = state() == second;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump(); thirdRedo = state() == third;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump(); thirdUndo = thirdUndo && state() == second;
		}
		stage = "save-reopen";
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
		CStringA history; history.Format("undo1-text=%s\nundo2-text=%s\nredo1-text=%s\nredo2-text=%s\ncaret-position=%d\nprior-history=%d\nselection-input=%d\nselection-undo=%d\nthird-tested=%d\nthird-input=%d\nthird-undo=%d\nthird-redo=%d\n",
			static_cast<LPCSTR>(codepoints(undo1Text)), static_cast<LPCSTR>(codepoints(undo2Text)),
			static_cast<LPCSTR>(codepoints(redo1Text)), static_cast<LPCSTR>(codepoints(redo2Text)), caretPosition, priorHistory, selectionInput, selectionUndo, thirdTested, thirdInput, thirdUndo, thirdRedo); report += history;
		CStringA counts; counts.Format("undo-count-after-undo=%d\nredo-count-after-undo=%d\n", afterUndo, afterUndoRedo); report += counts;
		passed = focusInside && caretPosition && priorHistory && restored && replay && selectionInput && selectionUndo &&
			(!thirdTested || (thirdInput && thirdUndo && thirdRedo)) && validText && saved && reopened && xmlMatches;
	}
	catch (const _com_error& error) { CStringA line; line.Format("stage=%s\nerror=0x%08lX\n", stage, static_cast<unsigned long>(error.Error())); report += line; }
	report += passed ? "result=pass\n" : "result=fail\n";
	DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Flush(); output.Close();
	::PostQuitMessage(passed ? 0 : 1); return 0;
}
