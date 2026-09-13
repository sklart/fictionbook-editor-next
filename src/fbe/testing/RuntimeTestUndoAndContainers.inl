	if (IsFbeTestScenario(L"split-undo-probe"))
	{
		wchar_t variant[48] = {};
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_UNDO_PROBE_VARIANT", variant, _countof(variant));
		CStringA header("variant\thresult\tdom_before\tdom_after\tdom_undo_1\tdom_undo_2\tdom_redo_1\tundos_required\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		try {
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr editable(document ? document->all->item(L"fbw_body") : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr bodies(editable ? MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr root;
			for (long i = 0; bodies && i < bodies->length; ++i) { MSHTML::IHTMLElementPtr e(bodies->item(_variant_t(i), _variant_t())); if (e && U::scmp(e->className, L"body") == 0) { root = e; break; } }
			if (!root) throw _com_error(E_FAIL);
			root->innerHTML = L"<DIV id=probe-old class=section><P>AAA 123 ZZZ</P></DIV>";
			IServiceProviderPtr service(document); CComPtr<IOleUndoManager> undoManager;
			if (service) service->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&undoManager));
			if (undoManager) undoManager->DiscardFrom(NULL);
			auto snapshot = [&]() { CString value(static_cast<const wchar_t*>(root->innerHTML)); value.Replace(L"\r", L""); value.Replace(L"\n", L""); value.Replace(L"\t", L" "); return value; };
			const CString before(snapshot()); HRESULT operationHr = S_OK;
			MSHTML::IHTMLElementPtr old(document->all->item(L"probe-old"));
			auto makeNext = [&]() { MSHTML::IHTMLElementPtr next(document->createElement(L"DIV")); next->className = L"section"; next->id = L"probe-old"; next->innerHTML = L"<DIV class=title><P>123</P></DIV><P>ZZZ</P>"; return next; };
			m_doc->m_body.BeginUndoUnit(L"split undo probe");
			try {
				if (wcscmp(variant, L"insert-adjacent") == 0) { MSHTML::IHTMLElementPtr next(makeNext()); old->id = L""; old->innerHTML = L"<P>AAA</P>"; MSHTML::IHTMLElement2Ptr(old)->insertAdjacentElement(L"afterEnd", next); }
				else if (wcscmp(variant, L"insert-before") == 0) { MSHTML::IHTMLElementPtr next(makeNext()); old->id = L""; old->innerHTML = L"<P>AAA</P>"; MSHTML::IHTMLDOMNodePtr parent(old->parentElement), oldNode(old); parent->insertBefore(MSHTML::IHTMLDOMNodePtr(next), oldNode->nextSibling.GetInterfacePtr()); }
				else if (wcscmp(variant, L"append-child") == 0) { MSHTML::IHTMLElementPtr next(makeNext()); old->id = L""; old->innerHTML = L"<P>AAA</P>"; MSHTML::IHTMLDOMNodePtr(root)->appendChild(MSHTML::IHTMLDOMNodePtr(next)); }
				else if (wcscmp(variant, L"pastehtml-content") == 0) { MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange()); range->moveToElementText(old); range->pasteHTML(L"<P>AAA</P></DIV><DIV id=probe-old class=section><DIV class=title><P>123</P></DIV><P>ZZZ</P>"); }
				else if (wcscmp(variant, L"pastehtml-root") == 0) { MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange()); range->moveToElementText(root); range->pasteHTML(L"<DIV class=section><P>AAA</P></DIV><DIV id=probe-old class=section><DIV class=title><P>123</P></DIV><P>ZZZ</P></DIV>"); }
				else if (wcscmp(variant, L"markup-after-end") == 0 || wcscmp(variant, L"markup-before-begin") == 0) { MSHTML::IMarkupServices2Ptr markup(m_doc->m_body.MarkupServices()); MSHTML::IMarkupPointerPtr start, finish; markup->CreateMarkupPointer(&start); markup->CreateMarkupPointer(&finish); const MSHTML::_ELEMENT_ADJACENCY edge = wcscmp(variant, L"markup-before-begin") == 0 ? MSHTML::ELEM_ADJ_BeforeBegin : MSHTML::ELEM_ADJ_AfterEnd; start->MoveAdjacentToElement(old, edge); finish->MoveAdjacentToElement(old, edge); MSHTML::IHTMLElementPtr next(makeNext()); operationHr = markup->InsertElement(next, start, finish); if (SUCCEEDED(operationHr)) { old->id = L""; old->innerHTML = L"<P>AAA</P>"; } }
				else if (wcscmp(variant, L"markup-parse-copy") == 0) { MSHTML::IMarkupServices2Ptr markup(m_doc->m_body.MarkupServices()); MSHTML::IMarkupPointerPtr sourceStart, sourceFinish, targetStart, targetFinish; MSHTML::IMarkupContainerPtr parsed; markup->CreateMarkupPointer(&sourceStart); markup->CreateMarkupPointer(&sourceFinish); operationHr = markup->ParseString(L"<DIV class=section><P>AAA</P></DIV><DIV id=probe-old class=section><DIV class=title><P>123</P></DIV><P>ZZZ</P></DIV>", 0, &parsed, sourceStart, sourceFinish); if (SUCCEEDED(operationHr)) { markup->CreateMarkupPointer(&targetStart); markup->CreateMarkupPointer(&targetFinish); targetStart->MoveAdjacentToElement(old, MSHTML::ELEM_ADJ_BeforeBegin); targetFinish->MoveAdjacentToElement(old, MSHTML::ELEM_ADJ_AfterEnd); operationHr = markup->remove(targetStart, targetFinish); if (SUCCEEDED(operationHr)) operationHr = markup->Copy(sourceStart, sourceFinish, targetStart); } }
				else if (wcscmp(variant, L"detached-subtree") == 0) { MSHTML::IHTMLElementPtr next(makeNext()); old->id = L""; MSHTML::IHTMLDOMNodePtr parent(old->parentElement), oldNode(old); parent->insertBefore(MSHTML::IHTMLDOMNodePtr(next), oldNode->nextSibling.GetInterfacePtr()); old->innerHTML = L"<P>AAA</P>"; }
				else if (wcscmp(variant, L"whole-innerhtml") == 0) { root->innerHTML = L"<DIV class=section><P>AAA</P></DIV><DIV id=probe-old class=section><DIV class=title><P>123</P></DIV><P>ZZZ</P></DIV>"; }
				else operationHr = E_INVALIDARG;
			} catch (const _com_error& error) { operationHr = error.Error(); }
			m_doc->m_body.EndUndoUnit();
			const CString after(snapshot()); BOOL handled = FALSE; m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled); const CString undo1(snapshot()); m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled); const CString undo2(snapshot()); m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled); const CString redo1(snapshot());
			const CString expected(L"<DIV class=section><P>AAA</P></DIV><DIV id=probe-old class=section><DIV class=title><P>123</P></DIV><P>ZZZ</P></DIV>");
			const bool oneUndo = SUCCEEDED(operationHr) && undo1 == before && redo1 == after;
			const long required = undo1 == before ? 1 : undo2 == before ? 2 : 3;
			CStringA row; row.Format("%S\t0x%08lX\t%S\t%S\t%S\t%S\t%S\t%ld\t%s\r\n", variant, static_cast<unsigned long>(operationHr), static_cast<LPCWSTR>(before), static_cast<LPCWSTR>(after), static_cast<LPCWSTR>(undo1), static_cast<LPCWSTR>(undo2), static_cast<LPCWSTR>(redo1), required, oneUndo && after == expected ? "pass" : "fail");
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(0); return 0;
		} catch (const _com_error& error) { CStringA row; row.Format("%S\t0x%08lX\t\t\t\t\t\t0\tfail\r\n", variant, static_cast<unsigned long>(error.Error())); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
	}
	if (IsFbeTestScenario(L"split-ole-undo-probe"))
	{
		CStringA header("manager_hr\topen_hr\tclose_hr\tparent_units\tstate_before\tstate_after\tstate_undo\tstate_redo\tdom_before\tdom_after\tdom_undo\tdom_redo\tundo_description_before\tundo_description_after\tundo_description_undo\tundo_description_redo\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		try {
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document()); IServiceProviderPtr service(document); CComPtr<IOleUndoManager> manager;
			const HRESULT managerHr = service ? service->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager)) : E_NOINTERFACE;
			if (FAILED(managerHr) || !manager) { CStringA row; row.Format("0x%08lX\t\t\t0\t\t\t\t\t\t\t\t\t\t\t\t\tfail\r\n", static_cast<unsigned long>(managerHr)); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(0); return 0; }
			MSHTML::IHTMLElementPtr editable(document->all->item(L"fbw_body")), root; MSHTML::IHTMLElementCollectionPtr divs(editable ? MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr()); for (long i = 0; divs && i < divs->length; ++i) { MSHTML::IHTMLElementPtr div(divs->item(_variant_t(i), _variant_t())); if (div && U::scmp(div->className, L"body") == 0) { root = div; break; } } if (!root) throw _com_error(E_FAIL);
			root->innerHTML = L"<DIV id=probe-old class=section><P>AAA 123 ZZZ</P></DIV>"; manager->DiscardFrom(NULL);
			auto compact = [](CString value) { value.Replace(L"\r", L""); value.Replace(L"\n", L""); value.Replace(L"\t", L" "); return value; };
			auto dom = [&]() { return compact(CString(static_cast<const wchar_t*>(root->innerHTML))); };
			auto state = [&]() { DWORD value = 0; return SUCCEEDED(manager->GetOpenParentState(&value)) ? value : 0xffffffffUL; };
			auto description = [&](bool redo) { BSTR value = NULL; HRESULT hr = redo ? manager->GetLastRedoDescription(&value) : manager->GetLastUndoDescription(&value); CString result = SUCCEEDED(hr) && value ? value : L""; if (value) ::SysFreeString(value); return compact(result); };
			const CString before(dom()), descriptionBefore(description(false)); const DWORD stateBefore = state();
			CComObject<CSplitUndoProbeParent>* rawParent = NULL; HRESULT openHr = CComObject<CSplitUndoProbeParent>::CreateInstance(&rawParent); if (SUCCEEDED(openHr)) rawParent->AddRef(); CComPtr<IOleParentUndoUnit> parent(rawParent);
			if (SUCCEEDED(openHr)) openHr = manager->Open(parent);
			if (SUCCEEDED(openHr)) { MSHTML::IHTMLElementPtr old(document->all->item(L"probe-old")), next(document->createElement(L"DIV")); next->className = L"section"; next->id = L"probe-old"; next->innerHTML = L"<DIV class=title><P>123</P></DIV><P>ZZZ</P>"; old->id = L""; old->innerHTML = L"<P>AAA</P>"; MSHTML::IHTMLDOMNodePtr parentNode(old->parentElement), oldNode(old); parentNode->insertBefore(MSHTML::IHTMLDOMNodePtr(next), oldNode->nextSibling.GetInterfacePtr()); }
			const HRESULT closeHr = SUCCEEDED(openHr) ? manager->Close(parent, TRUE) : E_FAIL; const CString after(dom()), descriptionAfter(description(false)); const DWORD stateAfter = state(); const long childCount = rawParent ? rawParent->UnitCount() : 0;
			BOOL handled = FALSE; m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled); const CString undone(dom()), descriptionUndo(description(true)); const DWORD stateUndo = state(); m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled); const CString redone(dom()), descriptionRedo(description(false)); const DWORD stateRedo = state();
			const bool passed = SUCCEEDED(managerHr) && SUCCEEDED(openHr) && SUCCEEDED(closeHr) && before == undone && after == redone;
			CStringA row; row.Format("0x%08lX\t0x%08lX\t0x%08lX\t%ld\t0x%08lX\t0x%08lX\t0x%08lX\t0x%08lX\t%S\t%S\t%S\t%S\t%S\t%S\t%S\t%S\t%s\r\n", static_cast<unsigned long>(managerHr), static_cast<unsigned long>(openHr), static_cast<unsigned long>(closeHr), childCount, stateBefore, stateAfter, stateUndo, stateRedo, static_cast<LPCWSTR>(before), static_cast<LPCWSTR>(after), static_cast<LPCWSTR>(undone), static_cast<LPCWSTR>(redone), static_cast<LPCWSTR>(descriptionBefore), static_cast<LPCWSTR>(descriptionAfter), static_cast<LPCWSTR>(descriptionUndo), static_cast<LPCWSTR>(descriptionRedo), passed ? "pass" : "fail"); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); if (rawParent) rawParent->Release(); ::PostQuitMessage(0); return 0;
		} catch (const _com_error& error) { CStringA row; row.Format("0x%08lX\t\t\t0\t\t\t\t\t\t\t\t\t\t\t\t\tfail\r\n", static_cast<unsigned long>(error.Error())); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
	}
	if (IsFbeTestScenario(L"split-container"))
	{
		wchar_t position[16] = {}, containerClass[16] = {}, containerId[64] = {}, tracePath[MAX_PATH] = {}, traceCase[64] = {}, route[16] = {}, selectionStartMarker[64] = {}, selectionEndMarker[64] = {};
		wchar_t splitFault[32] = {};
		const DWORD positionLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_POSITION", position, _countof(position));
		const DWORD containerClassLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_CONTAINER_CLASS", containerClass, _countof(containerClass));
		const DWORD containerIdLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_CONTAINER_ID", containerId, _countof(containerId));
		const DWORD traceLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TRACE", tracePath, _countof(tracePath));
		const DWORD traceCaseLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_CASE", traceCase, _countof(traceCase));
		const DWORD splitFaultLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_FAULT", splitFault, _countof(splitFault));
		const DWORD routeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_ROUTE", route, _countof(route));
		const DWORD selectionStartLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_SELECTION_START", selectionStartMarker, _countof(selectionStartMarker));
		const DWORD selectionEndLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_SELECTION_END", selectionEndMarker, _countof(selectionEndMarker));
		const bool viaWrapper = routeLength == 7 && wcscmp(route, L"wrapper") == 0;
		CStringA header("requested_container\tactual_container\tselection_collapsed\tselection_start_relative\tselection_end_relative\tselection_parent\tcheck_allowed\tcheck_dom_unchanged\tcheck_selection_unchanged\tcheck_dirty_unchanged\tchanged\tbefore_equals_undo\tafter_equals_redo\tselection_in_new\tfragments_preserved\tsaved\tresult\tfault_error\tdocument_changed\tcheck_status\tapply_status\thresult\tselection_text\tnew_title_text\tnew_remaining_text\tcaret_inserted\tcaret_undo_redo\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		try {
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr body(document ? document->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr editable(document ? document->all->item(L"fbw_body") : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr divs(editable ? MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr());
		const wchar_t* requestedContainerClass = containerClassLength ? containerClass : L"section";
		MSHTML::IHTMLElementPtr container;
		for (long index = 0; divs && index < divs->length; ++index) {
			MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
			if (div && U::scmp(div->className, requestedContainerClass) == 0 &&
				(!containerIdLength || U::scmp(div->id, containerId) == 0)) { container = div; break; }
		}
		CStringA requestedContainer; requestedContainer.Format("%S#%S", requestedContainerClass, containerIdLength ? containerId : L"");
		CStringA actualContainer;
		if (container) actualContainer.Format("%S#%S", static_cast<LPCWSTR>(container->className), containerIdLength ? static_cast<LPCWSTR>(container->id) : L"");
		if (!body || !editable || !container) { CStringA row; row.Format("%s\t%s\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tmissing-container\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
		MSHTML::IHTMLElementPtr contentRoot(container->parentElement);
		if (!contentRoot) { CStringA row; row.Format("%s\t%s\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tmissing-content-root\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
		auto contentHtml = [&]() -> CString {
			MSHTML::IHTMLElementCollectionPtr currentDivs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV"));
			for (long index = 0; currentDivs && index < currentDivs->length; ++index) {
				MSHTML::IHTMLElementPtr div(currentDivs->item(_variant_t(index), _variant_t()));
				if (div && U::scmp(div->className, L"body") == 0) return CString((const wchar_t*)div->innerHTML);
			}
			return CString();
		};
		m_doc->m_body.SetFocus();
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		const bool atStart = positionLength == 5 && wcscmp(position, L"start") == 0;
		const bool atEnd = positionLength == 3 && wcscmp(position, L"end") == 0;
		const bool caretScenario = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_CARET", nullptr, 0) > 0;
		if (caretScenario) {
			range->moveToElementText(container);
			if (!range->findText(L"abc", 0, 0)) { CStringA row; row.Format("%s\t%s\t0\tmissing-caret-marker\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tmissing-caret-marker\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
			range->collapse(VARIANT_FALSE);
		}
		else if (atStart) { range->moveToElementText(container); range->collapse(VARIANT_TRUE); }
		else {
			MSHTML::IHTMLElementCollectionPtr paragraphs(MSHTML::IHTMLElement2Ptr(container)->getElementsByTagName(L"P"));
			const long index = atEnd ? paragraphs->length - 1 : paragraphs->length / 2;
			MSHTML::IHTMLElementPtr paragraph(paragraphs && paragraphs->length ? paragraphs->item(_variant_t(index), _variant_t()) : MSHTML::IHTMLElementPtr());
			if (!paragraph) { CStringA row; row.Format("%s\t%s\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tmissing-paragraph\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
			const bool nonEmptySelection = positionLength >= 9 && wcsncmp(position, L"selection", 9) == 0;
			if (atEnd) { range->moveToElementText(container); range->collapse(VARIANT_FALSE); }
			else if (nonEmptySelection) {
				range->moveToElementText(container);
				const wchar_t* const startMarker = selectionStartLength && selectionStartLength < _countof(selectionStartMarker) ? selectionStartMarker : L"123";
				const wchar_t* const endMarker = selectionEndLength && selectionEndLength < _countof(selectionEndMarker) ? selectionEndMarker : startMarker;
				MSHTML::IHTMLTxtRangePtr selectionEnd(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
				selectionEnd->moveToElementText(container);
				// findText already leaves the range end immediately after the marker.
				// Moving it again would include following user text in a single-marker
				// selection (for example, "123 ZZZ" instead of "123").
				if (!range->findText(startMarker, 0, 0) || !selectionEnd->findText(endMarker, 0, 0) || FAILED(range->setEndPoint(L"EndToEnd", selectionEnd))) { CStringA row; row.Format("%s\t%s\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tmissing-selection-marker\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
			}
			else { range->moveToElementText(paragraph); range->collapse(VARIANT_TRUE); }
			// Stanza splitting is defined between verses.  Keep the test caret at
			// the beginning of the second visual paragraph instead of advancing
			// into its first character.
			if (!atEnd && !nonEmptySelection && wcscmp(requestedContainerClass, L"stanza") != 0 && !CString((const wchar_t*)paragraph->innerText).IsEmpty()) range->move(L"character", 1);
		}
		m_doc->m_body.SetFocus();
		range->select();
		MSHTML::IHTMLTxtRangePtr selectionBefore(document->selection->createRange());
		const CString selectionTextBefore(selectionBefore ? static_cast<const wchar_t*>(selectionBefore->text) : L"");
		MSHTML::IHTMLTxtRangePtr containerRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		containerRange->moveToElementText(container);
		const bool selectionCollapsed = selectionBefore && selectionBefore->compareEndPoints(L"StartToEnd", selectionBefore) == 0;
		const long selectionStartRelative = selectionBefore ? selectionBefore->compareEndPoints(L"StartToStart", containerRange) : 99;
		const long selectionEndRelative = selectionBefore ? selectionBefore->compareEndPoints(L"EndToEnd", containerRange) : 99;
		const CString before(contentHtml());
		const bool dirtyBefore = m_doc->DocChanged();
		FbeStructure::StructuralTrace trace(traceLength ? tracePath : nullptr, L"split", traceCaseLength ? traceCase : L"runtime");
		FbeStructure::BodyStructuralEditor editor(document, m_doc->m_body.MarkupServices(), trace.IsEnabled() ? &trace : nullptr);
		const FbeStructure::StructuralOperationResult checkResult = viaWrapper ? m_doc->m_body.SplitContainerResult(true) : editor.SplitContainer(true);
		const bool checkAllowed = checkResult.IsApplied();
		MSHTML::IHTMLTxtRangePtr selectionAfterCheck(document->selection->createRange());
		const bool checkDomUnchanged = before == contentHtml();
		const bool checkSelectionUnchanged = selectionAfterCheck &&
			selectionBefore && selectionBefore->compareEndPoints(L"StartToStart", selectionAfterCheck) == 0 &&
			selectionBefore->compareEndPoints(L"EndToEnd", selectionAfterCheck) == 0;
		const bool checkDirtyUnchanged = dirtyBefore == m_doc->DocChanged();
		const FbeStructure::SplitFailurePoint failurePoint = splitFaultLength
			? (wcscmp(splitFault, L"after-first-mutation") == 0
				? FbeStructure::SplitFailurePoint::AfterFirstMutation
				: FbeStructure::SplitFailurePoint::BeforeMutation)
			: FbeStructure::SplitFailurePoint::None;
		const FbeStructure::StructuralOperationResult splitResult = viaWrapper ? m_doc->m_body.SplitContainerResult(false, failurePoint) : editor.SplitContainer(false, failurePoint);
		const bool applied = splitResult.IsApplied();
		const CString after(contentHtml());
		trace.After(L"split-content", after);
		auto statusName = [](const FbeStructure::StructuralOperationResult& result) -> const char* {
			return result.IsApplied() ? "applied" : result.HasTechnicalFailure() ? "failed" : "not-applicable";
		};
		if (splitFaultLength) {
			BOOL handled = FALSE;
			if (splitResult.documentChanged) m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			const bool expectedChanged = wcscmp(splitFault, L"after-first-mutation") == 0;
			// A false documentChanged must not be accepted as evidence of restoration:
			// this fault scenario exists specifically to catch an incorrect flag.
			const bool undoRestored = before == contentHtml();
			const bool passed = checkAllowed && splitResult.HasTechnicalFailure() && splitResult.error == E_FAIL &&
				splitResult.documentChanged == expectedChanged && (expectedChanged ? before != after && undoRestored : before == after);
			CStringA row; row.Format("%s\t%s\t%d\t%ld\t%ld\t\t%d\t%d\t%d\t%d\t%d\t%d\t1\t1\t1\t0\t%s\t0x%08lX\t%d\t%s\t%s\t0x%08lX\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer, selectionCollapsed, selectionStartRelative, selectionEndRelative, checkAllowed, checkDomUnchanged, checkSelectionUnchanged, checkDirtyUnchanged, before != after, undoRestored, passed ? "pass" : "fail", static_cast<unsigned long>(splitResult.error), splitResult.documentChanged ? 1 : 0, statusName(checkResult), statusName(splitResult), static_cast<unsigned long>(splitResult.error));
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
		}
		if (checkResult.status == FbeStructure::StructuralOperationStatus::NotApplicable && splitResult.status == FbeStructure::StructuralOperationStatus::NotApplicable) {
			const bool passed = checkResult.error == S_OK && splitResult.error == S_OK && !checkResult.documentChanged && !splitResult.documentChanged && checkDomUnchanged && checkSelectionUnchanged && checkDirtyUnchanged && before == after;
			CStringA row; row.Format("%s\t%s\t%d\t%ld\t%ld\t\t%d\t%d\t%d\t%d\t0\t1\t1\t1\t1\t1\t%s\t0x%08lX\t%d\t%s\t%s\t0x%08lX\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer, selectionCollapsed, selectionStartRelative, selectionEndRelative, checkAllowed, checkDomUnchanged, checkSelectionUnchanged, checkDirtyUnchanged, passed ? "pass" : "fail", static_cast<unsigned long>(splitResult.error), splitResult.documentChanged ? 1 : 0, statusName(checkResult), statusName(splitResult), static_cast<unsigned long>(splitResult.error));
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
		}
		MSHTML::IHTMLTxtRangePtr selectionAfter(document->selection->createRange());
		MSHTML::IHTMLElementPtr selectionParent(selectionAfter ? selectionAfter->parentElement() : MSHTML::IHTMLElementPtr());
		while (selectionParent && U::scmp(selectionParent->tagName, L"DIV")) selectionParent = selectionParent->parentElement;
		CStringA selectionParentSummary;
		if (selectionParent) selectionParentSummary.Format("%S#%S", static_cast<LPCWSTR>(selectionParent->className), static_cast<LPCWSTR>(selectionParent->id));
		MSHTML::IHTMLElementPtr newContainer;
		if (containerIdLength) {
			MSHTML::IHTMLElementPtr adjacent(MSHTML::IHTMLDOMNodePtr(container)->nextSibling);
			if (adjacent && U::scmp(adjacent->className, container->className) == 0 && U::scmp(adjacent->id, containerId) == 0) newContainer = adjacent;
		} else {
			MSHTML::IHTMLElementCollectionPtr currentDivs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV"));
			for (long index = 0; currentDivs && index < currentDivs->length; ++index) {
				MSHTML::IHTMLElementPtr div(currentDivs->item(_variant_t(index), _variant_t()));
				if (div && U::scmp(div->className, container->className) == 0 && div->sourceIndex != container->sourceIndex) { newContainer = div; break; }
			}
		}
		MSHTML::IHTMLTxtRangePtr newContainerRange(newContainer ? MSHTML::IHTMLBodyElementPtr(body)->createTextRange() : MSHTML::IHTMLTxtRangePtr());
		if (newContainerRange) newContainerRange->moveToElementText(newContainer);
		const long selectionStartInNew = selectionAfter && newContainerRange ? selectionAfter->compareEndPoints(L"StartToStart", newContainerRange) : 99;
		const long selectionEndInNew = selectionAfter && newContainerRange ? selectionAfter->compareEndPoints(L"EndToEnd", newContainerRange) : 99;
		const bool selectionInNew = newContainer && selectionStartInNew >= 0 && selectionEndInNew <= 0;
		CString selectionMembership;
		selectionMembership.Format(L"start=%ld; end=%ld; new=%d", selectionStartInNew, selectionEndInNew, selectionInNew ? 1 : 0);
		trace.After(L"selection-membership", selectionMembership);
		BOOL handled = FALSE; CString undone, redone;
		if (!caretScenario) {
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled); undone = contentHtml(); trace.After(L"undo-content", undone);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled); redone = contentHtml();
		}
		bool caretInserted = !caretScenario, caretUndoRedo = !caretScenario;
		if (caretScenario && selectionAfter && newContainer) {
			const CString beforeInput(static_cast<const wchar_t*>(newContainer->innerText));
			m_doc->m_body.SetFocus();
			const HWND inputFocus = ::GetFocus();
			const bool insertedWithCommand = inputFocus && ::SendMessage(inputFocus, WM_CHAR, L'X', 0) != 0;
			const CString afterInput(static_cast<const wchar_t*>(newContainer->innerText));
			trace.After(L"caret-input-after", contentHtml());
			BOOL inputHandled = FALSE;
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, inputHandled);
			const CString undoneInput(static_cast<const wchar_t*>(newContainer->innerText));
			const CString splitUndoneAfterInput(contentHtml());
			trace.After(L"caret-input-undo", splitUndoneAfterInput);
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, inputHandled);
			const CString splitUndoDom(contentHtml());
			trace.After(L"caret-split-undo", splitUndoDom);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, inputHandled);
			const CString splitRedoDom(contentHtml());
			trace.After(L"caret-split-redo", splitRedoDom);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, inputHandled);
			const CString redoneInput(static_cast<const wchar_t*>(newContainer->innerText));
			trace.After(L"caret-input-redo", contentHtml());
			undone = splitUndoDom;
			redone = splitRedoDom;
			const bool hasTail = beforeInput.Find(L"def") >= 0;
			// A split at end creates an intentionally empty paragraph.  Its
			// regression is that a real WM_CHAR lands in that paragraph; all
			// non-empty cases must remain the stricter Xdef/not-defX contract.
			caretInserted = (insertedWithCommand || afterInput != beforeInput) &&
				(hasTail ? (afterInput.Find(L"Xdef") >= 0 && afterInput.Find(L"defX") < 0) : afterInput.Find(L"X") >= 0);
			caretUndoRedo = beforeInput == undoneInput && splitUndoneAfterInput == after && splitUndoDom == before && splitRedoDom == after && afterInput == redoneInput;
		}
		bool endTextInserted = !atEnd;
		if (atEnd && newContainer) {
			MSHTML::IHTMLElementPtr inputContainer(document->all->item(containerId));
			MSHTML::IHTMLElementCollectionPtr leaves(inputContainer ? MSHTML::IHTMLElement2Ptr(inputContainer)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr leaf(leaves && leaves->length ? leaves->item(0L) : MSHTML::IHTMLElementPtr());
			if (leaf) {
				MSHTML::IHTMLTxtRangePtr input(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
				input->moveToElementText(leaf); leaf->innerText = L"Tail";
				endTextInserted = inputContainer && CString((const wchar_t*)inputContainer->innerText).Find(L"Tail") >= 0;
			}
			CString endWorkflow;
			endWorkflow.Format(L"container=%d; leaf=%d; text=%d", inputContainer ? 1 : 0, leaf ? 1 : 0, endTextInserted ? 1 : 0);
			trace.After(L"end-workflow", endWorkflow);
		}
		int validationLine = 0, validationColumn = 0;
		const bool saved = applied && caretInserted && caretUndoRedo && endTextInserted && m_doc->Validate(validationLine, validationColumn) && m_doc->Save();
		const bool changed = before != after;
		auto countText = [&](const wchar_t* text) { long count = 0, offset = 0; while ((offset = after.Find(text, offset)) >= 0) { ++count; offset += static_cast<int>(wcslen(text)); } return count; };
		const bool selectionScenario = positionLength == 9 && wcscmp(position, L"selection") == 0;
		const CString originalText(container ? static_cast<const wchar_t*>(container->innerText) : L"");
		const CString splitText(newContainer ? static_cast<const wchar_t*>(newContainer->innerText) : L"");
		MSHTML::IHTMLElementPtr newTitle;
		MSHTML::IHTMLElementCollectionPtr newDivs(newContainer ? MSHTML::IHTMLElement2Ptr(newContainer)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr());
		for (long index = 0; newDivs && index < newDivs->length; ++index) { MSHTML::IHTMLElementPtr div(newDivs->item(_variant_t(index), _variant_t())); if (div && U::scmp(div->className, L"title") == 0) { newTitle = div; break; } }
		const CString newTitleText(newTitle ? static_cast<const wchar_t*>(newTitle->innerText) : L"");
		CString newRemainingText;
		MSHTML::IHTMLElementCollectionPtr newParagraphs(newContainer ? MSHTML::IHTMLElement2Ptr(newContainer)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		for (long index = 0; newParagraphs && index < newParagraphs->length; ++index) { MSHTML::IHTMLElementPtr paragraph(newParagraphs->item(_variant_t(index), _variant_t())); if (paragraph && paragraph->parentElement != newTitle) newRemainingText += static_cast<const wchar_t*>(paragraph->innerText); }
		const bool fragmentsPreserved = !selectionScenario ||
			(countText(L"AAA") == 1 && countText(L"123") == 1 && countText(L"ZZZ") == 1 &&
			 selectionTextBefore == L"123" && originalText.Find(L"AAA") >= 0 && originalText.Find(L"123") < 0 && originalText.Find(L"ZZZ") < 0 &&
			 newTitleText == L"123" && newRemainingText.Find(L"ZZZ") >= 0 && splitText.Find(L"AAA") < 0);
		const bool passed = checkAllowed && checkDomUnchanged && checkSelectionUnchanged && checkDirtyUnchanged && applied && changed && before == undone && after == redone && selectionInNew && fragmentsPreserved && caretInserted && caretUndoRedo && endTextInserted && saved;
		auto tsvField = [](CString value) { value.Replace(L"\r", L"\\r"); value.Replace(L"\n", L"\\n"); value.Replace(L"\t", L"\\t"); return CStringA(CW2A(value, CP_UTF8)); };
		const CStringA selectionTextField(tsvField(selectionTextBefore)), newTitleField(tsvField(newTitleText)), newRemainingField(tsvField(newRemainingText));
		CStringA row; row.Format("%s\t%s\t%d\t%ld\t%ld\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t0x%08lX\t%d\t%s\t%s\t0x%08lX\t%s\t%s\t%s\t%d\t%d\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer, selectionCollapsed, selectionStartRelative, selectionEndRelative, (LPCSTR)selectionParentSummary, checkAllowed, checkDomUnchanged, checkSelectionUnchanged, checkDirtyUnchanged, changed, before == undone, after == redone, selectionInNew, fragmentsPreserved, saved, passed ? "pass" : "fail", static_cast<unsigned long>(splitResult.error), splitResult.documentChanged ? 1 : 0, statusName(checkResult), statusName(splitResult), static_cast<unsigned long>(splitResult.error), (LPCSTR)selectionTextField, (LPCSTR)newTitleField, (LPCSTR)newRemainingField, caretInserted ? 1 : 0, caretUndoRedo ? 1 : 0);
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
		} catch (_com_error& error) {
			CStringA row; row.Format("unknown\tunknown\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tcom-0x%08lX\r\n", static_cast<unsigned long>(error.Error()));
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(1); return 0;
		}
	}
	if (IsFbeTestScenario(L"split-container-reopen"))
	{
		wchar_t containerId[64] = {};
		const DWORD containerIdLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_CONTAINER_ID", containerId, _countof(containerId));
		CStringA header("id_restored\tsection_complete\tsaved\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		try {
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr section(document && containerIdLength ? document->all->item(containerId) : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr paragraphs(section ? MSHTML::IHTMLElement2Ptr(section)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr firstParagraph(paragraphs && paragraphs->length ? paragraphs->item(0L) : MSHTML::IHTMLElementPtr());
			const bool idRestored = section && U::scmp(section->id, containerId) == 0;
			const bool sectionComplete = firstParagraph && !CString(static_cast<const wchar_t*>(firstParagraph->innerText)).IsEmpty();
			const bool saved = idRestored && sectionComplete && m_doc->Save();
			CStringA row; row.Format("%d\t%d\t%d\t%s\r\n", idRestored ? 1 : 0, sectionComplete ? 1 : 0, saved ? 1 : 0, saved ? "pass" : "fail");
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(saved ? 0 : 1); return 0;
		} catch (_com_error& error) {
			CStringA row; row.Format("0\t0\t0\tcom-0x%08lX\r\n", static_cast<unsigned long>(error.Error()));
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(1); return 0;
		}
	}
