if (IsFbeTestScenario(L"community-body-input"))
{
	CStringA report; bool passed = true; int cases = 0;
	auto pump = []() {
		const ULONGLONG until = ::GetTickCount64() + 80;
		while (::GetTickCount64() < until) {
			MSG msg = {}; while (::PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&msg); ::DispatchMessageW(&msg); }
			::Sleep(1);
		}
	};
	auto sendKey = [&](HWND target, UINT key) {
		const LPARAM scan = static_cast<LPARAM>(::MapVirtualKeyW(key, MAPVK_VK_TO_VSC) << 16);
		const LPARAM extended = key == VK_DELETE ? static_cast<LPARAM>(1u << 24) : 0;
		MSG message = {}; message.hwnd = target; message.message = WM_KEYDOWN; message.wParam = key;
		message.lParam = 1 | scan | extended;
		if (!PreTranslateMessage(&message)) { ::TranslateMessage(&message); ::DispatchMessageW(&message); }
		pump();
		message.message = WM_KEYUP; message.lParam |= static_cast<LPARAM>(0xC0000000u);
		if (!PreTranslateMessage(&message)) ::DispatchMessageW(&message);
		pump();
	};
	try {
		struct InputCase { const wchar_t* id; const wchar_t* tag; int position; };
		const InputCase targets[] = {
			{ L"input-first", NULL, 0 }, { L"input-middle", NULL, 1 },
			{ L"input-last", NULL, 2 }, { L"input-middle", L"EM", 1 }
		};
		const wchar_t* actions[] = { L"type", L"backspace", L"delete" };
		for (const InputCase& test : targets) for (const wchar_t* action : actions) {
			ShowView(BODY); m_doc->m_body.SetFocus(); pump();
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr root(MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"fbw_body"));
			MSHTML::IHTMLElementPtr paragraph(MSHTML::IHTMLDocument3Ptr(document)->getElementById(test.id));
			if (!root || !paragraph) CheckError(E_FAIL);
			MSHTML::IHTMLElementPtr target(paragraph);
			if (test.tag) {
				MSHTML::IHTMLElementCollectionPtr children(MSHTML::IHTMLElement2Ptr(paragraph)->getElementsByTagName(test.tag));
				target = children && children->length ? MSHTML::IHTMLElementPtr(children->item(_variant_t(0L), _variant_t())) : nullptr;
			}
			if (!target) CheckError(E_FAIL);
			const CString originalText(static_cast<LPCWSTR>(target->innerText));
			const int start = test.position == 0 ? 0 : test.position == 1 ? originalText.GetLength() / 2 : originalText.GetLength() - 2;
			const int length = 2;
			const CString originalHtml(static_cast<LPCWSTR>(root->innerHTML));
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
			if (start == 0 && !test.tag) { m_doc->m_body.GoTo(target, false); range = document->selection->createRange(); }
			else { range->moveToElementText(target); range->collapse(VARIANT_TRUE); range->move(L"character", start); }
			range->moveEnd(L"character", length); range->select(); pump();
			const CString selected(static_cast<LPCWSTR>(range->text));
			ShowView(SOURCE); pump(); ShowView(BODY); pump();
			const HWND focus = ::GetFocus(); wchar_t className[128] = {};
			if (focus) ::GetClassNameW(focus, className, _countof(className));
			const bool focusOk = focus && ::IsChild(m_doc->m_body.m_hWnd, focus) && wcscmp(className, L"Internet Explorer_Server") == 0;
			MSHTML::IHTMLTxtRangePtr returned(document->selection->createRange());
			const bool selectionOk = returned && CString(static_cast<LPCWSTR>(returned->text)) == selected && selected.GetLength() == length;
			if (focusOk && selectionOk) {
				if (wcscmp(action, L"delete") == 0) sendKey(focus, VK_DELETE);
				else if (wcscmp(action, L"backspace") == 0) sendKey(focus, VK_BACK);
				else ::SendMessageW(focus, WM_CHAR, L'X', 1);
				pump();
			}
			CString expected(originalText); expected.Delete(start, length);
			if (wcscmp(action, L"type") == 0) expected.Insert(start, L'X');
			const bool edited = CString(static_cast<LPCWSTR>(target->innerText)) == expected;
			const CString afterHtml(static_cast<LPCWSTR>(root->innerHTML));
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
			const bool undo = CString(static_cast<LPCWSTR>(root->innerHTML)) == originalHtml;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
			const bool redo = CString(static_cast<LPCWSTR>(root->innerHTML)) == afterHtml && afterHtml != originalHtml;
			m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
			const bool restored = CString(static_cast<LPCWSTR>(root->innerHTML)) == originalHtml;
			const bool ok = focusOk && selectionOk && edited && undo && redo && restored;
			report.AppendFormat("case-%S-%S-%d-%S=%d;focus=%d;selection=%d;edited=%d;undo=%d;redo=%d;restored=%d\n",
				test.id, test.tag ? test.tag : L"plain", test.position, action, ok, focusOk, selectionOk, edited, undo, redo, restored);
			passed = passed && ok; ++cases;
		}
		ShowView(BODY); m_doc->m_body.SetFocus(); pump();
		MSHTML::IHTMLDocument2Ptr arrowDocument(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr arrowParagraph(MSHTML::IHTMLDocument3Ptr(arrowDocument)->getElementById(L"input-middle"));
		if (!arrowParagraph) CheckError(E_FAIL);
		MSHTML::IHTMLTxtRangePtr origin(MSHTML::IHTMLBodyElementPtr(arrowDocument->body)->createTextRange());
		origin->moveToElementText(arrowParagraph); origin->collapse(VARIANT_TRUE); origin->move(L"character", 2); origin->select(); pump();
		ShowView(SOURCE); pump(); ShowView(BODY); pump();
		const HWND arrowFocus = ::GetFocus();
		const bool arrowFocusOk = arrowFocus && ::IsChild(m_doc->m_body.m_hWnd, arrowFocus);
		MSHTML::IHTMLTxtRangePtr expectedRight(origin->duplicate()); expectedRight->move(L"character", 1);
		if (arrowFocusOk) sendKey(arrowFocus, VK_RIGHT);
		MSHTML::IHTMLTxtRangePtr arrowActual(arrowDocument->selection->createRange());
		const bool right = arrowFocusOk && arrowActual->compareEndPoints(L"StartToStart", expectedRight) == 0 &&
			arrowActual->compareEndPoints(L"StartToEnd", arrowActual) == 0;
		if (arrowFocusOk) sendKey(arrowFocus, VK_LEFT);
		arrowActual = arrowDocument->selection->createRange();
		const bool left = arrowActual && arrowActual->compareEndPoints(L"StartToStart", origin) == 0 &&
			arrowActual->compareEndPoints(L"StartToEnd", arrowActual) == 0;
		BYTE savedKeys[256] = {}, shiftKeys[256] = {};
		const bool keysRead = ::GetKeyboardState(savedKeys) != FALSE;
		if (keysRead) memcpy(shiftKeys, savedKeys, sizeof(shiftKeys));
		shiftKeys[VK_SHIFT] |= 0x80; shiftKeys[VK_LSHIFT] |= 0x80;
		bool shiftRight = false, shiftLeft = false;
		if (keysRead && arrowFocusOk && ::SetKeyboardState(shiftKeys)) {
			try {
				sendKey(arrowFocus, VK_RIGHT);
				arrowActual = arrowDocument->selection->createRange();
				shiftRight = arrowActual && arrowActual->compareEndPoints(L"StartToStart", origin) == 0 &&
					arrowActual->compareEndPoints(L"EndToStart", expectedRight) == 0;
				sendKey(arrowFocus, VK_LEFT);
				arrowActual = arrowDocument->selection->createRange();
				shiftLeft = arrowActual && arrowActual->compareEndPoints(L"StartToStart", origin) == 0 &&
					arrowActual->compareEndPoints(L"StartToEnd", arrowActual) == 0;
			} catch (...) { ::SetKeyboardState(savedKeys); throw; }
			::SetKeyboardState(savedKeys);
		}
		report.AppendFormat("arrow-focus=%d\narrow-right=%d\narrow-left=%d\nshift-right=%d\nshift-left=%d\n",
			arrowFocusOk, right, left, shiftRight, shiftLeft);
		passed = passed && right && left && shiftRight && shiftLeft;
		report += "clipboard=NOT_TESTED\n";
		wchar_t outputPath[MAX_PATH] = {};
		if (!::GetEnvironmentVariableW(L"FBE_NEXT_TEST_BODY_INPUT_OUTPUT", outputPath, _countof(outputPath))) CheckError(E_INVALIDARG);
		ShowView(BODY); m_doc->m_body.SetFocus(); pump();
		MSHTML::IHTMLDocument2Ptr finalDocument(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr finalParagraph(MSHTML::IHTMLDocument3Ptr(finalDocument)->getElementById(L"input-first"));
		if (!finalParagraph) CheckError(E_FAIL);
		MSHTML::IHTMLTxtRangePtr finalRange(MSHTML::IHTMLBodyElementPtr(finalDocument->body)->createTextRange());
		finalRange->moveToElementText(finalParagraph); finalRange->collapse(VARIANT_TRUE); finalRange->move(L"character", 1);
		finalRange->moveEnd(L"character", 1); finalRange->select(); pump();
		ShowView(SOURCE); pump(); ShowView(BODY); pump();
		const HWND finalFocus = ::GetFocus();
		if (finalFocus && ::IsChild(m_doc->m_body.m_hWnd, finalFocus)) ::SendMessageW(finalFocus, WM_CHAR, L'Z', 1);
		pump();
		const bool finalEdited = CString(static_cast<LPCWSTR>(finalParagraph->innerText)) == L"BZginning text";
		const CString beforeSave(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
		const bool saved = m_doc->Save(outputPath); pump();
		const bool reopened = saved && LoadFile(outputPath) == OK; pump();
		const bool sameXml = reopened && CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == beforeSave;
		report.AppendFormat("final-edited=%d\nsave=%d\nreopen=%d\nxml-equal=%d\n", finalEdited, saved, reopened, sameXml);
		passed = passed && finalEdited && saved && reopened && sameXml;
	}
	catch (const _com_error& error) { report.AppendFormat("hresult=0x%08lX\n", static_cast<unsigned long>(error.Error())); passed = false; }
	report.AppendFormat("cases=%d\nresult=%s\n", cases, passed ? "PASS" : "FAIL");
	DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Flush(); output.Close();
	::PostQuitMessage(passed ? 0 : 1); return 0;
}
