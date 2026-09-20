	if (IsFbeTestScenario(L"visual-dom-normalizer"))
	{
		CStringA header("case\tparagraphs\tempty_divs\tbrs\texact_paragraphs\tempty_line\tnbsp\tformatting\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr body(document ? document->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr editable(document ? document->all->item(L"fbw_body") : MSHTML::IHTMLElementPtr());
		if (!body || !editable)
		{
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		const CString originalHtml((const wchar_t*)editable->innerHTML);
		const CString pastePayload(L"paste-alpha\x00a0bold\r\npaste-beta\r\n\r\npaste-gamma");
		bool pasteNormalized = false;
		CComPtr<IDataObject> originalClipboard;
		if (SUCCEEDED(::OleGetClipboard(&originalClipboard)) && ::OpenClipboard(m_hWnd)) {
			::EmptyClipboard();
			const SIZE_T pasteBytes = static_cast<SIZE_T>(pastePayload.GetLength() + 1) * sizeof(wchar_t);
			HGLOBAL pasteMemory = ::GlobalAlloc(GMEM_MOVEABLE, pasteBytes);
			wchar_t* pasteText = pasteMemory ? static_cast<wchar_t*>(::GlobalLock(pasteMemory)) : nullptr;
			if (pasteText) { wcscpy_s(pasteText, pastePayload.GetLength() + 1, pastePayload); ::GlobalUnlock(pasteMemory); }
			const bool clipboardReady = pasteMemory && pasteText && ::SetClipboardData(CF_UNICODETEXT, pasteMemory);
			if (!clipboardReady && pasteMemory) ::GlobalFree(pasteMemory);
			::CloseClipboard();
			if (clipboardReady) {
				MSHTML::IHTMLElementCollectionPtr initialParagraphs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"P"));
				MSHTML::IHTMLElementPtr initialParagraph(initialParagraphs && initialParagraphs->length ? initialParagraphs->item(0L) : MSHTML::IHTMLElementPtr());
				if (initialParagraph) { MSHTML::IHTMLTxtRangePtr pasteRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange()); pasteRange->moveToElementText(initialParagraph); pasteRange->collapse(VARIANT_FALSE); pasteRange->select(); BOOL pasteHandled = FALSE; m_doc->m_body.OnPaste(0, ID_EDIT_PASTE, 0, pasteHandled); }
				editable = document->all->item(L"fbw_body");
				const CString pastedHtml(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
				const CString pastedText(editable ? static_cast<LPCWSTR>(editable->innerText) : L"");
				const int alpha = pastedText.Find(L"paste-alpha"), beta = pastedText.Find(L"paste-beta"), gamma = pastedText.Find(L"paste-gamma");
				pasteNormalized = alpha >= 0 && beta > alpha && gamma > beta && pastedHtml.Find(L"<BR") < 0 && (pastedHtml.Find(L"&nbsp;") >= 0 || pastedHtml.Find(L"\x00a0") >= 0);
			}
			::OleSetClipboard(originalClipboard);
		}
		CStringA pasteRow; pasteRow.Format("paste-normal\t0\t0\t0\t%d\t%d\t%d\t%d\t%s\r\n", pasteNormalized ? 1 : 0, pasteNormalized ? 1 : 0, pasteNormalized ? 1 : 0, 1, pasteNormalized ? "pass" : "fail");
		output.Write(pasteRow, static_cast<DWORD>(pasteRow.GetLength()), &written);
		editable = document->all->item(L"fbw_body");
		if (!editable) { output.Close(); ::PostQuitMessage(1); return 0; }
		editable->innerHTML = originalHtml.AllocSysString();
		struct NormalizerCase { const wchar_t* name; const wchar_t* html; const wchar_t* text[3]; long paragraphs; bool nbsp; bool formatting; };
		const NormalizerCase cases[] = {
			{ L"single-br", L"<DIV class='section'><P>alpha<BR>beta</P></DIV>", { L"alpha", L"beta", L"" }, 2, false, false },
			{ L"double-br", L"<DIV class='section'><P>alpha<BR><BR>beta</P></DIV>", { L"alpha", L"", L"beta" }, 3, false, false },
			{ L"empty-p", L"<DIV class='section'><P>alpha</P><P></P><P>beta</P></DIV>", { L"alpha", L"", L"beta" }, 3, false, false },
			{ L"nbsp-p", L"<DIV class='section'><P>alpha</P><P>&nbsp;</P><P>beta</P></DIV>", { L"alpha", L"\x00a0", L"beta" }, 3, true, false },
			{ L"formatted-br", L"<DIV class='section'><P><STRONG>alpha</STRONG><BR><EM>beta</EM></P></DIV>", { L"alpha", L"beta", L"" }, 2, false, true }
		};
		bool allPassed = true;
		for (const NormalizerCase& testCase : cases)
		{
			editable->innerHTML = testCase.html;
			m_doc->m_body.Normalize(MSHTML::IHTMLDOMNodePtr(body));
			// SplitBRs replaces outerHTML, invalidating the original element proxy.
			editable = document->all->item(L"fbw_body");
			if (!editable) { output.Close(); ::PostQuitMessage(1); return 0; }
			auto countElements = [&](const wchar_t* tagName) -> long {
				MSHTML::IHTMLElementCollectionPtr elements(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(tagName));
				return elements ? elements->length : 0;
			};
			long emptyDivs = 0;
			MSHTML::IHTMLElementCollectionPtr divs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV"));
			for (long index = 0; divs && index < divs->length; ++index) {
				MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
				if (div && CString((const wchar_t*)div->innerHTML).Trim().IsEmpty()) ++emptyDivs;
			}
			MSHTML::IHTMLElementCollectionPtr paragraphs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"P"));
			bool exactParagraphs = paragraphs && paragraphs->length == testCase.paragraphs;
			for (long index = 0; exactParagraphs && index < testCase.paragraphs; ++index) {
				MSHTML::IHTMLElementPtr paragraph(paragraphs->item(_variant_t(index), _variant_t()));
				exactParagraphs = paragraph && (testCase.nbsp && index == 1 || CString(static_cast<LPCWSTR>(paragraph->innerText)) == testCase.text[index]);
			}
			const bool emptyLine = paragraphs && paragraphs->length >= 3 && CString(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(paragraphs->item(_variant_t(1L), _variant_t()))->innerText)).IsEmpty();
			CString middleHtml;
			if (paragraphs && paragraphs->length >= 3) middleHtml = static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(paragraphs->item(_variant_t(1L), _variant_t()))->innerHTML);
			middleHtml.MakeLower();
			const bool nbsp = !testCase.nbsp || middleHtml.Find(L"&nbsp;") >= 0 || middleHtml.Find(L"\x00a0") >= 0;
			const bool formatting = !testCase.formatting || (countElements(L"STRONG") == 1 && countElements(L"EM") == 1);
			const bool passed = countElements(L"BR") == 0 && emptyDivs == 0 && exactParagraphs &&
				(wcscmp(testCase.name, L"double-br") || emptyLine) && nbsp && formatting;
			allPassed = allPassed && passed;
			CStringA row;
			row.Format("%S\t%ld\t%ld\t%ld\t%d\t%d\t%d\t%d\t%s\r\n", testCase.name, countElements(L"P"), emptyDivs, countElements(L"BR"), exactParagraphs, emptyLine, nbsp, formatting, passed ? "pass" : "fail");
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
		}
		// Restore the fixture before exercising the ordinary production save path.
		editable->innerHTML = originalHtml.AllocSysString();
		int validationLine = 0, validationColumn = 0;
		const bool saved = m_doc->Validate(validationLine, validationColumn) && m_doc->Save();
		output.Flush(); output.Close();
		::PostQuitMessage(allPassed && saved ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"link-navigation-runtime"))
	{
		CStringA header("nested\ttarget\tsame_document\tbroken\treturned_second\tinserted_navigate\tinserted_same_unique\tinserted_returned\tinserted_returned_origin\tinserted_before\tdeleted_origin_fallback\tother_document_opened\tunchanged\tsecond_dom_unchanged\tsecond_dirty_unchanged\torigin_dom_unchanged\torigin_dirty_unchanged\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr editable(FBELinkNavigation::GetEditableBody(document));
		MSHTML::IHTMLElementCollectionPtr links(editable ? MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"A") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr internal(links && links->length > 0 ? links->item(0L) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr second(links && links->length > 1 ? links->item(1L) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr broken(links && links->length > 2 ? links->item(2L) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr strongs(internal ? MSHTML::IHTMLElement2Ptr(internal)->getElementsByTagName(L"STRONG") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr nested(strongs && strongs->length ? strongs->item(0L) : MSHTML::IHTMLElementPtr());
		const CString before(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
		MSHTML::IHTMLElementPtr nearest(FBELinkNavigation::FindNearestLinkElement(nested, editable));
		const CString targetId(FBELinkNavigation::GetInternalLinkTargetId(document, nearest));
		MSHTML::IHTMLElementPtr target(FBELinkNavigation::FindTargetElement(document, targetId));
		CString documentUrl;
		try { MSHTML::IHTMLDocument4Ptr document4(document); if(document4) documentUrl = static_cast<LPCWSTR>(document4->URLUnencoded); }
		catch(const _com_error&) { }
		const CString sameDocumentHref = documentUrl + L"#note-1";
		const bool sameDocument = FBELinkNavigation::GetInternalTargetId(static_cast<LPCWSTR>(sameDocumentHref), static_cast<LPCWSTR>(documentUrl)) == L"note-1";
		auto navigationLeavesDocumentUntouched = [&](const std::function<bool()>& operation, bool& domUnchanged, bool& dirtyUnchanged) -> bool {
			const CString snapshot(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
			const bool dirty = m_doc->DocChanged();
			const bool result = operation();
			domUnchanged = editable && snapshot == CString(static_cast<LPCWSTR>(editable->innerHTML));
			dirtyUnchanged = dirty == m_doc->DocChanged();
			return result && domUnchanged && dirtyUnchanged;
		};
		bool secondDomUnchanged = false, secondDirtyUnchanged = false, originDomUnchanged = false, originDirtyUnchanged = false;
		const CString secondTargetId(FBELinkNavigation::GetInternalLinkTargetId(document, second));
		const long secondUniqueNumber = FBELinkNavigation::GetLinkUniqueNumber(second);
		const bool secondNavigated = second && secondTargetId == targetId && navigationLeavesDocumentUntouched([&]() { return m_doc->m_body.NavigateInternalLink(second, secondTargetId); }, secondDomUnchanged, secondDirtyUnchanged);
		OnGoToFootnote(0, ID_GOTO_FOOTNOTE, nullptr);
		MSHTML::IHTMLTxtRangePtr returnedRange(document->selection->createRange());
		MSHTML::IHTMLElementPtr returnedLink(returnedRange ? FBELinkNavigation::FindNearestLinkElement(returnedRange->parentElement(), editable) : MSHTML::IHTMLElementPtr());
		const bool returnedSecond = secondNavigated && returnedLink == second;
		const CString brokenTargetId(FBELinkNavigation::GetInternalLinkTargetId(document, broken));
		const bool brokenInternal = !brokenTargetId.IsEmpty() && !FBELinkNavigation::FindTargetElement(document, brokenTargetId);
		MSHTML::IHTMLElementPtr inserted;
		MSHTML::IHTMLDOMNodePtr secondNode(second);
		MSHTML::IHTMLDOMNodePtr secondParent(secondNode ? secondNode->parentNode : MSHTML::IHTMLDOMNodePtr());
		bool insertedBefore = false;
		bool insertedNavigate = false, insertedSameUnique = false, insertedReturned = false, insertedReturnedOrigin = false;
		try {
			inserted = document->createElement(L"A");
			if (inserted && second && secondParent) {
				const bool originSaved = navigationLeavesDocumentUntouched([&]() { return m_doc->m_body.NavigateInternalLink(second, secondTargetId); }, originDomUnchanged, originDirtyUnchanged);
				inserted->setAttribute(L"href", _variant_t(L"#note-1"), 2);
				inserted->innerText = L"inserted source";
				secondParent->insertBefore(MSHTML::IHTMLDOMNodePtr(inserted), secondNode.GetInterfacePtr());
				MSHTML::IHTMLElementCollectionPtr currentLinks(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"A"));
				MSHTML::IHTMLElementPtr currentSecond;
				for (long index = 0; currentLinks && index < currentLinks->length; ++index) {
					MSHTML::IHTMLElementPtr candidate(currentLinks->item(index));
					if (candidate && CString(static_cast<LPCWSTR>(candidate->innerText)) == L"second source") { currentSecond = candidate; break; }
				}
				const long currentSecondUniqueNumber = FBELinkNavigation::GetLinkUniqueNumber(currentSecond);
				// The saved history belongs to the pre-insertion second link.  Do not
				// navigate again here: that would replace the origin under test.
				insertedNavigate = originSaved;
				insertedSameUnique = currentSecondUniqueNumber == secondUniqueNumber;
				insertedReturned = m_doc->m_body.ReturnToLinkNavigationOrigin();
				MSHTML::IHTMLTxtRangePtr returnedOriginRange(document->selection->createRange());
				MSHTML::IHTMLTxtRangePtr currentSecondRange(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
				if (currentSecondRange && currentSecond) currentSecondRange->moveToElementText(currentSecond);
				insertedReturnedOrigin = insertedReturned && currentSecondRange &&
					returnedOriginRange->compareEndPoints(L"StartToStart", currentSecondRange) >= 0 &&
					returnedOriginRange->compareEndPoints(L"EndToEnd", currentSecondRange) <= 0;
				insertedBefore = insertedNavigate && insertedSameUnique && insertedReturnedOrigin;
				second = currentSecond;
			}
		} catch (const _com_error&) { insertedBefore = false; }
		bool deletedOriginFallback = false;
		try {
			MSHTML::IHTMLDOMNodePtr currentSecondNode(second);
			if (currentSecondNode && m_doc->m_body.NavigateInternalLink(second, secondTargetId)) {
				currentSecondNode->removeNode(VARIANT_TRUE);
				deletedOriginFallback = !m_doc->m_body.ReturnToLinkNavigationOrigin();
			}
		} catch (const _com_error&) { deletedOriginFallback = false; }
		wchar_t replacementPath[MAX_PATH] = {};
		const DWORD replacementPathLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_NAVIGATION_SECOND_FILE", replacementPath, _countof(replacementPath));
		bool otherDocumentOpened = false;
		if (replacementPathLength && replacementPathLength < _countof(replacementPath) && internal && m_doc->m_body.NavigateInternalLink(internal, targetId)) {
			// The preceding insertion/removal is intentional history coverage.  It
			// must not prompt during this separate ordinary-open regression step.
			m_doc->MarkSavePoint();
			otherDocumentOpened = LoadFile(replacementPath) == OK && !m_doc->m_body.ReturnToLinkNavigationOrigin();
		}
		// Both ordinary transitions above were independently compared with the
		// live DOM and dirty state.  Later insertion/removal intentionally edits
		// the fixture and is covered by its own history assertions.
		const bool unchanged = secondDomUnchanged && secondDirtyUnchanged && originDomUnchanged && originDirtyUnchanged;
		const bool passed = nearest == internal && target && sameDocument && brokenInternal && returnedSecond && insertedBefore && deletedOriginFallback && otherDocumentOpened && unchanged;
		CStringA row;
		row.Format("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\r\n", nearest == internal, target ? 1 : 0, sameDocument, brokenInternal, returnedSecond, insertedNavigate, insertedSameUnique, insertedReturned, insertedReturnedOrigin, insertedBefore, deletedOriginFallback, otherDocumentOpened, unchanged, secondDomUnchanged, secondDirtyUnchanged, originDomUnchanged, originDirtyUnchanged, passed ? "pass" : "fail");
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"auto-url-detect-runtime"))
	{
		CStringA header("typed_initial\tinitial\ttyped_source_roundtrip\tsource_roundtrip\ttyped_saved_reopened\tsaved_reopened\tundo\tredo\tmanual_link\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		const wchar_t* samples[] = { L"\\\\слово", L"\\\\server\\share", L"C:\\Books\\book.fb2", L"http://example.org", L"https://example.org", L"user@example.org" };
		auto getEditable = [&]() -> MSHTML::IHTMLElementPtr {
			return FBELinkNavigation::GetEditableBody(MSHTML::IHTMLDocument2Ptr(m_doc->m_body.Document()));
		};
		auto hasExpectedPlainText = [&](bool allowManualLink) -> bool {
			MSHTML::IHTMLElementPtr editable(getEditable());
			if (!editable) return false;
			const CString text(static_cast<LPCWSTR>(editable->innerText));
			for (const wchar_t* sample : samples) if (text.Find(sample) < 0) return false;
			MSHTML::IHTMLElementCollectionPtr links(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"A"));
			if (!links) return !allowManualLink;
			if (!allowManualLink) return links->length == 0;
			MSHTML::IHTMLElementPtr link(links->length == 1 ? links->item(0L) : MSHTML::IHTMLElementPtr());
			return link && CString(static_cast<LPCWSTR>(link->innerText)) == L"manual-link";
		};
		auto hasUnlinkedTypedText = [&]() -> bool {
			MSHTML::IHTMLDocument2Ptr current(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr typed(current && current->all ? current->all->item(L"auto-url-typed") : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr links(typed ? MSHTML::IHTMLElement2Ptr(typed)->getElementsByTagName(L"A") : MSHTML::IHTMLElementCollectionPtr());
			return typed && CString(static_cast<LPCWSTR>(typed->innerText)) == L"\\\\слово" && (!links || links->length == 0);
		};
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		bool enteredByUserInput = false;
		try
		{
			MSHTML::IHTMLElementPtr inputTarget(document && document->all ? document->all->item(L"auto-url-typed") : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementPtr focusTarget(document && document->all ? document->all->item(L"auto-url-focus") : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLTxtRangePtr inputRange(inputTarget ? MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange() : MSHTML::IHTMLTxtRangePtr());
			if (inputRange && inputTarget && focusTarget)
			{
				inputRange->moveToElementText(inputTarget); inputRange->collapse(VARIANT_TRUE); inputRange->select();
				m_doc->m_body.SetFocus();
				const HWND inputFocus = ::GetFocus();
				const wchar_t* typedText = L"\\\\слово";
				enteredByUserInput = inputFocus != NULL;
				for (const wchar_t* character = typedText; enteredByUserInput && *character; ++character)
					::SendMessage(inputFocus, WM_CHAR, *character, 0);
				MSHTML::IHTMLTxtRangePtr focusRange(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
				focusRange->moveToElementText(focusTarget); focusRange->collapse(VARIANT_TRUE); focusRange->select();
				enteredByUserInput = enteredByUserInput && CString(static_cast<LPCWSTR>(inputTarget->innerText)) == typedText;
			}
		}
		catch (const _com_error&) { enteredByUserInput = false; }
		const bool typedInitial = enteredByUserInput && hasUnlinkedTypedText();
		const bool initial = typedInitial && hasExpectedPlainText(false);
		ShowView(SOURCE); ShowView(BODY);
		const bool typedSourceRoundtrip = hasUnlinkedTypedText();
		const bool sourceRoundtrip = initial && typedSourceRoundtrip && hasExpectedPlainText(false);
		const CString filename(m_doc->m_filename);
		const bool saved = sourceRoundtrip && m_doc->Save();
		const bool reopened = saved && LoadFile(filename) == OK;
		ShowView(BODY);
		document = m_doc->m_body.Document();
		const bool typedSavedReopened = reopened && hasUnlinkedTypedText();
		const bool savedReopened = reopened && typedSavedReopened && hasExpectedPlainText(false);
		bool undo = false, redo = false, manualLink = false;
		try {
			MSHTML::IHTMLElementPtr manualTarget(document && document->all ? document->all->item(L"auto-url-manual") : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLTxtRangePtr range(manualTarget ? MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange() : MSHTML::IHTMLTxtRangePtr());
			if (range && manualTarget) {
				range->moveToElementText(manualTarget); range->select();
				BOOL handled = FALSE; m_doc->m_body.OnStyleLink(0, ID_STYLE_LINK, NULL, handled);
				const bool created = hasExpectedPlainText(true);
				m_doc->m_body.OnUndo(0, ID_EDIT_UNDO, NULL, handled);
				undo = created && hasExpectedPlainText(false);
				m_doc->m_body.OnRedo(0, ID_EDIT_REDO, NULL, handled);
				redo = undo && hasExpectedPlainText(true);
				manualLink = redo;
			}
		} catch (const _com_error&) { undo = redo = false; }
		const bool passed = initial && sourceRoundtrip && savedReopened && undo && redo && manualLink;
		CStringA row;
		row.Format("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\r\n", typedInitial, initial, typedSourceRoundtrip, sourceRoundtrip, typedSavedReopened, savedReopened, undo, redo, manualLink, passed ? "pass" : "fail");
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"script-document-path-runtime"))
	{
		struct PathApiValues { CString path, name, directory; };
		auto readEnvironmentPath = [](const wchar_t* name) -> CString {
			const DWORD length = ::GetEnvironmentVariable(name, NULL, 0);
			if (!length) return CString();
			std::vector<wchar_t> value(static_cast<size_t>(length));
			return ::GetEnvironmentVariable(name, value.data(), length) ? CString(value.data()) : CString();
		};
		auto readExternal = [&]() -> PathApiValues {
			PathApiValues values;
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLWindow2Ptr window(document ? document->parentWindow : MSHTML::IHTMLWindow2Ptr());
			IDispatch* rawExternal = NULL;
			if (!window || FAILED(window->get_external(&rawExternal)) || !rawExternal) return values;
			CComPtr<IDispatch> external; external.Attach(rawExternal);
			auto invoke = [&](const wchar_t* method, CString& value) -> bool {
				LPOLESTR name = const_cast<LPOLESTR>(method); DISPID dispid = DISPID_UNKNOWN;
				if (FAILED(external->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &dispid))) return false;
				DISPPARAMS parameters = {}; VARIANT result; ::VariantInit(&result); EXCEPINFO exception = {}; UINT argument = UINT_MAX;
				const HRESULT status = external->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &parameters, &result, &exception, &argument);
				const bool succeeded = SUCCEEDED(status) && V_VT(&result) == VT_BSTR;
				if (succeeded) value = V_BSTR(&result) ? CString(V_BSTR(&result)) : CString();
				::VariantClear(&result); return succeeded;
			};
			if (!invoke(L"GetDocumentFilePath", values.path) || !invoke(L"GetDocumentFileName", values.name) || !invoke(L"GetDocumentDirectory", values.directory)) return PathApiValues();
			return values;
		};
		auto matches = [](const PathApiValues& actual, const CString& expected) -> bool {
			const int separator = expected.ReverseFind(L'\\');
			const CString expectedName = separator >= 0 ? expected.Mid(separator + 1) : expected;
			const CString expectedDirectory = separator == 2 && expected.GetLength() >= 3 && expected[1] == L':' ? expected.Left(3) : separator > 0 ? expected.Left(separator) : CString();
			const bool absolute = expected.GetLength() >= 3 && ((expected[0] >= L'A' && expected[0] <= L'Z') || (expected[0] >= L'a' && expected[0] <= L'z')) && expected[1] == L':' && expected[2] == L'\\';
			return absolute && actual.path == expected && actual.name == expectedName && actual.directory == expectedDirectory;
		};
		const CString first(readEnvironmentPath(L"FBE_NEXT_TEST_DOCUMENT_PATH_FIRST"));
		const CString saveAs(readEnvironmentPath(L"FBE_NEXT_TEST_SAVE_PATH"));
		const CString second(readEnvironmentPath(L"FBE_NEXT_TEST_DOCUMENT_PATH_SECOND"));
		const PathApiValues untitled(readExternal());
		const bool unsaved = untitled.path.IsEmpty() && untitled.name.IsEmpty() && untitled.directory.IsEmpty();
		const bool opened = !first.IsEmpty() && LoadFile(first) == OK && matches(readExternal(), first);
		const bool savedAs = opened && !saveAs.IsEmpty() && SaveFile(true) == OK && matches(readExternal(), saveAs);
		const PathApiValues beforeSecond(readExternal());
		const bool otherOpened = savedAs && !second.IsEmpty() && LoadFile(second) == OK && matches(readExternal(), second) && beforeSecond.path != second;
		const bool unicode = first.Find(L"путь") >= 0 && first.Find(L"книга") >= 0 && second.Find(L"путь") >= 0 && second.Find(L"книга") >= 0;
		const bool passed = unsaved && opened && savedAs && otherOpened && unicode;
		CStringA report;
		report.Format("unsaved=%d\nopened=%d\nsave_as=%d\nother_opened=%d\nunicode=%d\nresult=%s\n", unsaved, opened, savedAs, otherOpened, unicode, passed ? "pass" : "fail");
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"reference-navigation-runtime"))
	{
		CStringA header("footnote_check\tfootnote_target\treference_check\treference_target\tcheck_unchanged\tdom_unchanged\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr editable(FBELinkNavigation::GetEditableBody(document));
		MSHTML::IHTMLElementCollectionPtr links(editable ? MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"A") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr link(links && links->length ? links->item(0L) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr strongs(link ? MSHTML::IHTMLElement2Ptr(link)->getElementsByTagName(L"STRONG") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr nested(strongs && strongs->length ? strongs->item(0L) : MSHTML::IHTMLElementPtr());
		const CString originalHtml(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
		auto selectElement = [&](MSHTML::IHTMLElementPtr element) -> bool
		{
			if (!element || !document || !document->body) return false;
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
			range->moveToElementText(element); range->collapse(VARIANT_TRUE); range->select(); return true;
		};
		auto sameSelection = [&]() -> bool
		{
			MSHTML::IHTMLTxtRangePtr first(document && document->selection ? MSHTML::IHTMLTxtRangePtr(document->selection->createRange()) : MSHTML::IHTMLTxtRangePtr());
			if (!first) return false;
			const bool canFootnote = m_doc->m_body.GoToFootnote(true);
			MSHTML::IHTMLTxtRangePtr after(document && document->selection ? MSHTML::IHTMLTxtRangePtr(document->selection->createRange()) : MSHTML::IHTMLTxtRangePtr());
			const bool unchangedFootnote = after && first->compareEndPoints(L"StartToStart", after) == 0 &&
				first->compareEndPoints(L"EndToEnd", after) == 0;
			return canFootnote && unchangedFootnote;
		};
		const bool selectedLink = selectElement(nested ? nested : link);
		const bool footnoteCheck = selectedLink && sameSelection();
		const bool footnoteMoved = footnoteCheck && m_doc->m_body.GoToFootnote(false);
		MSHTML::IHTMLElementPtr noteParent;
		try { noteParent = MSHTML::IHTMLTxtRangePtr(document->selection->createRange())->parentElement(); } catch (const _com_error&) { }
		while (noteParent && CString(static_cast<LPCWSTR>(noteParent->id)) != L"note-1") noteParent = noteParent->parentElement;
		const bool footnoteTarget = footnoteMoved && noteParent;
		MSHTML::IHTMLTxtRangePtr beforeReference(document && document->selection ? MSHTML::IHTMLTxtRangePtr(document->selection->createRange()) : MSHTML::IHTMLTxtRangePtr());
		const bool referenceCheck = beforeReference && m_doc->m_body.GoToReference(true);
		MSHTML::IHTMLTxtRangePtr afterReference(document && document->selection ? MSHTML::IHTMLTxtRangePtr(document->selection->createRange()) : MSHTML::IHTMLTxtRangePtr());
		const bool referenceCheckUnchanged = beforeReference && afterReference &&
			beforeReference->compareEndPoints(L"StartToStart", afterReference) == 0 &&
			beforeReference->compareEndPoints(L"EndToEnd", afterReference) == 0;
		m_doc->m_body.GoToReference(false);
		MSHTML::IHTMLTxtRangePtr selectedAfterReference(document && document->selection ? MSHTML::IHTMLTxtRangePtr(document->selection->createRange()) : MSHTML::IHTMLTxtRangePtr());
		MSHTML::IHTMLTxtRangePtr expectedReference(link && document && document->body ? MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange() : MSHTML::IHTMLTxtRangePtr());
		if (expectedReference)
		{
			expectedReference->moveToElementText(link); expectedReference->collapse(VARIANT_TRUE);
			expectedReference->move(L"character", CString(static_cast<LPCWSTR>(link->innerText)).GetLength());
		}
		const bool referenceTarget = selectedAfterReference && expectedReference &&
			selectedAfterReference->compareEndPoints(L"StartToStart", expectedReference) == 0 &&
			selectedAfterReference->compareEndPoints(L"EndToEnd", expectedReference) == 0;
		const bool unchanged = editable && originalHtml == CString(static_cast<LPCWSTR>(editable->innerHTML));
		const bool passed = footnoteCheck && footnoteTarget && referenceCheck && referenceCheckUnchanged && referenceTarget && unchanged;
		CStringA row;
		row.Format("%d\t%d\t%d\t%d\t%d\t%d\t%s\r\n", footnoteCheck, footnoteTarget, referenceCheck,
			referenceTarget, referenceCheckUnchanged, unchanged, passed ? "pass" : "fail");
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
