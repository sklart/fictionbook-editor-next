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
		const CString configuredMarker = _Settings.GetNBSPChar();
		ShowView(BODY); m_doc->m_body.SetFocus(); pump();
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		auto paragraph = [&]() -> MSHTML::IHTMLElementPtr {
			return MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"nbsp-probe");
		};
		auto state = [&]() -> CString {
			MSHTML::IHTMLElementPtr p = paragraph();
			if (!p) CheckError(E_FAIL);
			CString html(static_cast<LPCWSTR>(p->outerHTML));
			// MSHTML can serialize this fixture's internal link as either an
			// absolute document URL or its equivalent fragment after view changes.
			const int href = html.Find(L"href=\"file:///");
			const int fragment = href >= 0 ? html.Find(L"#nbsp-probe\"", href) : -1;
			if (fragment >= 0 && html.Find(L'\"', href + 6) == fragment + 11)
				html.Delete(href + 6, fragment - href - 6);
			return html;
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
		const CString firstXml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
		stage = "second-space";
		::SendMessageW(focus, WM_CHAR, L' ', 1); pump();
		const CString second = state(), secondText = text(); const int secondUndo = historyCount(false);
		MSHTML::IHTMLTxtRangePtr caretAfter(document->selection->createRange());
		MSHTML::IHTMLTxtRangePtr prefix(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
		prefix->moveToElementText(paragraph()); prefix->setEndPoint(L"EndToStart", caretAfter);
		const bool caretPosition = caretAfter->compareEndPoints(L"StartToEnd", caretAfter) == 0 &&
			CString(static_cast<LPCWSTR>(prefix->text)).GetLength() == 7;
		wchar_t sourceFlag[2] = {};
		const bool sourceTested = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_NBSP_SOURCE", sourceFlag, _countof(sourceFlag)) && sourceFlag[0] == L'1';
		bool sourceRoundTrip = true;
		bool sourceLinkValid = true;
		CString sourceAfter;
		if (sourceTested) {
			stage = "source-roundtrip";
			ShowView(SOURCE); pump();
			const bool sourceActive = IsSourceActive();
			if (sourceActive && second.Find(L"<A ") >= 0) {
				const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
				if (length < 0 || length > INT_MAX - 1) CheckError(E_FAIL);
				std::vector<char> source(static_cast<size_t>(length) + 1);
				m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
				sourceLinkValid = strstr(source.data(), "l:href=\"#nbsp-probe\"") != NULL;
			}
			ShowView(BODY); pump();
			document = m_doc->m_body.Document();
			prior = MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"nbsp-prior");
			sourceAfter = state();
			sourceRoundTrip = sourceActive && !IsSourceActive() && prior && sourceAfter == second;
		}
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
		wchar_t directFlag[2] = {};
		const bool directTested = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_NBSP_DIRECT", directFlag, _countof(directFlag)) && directFlag[0] == L'1';
		wchar_t directSelectionFlag[2] = {};
		const bool directSelectionTested = directTested && ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_NBSP_DIRECT_SELECTION", directSelectionFlag, _countof(directSelectionFlag)) && directSelectionFlag[0] == L'1';
		bool directInput = true, directUndo = true, directRedo = true;
		bool directSelectionInput = true, directSelectionUndo = true, directSelectionRedo = true;
		if (directTested) {
			stage = "direct-nbsp-input";
			MSHTML::IHTMLTxtRangePtr direct(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
			direct->moveToElementText(paragraph()); direct->collapse(VARIANT_TRUE); direct->move(L"character", 8); direct->select(); pump();
			const HWND directFocus = ::GetFocus();
			if (!directFocus || (directFocus != m_doc->m_body.m_hWnd && !::IsChild(m_doc->m_body.m_hWnd, directFocus))) CheckError(E_FAIL);
			::SendMessageW(directFocus, WM_CHAR, L'\u00a0', 1); pump();
			const CString inserted = state();
			CString expectedInserted(second);
			const CString htmlMarker = configuredMarker == L"\u00a0" ? CString(L"&nbsp;") : configuredMarker;
			const int expectedReplacements = expectedInserted.Replace(L"word2", CString(L"w") + htmlMarker + L"ord2");
			directInput = expectedReplacements == 1 && inserted == expectedInserted &&
				text() == secondText.Left(8) + configuredMarker + secondText.Mid(8);
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump(); directUndo = state() == second;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump(); directRedo = state() == inserted;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump(); directUndo = directUndo && state() == second;
			if (directSelectionTested) {
				stage = "direct-nbsp-selection";
				MSHTML::IHTMLTxtRangePtr selectedDirect(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
				selectedDirect->moveToElementText(paragraph()); selectedDirect->collapse(VARIANT_TRUE);
				selectedDirect->move(L"character", 8); selectedDirect->moveEnd(L"character", 1); selectedDirect->select(); pump();
				const HWND selectedFocus = ::GetFocus();
				if (!selectedFocus || (selectedFocus != m_doc->m_body.m_hWnd && !::IsChild(m_doc->m_body.m_hWnd, selectedFocus))) CheckError(E_FAIL);
				::SendMessageW(selectedFocus, WM_CHAR, L'\u00a0', 1); pump();
				const CString selectedState = state();
				CString expectedSelection(second);
				const int selectionReplacements = expectedSelection.Replace(L"word2", CString(L"w") + htmlMarker + L"rd2");
				directSelectionInput = selectionReplacements == 1 && selectedState == expectedSelection && text() == secondText.Left(8) + configuredMarker + secondText.Mid(9);
				m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump(); directSelectionUndo = state() == second;
				m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump(); directSelectionRedo = state() == selectedState;
				m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump(); directSelectionUndo = directSelectionUndo && state() == second;
			}
		}
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
		wchar_t toggleFlag[2] = {};
		const DWORD toggleLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_NBSP_TOGGLE", toggleFlag, _countof(toggleFlag));
		const bool otherNodeTest = toggleLength == 1 && (toggleFlag[0] == L'8' || toggleFlag[0] == L'9');
		const bool toggleTested = toggleLength == 1 && (toggleFlag[0] == L'1' || toggleFlag[0] == L'2' || toggleFlag[0] == L'5' || toggleFlag[0] == L'6' || toggleFlag[0] == L'7' || otherNodeTest);
		const bool repeatTested = toggleLength == 1 && (toggleFlag[0] == L'3' || toggleFlag[0] == L'4');
		const bool toggleApplied = toggleTested && (toggleFlag[0] == L'1' || toggleFlag[0] == L'5' || toggleFlag[0] == L'6' || toggleFlag[0] == L'8');
		const bool emptyRedoTest = toggleLength == 1 && (toggleFlag[0] == L'6' || toggleFlag[0] == L'7');
		CString beforeEmptyRedoXml, editedEmptyRedoXml;
		bool emptyRedoReady = true;
		if(otherNodeTest) {
			stage = "other-node-edit";
			beforeEmptyRedoXml = static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml);
			MSHTML::IHTMLTxtRangePtr other(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
			other->moveToElementText(prior); other->collapse(VARIANT_TRUE);
			other->move(L"character", 2); other->moveEnd(L"character", 1); other->select(); pump();
			m_doc->m_body.BeginUndoUnit(L"other node test edit");
			try { other->text = L"i"; } catch (...) { m_doc->m_body.EndUndoUnit(); throw; }
			m_doc->m_body.EndUndoUnit(); pump();
			editedEmptyRedoXml = static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml);
			emptyRedoReady = historyCount(true) == 0 && beforeEmptyRedoXml != editedEmptyRedoXml;
		}
		if(emptyRedoTest) {
			stage = "empty-redo-edit";
			beforeEmptyRedoXml = static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml);
			MSHTML::IHTMLTxtRangePtr replacement(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
			replacement->moveToElementText(paragraph()); replacement->collapse(VARIANT_TRUE);
			replacement->move(L"character", 8); replacement->moveEnd(L"character", 1); replacement->select(); pump();
			m_doc->m_body.BeginUndoUnit(L"empty redo test edit");
			try { replacement->text = L"O"; }
			catch (...) { m_doc->m_body.EndUndoUnit(); throw; }
			m_doc->m_body.EndUndoUnit(); pump();
			editedEmptyRedoXml = static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml);
			emptyRedoReady = historyCount(true) == 0 && beforeEmptyRedoXml != editedEmptyRedoXml && text().Right(5) == L"wOrd2";
		}
		bool changedDisplay = true, unchangedOther = true, toggleRestored = true, toggleUndoRedo = true;
		CStringA toggleReport; toggleReport.Format("toggle-tested=%d\ntoggle-applied=%d\n", toggleTested, toggleApplied);
		if(toggleTested) {
		stage = "toggle-display";
		const CString markerBefore = _Settings.GetNBSPChar();
		const CString markerOther = markerBefore == L"\u00a0" ? CString(L"\u25a1") : CString(L"\u00a0");
		const CString beforeToggle = state();
		const CString xmlBeforeToggle(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
		const bool dirtyBeforeToggle = m_doc->DocChanged();
		CString expectedAfterToggle(beforeToggle);
		if(toggleApplied) {
			_Settings.SetNBSPChar(markerOther); ApplyConfChanges(false, false); pump();
			const CString textOther = text();
			const CString xmlOther(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
			changedDisplay = textOther.Find(markerOther) >= 0 && textOther.Find(markerBefore) < 0;
			if(toggleFlag[0] == L'5' || toggleFlag[0] == L'6' || toggleFlag[0] == L'8') expectedAfterToggle = state();
			unchangedOther = xmlOther == xmlBeforeToggle && m_doc->DocChanged() == dirtyBeforeToggle;
			if(toggleFlag[0] != L'5' && toggleFlag[0] != L'6' && toggleFlag[0] != L'8') { _Settings.SetNBSPChar(markerBefore); ApplyConfChanges(false, false); pump(); }
		}
		const CString xmlAfterToggle(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
		toggleRestored = state() == expectedAfterToggle && xmlAfterToggle == xmlBeforeToggle && m_doc->DocChanged() == dirtyBeforeToggle;
		const int toggleUndoCount = historyCount(false), toggleRedoCount = historyCount(true);
		m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
		const CString toggleUndo = state(), toggleUndoText = text();
		const CString toggleUndoXml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
		const int toggleUndoCountAfter = historyCount(false), toggleRedoCountAfter = historyCount(true);
		m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
		const CString toggleRedo = state(), toggleRedoText = text();
		toggleUndoRedo = (emptyRedoTest || otherNodeTest) ?
			emptyRedoReady && toggleUndoXml == beforeEmptyRedoXml &&
				CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == editedEmptyRedoXml && toggleRedo == expectedAfterToggle :
			toggleUndo == (steps == 2 ? first : original) && toggleRedo == expectedAfterToggle;
		bool earlierUndo = true, earlierRedo = true;
		if(otherNodeTest) {
			const bool otherUndo = toggleUndoXml == beforeEmptyRedoXml;
			const bool otherRedo = CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == editedEmptyRedoXml;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
			earlierUndo = CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == firstXml;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
			earlierRedo = CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == beforeEmptyRedoXml;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
			toggleUndoRedo = toggleUndoRedo && earlierUndo && earlierRedo;
			CStringA otherReport; otherReport.Format("other-node-tested=1\nother-node-ready=%d\nother-node-undo=%d\nother-node-redo=%d\nother-node-earlier-undo=%d\nother-node-earlier-redo=%d\n", emptyRedoReady, otherUndo, otherRedo, earlierUndo, earlierRedo); toggleReport += otherReport;
		}
		if(emptyRedoTest) {
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
			const bool latestUndoAgain = CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == beforeEmptyRedoXml;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
			earlierUndo = latestUndoAgain && CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == firstXml;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
			const bool earlierRedoStep = CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == beforeEmptyRedoXml;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
			earlierRedo = earlierRedoStep && CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == editedEmptyRedoXml;
			toggleUndoRedo = toggleUndoRedo && toggleRedoCount == 0 && earlierUndo && earlierRedo;
		}
		CStringA emptyRedoReport; emptyRedoReport.Format("empty-redo-tested=%d\nempty-redo-ready=%d\nearlier-undo=%d\nearlier-redo=%d\n", emptyRedoTest, emptyRedoReady, earlierUndo, earlierRedo); toggleReport += emptyRedoReport;
		CStringA toggleDetail; toggleDetail.Format("toggle-display=%d\ntoggle-xml-unchanged=%d\ntoggle-restored=%d\ntoggle-undo-redo=%d\ntoggle-undo-text=%s\ntoggle-redo-text=%s\ntoggle-undo-count=%d\ntoggle-redo-count=%d\ntoggle-undo-count-after=%d\ntoggle-redo-count-after=%d\n",
			changedDisplay, unchangedOther, toggleRestored, toggleUndoRedo, static_cast<LPCSTR>(codepoints(toggleUndoText)), static_cast<LPCSTR>(codepoints(toggleRedoText)),
			toggleUndoCount, toggleRedoCount, toggleUndoCountAfter, toggleRedoCountAfter);
		toggleReport += toggleDetail;
		}
		stage = "save-reopen";
		bool directSaveInput = true;
		if (directTested) {
			MSHTML::IHTMLTxtRangePtr directSave(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
			directSave->moveToElementText(paragraph()); directSave->collapse(VARIANT_TRUE); directSave->move(L"character", 8);
			if (directSelectionTested) directSave->moveEnd(L"character", 1);
			directSave->select(); pump();
			const HWND directSaveFocus = ::GetFocus();
			if (!directSaveFocus || (directSaveFocus != m_doc->m_body.m_hWnd && !::IsChild(m_doc->m_body.m_hWnd, directSaveFocus))) CheckError(E_FAIL);
			::SendMessageW(directSaveFocus, WM_CHAR, L'\u00a0', 1); pump();
			CString expectedSaved(second);
			const CString htmlMarker = configuredMarker == L"\u00a0" ? CString(L"&nbsp;") : configuredMarker;
			const int replacements = expectedSaved.Replace(L"word2", CString(L"w") + htmlMarker + (directSelectionTested ? L"rd2" : L"ord2"));
			directSaveInput = replacements == 1 && state() == expectedSaved;
		}
		const CString xml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
		const bool validText = firstText == L"word1 word2" && secondText.GetLength() == 12 &&
			secondText.Left(5) == L"word1" && secondText.Right(5) == L"word2" &&
			(secondText[5] == L' ' || secondText[5] == L'\x00a0' || secondText[5] == configuredMarker[0]) &&
			(secondText[6] == L' ' || secondText[6] == L'\x00a0' || secondText[6] == configuredMarker[0]);
		const bool saved = m_doc->Save(target); pump();
		const bool reopened = saved && LoadFile(target) == OK; pump();
		const CString reopenedXml(reopened ? static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml) : L"");
		const bool xmlMatches = reopened && xml == reopenedXml;
		bool repeatPreserved = true, repeatInserted = true, repeatRoundTrip = true;
		if(repeatTested && reopened) {
			stage = "repeat-settings";
			const CString previousMarker = _Settings.GetNBSPChar();
			if(previousMarker != L"\u25a1") CheckError(E_INVALIDARG);
			_Settings.SetNBSPChar(toggleFlag[0] == L'4' ? L"\u25ab" : L"\u00a0"); ApplyConfChanges(false, false); pump();
			document = m_doc->m_body.Document(); ShowView(BODY); m_doc->m_body.SetFocus(); pump();
			MSHTML::IHTMLTxtRangePtr literal(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
			literal->moveToElementText(paragraph()); literal->collapse(VARIANT_FALSE); literal->select(); pump();
			const HWND literalFocus = ::GetFocus();
			if(!literalFocus || (literalFocus != m_doc->m_body.m_hWnd && !::IsChild(m_doc->m_body.m_hWnd, literalFocus))) CheckError(E_FAIL);
			::SendMessageW(literalFocus, WM_CHAR, L'\u25a1', 1); pump();
			const CString literalBefore = text(), stateBeforeRepeat = state();
			const CString xmlBeforeRepeat(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
			repeatInserted = literalBefore.Right(1) == L"\u25a1" && xmlBeforeRepeat.Find(L"\u25a1") >= 0;
			ApplyConfChanges(false, false); pump();
			const CString xmlAfterRepeat(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
			repeatPreserved = state() == stateBeforeRepeat && text() == literalBefore && xmlAfterRepeat == xmlBeforeRepeat;
			const CString repeatPath = CString(target) + L".repeat.fb2";
			const bool repeatSaved = m_doc->Save(repeatPath); pump();
			const bool repeatOpened = repeatSaved && LoadFile(repeatPath) == OK; pump();
			repeatRoundTrip = repeatOpened && xmlBeforeRepeat == CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
		}
		report.Format("mode=%s\nfocus=%d\nundo-focus-routed=%d\nundo-count-before=%d\nundo-count-first=%d\nundo-count-second=%d\nfirst-text=%s\nsecond-text=%s\nundo-steps=%d\nrestored=%d\nreplay=%d\nvalid-text=%d\nsaved=%d\nreopened=%d\nxml-matches=%d\n",
			static_cast<LPCSTR>(codepoints(configuredMarker)), focusInside, routedFocus, initialUndo, firstUndo, secondUndo, static_cast<LPCSTR>(codepoints(firstText)),
			static_cast<LPCSTR>(codepoints(secondText)), steps, restored, replay, validText, saved, reopened, xmlMatches);
		CStringA history; history.Format("undo1-text=%s\nundo2-text=%s\nredo1-text=%s\nredo2-text=%s\ncaret-position=%d\nprior-history=%d\nselection-input=%d\nselection-undo=%d\nthird-tested=%d\nthird-input=%d\nthird-undo=%d\nthird-redo=%d\n",
			static_cast<LPCSTR>(codepoints(undo1Text)), static_cast<LPCSTR>(codepoints(undo2Text)),
			static_cast<LPCSTR>(codepoints(redo1Text)), static_cast<LPCSTR>(codepoints(redo2Text)), caretPosition, priorHistory, selectionInput, selectionUndo, thirdTested, thirdInput, thirdUndo, thirdRedo); report += history;
		report += toggleReport;
		CStringA directReport; directReport.Format("direct-tested=%d\ndirect-input=%d\ndirect-undo=%d\ndirect-redo=%d\ndirect-save-input=%d\n", directTested, directInput, directUndo, directRedo, directSaveInput); report += directReport;
		CStringA directSelectionReport; directSelectionReport.Format("direct-selection-tested=%d\ndirect-selection-input=%d\ndirect-selection-undo=%d\ndirect-selection-redo=%d\n", directSelectionTested, directSelectionInput, directSelectionUndo, directSelectionRedo); report += directSelectionReport;
		CStringA sourceReport; sourceReport.Format("source-tested=%d\nsource-roundtrip=%d\nsource-link-valid=%d\n", sourceTested, sourceRoundTrip, sourceLinkValid); report += sourceReport;
		if (sourceTested && !sourceRoundTrip) {
			CStringA details; details.Format("source-before-utf16=%s\nsource-after-utf16=%s\n", static_cast<LPCSTR>(codepoints(second)), static_cast<LPCSTR>(codepoints(sourceAfter))); report += details;
		}
		CStringA repeatReport; repeatReport.Format("repeat-applied=%d\nrepeat-inserted=%d\nrepeat-preserved=%d\nrepeat-roundtrip=%d\n", repeatTested, repeatInserted, repeatPreserved, repeatRoundTrip); report += repeatReport;
		CStringA counts; counts.Format("undo-count-after-undo=%d\nredo-count-after-undo=%d\n", afterUndo, afterUndoRedo); report += counts;
		passed = focusInside && caretPosition && sourceRoundTrip && sourceLinkValid && priorHistory && restored && replay && directInput && directUndo && directRedo && directSaveInput && directSelectionInput && directSelectionUndo && directSelectionRedo && selectionInput && selectionUndo &&
			(!thirdTested || (thirdInput && thirdUndo && thirdRedo)) && changedDisplay && unchangedOther && toggleRestored && toggleUndoRedo && repeatInserted && repeatPreserved && repeatRoundTrip && validText && saved && reopened && xmlMatches;
	}
	catch (const _com_error& error) { CStringA line; line.Format("stage=%s\nerror=0x%08lX\n", stage, static_cast<unsigned long>(error.Error())); report += line; }
	report += passed ? "result=pass\n" : "result=fail\n";
	DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Flush(); output.Close();
	::PostQuitMessage(passed ? 0 : 1); return 0;
}
