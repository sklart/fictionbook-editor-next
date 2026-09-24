	if (IsFbeTestScenario(L"table-structural"))
	{
		const ULONGLONG start = ::GetTickCount64();
		auto appendStructuralPhase = [&](const char* phase, long gridBuildCalls = -1)
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementCollectionPtr rows(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TR") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementCollectionPtr td(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TD") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementCollectionPtr th(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TH") : MSHTML::IHTMLElementCollectionPtr());
			CStringA row;
			row.Format("%s\t%I64u\t%ld\t%ld\t%ld\t%ld\t%ld\t%s\r\n", phase, ::GetTickCount64() - start,
				tables ? tables->length : 0, rows ? rows->length : 0, td ? td->length : 0, th ? th->length : 0, gridBuildCalls, (LPCSTR)m_doc->m_body.TableStructuralSnapshot());
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		auto selectFirstCell = [&]() -> bool
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr cells(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TD") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr cell(cells && cells->length ? cells->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr());
			if (!cell) { cells = body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TH") : MSHTML::IHTMLElementCollectionPtr(); cell = cells && cells->length ? cells->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr(); }
			if (!cell) return false;
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
			range->moveToElementText(cell); range->collapse(VARIANT_TRUE); range->select(); return true;
		};
		auto selectFirstTwoCells = [&](bool headers) -> bool
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr cells(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(headers ? L"TH" : L"TD") : MSHTML::IHTMLElementCollectionPtr());
			if (!cells || cells->length < 2) cells = body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(headers ? L"TD" : L"TH") : MSHTML::IHTMLElementCollectionPtr();
			if (!cells || cells->length < 2) return false;
			MSHTML::IHTMLElementPtr first(cells->item(_variant_t(0L), _variant_t())), last(cells->item(_variant_t(1L), _variant_t()));
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange()), end(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
			 range->moveToElementText(first); end->moveToElementText(last); range->setEndPoint(L"EndToEnd", end); range->select(); return true;
		};
		auto selectConfiguredCells = [&](bool bulk, bool headers) -> bool
		{
			wchar_t target[64] = {};
			const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_TARGET", target, _countof(target));
			if (!length || length >= _countof(target)) return bulk ? selectFirstTwoCells(headers) : selectFirstCell();
			long firstRow = -1, firstColumn = -1, lastRow = -1, lastColumn = -1;
			if (swscanf_s(target, L"%ld,%ld:%ld,%ld", &firstRow, &firstColumn, &lastRow, &lastColumn) != 4) {
				if (swscanf_s(target, L"%ld,%ld", &firstRow, &firstColumn) != 2) return false;
				lastRow = firstRow; lastColumn = firstColumn;
			}
			return m_doc->m_body.SelectTableLogicalRangeForTest(firstRow, firstColumn, lastRow, lastColumn);
		};
		auto applyConfiguredRuntimeCellStyle = [&]() -> bool
		{
			wchar_t cssText[256] = {};
			const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_RUNTIME_STYLE", cssText, _countof(cssText));
			if (!length) return true;
			if (length >= _countof(cssText)) return false;
			MSHTML::IHTMLElementPtr cell(m_doc->m_body.SelectionStructTableCon());
			MSHTML::IHTMLStylePtr style(cell ? cell->style : MSHTML::IHTMLStylePtr());
			if (!style) return false;
			style->cssText = cssText;
			return true;
		};
		typedef LRESULT (CFBEView::*TableHandler)(WORD, WORD, HWND, BOOL&);
		struct Operation { const char* name; UINT command; TableHandler handler; bool bulk, selectHeaders; };
		const Operation operations[] = {
			{ "toggle-header", ID_TABLE_TOGGLE_HEADER_CELL, &CFBEView::OnTableToggleHeaderCell, false, false }, { "insert-row-above", ID_TABLE_INSERT_ROW_ABOVE, &CFBEView::OnTableInsertRowAbove, false, false },
			{ "insert-row-below", ID_TABLE_INSERT_ROW_BELOW, &CFBEView::OnTableInsertRowBelow, false, false }, { "delete-row", ID_TABLE_DELETE_ROW, &CFBEView::OnTableDeleteRow, false, false },
			{ "insert-column-left", ID_TABLE_INSERT_COLUMN_LEFT, &CFBEView::OnTableInsertColumnLeft, false, false }, { "insert-column-right", ID_TABLE_INSERT_COLUMN_RIGHT, &CFBEView::OnTableInsertColumnRight, false, false },
			{ "delete-column", ID_TABLE_DELETE_COLUMN, &CFBEView::OnTableDeleteColumn, false, false }, { "make-header", ID_TABLE_MAKE_HEADER_CELLS, &CFBEView::OnTableMakeHeaderCells, true, false },
			{ "make-normal", ID_TABLE_MAKE_NORMAL_CELLS, &CFBEView::OnTableMakeNormalCells, true, true }
		};
		wchar_t routeThroughFrame[4] = {};
		const bool useCommandRoute = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_ROUTE", routeThroughFrame, _countof(routeThroughFrame)) == 1 && routeThroughFrame[0] == L'1';
		auto invokeOperation = [&](const Operation& operation, BOOL& handled) -> bool
		{
			wchar_t target[64] = {};
			const DWORD targetLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_TARGET", target, _countof(target));
			long row = -1, column = -1;
			if (targetLength && targetLength < _countof(target) && strcmp(operation.name, "delete-column") == 0 &&
				swscanf_s(target, L"%ld,%ld", &row, &column) == 2 && column >= 0) {
				return m_doc->m_body.DeleteTableLogicalColumnForTest(column);
			}
			if(useCommandRoute)
			{
				m_doc->m_body.SetFocus();
				::SendMessage(m_hWnd, WM_COMMAND, MAKEWPARAM(operation.command, 0), 0);
				return true;
			}
			(m_doc->m_body.*operation.handler)(0, 0, m_doc->m_body, handled);
			return true;
		};
		CStringA header("phase\telapsed_ms\ttable_count\ttr_count\ttd_count\tth_count\tgrid_build_calls\tgrid_signature\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();
		for (size_t index = 0; index < _countof(operations); ++index)
		{
			wchar_t requestedOperation[64] = {};
			const DWORD requestedOperationLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_OPERATION", requestedOperation, _countof(requestedOperation));
			if (requestedOperationLength && (requestedOperationLength >= _countof(requestedOperation) || _stricmp((LPCSTR)CStringA(requestedOperation), operations[index].name) != 0)) continue;
			if (!selectConfiguredCells(operations[index].bulk, operations[index].selectHeaders) || !applyConfiguredRuntimeCellStyle()) { output.Close(); ::PostQuitMessage(1); return 0; }
			CStringA phase; phase.Format("%s-before", operations[index].name); appendStructuralPhase(phase);
			CFBEView::ResetTableGridBuildCountForTest();
			BOOL handled = FALSE; if (!invokeOperation(operations[index], handled)) { output.Close(); ::PostQuitMessage(1); return 0; }
			const long gridBuildCalls = CFBEView::TableGridBuildCountForTest();
			phase.Format("%s-after", operations[index].name); appendStructuralPhase(phase, gridBuildCalls);
			wchar_t secondOperation[64] = {};
			const DWORD secondOperationLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_SECOND_OPERATION", secondOperation, _countof(secondOperation));
			if (secondOperationLength && secondOperationLength < _countof(secondOperation)) {
				for (size_t secondIndex = 0; secondIndex < _countof(operations); ++secondIndex) {
					if (_stricmp((LPCSTR)CStringA(secondOperation), operations[secondIndex].name) != 0) continue;
					if (!selectConfiguredCells(operations[secondIndex].bulk, operations[secondIndex].selectHeaders)) { output.Close(); ::PostQuitMessage(1); return 0; }
					phase.Format("%s-second-before", operations[secondIndex].name); appendStructuralPhase(phase);
					CFBEView::ResetTableGridBuildCountForTest();
					if (!invokeOperation(operations[secondIndex], handled)) { output.Close(); ::PostQuitMessage(1); return 0; }
					phase.Format("%s-second-after", operations[secondIndex].name); appendStructuralPhase(phase, CFBEView::TableGridBuildCountForTest());
					break;
				}
			}
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			phase.Format("%s-undo", operations[index].name); appendStructuralPhase(phase);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
			phase.Format("%s-redo", operations[index].name); appendStructuralPhase(phase);
		}
		if (!m_doc->Save()) { appendStructuralPhase("save-failed;phase=save;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable"); output.Close(); ::PostQuitMessage(1); return 0; }
		appendStructuralPhase("save-complete"); output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"image-undo-probe"))
	{
		const ULONGLONG start = ::GetTickCount64();
		auto appendProbePhase = [&](const char* phase)
		{
			CStringA row;
			row.Format("%s\t%I64u\r\n", phase, ::GetTickCount64() - start);
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		CStringA header("phase\telapsed_ms\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();
		wchar_t probeName[64] = {};
		const DWORD probeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_PROBE", probeName, _countof(probeName));
		if (!probeLength || probeLength >= _countof(probeName)) { appendProbePhase("probe-failed;reason=name"); output.Close(); ::PostQuitMessage(1); return 0; }
		wchar_t imagePath[MAX_PATH] = {};
		const DWORD imagePathLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_PATH", imagePath, _countof(imagePath));
		if (imagePathLength == 0 || imagePathLength >= _countof(imagePath) || ::GetFileAttributes(imagePath) == INVALID_FILE_ATTRIBUTES)
		{
			appendProbePhase("probe-failed;reason=image-path"); output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendProbePhase("open-complete");
		MSHTML::IHTMLBodyElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLBodyElementPtr());
		MSHTML::IHTMLElementCollectionPtr paragraphs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr paragraph, section;
		for (long paragraphIndex = 0; paragraphs && paragraphIndex < paragraphs->length && !paragraph; ++paragraphIndex)
		{
			MSHTML::IHTMLElementPtr candidate(paragraphs->item(_variant_t(paragraphIndex), _variant_t()));
			for (MSHTML::IHTMLElementPtr ancestor(candidate); ancestor; ancestor = ancestor->parentElement)
			{
				const _bstr_t tagName(ancestor->tagName), className(ancestor->className);
				const wchar_t* const tagText = tagName;
				const wchar_t* const classText = className;
				if (tagText && classText && _wcsicmp(tagText, L"DIV") == 0 && _wcsicmp(classText, L"section") == 0) { paragraph = candidate; section = ancestor; break; }
			}
		}
		CComDispatchDriver script(m_doc->m_body.Script());
		if (!body || !paragraph || !section || !script) { appendProbePhase("probe-failed;reason=document"); output.Close(); ::PostQuitMessage(1); return 0; }
		auto refreshSection = [&]() -> bool
		{
			body = m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLBodyElementPtr();
			section = NULL;
			MSHTML::IHTMLElementCollectionPtr divs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr());
			for (long index = 0; divs && index < divs->length; ++index)
			{
				MSHTML::IHTMLElementPtr candidate(divs->item(_variant_t(index), _variant_t()));
				if (candidate && _wcsicmp(static_cast<const wchar_t*>(_bstr_t(candidate->className)), L"section") == 0) { section = candidate; break; }
			}
			return body && section;
		};
		auto snapshotSection = [&](CString& outerHtml, CString& childOrder) -> bool
		{
			outerHtml.Empty(); childOrder.Empty();
			if (!section) return false;
			outerHtml = static_cast<const wchar_t*>(_bstr_t(section->outerHTML));
			for (MSHTML::IHTMLDOMNodePtr node(MSHTML::IHTMLDOMNodePtr(section)->firstChild); node; node = node->nextSibling)
			{
				if (!childOrder.IsEmpty()) childOrder += L"|";
				if (node->nodeType != NODE_ELEMENT) { CString item; item.Format(L"#node%ld", node->nodeType); childOrder += item; continue; }
				MSHTML::IHTMLElementPtr element(node);
				const _bstr_t tag(element ? element->tagName : L""), id(element ? element->id : L""), className(element ? element->className : L"");
				CString item; item.Format(L"%s#%s.%s", static_cast<const wchar_t*>(tag), static_cast<const wchar_t*>(id), static_cast<const wchar_t*>(className)); childOrder += item;
			}
			return true;
		};
		auto appendSectionSnapshot = [&](const char* name, const CString& outerHtml, const CString& childOrder)
		{
			ULONGLONG hash = 1469598103934665603ULL;
			for (int index = 0; index < outerHtml.GetLength(); ++index) { hash ^= static_cast<unsigned short>(outerHtml[index]); hash *= 1099511628211ULL; }
			CStringA phase; phase.Format("%s;outerhtml-length=%d;outerhtml-fnv64=%I64X;child-order=%S", name, outerHtml.GetLength(), hash, static_cast<const wchar_t*>(childOrder)); appendProbePhase(phase);
		};
		wchar_t undoRedoCyclesText[8] = {};
		const DWORD undoRedoCyclesLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_UNDO_REDO_CYCLES", undoRedoCyclesText, _countof(undoRedoCyclesText));
		const int undoRedoCycles = undoRedoCyclesLength && undoRedoCyclesLength < _countof(undoRedoCyclesText) ? _wtoi(undoRedoCyclesText) : 5;
		if (undoRedoCycles != 0 && undoRedoCycles != 1 && undoRedoCycles != 2 && undoRedoCycles != 5) { appendProbePhase("probe-failed;reason=undo-redo-cycles"); output.Close(); ::PostQuitMessage(1); return 0; }
		auto selectParagraphPosition = [&](const CString& position) -> bool
		{
			MSHTML::IHTMLTxtRangePtr range(body->createTextRange());
			if (!range) return false;
			range->moveToElementText(paragraph);
			if (position == L"end") range->collapse(VARIANT_FALSE);
			else { range->collapse(VARIANT_TRUE); if (position == L"middle") range->move(L"character", 3); else if (range->move(L"character", 1) == 1) range->move(L"character", -1); }
			range->select(); return true;
		};
		auto addBinary = [&](bool fillCoverList, _variant_t& binaryId) -> HRESULT
		{
			_variant_t data;
			HRESULT hr = U::LoadFile(imagePath, &data);
			if (FAILED(hr)) return hr;
			_variant_t args[4];
			args[0] = data;
			args[1] = L"image/jpeg";
			args[2] = L"undo-probe-binary";
			args[3] = L"";
			hr = script.InvokeN(L"apiAddBinary", args, 4, &binaryId);
			if (SUCCEEDED(hr) && fillCoverList) hr = script.Invoke0(L"FillCoverList");
			return hr;
		};
		auto blockImageCountFor = [&](MSHTML::IHTMLBodyElementPtr targetBody) -> long
		{
			long count = 0;
			MSHTML::IHTMLElementCollectionPtr divs(targetBody ? MSHTML::IHTMLElement2Ptr(targetBody)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr());
			for (long index = 0; divs && index < divs->length; ++index)
			{
				MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
				if (div && _wcsicmp(static_cast<const wchar_t*>(_bstr_t(div->className)), L"image") == 0) ++count;
			}
			return count;
		};
		auto blockImageCount = [&]() -> long { return blockImageCountFor(body); };
		const CString probe(probeName);
		MSHTML::IHTMLBodyElementPtr cachedBody;
		HRESULT hr = S_OK;
		_variant_t binaryId;
		if (probe == L"binary" || probe == L"binary-fill")
		{
			appendProbePhase("api-add-binary-start");
			hr = addBinary(probe == L"binary-fill", binaryId);
			if (FAILED(hr)) { appendProbePhase("probe-failed;phase=api-add-binary"); output.Close(); ::PostQuitMessage(1); return 0; }
			appendProbePhase(probe == L"binary-fill" ? "fill-cover-list-complete" : "api-add-binary-complete");
		}
		else if (probe == L"image" || probe == L"image-inline" || probe == L"image-start" || probe == L"image-end" || probe == L"image-middle" || probe == L"image-hold-start" || probe == L"image-clear-start" || probe == L"image-discard-start" || probe == L"image-inspect-release-start")
		{
			wchar_t existingId[256] = {};
			const DWORD existingIdLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_BINARY_ID", existingId, _countof(existingId));
			const CString position = probe == L"image-end" ? L"end" : (probe == L"image-middle" ? L"middle" : L"start");
			if (!existingIdLength || existingIdLength >= _countof(existingId) || !selectParagraphPosition(position)) { appendProbePhase("probe-failed;reason=existing-binary-or-selection"); output.Close(); ::PostQuitMessage(1); return 0; }
			_variant_t check(false), inserted, id(existingId);
			appendProbePhase("image-insert-start");
			if (probe == L"image-discard-start") hr = script.Invoke2(L"InsImage", &check, &id, NULL);
			else hr = script.Invoke2(probe == L"image-inline" ? L"InsInlineImage" : L"InsImage", &check, &id, &inserted);
			if (FAILED(hr)) { appendProbePhase("probe-failed;phase=image-insert"); output.Close(); ::PostQuitMessage(1); return 0; }
			if (probe == L"image-clear-start") { inserted.Clear(); appendProbePhase("image-result-cleared"); }
			else if (probe == L"image-inspect-release-start")
			{
				MSHTML::IHTMLElementPtr returned(V_VT(&inserted) == VT_DISPATCH ? V_DISPATCH(&inserted) : NULL);
				const bool valid = returned && _wcsicmp(static_cast<const wchar_t*>(_bstr_t(returned->tagName)), L"DIV") == 0 && _wcsicmp(static_cast<const wchar_t*>(_bstr_t(returned->className)), L"image") == 0;
				returned = NULL; inserted.Clear();
				if (!valid) { appendProbePhase("probe-failed;phase=image-result-inspect"); output.Close(); ::PostQuitMessage(1); return 0; }
				appendProbePhase("image-result-inspected-and-released");
			}
			else if (probe == L"image-hold-start") appendProbePhase("image-result-held");
			else if (probe == L"image-discard-start") appendProbePhase("image-result-discarded");
			appendProbePhase("image-insert-complete");
		}
		else if (probe == L"call-insimage-empty")
		{
			if (!selectParagraphPosition(L"start")) { appendProbePhase("probe-failed;reason=selection"); output.Close(); ::PostQuitMessage(1); return 0; }
			appendProbePhase("call-insimage-empty-start");
			{
				MSHTML::IHTMLDOMNodePtr returned(m_doc->m_body.Call(L"InsImage"));
				if (!returned) { appendProbePhase("probe-failed;phase=call-insimage-empty"); output.Close(); ::PostQuitMessage(1); return 0; }
			}
			appendProbePhase("call-insimage-empty-return-released");
			appendProbePhase("image-insert-complete");
		}
		else if (probe == L"body-cache-cached" || probe == L"body-cache-fresh" || probe == L"body-cache-production-undo")
		{
			if (!selectParagraphPosition(L"start")) { appendProbePhase("probe-failed;reason=selection"); output.Close(); ::PostQuitMessage(1); return 0; }
			cachedBody = body;
			MSHTML::IHTMLWindow2Ptr window(m_doc->m_body.Document()->parentWindow);
			const wchar_t* exactInsert = L"function FbeBodyCacheProbe(){var rng=document.selection.createRange(),p=rng.parentElement();while(p&&p.tagName!='P')p=p.parentElement;if(!p)throw new Error('paragraph');window.external.BeginUndoUnit(document,'insert image');p.insertAdjacentHTML('beforeBegin',\"<DIV contentEditable='false' class='image' href='#existing-image'></DIV>\");window.external.EndUndoUnit(document);}";
			appendProbePhase("body-cache-insert-start");
			hr = window ? window->execScript(_bstr_t(exactInsert), _bstr_t(L"JScript")) : E_NOINTERFACE;
			if (SUCCEEDED(hr)) hr = script.Invoke0(L"FbeBodyCacheProbe");
			if (FAILED(hr)) { appendProbePhase("probe-failed;phase=body-cache-insert"); output.Close(); ::PostQuitMessage(1); return 0; }
			appendProbePhase("body-cache-insert-complete");
		}
		else if (probe.Left(14) == L"fbe285-bisect-")
		{
			if (!selectParagraphPosition(L"start")) { appendProbePhase("probe-failed;reason=selection"); output.Close(); ::PostQuitMessage(1); return 0; }
			MSHTML::IHTMLWindow2Ptr window(m_doc->m_body.Document()->parentWindow);
			if (!window) { appendProbePhase("probe-failed;reason=window"); output.Close(); ::PostQuitMessage(1); return 0; }
			const bool normalize = probe != L"fbe285-bisect-no-normalize" && probe != L"fbe285-bisect-no-whole-no-normalize" && probe != L"fbe285-bisect-compile-label-unconditional" && probe != L"fbe285-bisect-minimal";
			const bool useWhole = probe != L"fbe285-bisect-no-whole" && probe != L"fbe285-bisect-no-whole-no-normalize" && probe != L"fbe285-bisect-no-whole-unconditional" && probe != L"fbe285-bisect-no-whole-if-true" && probe != L"fbe285-bisect-compile-label-unconditional" && probe != L"fbe285-bisect-minimal";
			const bool moveToElement = useWhole && probe != L"fbe285-bisect-no-move-to-element";
			const bool duplicate = moveToElement && probe != L"fbe285-bisect-no-duplicate";
			const bool setEndPoint = duplicate && probe != L"fbe285-bisect-no-set-endpoint";
			const bool readText = setEndPoint && probe != L"fbe285-bisect-no-text-read";
			CStringW algorithm;
			if (probe == L"fbe285-bisect-compile-exact")
			{
				algorithm = L"var rng=document.selection.createRange(),p=rng.parentElement();while(p&&p.tagName!='P')p=p.parentElement;if(!p)throw new Error('paragraph');window.external.BeginUndoUnit(document,'compile probe');p.insertAdjacentHTML('beforeBegin',\"<DIV contentEditable='false' class='image' href='#existing-image'></DIV>\");window.external.EndUndoUnit(document);";
			}
			else
			{
				algorithm = L"var rng=document.selection.createRange();";
				if (normalize) algorithm += L"if(rng.compareEndPoints('StartToEnd',rng)!=0){rng.collapse(true);if(rng.move('character',1)==1)rng.move('character',-1);}";
				algorithm += L"var pp=rng.parentElement();while(pp&&pp.tagName!='P')pp=pp.parentElement;if(!pp)throw new Error('paragraph');";
			if (useWhole) algorithm += L"var whole=document.body.createTextRange();";
			if (moveToElement) algorithm += L"whole.moveToElementText(pp);";
			if (duplicate) algorithm += L"var left=whole.duplicate(),right=whole.duplicate();";
			if (setEndPoint) algorithm += L"left.setEndPoint('EndToStart',rng);right.setEndPoint('StartToEnd',rng);";
			if (readText) algorithm += L"var atStart=(left.text==null||left.text=='');";
			else if (probe != L"fbe285-bisect-minimal") algorithm += L"var atStart=true;";
			algorithm += (probe == L"fbe285-bisect-compile-label-unconditional" || probe == L"fbe285-bisect-minimal") ? L"window.external.BeginUndoUnit(document,'compile probe');" : L"window.external.BeginUndoUnit(document,'insert image');";
			if (probe == L"fbe285-bisect-no-whole-unconditional" || probe == L"fbe285-bisect-compile-label-unconditional" || probe == L"fbe285-bisect-minimal") algorithm += L"pp.insertAdjacentHTML('beforeBegin',\"<DIV contentEditable='false' class='image' href='#existing-image'></DIV>\");";
			else if (probe == L"fbe285-bisect-no-whole-if-true") algorithm += L"if(true)pp.insertAdjacentHTML('beforeBegin',\"<DIV contentEditable='false' class='image' href='#existing-image'></DIV>\");";
			else algorithm += L"if(atStart)pp.insertAdjacentHTML('beforeBegin',\"<DIV contentEditable='false' class='image' href='#existing-image'></DIV>\");";
				algorithm += L"window.external.EndUndoUnit(document);";
			}
			CStringW functionScript(L"function Fbe285BisectProbe(){"); functionScript += algorithm; functionScript += L"}";
			appendProbePhase("bisect-register-start");
			hr = window->execScript(_bstr_t(functionScript), _bstr_t(L"JScript"));
			if (SUCCEEDED(hr)) appendProbePhase("bisect-register-complete");
			if (SUCCEEDED(hr)) { appendProbePhase("bisect-insert-start"); hr = script.Invoke0(L"Fbe285BisectProbe"); }
			if (FAILED(hr)) { appendProbePhase("probe-failed;phase=bisect-insert"); output.Close(); ::PostQuitMessage(1); return 0; }
			CStringA operations;
			operations.Format("bisect-operations;normalize=%d;whole=%d;move-to-element=%d;duplicate=%d;set-end-point=%d;read-text=%d;insert=%S", normalize, useWhole, moveToElement, duplicate, setEndPoint, readText, probe == L"fbe285-bisect-no-whole-unconditional" ? L"unconditional" : (probe == L"fbe285-bisect-no-whole-if-true" ? L"if-true" : L"if-at-start"));
			appendProbePhase(operations);
			appendProbePhase("bisect-insert-complete");
		}
		else if (probe == L"image-no-url" || probe == L"plain-block" || probe == L"plain-block-after" || probe == L"plain-block-auto" || probe == L"plain-block-markup" || probe == L"plain-block-custom" || probe == L"plain-block-adjacent-html" || probe == L"plain-block-adjacent-html-after" || probe == L"plain-block-adjacent-html-image" || probe == L"plain-block-adjacent-html-image-after" || probe == L"plain-block-range-html" || probe == L"plain-block-range-html-after" || probe == L"fbe285-start" || probe == L"fbe285-end" || probe == L"fbe285-middle" || probe == L"fbe285-return-hold-start" || probe == L"fbe285-dispatch-full-start" || probe == L"fbe285-dispatch-full-invoke2-start" || probe == L"fbe285-dispatch-dom-start" || probe == L"fbe285-exec-full-start" || probe == L"fbe285-exec-split-start")
		{
			appendProbePhase(probe == L"image-no-url" ? "image-no-url-start" : "plain-block-start");
			if (probe != L"plain-block-auto" && probe != L"plain-block-custom" && probe.Left(7) != L"fbe285-") m_doc->m_body.BeginUndoUnit(probe == L"image-no-url" ? L"undo probe image without URL" : L"undo probe plain block");
			MSHTML::IHTMLElementPtr image(m_doc->m_body.Document()->createElement(L"DIV"));
			if (probe == L"image-no-url") { image->className = L"image"; image->setAttribute(L"href", _variant_t(L"#existing-image"), 0); }
			if (probe == L"plain-block-markup")
			{
				MSHTML::IMarkupServices2Ptr markup(m_doc->m_body.MarkupServices());
				MSHTML::IMarkupPointerPtr startPointer, finish;
				hr = markup->CreateMarkupPointer(&startPointer); if (SUCCEEDED(hr)) hr = markup->CreateMarkupPointer(&finish);
				if (SUCCEEDED(hr)) hr = startPointer->MoveAdjacentToElement(paragraph, MSHTML::ELEM_ADJ_BeforeBegin);
				if (SUCCEEDED(hr)) hr = finish->MoveAdjacentToElement(paragraph, MSHTML::ELEM_ADJ_BeforeBegin);
				if (SUCCEEDED(hr)) hr = markup->InsertElement(image, startPointer, finish);
				if (FAILED(hr)) { m_doc->m_body.EndUndoUnit(); appendProbePhase("probe-failed;phase=markup-insert"); output.Close(); ::PostQuitMessage(1); return 0; }
			}
			else if (probe == L"plain-block-custom")
			{
				IServiceProviderPtr service(m_doc->m_body.Document()); CComPtr<IOleUndoManager> manager;
				hr = service ? service->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager)) : E_NOINTERFACE;
				{
					CUndoManagerEnableScope disabled(manager);
					if (FAILED(hr) || !disabled.Active()) { appendProbePhase("probe-failed;phase=undo-disable"); output.Close(); ::PostQuitMessage(1); return 0; }
					MSHTML::IHTMLElement2Ptr(paragraph)->insertAdjacentElement(L"beforeBegin", image);
				}
				CComObject<CBlockImageUndoProbeUnit>* raw = NULL; hr = CComObject<CBlockImageUndoProbeUnit>::CreateInstance(&raw);
				if (SUCCEEDED(hr)) { raw->AddRef(); raw->Initialize(image, paragraph); hr = manager->Add(raw); raw->Release(); }
				if (FAILED(hr)) { appendProbePhase("probe-failed;phase=undo-add"); output.Close(); ::PostQuitMessage(1); return 0; }
			}
			else if (probe == L"plain-block-adjacent-html" || probe == L"plain-block-adjacent-html-after" || probe == L"plain-block-adjacent-html-image" || probe == L"plain-block-adjacent-html-image-after")
			{
				const bool beforeBegin = probe == L"plain-block-adjacent-html" || probe == L"plain-block-adjacent-html-image";
				const bool withImage = probe == L"plain-block-adjacent-html-image" || probe == L"plain-block-adjacent-html-image-after";
				paragraph->insertAdjacentHTML(beforeBegin ? L"beforeBegin" : L"afterEnd", withImage ? L"<DIV contentEditable='false' class='image' href='#existing-image'><IMG src='fbw-internal:#existing-image'></DIV>" : L"<DIV class='probe-block'></DIV>");
			}
			else if (probe == L"plain-block-range-html" || probe == L"plain-block-range-html-after")
			{
				MSHTML::IHTMLTxtRangePtr boundary(body->createTextRange());
				boundary->moveToElementText(paragraph); boundary->collapse(probe == L"plain-block-range-html" ? VARIANT_TRUE : VARIANT_FALSE); boundary->pasteHTML(L"<DIV class='probe-block'></DIV>");
			}
			else if (probe.Left(7) == L"fbe285-")
			{
				MSHTML::IHTMLTxtRangePtr range(body->createTextRange());
				range->moveToElementText(paragraph);
				if (probe == L"fbe285-end") range->collapse(VARIANT_FALSE);
				else { range->collapse(VARIANT_TRUE); if (probe == L"fbe285-middle") range->move(L"character", 3); else if (range->move(L"character", 1) == 1) range->move(L"character", -1); }
				range->select();
				appendProbePhase("fbe285-selection-complete");
				// FBE 2.8.5 main.js algorithm, with the fixture's pre-existing binary id.
				const wchar_t* const legacyAlgorithm =
					L"var rng=document.selection.createRange();if(!rng || !(\"compareEndPoints\" in rng))throw new Error('range');if(rng.compareEndPoints(\"StartToEnd\",rng)!=0){rng.collapse(true);if(rng.move(\"character\",1)==1)rng.move(\"character\",-1);}var cp=rng.parentElement(),pp=cp;while(pp&&pp.tagName!=\"P\")pp=pp.parentElement;var pe=cp;while(pe&&(pe.tagName!=\"DIV\"||pe.className!=\"section\"))pe=pe.parentElement;if(!pe||!pp)throw new Error('section');var owner=pp.parentElement;while(owner&&owner.tagName!=\"DIV\")owner=owner.parentElement;if(!owner||owner.sourceIndex!=pe.sourceIndex)throw new Error('owner');var ht=\"<DIV onresizestart='return false' contentEditable='false' class='image' href='#existing-image'><IMG src='fbw-internal:#existing-image'></DIV>\";var whole=document.body.createTextRange();whole.moveToElementText(pp);var left=whole.duplicate();left.setEndPoint(\"EndToStart\",rng);var right=whole.duplicate();right.setEndPoint(\"StartToEnd\",rng);var leftText=left.text,rightText=right.text;if(leftText==null||leftText==\"\"){pp.insertAdjacentHTML(\"beforeBegin\",ht);}else if(rightText==null||rightText==\"\"){pp.insertAdjacentHTML(\"afterEnd\",ht);}else{var leftHTML=left.htmlText,rightHTML=right.htmlText,rp=pp.cloneNode(false);if(rp.id)rp.removeAttribute(\"id\");pp.innerHTML=leftHTML;rp.innerHTML=rightHTML;InflateIt(pp);InflateIt(rp);pp.insertAdjacentHTML(\"afterEnd\",ht);var image=pp.nextSibling;image.insertAdjacentElement(\"afterEnd\",rp);var nr=document.body.createTextRange();nr.moveToElementText(rp);nr.collapse(true);nr.select();}";
				MSHTML::IHTMLElementPtr legacyParent(paragraph->parentElement);
				CStringA legacyContainer;
				legacyContainer.Format("legacy-container=%S.%S", legacyParent ? static_cast<const wchar_t*>(_bstr_t(legacyParent->tagName)) : L"none", legacyParent ? static_cast<const wchar_t*>(_bstr_t(legacyParent->className)) : L"none");
				appendProbePhase(legacyContainer);
				CStringW legacyScript(L"(function(){");
				legacyScript += legacyAlgorithm;
				// The original function returns when the selected element is outside a section.
				// Keep that control flow in the probe: a JavaScript throw opens a modal dialog.
				legacyScript.Replace(L"throw new Error('range');", L"return;");
				legacyScript.Replace(L"throw new Error('section');", L"return;");
				legacyScript.Replace(L"throw new Error('owner');", L"return;");
				if (probe == L"fbe285-return-hold-start") legacyScript += L"window.Fbe285ProbeReturn=function(){return pp.previousSibling;};";
				legacyScript += L"})();";
				MSHTML::IHTMLWindow2Ptr window(m_doc->m_body.Document()->parentWindow);
				const bool dispatchFull = probe == L"fbe285-dispatch-full-start" || probe == L"fbe285-dispatch-full-invoke2-start";
				const bool dispatchFullInvoke2 = probe == L"fbe285-dispatch-full-invoke2-start";
				const bool dispatchDom = probe == L"fbe285-dispatch-dom-start";
				const bool execFull = probe == L"fbe285-exec-full-start";
				if (dispatchFull || dispatchDom)
				{
					CStringW functionScript(L"function Fbe285DispatchProbe(){");
					if (dispatchFull) functionScript += L"window.external.BeginUndoUnit(document,'insert image');";
					functionScript += legacyAlgorithm;
					if (dispatchFull) functionScript += L"window.external.EndUndoUnit(document);";
					functionScript += L"}";
					functionScript.Replace(L"throw new Error('range');", L"return;");
					functionScript.Replace(L"throw new Error('section');", L"return;");
					functionScript.Replace(L"throw new Error('owner');", L"return;");
					hr = window ? window->execScript(_bstr_t(functionScript), _bstr_t(L"JScript")) : E_NOINTERFACE;
					if (SUCCEEDED(hr) && dispatchDom) { appendProbePhase("fbe285-begin-undo-start"); hr = window->execScript(_bstr_t(L"window.external.BeginUndoUnit(document,'insert image');"), _bstr_t(L"JScript")); if (SUCCEEDED(hr)) appendProbePhase("fbe285-begin-undo-complete"); }
					if (SUCCEEDED(hr)) { appendProbePhase("fbe285-dispatch-start"); _variant_t check(false), existingId(L"existing-image"), result; hr = dispatchFullInvoke2 ? script.Invoke2(L"Fbe285DispatchProbe", &check, &existingId, &result) : script.Invoke0(L"Fbe285DispatchProbe"); }
					if (SUCCEEDED(hr)) appendProbePhase("fbe285-dispatch-complete");
					if (SUCCEEDED(hr) && dispatchDom) { appendProbePhase("fbe285-end-undo-start"); hr = window->execScript(_bstr_t(L"window.external.EndUndoUnit(document);"), _bstr_t(L"JScript")); if (SUCCEEDED(hr)) appendProbePhase("fbe285-end-undo-complete"); }
				}
				else if (execFull)
				{
					CStringW fullScript(L"(function(){window.external.BeginUndoUnit(document,'insert image');");
					fullScript += legacyAlgorithm; fullScript += L"window.external.EndUndoUnit(document);})();";
					fullScript.Replace(L"throw new Error('range');", L"return;"); fullScript.Replace(L"throw new Error('section');", L"return;"); fullScript.Replace(L"throw new Error('owner');", L"return;");
					appendProbePhase("fbe285-exec-full-start"); hr = window ? window->execScript(_bstr_t(fullScript), _bstr_t(L"JScript")) : E_NOINTERFACE; if (SUCCEEDED(hr)) appendProbePhase("fbe285-exec-full-complete");
				}
				else
				{
					appendProbePhase("fbe285-begin-undo-start");
					hr = window ? window->execScript(_bstr_t(L"window.external.BeginUndoUnit(document,'insert image');"), _bstr_t(L"JScript")) : E_NOINTERFACE;
					if (SUCCEEDED(hr)) appendProbePhase("fbe285-begin-undo-complete");
					if (SUCCEEDED(hr)) { appendProbePhase("fbe285-dom-start"); hr = window->execScript(_bstr_t(legacyScript), _bstr_t(L"JScript")); }
					if (SUCCEEDED(hr)) appendProbePhase("fbe285-dom-complete");
					if (SUCCEEDED(hr)) { appendProbePhase("fbe285-end-undo-start"); hr = window->execScript(_bstr_t(L"window.external.EndUndoUnit(document);"), _bstr_t(L"JScript")); }
					if (SUCCEEDED(hr)) appendProbePhase("fbe285-end-undo-complete");
				}
				if (SUCCEEDED(hr) && probe == L"fbe285-return-hold-start")
				{
					_variant_t held;
					hr = script.Invoke0(L"Fbe285ProbeReturn", &held);
					if (FAILED(hr) || V_VT(&held) != VT_DISPATCH) { appendProbePhase("probe-failed;phase=fbe285-result"); output.Close(); ::PostQuitMessage(1); return 0; }
					appendProbePhase("fbe285-result-held");
					// Keep the returned DOM dispatch alive through Undo/Redo below.
					binaryId = held;
				}
				if (FAILED(hr)) { appendProbePhase("probe-failed;phase=fbe285-insert"); output.Close(); ::PostQuitMessage(1); return 0; }
			}
			else MSHTML::IHTMLElement2Ptr(paragraph)->insertAdjacentElement(probe == L"plain-block-after" ? L"afterEnd" : L"beforeBegin", image);
			if (probe != L"plain-block-auto" && probe != L"plain-block-custom" && probe.Left(7) != L"fbe285-") m_doc->m_body.EndUndoUnit();
			appendProbePhase(probe == L"image-no-url" ? "image-no-url-complete" : "plain-block-complete");
		}
		else { appendProbePhase("probe-failed;reason=unknown-name"); output.Close(); ::PostQuitMessage(1); return 0; }
		CString insertedOuterHtml, insertedChildOrder, finalOuterHtml, finalChildOrder;
		if (!snapshotSection(insertedOuterHtml, insertedChildOrder)) { appendProbePhase("probe-failed;phase=dom-after-insert"); output.Close(); ::PostQuitMessage(1); return 0; }
		appendSectionSnapshot("dom-after-insert", insertedOuterHtml, insertedChildOrder);
		if (undoRedoCycles > 0)
		{
			m_doc->m_body.SetFocus();
			appendProbePhase("undo-start");
			BOOL handled = FALSE;
			if (probe == L"body-cache-production-undo") m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			else m_doc->m_body.ExecCommand(IDM_UNDO);
			appendProbePhase("undo-complete");
			if (probe == L"body-cache-cached")
			{
				const long count = blockImageCountFor(cachedBody); CStringA phase; phase.Format("cached-body-count=%ld", count); appendProbePhase(phase);
			}
			else if (probe == L"body-cache-fresh")
			{
				MSHTML::IHTMLDocument2Ptr freshDocument(m_doc->m_body.Document());
				MSHTML::IHTMLBodyElementPtr freshBody(freshDocument ? freshDocument->body : MSHTML::IHTMLBodyElementPtr());
				const long count = blockImageCountFor(freshBody); CStringA phase; phase.Format("fresh-body-count=%ld", count); appendProbePhase(phase);
			}
			if (probe.Left(14) != L"fbe285-bisect-" && (probe == L"image" || probe == L"image-start" || probe == L"image-end" || probe == L"image-middle" || probe == L"image-hold-start" || probe == L"image-clear-start" || probe == L"image-discard-start" || probe == L"image-inspect-release-start" || probe == L"call-insimage-empty" || probe == L"image-no-url" || probe == L"plain-block" || probe == L"plain-block-auto") && blockImageCount() != 0)
			{
				appendProbePhase("probe-failed;phase=undo;reason=image-remained"); output.Close(); ::PostQuitMessage(1); return 0;
			}
			for (int cycle = 0; cycle < undoRedoCycles; ++cycle)
			{
				m_doc->m_body.SetFocus(); CStringA phase; phase.Format("redo-start-%d", cycle + 1); appendProbePhase(phase); if (probe == L"body-cache-production-undo") m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled); else m_doc->m_body.ExecCommand(IDM_REDO); phase.Format("redo-complete-%d", cycle + 1); appendProbePhase(phase);
				if (cycle + 1 == undoRedoCycles) break;
				m_doc->m_body.SetFocus(); phase.Format("undo-start-%d", cycle + 2); appendProbePhase(phase); m_doc->m_body.ExecCommand(IDM_UNDO); phase.Format("undo-complete-%d", cycle + 2); appendProbePhase(phase);
			}
		}
		else appendProbePhase("undo-redo-skipped");
		if (!snapshotSection(finalOuterHtml, finalChildOrder)) { appendProbePhase("probe-failed;phase=dom-before-save"); output.Close(); ::PostQuitMessage(1); return 0; }
		appendSectionSnapshot("dom-before-save", finalOuterHtml, finalChildOrder);
		CStringA domComparison;
		domComparison.Format("dom-compare;outerhtml-equal=%d;child-order-equal=%d", insertedOuterHtml == finalOuterHtml ? 1 : 0, insertedChildOrder == finalChildOrder ? 1 : 0);
		appendProbePhase(domComparison);
		if (undoRedoCycles > 0 && (insertedOuterHtml != finalOuterHtml || insertedChildOrder != finalChildOrder)) { appendProbePhase("probe-failed;phase=dom-compare"); output.Close(); ::PostQuitMessage(1); return 0; }
		if (probe.Left(14) == L"fbe285-bisect-") { appendProbePhase("bisect-complete"); output.Close(); ::PostQuitMessage(0); return 0; }
		if (probe.Left(11) == L"body-cache-") { appendProbePhase("body-cache-complete"); output.Close(); ::PostQuitMessage(0); return 0; }
		appendProbePhase("idle-start");
		for (int idle = 0; idle < 8; ++idle) { MSG message = {}; if (::PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessage(&message); } else ::Sleep(5); }
		appendProbePhase("idle-complete");
		appendProbePhase("save-start"); if (!m_doc->Save()) { appendProbePhase("probe-failed;phase=save"); output.Close(); ::PostQuitMessage(1); return 0; }
		appendProbePhase("save-complete");
		if (probe == L"image" || probe == L"image-start" || probe == L"image-end" || probe == L"image-middle" || probe == L"image-hold-start" || probe == L"image-clear-start" || probe == L"image-discard-start" || probe == L"image-inspect-release-start")
		{
			const CString filename(m_doc->m_filename);
			appendProbePhase("reopen-start");
			if (filename.IsEmpty() || LoadFile(filename) != OK || !refreshSection()) { appendProbePhase("probe-failed;phase=reopen"); output.Close(); ::PostQuitMessage(1); return 0; }
			CString reopenedOuterHtml, reopenedChildOrder;
			if (!snapshotSection(reopenedOuterHtml, reopenedChildOrder)) { appendProbePhase("probe-failed;phase=reopen-dom"); output.Close(); ::PostQuitMessage(1); return 0; }
			appendSectionSnapshot("dom-after-reopen", reopenedOuterHtml, reopenedChildOrder);
			const bool orderMatches = (probe == L"image-end" && reopenedChildOrder.Find(L"P#") == 0 && reopenedChildOrder.Find(L"|DIV#(null).image") > 0) ||
				(probe == L"image-middle" && reopenedChildOrder.Find(L"P#") == 0 && reopenedChildOrder.Find(L"|DIV#(null).image|P#") > 0) ||
				((probe == L"image" || probe == L"image-start" || probe == L"image-hold-start" || probe == L"image-clear-start" || probe == L"image-discard-start" || probe == L"image-inspect-release-start") && reopenedChildOrder.Find(L"DIV#(null).image|P#") == 0);
			if (!orderMatches || reopenedOuterHtml.Find(L"existing-image") < 0) { appendProbePhase("probe-failed;phase=reopen-structure"); output.Close(); ::PostQuitMessage(1); return 0; }
			appendProbePhase("reopen-complete");
		}
		output.Close(); ::PostQuitMessage(0); return 0;
	}
	if (IsFbeTestScenario(L"binary-import-image"))
	{
		static bool imageImportRunnerQueued = false;
		if (!imageImportRunnerQueued)
		{
			imageImportRunnerQueued = true;
			output.Close();
			SetTimer(IMAGE_IMPORT_TEST_TIMER_ID, 250);
			return 0;
		}
		const ULONGLONG start = ::GetTickCount64();
		auto appendImportPhase = [&](const char* phase)
		{
			const FbeSourceDiagnostics::ProcessMemorySnapshot memory = FbeSourceDiagnostics::GetProcessMemorySnapshot();
			CStringA row;
			row.Format("%s\t%I64u\t%I64u\t%I64u\r\n", phase, ::GetTickCount64() - start,
				static_cast<unsigned __int64>(memory.privateBytes), static_cast<unsigned __int64>(memory.workingSetBytes));
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		CStringA header("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();

		wchar_t imagePath[MAX_PATH] = {};
		const DWORD imagePathLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_PATH", imagePath, _countof(imagePath));
		if (imagePathLength == 0 || imagePathLength >= _countof(imagePath) || ::GetFileAttributes(imagePath) == INVALID_FILE_ATTRIBUTES)
		{
			appendImportPhase("import-failed;phase=import;reason=image-path");
			output.Close(); ::PostQuitMessage(1); return 0;
		}

		appendImportPhase("open-complete");
		appendImportPhase("import-start");
		// Exercise the same image-import route as the UI, including the generated
		// binary id and apiAddBinary call, rather than constructing FB2 XML here.
		::ShowWindow(m_hWnd, SW_RESTORE);
		::SetForegroundWindow(m_hWnd);
		m_doc->m_body.SetFocus();
		MSHTML::IHTMLBodyElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLBodyElementPtr());
		MSHTML::IHTMLElementCollectionPtr paragraphs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		auto isSectionDiv = [](const MSHTML::IHTMLElementPtr& element) -> bool
		{
			if (!element) return false;
			const _bstr_t tagName(element->tagName);
			const _bstr_t className(element->className);
			const wchar_t* const tagText = tagName;
			const wchar_t* const classText = className;
			return tagText && classText && _wcsicmp(tagText, L"DIV") == 0 && _wcsicmp(classText, L"section") == 0;
		};
		MSHTML::IHTMLElementPtr paragraph;
		for (long index = 0; paragraphs && index < paragraphs->length && !paragraph; ++index)
		{
			MSHTML::IHTMLElementPtr candidate(paragraphs->item(_variant_t(index), _variant_t()));
			for (MSHTML::IHTMLElementPtr ancestor(candidate); ancestor; ancestor = ancestor->parentElement)
			{
				if (isSectionDiv(ancestor))
				{
					paragraph = candidate;
					break;
				}
			}
		}
		MSHTML::IHTMLTxtRangePtr range(body ? body->createTextRange() : MSHTML::IHTMLTxtRangePtr());
		if (!range || !paragraph)
		{
			appendImportPhase("import-failed;phase=import;reason=section-range");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		range->moveToElementText(paragraph);
		range->collapse(VARIANT_TRUE);
		wchar_t caretOffsetText[16] = {};
		const DWORD caretOffsetLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_CARET_OFFSET", caretOffsetText, _countof(caretOffsetText));
		const long caretOffset = caretOffsetLength && caretOffsetLength < _countof(caretOffsetText) ? _wtol(caretOffsetText) : 1;
		// MSHTML represents a literal paragraph start as a structural boundary.
		// Move through the first character and back so the selection remains in
		// the paragraph while its final position is immediately before it.
		const long moved = caretOffset == 0 ? (range->move(L"character", 1) == 1 ? range->move(L"character", -1) : 0) : range->move(L"character", caretOffset);
		if (caretOffset < 0 || (caretOffset == 0 ? moved != -1 : moved != caretOffset))
		{
			appendImportPhase("import-failed;phase=import;reason=section-caret");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		range->select();
		auto selectionIsInSection = [&]() -> bool
		{
			MSHTML::IHTMLTxtRangePtr selected(m_doc->m_body.Document()->selection->createRange());
			MSHTML::IHTMLElementPtr element(selected ? selected->parentElement() : MSHTML::IHTMLElementPtr());
			while (element && !isSectionDiv(element)) element = element->parentElement;
			return isSectionDiv(element);
		};
		const ULONGLONG selectionDeadline = ::GetTickCount64() + 1000;
		while (!selectionIsInSection() && ::GetTickCount64() < selectionDeadline)
		{
			MSG message = {};
			if (::PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
			{
				if (message.message == WM_QUIT) { ::PostQuitMessage(static_cast<int>(message.wParam)); break; }
				::TranslateMessage(&message);
				::DispatchMessage(&message);
			}
			else ::Sleep(1);
		}
		if (!selectionIsInSection())
		{
			appendImportPhase("import-failed;phase=import;reason=section-selection");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		wchar_t inlineMode[2] = {};
		const bool inlineImage = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_INLINE", inlineMode, _countof(inlineMode)) != 1 || inlineMode[0] != L'0';
		m_doc->m_body.AddImage(imagePath, inlineImage);
		appendImportPhase("import-complete");
		wchar_t undoRedoMode[2] = {};
		const bool undoRedo = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_UNDO_REDO", undoRedoMode, _countof(undoRedoMode)) == 1 && undoRedoMode[0] == L'1';
		wchar_t undoRedoCyclesText[8] = {};
		const DWORD undoRedoCyclesLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_UNDO_REDO_CYCLES", undoRedoCyclesText, _countof(undoRedoCyclesText));
		const int undoRedoCycles = undoRedoCyclesLength && undoRedoCyclesLength < _countof(undoRedoCyclesText) ? _wtoi(undoRedoCyclesText) : 1;
		if (undoRedoCycles < 1 || undoRedoCycles > 5) { appendImportPhase("import-failed;phase=undo;reason=undo-redo-cycles"); output.Close(); ::PostQuitMessage(1); return 0; }
		if (!inlineImage && undoRedo)
		{
			appendImportPhase("post-insert-idle-start");
			for (int idle = 0; idle < 8; ++idle) { MSG message = {}; if (::PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessage(&message); } else ::Sleep(5); }
			appendImportPhase("post-insert-idle-complete");
			auto sectionOuterHtml = [&]() -> CString
			{
				MSHTML::IHTMLWindow2Ptr window(m_doc->m_body.Document() ? m_doc->m_body.Document()->parentWindow : MSHTML::IHTMLWindow2Ptr());
				const wchar_t* snapshotScript = L"function FbeRuntimeImageSectionSnapshot(){var divs=document.body.getElementsByTagName('DIV');for(var i=0;i<divs.length;i++)if(divs[i].className=='section')return divs[i].outerHTML;return '';}";
				if (!window || FAILED(window->execScript(_bstr_t(snapshotScript), _bstr_t(L"JScript")))) return CString();
				CComDispatchDriver script(m_doc->m_body.Script()); _variant_t snapshot;
				return SUCCEEDED(script.Invoke0(L"FbeRuntimeImageSectionSnapshot", &snapshot)) && V_VT(&snapshot) == VT_BSTR ? static_cast<const wchar_t*>(_bstr_t(snapshot)) : CString();
			};
			CString insertedSection;
			insertedSection = sectionOuterHtml();
			if (insertedSection.IsEmpty()) { appendImportPhase("import-failed;phase=undo;reason=initial-section"); output.Close(); ::PostQuitMessage(1); return 0; }
			for (int cycle = 0; cycle < undoRedoCycles; ++cycle)
			{
				BOOL handled = FALSE;
				m_doc->m_body.SetFocus(); appendImportPhase("undo-start");
				m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
				appendImportPhase("undo-complete");
				// Do not dereference MSHTML DOM between the native Undo and Redo.
				m_doc->m_body.SetFocus(); appendImportPhase("redo-start");
				m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
				appendImportPhase("redo-complete");
			}
			const CString finalSection = sectionOuterHtml();
			if (finalSection != insertedSection) { appendImportPhase("import-failed;phase=redo;reason=final-dom"); output.Close(); ::PostQuitMessage(1); return 0; }
			appendImportPhase("redo-dom-complete");
		}
		appendImportPhase("save-start");
		if (!m_doc->Save())
		{
			appendImportPhase("save-failed;phase=save;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendImportPhase("save-complete");
		if (!inlineImage && undoRedo)
		{
			const CString filename(m_doc->m_filename);
			appendImportPhase("reopen-start");
			if (filename.IsEmpty() || LoadFile(filename) != OK) { appendImportPhase("import-failed;phase=reopen"); output.Close(); ::PostQuitMessage(1); return 0; }
			MSHTML::IHTMLDocument2Ptr reopenedDocument(m_doc->m_body.Document());
			MSHTML::IHTMLBodyElementPtr reopenedBody(reopenedDocument ? reopenedDocument->body : MSHTML::IHTMLBodyElementPtr());
			MSHTML::IHTMLElementPtr reopenedElement(reopenedBody);
			const CString reopenedHtml(reopenedElement ? static_cast<const wchar_t*>(_bstr_t(reopenedElement->outerHTML)) : L"");
			if (reopenedHtml.Find(L"fbw-internal:#") < 0 || reopenedHtml.Find(L"href=\"#") < 0) { appendImportPhase("import-failed;phase=reopen;reason=image-runtime-url"); output.Close(); ::PostQuitMessage(1); return 0; }
			appendImportPhase("reopen-complete");
		}
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"binary-roundtrip"))
	{
		const ULONGLONG start = ::GetTickCount64();
		auto appendBinaryPhase = [&](const char* phase)
		{
			const FbeSourceDiagnostics::ProcessMemorySnapshot memory = FbeSourceDiagnostics::GetProcessMemorySnapshot();
			CStringA row;
			row.Format("%s\t%I64u\t%I64u\t%I64u\r\n", phase, ::GetTickCount64() - start,
				static_cast<unsigned __int64>(memory.privateBytes), static_cast<unsigned __int64>(memory.workingSetBytes));
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		CStringA header("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();
		appendBinaryPhase("open-complete");
		appendBinaryPhase("save-start");
		if (!m_doc->Save())
		{
			appendBinaryPhase("save-failed;phase=save;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendBinaryPhase("save-complete");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"idle-performance"))
	{
		// Prime the event-driven UI once, then prove that an unchanged document
		// has no command, selection or toolbar work over a long idle streak.
		if (!StartupTrace::Enabled()) { output.Close(); ::PostQuitMessage(1); return 0; }
		wchar_t idleView[16] = {};
		const DWORD idleViewLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IDLE_VIEW", idleView, _countof(idleView));
		const bool sourceIdle = idleViewLength == 6 && std::wcscmp(idleView, L"source") == 0;
		const bool descriptionIdle = idleViewLength == 11 && std::wcscmp(idleView, L"description") == 0;
		if (sourceIdle)
			ShowView(SOURCE);
		else if (descriptionIdle)
			ShowView(DESC);
		MSHTML::IHTMLElementPtr descriptionInput;
		if (descriptionIdle)
		{
			descriptionInput = m_doc->m_body.Document()->all->item(L"tiTitle");
			if (!descriptionInput) { output.Close(); ::PostQuitMessage(1); return 0; }
			MSHTML::IHTMLElement2Ptr(descriptionInput)->focus();
		}
		InvalidateUi(UiDirtyAll);
		m_sel_changed = true;
		OnIdle();

		const ULONGLONG idleBefore = g_idleProfile.count;
		const ULONGLONG commandBefore = g_idleProfile.commandUpdates;
		const ULONGLONG selectionBefore = g_idleProfile.selectionUpdates;
		const ULONGLONG toolbarBefore = g_idleProfile.toolbarUpdates;
		const ULONGLONG fileBefore = g_idleProfile.fileChecks;
		const ULONGLONG clipboardBefore = g_idleProfile.clipboardChecks;
		const ULONGLONG checkCommandBefore = StartupTrace::UiCheckCommandCount();
		const ULONGLONG selectionContainerBefore = StartupTrace::UiSelectionContainerQueryCount();
		const ULONGLONG selectionStructConBefore = StartupTrace::UiSelectionStructConQueryCount();
		const ULONGLONG selectionStructTableConBefore = StartupTrace::UiSelectionStructTableConQueryCount();
		const ULONGLONG comBefore = StartupTrace::UiComCallCount();
		const ULONGLONG started = ::GetTickCount64();
		const int idleCycles = 1000;
		for (int cycle = 0; cycle < idleCycles; ++cycle)
			OnIdle();

		CStringA report;
		report.Format("view\t%s\r\ndescription_input_focused\t%d\r\nidle_cycles\t%I64u\r\nelapsed_ms\t%I64u\r\ncommand_state_updates\t%I64u\r\nselection_context_builds\t%I64u\r\ntoolbar_updates\t%I64u\r\nfile_fingerprint_checks\t%I64u\r\nclipboard_checks\t%I64u\r\ncheck_command_calls\t%I64u\r\nselection_container_queries\t%I64u\r\nselection_struct_con_queries\t%I64u\r\nselection_struct_table_con_queries\t%I64u\r\njs_com_calls\t%I64u\r\n",
			sourceIdle ? "source" : (descriptionIdle ? "description" : "body"), descriptionInput ? 1 : 0, g_idleProfile.count - idleBefore, ::GetTickCount64() - started,
			g_idleProfile.commandUpdates - commandBefore, g_idleProfile.selectionUpdates - selectionBefore,
			g_idleProfile.toolbarUpdates - toolbarBefore, g_idleProfile.fileChecks - fileBefore,
			g_idleProfile.clipboardChecks - clipboardBefore, StartupTrace::UiCheckCommandCount() - checkCommandBefore,
			StartupTrace::UiSelectionContainerQueryCount() - selectionContainerBefore,
			StartupTrace::UiSelectionStructConQueryCount() - selectionStructConBefore,
			StartupTrace::UiSelectionStructTableConQueryCount() - selectionStructTableConBefore,
			StartupTrace::UiComCallCount() - comBefore);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(written == static_cast<DWORD>(report.GetLength()) ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"idle-interaction-performance"))
	{
		// Use the production BODY notification route after real MSHTML range
		// changes.  This distinguishes one event-driven update per interaction
		// from work accidentally repeated during the following idle streak.
		if (!StartupTrace::Enabled() || !m_doc || !m_doc->m_body.Document())
		{
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		MSHTML::IHTMLBodyElementPtr body(m_doc->m_body.Document()->body);
		MSHTML::IHTMLElementCollectionPtr paragraphs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLTxtRangePtr range(body ? body->createTextRange() : MSHTML::IHTMLTxtRangePtr());
		if (!body || !paragraphs || !paragraphs->length || !range)
		{
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		const int caretMoves = 1000;
		const int selectionChanges = 1000;
		const int typingEdits = 10;
		const int stableIdleCycles = 1000;
		auto selectParagraph = [&](int index, bool extend) -> bool
		{
			MSHTML::IHTMLElementPtr paragraph(paragraphs->item(_variant_t(static_cast<long>(index % paragraphs->length)), _variant_t()));
			if (!paragraph) return false;
			range->moveToElementText(paragraph);
			range->collapse(VARIANT_TRUE);
			if (extend) range->moveEnd(L"character", 1);
			range->select();
			return true;
		};

		InvalidateUi(UiDirtyAll); m_sel_changed = true; OnIdle();
		const ULONGLONG idleBefore = g_idleProfile.count;
		const ULONGLONG commandBefore = g_idleProfile.commandUpdates;
		const ULONGLONG selectionBefore = g_idleProfile.selectionUpdates;
		const ULONGLONG toolbarBefore = g_idleProfile.toolbarUpdates;
		const ULONGLONG fileBefore = g_idleProfile.fileChecks;
		const ULONGLONG clipboardBefore = g_idleProfile.clipboardChecks;
		const ULONGLONG checkCommandBefore = StartupTrace::UiCheckCommandCount();
		const ULONGLONG selectionContainerBefore = StartupTrace::UiSelectionContainerQueryCount();
		const ULONGLONG selectionContextContainerBefore = StartupTrace::UiSelectionContextContainerQueryCount();
		const ULONGLONG comBefore = StartupTrace::UiComCallCount();
		const ULONGLONG started = ::GetTickCount64();
		BOOL handled = FALSE;
		for (int index = 0; index < caretMoves; ++index)
		{
			if (!selectParagraph(index, false)) { output.Close(); ::PostQuitMessage(1); return 0; }
			OnEdSelChange(0, 0, NULL, handled); OnIdle();
		}
		for (int index = 0; index < selectionChanges; ++index)
		{
			if (!selectParagraph(index, true)) { output.Close(); ::PostQuitMessage(1); return 0; }
			OnEdSelChange(0, 0, NULL, handled); OnIdle();
		}
		for (int index = 0; index < typingEdits; ++index)
		{
			MSHTML::IHTMLElementPtr paragraph(paragraphs->item(_variant_t(static_cast<long>(index % paragraphs->length)), _variant_t()));
			_bstr_t text(paragraph->innerText);
			CString edited(static_cast<const wchar_t*>(text)); edited += L" x";
			paragraph->innerText = _bstr_t(static_cast<const wchar_t*>(edited));
			OnEdChange(0, 0, NULL, handled); OnIdle();
		}
		const ULONGLONG interactionIdleCycles = g_idleProfile.count - idleBefore;
		const ULONGLONG interactionElapsed = ::GetTickCount64() - started;
		const ULONGLONG idleBeforeStable = g_idleProfile.count;
		const ULONGLONG commandBeforeStable = g_idleProfile.commandUpdates;
		const ULONGLONG selectionBeforeStable = g_idleProfile.selectionUpdates;
		const ULONGLONG toolbarBeforeStable = g_idleProfile.toolbarUpdates;
		const ULONGLONG checkCommandBeforeStable = StartupTrace::UiCheckCommandCount();
		const ULONGLONG comBeforeStable = StartupTrace::UiComCallCount();
		for (int cycle = 0; cycle < stableIdleCycles; ++cycle)
			OnIdle();

		CStringA report;
		report.Format("caret_moves\t%d\r\nselection_changes\t%d\r\ntyping_edits\t%d\r\ninteraction_idle_cycles\t%I64u\r\ninteraction_elapsed_ms\t%I64u\r\ncommand_state_updates\t%I64u\r\nselection_context_builds\t%I64u\r\nselection_container_queries\t%I64u\r\nselection_context_container_queries\t%I64u\r\ntoolbar_updates\t%I64u\r\nfile_fingerprint_checks\t%I64u\r\nclipboard_checks\t%I64u\r\ncheck_command_calls\t%I64u\r\njs_com_calls\t%I64u\r\nstable_idle_cycles\t%I64u\r\nstable_command_state_updates\t%I64u\r\nstable_selection_context_builds\t%I64u\r\nstable_toolbar_updates\t%I64u\r\nstable_check_command_calls\t%I64u\r\nstable_js_com_calls\t%I64u\r\n",
			caretMoves, selectionChanges, typingEdits, interactionIdleCycles, interactionElapsed,
			g_idleProfile.commandUpdates - commandBefore, g_idleProfile.selectionUpdates - selectionBefore,
			StartupTrace::UiSelectionContainerQueryCount() - selectionContainerBefore,
			StartupTrace::UiSelectionContextContainerQueryCount() - selectionContextContainerBefore,
			g_idleProfile.toolbarUpdates - toolbarBefore, g_idleProfile.fileChecks - fileBefore,
			g_idleProfile.clipboardChecks - clipboardBefore, StartupTrace::UiCheckCommandCount() - checkCommandBefore,
			StartupTrace::UiComCallCount() - comBefore, g_idleProfile.count - idleBeforeStable,
			g_idleProfile.commandUpdates - commandBeforeStable, g_idleProfile.selectionUpdates - selectionBeforeStable,
			g_idleProfile.toolbarUpdates - toolbarBeforeStable, StartupTrace::UiCheckCommandCount() - checkCommandBeforeStable,
			StartupTrace::UiComCallCount() - comBeforeStable);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(written == static_cast<DWORD>(report.GetLength()) ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"spellcheck-local-edit"))
	{
		MSHTML::IHTMLBodyElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLBodyElementPtr());
		MSHTML::IHTMLElementCollectionPtr paragraphs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr paragraph(paragraphs && paragraphs->length ? paragraphs->item(_variant_t(paragraphs->length - 1), _variant_t()) : MSHTML::IHTMLElementPtr());
		if (!body || !paragraph || !m_Speller)
		{
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		m_doc->m_body.SetFocus();
		MSHTML::IHTMLTxtRangePtr range(body->createTextRange());
		if (!range) { output.Close(); ::PostQuitMessage(1); return 0; }
		range->moveToElementText(paragraph);
		range->collapse(VARIANT_TRUE);
		if (range->move(L"character", 1) != 1) { output.Close(); ::PostQuitMessage(1); return 0; }
		range->select();
		_bstr_t paragraphText(paragraph->innerText);
		CString editedText(static_cast<const wchar_t*>(paragraphText));
		editedText += L" localedit";
		paragraph->innerText = _bstr_t(static_cast<const wchar_t*>(editedText));
		m_Speller->SetEnabled(true);
		// Set the same settings gate used by OnEdChange, but do not trigger a
		// viewport-wide highlight pass before the local-edit measurement.
		_Settings.SetHighlightMisspells(true);
		m_Speller->ResetTestDiagnostics();
		BOOL handled = FALSE;
		OnEdChange(0, 0, NULL, handled);
		CStringA row;
		row.Format("paragraph_count\t%ld\r\ncheck_element_calls\t%ld\r\nvisited_paragraphs\t%ld\r\n", paragraphs->length, m_Speller->GetTestCheckElementCalls(), m_Speller->GetTestVisitedParagraphs());
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close();
		// The fixture is intentionally edited in memory only; terminate the
		// unattended message loop without opening the normal dirty-document UI.
		::PostQuitMessage(0); return 0;
	}
	if (IsFbeTestScenario(L"spellcheck-scroll"))
	{
		if (!m_doc || !m_doc->m_body.Document() || !m_Speller) { output.Close(); ::PostQuitMessage(1); return 0; }
		m_Speller->SetEnabled(true); m_Speller->ResetTestDiagnostics();
		InvalidateUi(UiDirtyAll); m_sel_changed = true; OnIdle();
		const ULONGLONG commands = g_idleProfile.commandUpdates, selections = g_idleProfile.selectionUpdates, toolbars = g_idleProfile.toolbarUpdates;
		m_doc->m_body.OnScroll(NULL);
		MSG message = {}; while (::PeekMessage(&message, m_hWnd, AU::WM_BODY_SCROLL, AU::WM_BODY_SCROLL, PM_REMOVE)) ::DispatchMessage(&message);
		OnIdle();
		CStringA report;
		report.Format("spell_scroll_checks\t%ld\r\ncommand_state_updates\t%I64u\r\nselection_context_builds\t%I64u\r\ntoolbar_updates\t%I64u\r\n", m_Speller->GetTestCheckScrollCalls(), g_idleProfile.commandUpdates - commands, g_idleProfile.selectionUpdates - selections, g_idleProfile.toolbarUpdates - toolbars);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close(); ::PostQuitMessage(written == static_cast<DWORD>(report.GetLength()) ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"clipboard-fallback"))
	{
		const unsigned char pixels[] = { 0, 0, 255, 0 }; HBITMAP bitmap = ::CreateBitmap(1, 1, 1, 32, pixels);
		if (!bitmap || !::OpenClipboard(m_hWnd) || !::EmptyClipboard() || !::SetClipboardData(CF_BITMAP, bitmap)) { if (bitmap) ::DeleteObject(bitmap); output.Close(); ::PostQuitMessage(1); return 0; }
		::CloseClipboard(); m_clipboard_listener_registered = false; m_clipboard_has_bitmap = false; m_clipboard_fallback_check_started = false;
		const ULONGLONG checks = g_idleProfile.clipboardChecks; OnIdle();
		CStringA report; report.Format("clipboard_checks\t%I64u\r\nbitmap_detected\t%d\r\n", g_idleProfile.clipboardChecks - checks, m_clipboard_has_bitmap ? 1 : 0);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close(); ::PostQuitMessage(written == static_cast<DWORD>(report.GetLength()) ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"spellcheck-russian-yo"))
	{
		// Exercise the production CSpeller path after the Russian document has
		// selected its dictionary.  The test deliberately does not duplicate the
		// normalization performed by SpellCheck().
		if (!m_doc || !m_doc->m_body.Document())
		{
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		if (!m_Speller)
		{
			m_Speller = new CSpeller(U::GetProgDir() + L"dict\\");
			m_Speller->SetFrame(m_hWnd);
			m_Speller->AttachDocument(m_doc->m_body.Document());
		}
		m_Speller->SetDocumentLanguage();
		struct SpellCase { const char* name; LPCWSTR word; };
		const SpellCase cases[] = {
			{ "lower-e", L"ежик" }, { "lower-yo", L"ёжик" },
			{ "upper-e", L"Ежик" }, { "upper-yo", L"Ёжик" },
			{ "accent-e", L"е\u0301жик" }, { "accent-yo", L"ё\u0301жик" }
		};
		CStringA header("case\tspell_result\texpected\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		bool passed = true;
		for (size_t index = 0; index < _countof(cases); ++index)
		{
			const SPELL_RESULT result = m_Speller->SpellCheck(CString(cases[index].word));
			const bool accepted = result == SPELL_OK;
			CStringA row; row.Format("%s\t%d\t1\r\n", cases[index].name, accepted ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
			passed = passed && accepted;
		}
		output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"table-toolbar-rendering"))
	{
		// This is deliberately a UI-level probe.  The toolbar state and the
		// pixels it paints are recorded independently, so a disabled command is
		// never confused with an enabled command rendered as disabled.
		auto ensureTableToolbarCommands = [&]() -> bool
		{
			// A normal process honours a user's persisted custom toolbar layout,
			// which may intentionally omit table actions.  This test-mode probe
			// needs those controls to exist in order to sample their native state;
			// add only missing buttons to its own short-lived process.
			for (size_t index = 0; index < kTableToolbarCommandCount; ++index)
			{
				const TableToolbarCommand& command = kTableToolbarCommands[index];
				if (m_CmdToolbar.CommandToIndex(command.commandId) >= 0) continue;
				const int image = m_table_toolbar_image_indices[index];
				if (image < 0) return false;
				m_CmdToolbar.AddButton(command.commandId, TBSTYLE_BUTTON, TBSTATE_ENABLED, image,
					StripMenuMnemonics(FbeLoadRuntimeStringByKey(command.localizationKey, command.fallbackText)), 0);
			}
			return true;
		};
		auto selectElement = [&](const wchar_t* tag, long index) -> bool
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr elements(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(tag) : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr element(elements && elements->length > index ? elements->item(_variant_t(index), _variant_t()) : MSHTML::IHTMLElementPtr());
			if (!element) return false;
			m_doc->m_body.SetFocus();
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
			if (!range) return false;
			range->moveToElementText(element);
			range->collapse(VARIANT_TRUE);
			// Keep the test caret inside the target element rather than on its boundary.
			if (range->move(L"character", 1) != 1) return false;
			range->select();

			// MSHTML can publish the new selection asynchronously. Wait until
			// SelectionStructTableCon observes the context required by this phase
			// before the toolbar state is sampled.
			const bool expectTableContext = _wcsicmp(tag, L"TD") == 0 || _wcsicmp(tag, L"TH") == 0;
			const ULONGLONG deadline = ::GetTickCount64() + 1000;
			for (;;)
			{
				const bool hasTableContext = (bool)m_doc->m_body.SelectionStructTableCon();
				if (hasTableContext == expectTableContext)
					return true;
				if (::GetTickCount64() >= deadline)
					return false;

				MSG msg = {};
				bool pumpedMessage = false;
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					if (msg.message == WM_QUIT)
					{
						::PostQuitMessage(static_cast<int>(msg.wParam));
						return false;
					}
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
					pumpedMessage = true;
				}
				if (!pumpedMessage)
					::Sleep(1);
			}
		};
		auto updateTableCommands = [&](bool tableCommandEnabled)
		{
			// selectElement has just synchronously verified this same selection gate.
			// Do not query it again after UIUpdateToolBar: MSHTML can then restore an
			// earlier native selection on an inactive hosted-runner desktop.
			// Let the toolbar settle first. UIUpdateToolBar dispatches idle updates
			// that can otherwise overwrite the state sampled by this test fixture.
			UIUpdateToolBar();
			for (size_t index = 0; index < kTableToolbarCommandCount; ++index)
			{
				const UINT commandId = kTableToolbarCommands[index].commandId;
				UIEnable(commandId, tableCommandEnabled);
				// The test samples the native toolbar, not the delayed WTL update map.
				m_CmdToolbar.SendMessage(TB_ENABLEBUTTON, commandId, MAKELONG(tableCommandEnabled, 0));
			}
			m_CmdToolbar.Invalidate(); m_CmdToolbar.UpdateWindow();
		};
		auto chromaPixels = [&](const RECT& rect) -> long
		{
			HDC source = ::GetDC(m_CmdToolbar); if (!source) return -1;
			RECT client = {}; ::GetClientRect(m_CmdToolbar, &client);
			HDC memory = ::CreateCompatibleDC(source); HBITMAP bitmap = ::CreateCompatibleBitmap(source, client.right, client.bottom);
			HGDIOBJ old = memory && bitmap ? ::SelectObject(memory, bitmap) : NULL;
			if (!memory || !bitmap || !old || !::PrintWindow(m_CmdToolbar, memory, PW_CLIENTONLY)) { if (old) ::SelectObject(memory, old); if (bitmap) ::DeleteObject(bitmap); if (memory) ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return -1; }
			long chroma = 0;
			for (int y = rect.top; y < rect.bottom; ++y) for (int x = rect.left; x < rect.right; ++x) { const COLORREF pixel = ::GetPixel(memory, x, y); const int r = GetRValue(pixel), g = GetGValue(pixel), b = GetBValue(pixel); if (max(r, max(g, b)) - min(r, min(g, b)) >= 32) ++chroma; }
			::SelectObject(memory, old); ::DeleteObject(bitmap); ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return chroma;
		};
		auto imageBlackPixels = [&](const RECT& rect) -> long
		{
			HDC source = ::GetDC(m_CmdToolbar); if (!source) return -1;
			RECT client = {}; ::GetClientRect(m_CmdToolbar, &client);
			HDC memory = ::CreateCompatibleDC(source); HBITMAP bitmap = ::CreateCompatibleBitmap(source, client.right, client.bottom);
			HGDIOBJ old = memory && bitmap ? ::SelectObject(memory, bitmap) : NULL;
			if (!memory || !bitmap || !old || !::PrintWindow(m_CmdToolbar, memory, PW_CLIENTONLY)) { if (old) ::SelectObject(memory, old); if (bitmap) ::DeleteObject(bitmap); if (memory) ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return -1; }
			const int left = rect.left + (rect.right - rect.left - 24) / 2;
			const int top = rect.top + (rect.bottom - rect.top - 24) / 2;
			long black = 0;
			for (int y = top; y < top + 24; ++y) for (int x = left; x < left + 24; ++x) if (::GetPixel(memory, x, y) == RGB(0, 0, 0)) ++black;
			::SelectObject(memory, old); ::DeleteObject(bitmap); ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return black;
		};
		const bool imageListHasMask = ImageListHasMaskPlane(m_CmdToolbar.GetImageList());
		auto appendPhase = [&](const char* phase)
		{
			for (size_t index = 0; index < kTableToolbarCommandCount; ++index)
			{
				const UINT command = kTableToolbarCommands[index].commandId;
				RECT rect = {}; const bool hasRect = m_CmdToolbar.GetItemRect(m_CmdToolbar.CommandToIndex(command), &rect) != FALSE;
				const DWORD state = static_cast<DWORD>(m_CmdToolbar.SendMessage(TB_GETSTATE, command, 0));
				const int image = static_cast<int>(m_CmdToolbar.SendMessage(TB_GETBITMAP, command, 0));
				CStringA row; row.Format("%s\t%u\t%lu\t%d\t%d\t%d\t%d\t%ld\t%d\t%ld\r\n", phase, command, state,
					(state & TBSTATE_ENABLED) != 0 ? 1 : 0, (state & TBSTATE_CHECKED) != 0 ? 1 : 0,
					(state & TBSTATE_HIDDEN) != 0 ? 1 : 0, image, hasRect ? chromaPixels(rect) : -1,
					imageListHasMask ? 1 : 0, hasRect ? imageBlackPixels(rect) : -1);
				DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
			}
			output.Flush();
		};
		CStringA header("phase\tcommand_id\ttb_state\tenabled\tchecked\thidden\timage_index\tchroma_pixels\timage_list_has_mask\timage_black_pixels\r\n"); DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		if (!ensureTableToolbarCommands()) { output.Close(); ::PostQuitMessage(1); return 0; }
		ShowView(BODY);
		if (!selectElement(L"P", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(false); appendPhase("outside-1");
		if (!selectElement(L"TD", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(true); appendPhase("inside-1");
		if (!selectElement(L"TD", 1)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(true); appendPhase("inside-multi");
		if (!selectElement(L"P", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(false); appendPhase("outside-2");
		if (!selectElement(L"TH", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(true); appendPhase("inside-2");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"context-attribute-bars-runtime"))
	{
		const ContextAttributeBarsDiagnostics diagnostics = m_contextAttributeBars.RunDiagnostics();
		CStringA row; row.Format("controls\t%d\r\nids\t%d\r\ncatalogs\t%d\r\nstate\t%d\r\navailability\t%d\r\nlayout\t%d\r\n", diagnostics.controls ? 1 : 0, diagnostics.ids ? 1 : 0, diagnostics.catalogs ? 1 : 0, diagnostics.state ? 1 : 0, diagnostics.availability ? 1 : 0, diagnostics.layout ? 1 : 0);
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"source-editor-ui-runtime"))
	{
		const SourceEditorControlDiagnostics diagnostics = m_source.RunDiagnostics();
		CStringA row; row.Format("created\t%d\r\nutf8\t%d\r\neol\t%d\r\neol_visibility\t%d\r\nwrapping\t%d\r\nwhitespace\t%d\r\nline_numbers\t%d\r\nfolding\t%d\r\nstyles\t%d\r\ntag_state\t%d\r\nmetrics\t%d\r\nreapply\t%d\r\n", diagnostics.created ? 1 : 0, diagnostics.utf8 ? 1 : 0, diagnostics.eol ? 1 : 0, diagnostics.eolVisibility ? 1 : 0, diagnostics.wrapping ? 1 : 0, diagnostics.whitespace ? 1 : 0, diagnostics.lineNumbers ? 1 : 0, diagnostics.folding ? 1 : 0, diagnostics.styles ? 1 : 0, diagnostics.tagState ? 1 : 0, diagnostics.metrics ? 1 : 0, diagnostics.reapply ? 1 : 0);
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"status-bar-long-paint"))
	{
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		CStringA header("length\tset\troundtrip\tpainted\towner_draw\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		const int lengths[] = { 0, 1023, 1024, 1500, 10000 };
		for(int length : lengths)
		{
			CStringW text;
			for(int index = 0; index < length; ++index) text.AppendChar(L'X');
			const BOOL set = static_cast<BOOL>(m_status.SendMessage(SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.GetString())));
			const LRESULT info = m_status.SendMessage(SB_GETTEXTLENGTHW, 0, 0);
			std::vector<wchar_t> received(static_cast<size_t>(LOWORD(static_cast<DWORD>(info))) + 1, L'\0');
			m_status.SendMessage(SB_GETTEXTW, 0, reinterpret_cast<LPARAM>(received.data()));
			m_status.Invalidate();
			const BOOL painted = m_status.UpdateWindow();
			CStringA row; row.Format("%d\t%d\t%d\t%d\t0\r\n", length, set ? 1 : 0,
				info != -1 && LOWORD(static_cast<DWORD>(info)) == length &&
					static_cast<int>(wcslen(received.data())) == length ? 1 : 0, painted ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
		}
		const BOOL ownerSet = static_cast<BOOL>(m_status.SendMessage(SB_SETTEXTW, SBT_OWNERDRAW, static_cast<LPARAM>(0x12345678)));
		const LRESULT ownerInfo = m_status.SendMessage(SB_GETTEXTLENGTHW, 0, 0);
		m_status.Invalidate();
		const BOOL ownerPainted = m_status.UpdateWindow();
		CStringA row; row.Format("owner\t%d\t0\t%d\t%d\r\n", ownerSet ? 1 : 0,
			ownerPainted ? 1 : 0, (HIWORD(static_cast<DWORD>(ownerInfo)) & SBT_OWNERDRAW) != 0 ? 1 : 0);
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"theme-application-runtime"))
	{
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		ThemeManager::ResetApplyDiagnostics();
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const ThemeManager::ApplyDiagnostics initial = ThemeManager::GetApplyDiagnostics();
		ThemeManager::ApplyToWindow(m_hWnd);
		ThemeManager::ApplyToWindow(m_hWnd);
		const ThemeManager::ApplyDiagnostics repeated = ThemeManager::GetApplyDiagnostics();
		HWND newChild = ::CreateWindowExW(0, WC_STATICW, L"Theme child probe", WS_CHILD | WS_VISIBLE,
			0, 0, 80, 20, m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
		for(int iteration = 0; iteration < 100; ++iteration)
		{
			MSG message = {};
			if(!::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) break;
			::TranslateMessage(&message); ::DispatchMessageW(&message);
		}
		const ThemeManager::ApplyDiagnostics afterChild = ThemeManager::GetApplyDiagnostics();
		const bool newChildThemed = newChild != NULL && afterChild.applied > repeated.applied;
		if(newChild != NULL) ::DestroyWindow(newChild);
		const int bandCount = static_cast<int>(m_rebar.GetBandCount());
		REBARBANDINFO originalBand = {}; originalBand.cbSize = sizeof(originalBand); originalBand.fMask = RBBIM_ID | RBBIM_STYLE;
		const bool haveBand = bandCount > 0 && m_rebar.GetBandInfo(0, &originalBand) != FALSE;
		if(haveBand)
		{
			REBARBANDINFO hidden = originalBand; hidden.fMask = RBBIM_STYLE;
			hidden.fStyle |= RBBS_HIDDEN;
			m_rebar.SetBandInfo(0, &hidden);
		}
		const DWORD gdiBefore = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		const DWORD userBefore = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
		DWORD userAt20 = userBefore;
		int setUserDelta = 0, applyUserDelta = 0;
		for(int iteration = 0; iteration < 40; ++iteration)
		{
			const DWORD beforeSet = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
			ThemeManager::SetSelectedTheme(iteration % 2 == 0 ? INTERFACE_THEME_LIGHT : INTERFACE_THEME_DARK);
			const DWORD afterSet = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
			ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
			const DWORD afterApply = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
			setUserDelta += static_cast<int>(afterSet) - static_cast<int>(beforeSet);
			applyUserDelta += static_cast<int>(afterApply) - static_cast<int>(afterSet);
			if(iteration == 19) userAt20 = afterApply;
		}
		const DWORD gdiAfter = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		const DWORD userAfter = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
		REBARBANDINFO finalBand = {}; finalBand.cbSize = sizeof(finalBand); finalBand.fMask = RBBIM_ID | RBBIM_STYLE;
		const bool hiddenPreserved = haveBand && m_rebar.GetBandInfo(0, &finalBand) &&
			finalBand.wID == originalBand.wID && (finalBand.fStyle & RBBS_HIDDEN) != 0;
		if(haveBand)
		{
			REBARBANDINFO restored = finalBand; restored.fMask = RBBIM_STYLE;
			restored.fStyle = (finalBand.fStyle & ~RBBS_HIDDEN) | (originalBand.fStyle & RBBS_HIDDEN);
			m_rebar.SetBandInfo(0, &restored);
		}
		bool orderPreserved = bandCount > 1;
		if(orderPreserved)
		{
			const BOOL moved = static_cast<BOOL>(m_rebar.SendMessage(RB_MOVEBAND, 0, bandCount - 1));
			ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
			ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
			REBARBANDINFO movedBand = {}; movedBand.cbSize = sizeof(movedBand); movedBand.fMask = RBBIM_ID;
			orderPreserved = moved && m_rebar.GetBandInfo(bandCount - 1, &movedBand) && movedBand.wID == originalBand.wID;
			m_rebar.SendMessage(RB_MOVEBAND, bandCount - 1, 0);
			ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
			ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		}
		const UINT probeBandId = 0x7f20;
		bool deletedBandCleared = false, reusedBandFresh = false;
		HWND firstProbe = ::CreateWindowExW(0, WC_STATICW, L"rebar first", WS_CHILD | WS_VISIBLE,
			0, 0, 40, 16, m_rebar, NULL, _Module.GetModuleInstance(), NULL);
		if(firstProbe != NULL)
		{
			REBARBANDINFO probe = {}; probe.cbSize = sizeof(probe);
			probe.fMask = RBBIM_ID | RBBIM_CHILD | RBBIM_STYLE | RBBIM_CHILDSIZE | RBBIM_SIZE;
			probe.wID = probeBandId; probe.hwndChild = firstProbe; probe.fStyle = RBBS_CHILDEDGE;
			probe.cxMinChild = 40; probe.cyMinChild = 16; probe.cx = 60;
			if(m_rebar.InsertBand(-1, &probe))
			{
				ApplyMainRebarTheme(m_rebar);
				const int probeIndex = static_cast<int>(m_rebar.GetBandCount()) - 1;
				m_rebar.DeleteBand(probeIndex);
				const std::map<HWND, std::map<UINT, RebarBandThemeState> >::const_iterator cache = g_rebarBaseBandStyles.find(m_rebar);
				deletedBandCleared = cache == g_rebarBaseBandStyles.end() || cache->second.find(probeBandId) == cache->second.end();
			}
			::DestroyWindow(firstProbe);
		}
		HWND secondProbe = ::CreateWindowExW(0, WC_STATICW, L"rebar second", WS_CHILD | WS_VISIBLE,
			0, 0, 40, 16, m_rebar, NULL, _Module.GetModuleInstance(), NULL);
		if(secondProbe != NULL)
		{
			REBARBANDINFO probe = {}; probe.cbSize = sizeof(probe);
			probe.fMask = RBBIM_ID | RBBIM_CHILD | RBBIM_STYLE | RBBIM_CHILDSIZE | RBBIM_SIZE;
			probe.wID = probeBandId; probe.hwndChild = secondProbe; probe.fStyle = 0;
			probe.cxMinChild = 40; probe.cyMinChild = 16; probe.cx = 60;
			if(m_rebar.InsertBand(-1, &probe))
			{
				ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
				ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
				REBARBANDINFO current = {}; current.cbSize = sizeof(current); current.fMask = RBBIM_STYLE;
				reusedBandFresh = m_rebar.GetBandInfo(static_cast<int>(m_rebar.GetBandCount()) - 1, &current) &&
					(current.fStyle & RBBS_CHILDEDGE) == 0;
				m_rebar.DeleteBand(static_cast<int>(m_rebar.GetBandCount()) - 1);
			}
			::DestroyWindow(secondProbe);
		}
		const ThemeManager::ApplyDiagnostics beforeRefresh = ThemeManager::GetApplyDiagnostics();
		const bool refreshChanged = ThemeManager::RefreshSystemTheme();
		const ThemeManager::ApplyDiagnostics afterRefresh = ThemeManager::GetApplyDiagnostics();
		CStringA row; row.Format("initial_applied\t%I64u\r\nrepeated_delta\t%I64u\r\nnew_child_auto\t%d\r\nreentrant\t%I64u\r\nhidden_preserved\t%d\r\norder_preserved\t%d\r\ndeleted_band_cleared\t%d\r\nreused_band_fresh\t%d\r\ngdi_delta\t%d\r\nuser_delta\t%d\r\nuser_delta_first20\t%d\r\nuser_delta_second20\t%d\r\nset_user_delta\t%d\r\napply_user_delta\t%d\r\ntheme_message_user\t%I64d\r\nnative_palette_user\t%I64d\r\nfbe_message_user\t%I64d\r\nredraw_user\t%I64d\r\nrefresh_noop\t%d\r\n",
			initial.applied, repeated.applied - initial.applied, newChildThemed ? 1 : 0,
			afterRefresh.reentrant, hiddenPreserved ? 1 : 0,
			orderPreserved ? 1 : 0, deletedBandCleared ? 1 : 0, reusedBandFresh ? 1 : 0,
			static_cast<int>(gdiAfter) - static_cast<int>(gdiBefore),
			static_cast<int>(userAfter) - static_cast<int>(userBefore),
			static_cast<int>(userAt20) - static_cast<int>(userBefore),
			static_cast<int>(userAfter) - static_cast<int>(userAt20),
			setUserDelta, applyUserDelta,
			afterRefresh.themeMessageUser, afterRefresh.nativePaletteUser,
			afterRefresh.fbeMessageUser, afterRefresh.redrawUser,
			!refreshChanged && afterRefresh.applied == beforeRefresh.applied ? 1 : 0);
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"themed-message-behavior"))
	{
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		CStringA header("case\tresult\twindow\tgeometry\tcontent\tkeyboard\tdpi_geometry\tdpi_font\tbounds\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		auto runDialog = [&](const char* name, UINT type, int action, int expected, int textLength)
		{
			CStringW caption(L"FBE themed message behavior probe");
			CStringW message;
			for(int index = 0; index < textLength; ++index) message.AppendChar(L'X');
			bool found = false, geometry = false, content = false, keyboard = false;
			bool dpiGeometry = true, dpiFont = true;
			CStringA bounds;
			std::thread driver([&]()
			{
				HWND dialog = NULL;
				for(int retry = 0; retry < 500 && dialog == NULL; ++retry)
				{
					dialog = ::FindWindowW(L"FBEThemedMessageDialog", caption);
					if(dialog == NULL) ::Sleep(10);
				}
				if(dialog == NULL) return;
				found = true;
				const UINT firstId = (type & MB_TYPEMASK) == MB_YESNO || (type & MB_TYPEMASK) == MB_YESNOCANCEL ? IDYES : IDOK;
				HWND firstButton = NULL, edit = NULL;
				for(int retry = 0; retry < 500 && (firstButton == NULL || edit == NULL || !::IsWindowVisible(dialog)); ++retry)
				{
					firstButton = ::GetDlgItem(dialog, firstId);
					edit = ::FindWindowExW(dialog, NULL, WC_EDITW, NULL);
					if(firstButton == NULL || edit == NULL || !::IsWindowVisible(dialog)) ::Sleep(10);
				}
				if(firstButton == NULL || edit == NULL) return;
				::Sleep(20); // Initial default focus is assigned immediately after ShowWindow.
				RECT outer = {}, client = {}, buttonRect = {};
				::GetWindowRect(dialog, &outer); ::GetClientRect(dialog, &client);
				if(firstButton != NULL) ::GetWindowRect(firstButton, &buttonRect);
				::MapWindowPoints(NULL, dialog, reinterpret_cast<LPPOINT>(&buttonRect), 2);
				MONITORINFO monitor = {}; monitor.cbSize = sizeof(monitor);
				HMONITOR nearest = ::MonitorFromWindow(dialog, MONITOR_DEFAULTTONEAREST);
				geometry = nearest != NULL && ::GetMonitorInfoW(nearest, &monitor) &&
					outer.left >= monitor.rcWork.left && outer.top >= monitor.rcWork.top &&
					outer.right <= monitor.rcWork.right && outer.bottom <= monitor.rcWork.bottom &&
					firstButton != NULL && buttonRect.left >= client.left && buttonRect.top >= client.top &&
					buttonRect.right <= client.right && buttonRect.bottom <= client.bottom;
				bounds.Format("outer=%ld,%ld,%ld,%ld;work=%ld,%ld,%ld,%ld;client=%ld,%ld,%ld,%ld;button=%ld,%ld,%ld,%ld",
					outer.left, outer.top, outer.right, outer.bottom,
					monitor.rcWork.left, monitor.rcWork.top, monitor.rcWork.right, monitor.rcWork.bottom,
					client.left, client.top, client.right, client.bottom,
					buttonRect.left, buttonRect.top, buttonRect.right, buttonRect.bottom);
				content = edit != NULL && ::GetWindowTextLengthW(edit) == textLength;
				switch(action)
				{
				case 0: // A forged notification without a button HWND must be ignored.
					::SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDYES, BN_CLICKED), 0);
					keyboard = ::IsWindow(dialog) != FALSE;
					if(HWND button = ::GetDlgItem(dialog, IDNO)) ::SendMessageW(button, BM_CLICK, 0, 0);
					break;
				case 1: // Yes/No has no Esc answer.
					::PostMessageW(dialog, WM_KEYDOWN, VK_ESCAPE, 0); ::Sleep(80);
					keyboard = ::IsWindow(dialog) != FALSE;
					if(HWND button = ::GetDlgItem(dialog, IDNO)) ::SendMessageW(button, BM_CLICK, 0, 0);
					break;
				case 2: { // Tab changes focus; Enter activates that button, not the old default.
					if(firstButton != NULL) ::PostMessageW(firstButton, WM_KEYDOWN, VK_TAB, 0);
					HWND noButton = ::GetDlgItem(dialog, IDNO);
					for(int retry = 0; retry < 100 && !keyboard; ++retry)
					{
						GUITHREADINFO focus = {}; focus.cbSize = sizeof(focus);
						keyboard = noButton != NULL &&
							::GetGUIThreadInfo(::GetWindowThreadProcessId(dialog, NULL), &focus) &&
							focus.hwndFocus == noButton;
						if(!keyboard) ::Sleep(10);
					}
					if(keyboard) ::PostMessageW(noButton, WM_KEYDOWN, VK_RETURN, 0);
					else if(noButton != NULL) ::SendMessageW(noButton, BM_CLICK, 0, 0);
					break;
				}
				case 3: // The second button is the native default for MB_DEFBUTTON2.
					keyboard = true; ::PostMessageW(dialog, WM_KEYDOWN, VK_RETURN, 0); break;
				case 4:
					keyboard = true; ::PostMessageW(dialog, WM_CLOSE, 0, 0); break;
				case 5:
					keyboard = true; ::PostMessageW(dialog, WM_KEYDOWN, VK_ESCAPE, 0); break;
				case 6:
					keyboard = true;
					for(UINT dpi : { 96u, 120u, 144u, 192u })
					{
						RECT suggested = {}; ::GetWindowRect(dialog, &suggested);
						::SendMessageW(dialog, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), reinterpret_cast<LPARAM>(&suggested));
						RECT resized = {}, resizedClient = {}, resizedButton = {};
						::GetWindowRect(dialog, &resized); ::GetClientRect(dialog, &resizedClient);
						::GetWindowRect(firstButton, &resizedButton);
						::MapWindowPoints(NULL, dialog, reinterpret_cast<LPPOINT>(&resizedButton), 2);
						MONITORINFO currentMonitor = {}; currentMonitor.cbSize = sizeof(currentMonitor);
						HMONITOR current = ::MonitorFromWindow(dialog, MONITOR_DEFAULTTONEAREST);
						dpiGeometry = dpiGeometry && current != NULL && ::GetMonitorInfoW(current, &currentMonitor) &&
							resized.left >= currentMonitor.rcWork.left && resized.top >= currentMonitor.rcWork.top &&
							resized.right <= currentMonitor.rcWork.right && resized.bottom <= currentMonitor.rcWork.bottom &&
							resizedButton.left >= resizedClient.left && resizedButton.top >= resizedClient.top &&
							resizedButton.right <= resizedClient.right && resizedButton.bottom <= resizedClient.bottom;
						LOGFONTW font = {};
						HFONT editFont = reinterpret_cast<HFONT>(::SendMessageW(edit, WM_GETFONT, 0, 0));
						dpiFont = dpiFont && editFont != NULL && ::GetObjectW(editFont, sizeof(font), &font) == sizeof(font) &&
							font.lfHeight != 0;
						content = content && ::GetWindowTextLengthW(edit) == textLength;
					}
					if(HWND button = ::GetDlgItem(dialog, IDOK)) ::SendMessageW(button, BM_CLICK, 0, 0);
					break;
				case 7: { // The localized '&' mnemonic must activate its actual button.
					HWND noButton = ::GetDlgItem(dialog, IDNO);
					wchar_t label[128] = {};
					if(noButton != NULL) ::GetWindowTextW(noButton, label, _countof(label));
					const wchar_t* mnemonic = wcschr(label, L'&');
					if(mnemonic != NULL && mnemonic[1] != L'\0')
						::PostMessageW(dialog, WM_SYSCHAR, static_cast<WPARAM>(mnemonic[1]), 1 << 29);
					::Sleep(100);
					keyboard = mnemonic != NULL && !::IsWindow(dialog);
					if(::IsWindow(dialog) && noButton != NULL) ::SendMessageW(noButton, BM_CLICK, 0, 0);
					break;
				}
				case 8: { // Arrows move button focus; Space activates the focused button.
					HWND noButton = ::GetDlgItem(dialog, IDNO);
					::PostMessageW(firstButton, WM_KEYDOWN, VK_RIGHT, 0);
					::Sleep(80);
					GUITHREADINFO focus = {}; focus.cbSize = sizeof(focus);
					keyboard = noButton != NULL && ::GetGUIThreadInfo(::GetWindowThreadProcessId(dialog, NULL), &focus) &&
						focus.hwndFocus == noButton;
					if(keyboard)
					{
						::PostMessageW(noButton, WM_KEYDOWN, VK_SPACE, 0);
						::PostMessageW(noButton, WM_KEYUP, VK_SPACE, 0);
						::Sleep(100);
						keyboard = !::IsWindow(dialog);
					}
					if(::IsWindow(dialog) && noButton != NULL) ::SendMessageW(noButton, BM_CLICK, 0, 0);
					break;
				}
				case 9: { // A long message must be focusable and readable to its last line.
					::PostMessageW(firstButton, WM_KEYDOWN, VK_TAB, 0);
					bool editFocused = false;
					for(int retry = 0; retry < 100 && !editFocused; ++retry)
					{
						GUITHREADINFO focus = {}; focus.cbSize = sizeof(focus);
						editFocused = ::GetGUIThreadInfo(::GetWindowThreadProcessId(dialog, NULL), &focus) &&
							focus.hwndFocus == edit;
						if(!editFocused) ::Sleep(10);
					}
					bool lastLineVisible = false, returnedToButton = false;
					LRESULT observedLines = 0, observedLast = 0, observedPosition = 0;
					RECT observedTextArea = {};
					if(editFocused)
					{
						for(int page = 0; page < 256; ++page)
							::SendMessageW(edit, WM_KEYDOWN, VK_NEXT, 0);
						observedLines = ::SendMessageW(edit, EM_GETLINECOUNT, 0, 0);
						observedLast = ::SendMessageW(edit, EM_LINEINDEX, observedLines - 1, 0);
						observedPosition = ::SendMessageW(edit, EM_POSFROMCHAR, observedLast, 0);
						::SendMessageW(edit, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&observedTextArea));
						const int y = GET_Y_LPARAM(observedPosition);
						lastLineVisible = observedLines > 1 && y >= observedTextArea.top && y < observedTextArea.bottom;
						::PostMessageW(edit, WM_KEYDOWN, VK_TAB, 0);
						for(int retry = 0; retry < 100 && !returnedToButton; ++retry)
						{
							GUITHREADINFO focus = {}; focus.cbSize = sizeof(focus);
							returnedToButton = ::GetGUIThreadInfo(::GetWindowThreadProcessId(dialog, NULL), &focus) &&
								focus.hwndFocus == firstButton;
							if(!returnedToButton) ::Sleep(10);
						}
					}
					keyboard = editFocused && lastLineVisible && returnedToButton;
					bounds.AppendFormat(";focus=%d;last=%d;return=%d;lines=%Id;index=%Id;y=%d;area=%ld,%ld",
						editFocused ? 1 : 0, lastLineVisible ? 1 : 0, returnedToButton ? 1 : 0,
						observedLines, observedLast, GET_Y_LPARAM(observedPosition), observedTextArea.top, observedTextArea.bottom);
					if(keyboard) ::PostMessageW(firstButton, WM_KEYDOWN, VK_RETURN, 0);
					else ::SendMessageW(firstButton, BM_CLICK, 0, 0); // Cleanup only; keyboard remains failed.
					break;
				}
				}
			});
			const int result = ThemeManager::MessageBox(m_hWnd, message, caption, type);
			driver.join();
			CStringA row; row.Format("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\r\n", name, result == expected ? 1 : 0,
				found ? 1 : 0, geometry ? 1 : 0, content ? 1 : 0, keyboard ? 1 : 0,
				dpiGeometry ? 1 : 0, dpiFont ? 1 : 0, bounds.GetString());
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		runDialog("forged-command", MB_YESNO | MB_ICONQUESTION, 0, IDNO, 20);
		runDialog("yesno-escape", MB_YESNO | MB_ICONQUESTION, 1, IDNO, 20);
		runDialog("focused-enter", MB_YESNOCANCEL | MB_ICONWARNING, 2, IDNO, 20);
		runDialog("default-second", MB_YESNO | MB_DEFBUTTON2, 3, IDNO, 20);
		runDialog("close-cancel", MB_YESNOCANCEL, 4, IDCANCEL, 20);
		runDialog("escape-cancel", MB_OKCANCEL, 5, IDCANCEL, 20);
		runDialog("long-warning", MB_OK | MB_ICONWARNING, 6, IDOK, 10000);
		runDialog("localized-mnemonic", MB_YESNO, 7, IDNO, 20);
		runDialog("arrow-space", MB_YESNO, 8, IDNO, 20);
		runDialog("long-keyboard-scroll", MB_OK | MB_ICONWARNING, 9, IDOK, 10000);
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"themed-message-quit"))
	{
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		const DWORD mainThread = ::GetCurrentThreadId();
		bool found = false, posted = false;
		std::thread driver([&]()
		{
			HWND dialog = NULL;
			for(int retry = 0; retry < 500 && dialog == NULL; ++retry)
			{
				dialog = ::FindWindowW(L"FBEThemedMessageDialog", L"FBE themed quit probe");
				if(dialog == NULL) ::Sleep(10);
			}
			if(dialog == NULL) return;
			found = true;
			posted = ::PostThreadMessageW(mainThread, WM_QUIT, 73, 0) != FALSE;
		});
		const int answer = ThemeManager::MessageBox(m_hWnd, L"Application exit must not become a dialog answer.",
			L"FBE themed quit probe", MB_YESNOCANCEL | MB_ICONWARNING);
		driver.join();
		const bool ownerEnabled = ::IsWindowEnabled(m_hWnd) != FALSE;
		const bool destroyed = ::FindWindowW(L"FBEThemedMessageDialog", L"FBE themed quit probe") == NULL;
		CStringA row; row.Format("found\t%d\r\nposted\t%d\r\nno_answer\t%d\r\nowner_enabled\t%d\r\ndestroyed\t%d\r\n",
			found ? 1 : 0, posted ? 1 : 0, answer == 0 ? 1 : 0, ownerEnabled ? 1 : 0, destroyed ? 1 : 0);
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
		output.Close(); return 0; // Show reposts the original WM_QUIT for the outer loop.
	}
	if (IsFbeTestScenario(L"themed-message-native-parity"))
	{
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		struct ButtonSet { const char* name; UINT type; UINT ids[3]; int count; };
		const ButtonSet sets[] = {
			{ "ok", MB_OK, { IDOK, 0, 0 }, 1 },
			{ "okcancel", MB_OKCANCEL, { IDOK, IDCANCEL, 0 }, 2 },
			{ "yesno", MB_YESNO, { IDYES, IDNO, 0 }, 2 },
			{ "yesnocancel", MB_YESNOCANCEL, { IDYES, IDNO, IDCANCEL }, 3 },
			{ "retrycancel", MB_RETRYCANCEL, { IDRETRY, IDCANCEL, 0 }, 2 },
			{ "abortretryignore", MB_ABORTRETRYIGNORE, { IDABORT, IDRETRY, IDIGNORE }, 3 }
		};
		CStringA header("set\tbutton\tnative\tthemed\tbuttons_found\tparity\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		auto invoke = [&](bool themed, UINT type, UINT buttonId, int buttonIndex, bool& found) -> int
		{
			const wchar_t* caption = themed ? L"FBE themed parity probe" : L"FBE native parity probe";
			std::thread driver([&]()
			{
				HWND dialog = NULL;
				for(int retry = 0; retry < 500 && dialog == NULL; ++retry)
				{
					dialog = ::FindWindowW(themed ? L"FBEThemedMessageDialog" : NULL, caption);
					if(dialog == NULL) ::Sleep(10);
				}
				if(dialog == NULL) return;
				HWND button = NULL;
				for(int retry = 0; retry < 500 && button == NULL; ++retry)
				{
					if(themed)
						button = ::GetDlgItem(dialog, buttonId);
					else
					{
						std::vector<HWND> buttons;
						::EnumChildWindows(dialog, [](HWND child, LPARAM context) -> BOOL
						{
							std::vector<HWND>* buttons = reinterpret_cast<std::vector<HWND>*>(context);
							wchar_t className[32] = {};
							::GetClassNameW(child, className, _countof(className));
							if(_wcsicmp(className, L"Button") == 0 && ::IsWindowVisible(child) && ::IsWindowEnabled(child))
								buttons->push_back(child);
							return TRUE;
						}, reinterpret_cast<LPARAM>(&buttons));
						std::sort(buttons.begin(), buttons.end(), [](HWND left, HWND right)
						{
							RECT leftRect = {}, rightRect = {};
							::GetWindowRect(left, &leftRect); ::GetWindowRect(right, &rightRect);
							return leftRect.left < rightRect.left;
						});
						if(buttonIndex < static_cast<int>(buttons.size())) button = buttons[buttonIndex];
					}
					if(button == NULL) ::Sleep(10);
				}
				if(button != NULL) { found = true; ::SendMessageW(button, BM_CLICK, 0, 0); }
			});
			const int answer = themed ? ThemeManager::MessageBox(m_hWnd, L"Compare the selected button.", caption, type) :
				::MessageBoxW(m_hWnd, L"Compare the selected button.", caption, type);
			driver.join();
			return answer;
		};
		for(const ButtonSet& set : sets)
			for(int index = 0; index < set.count; ++index)
			{
				bool nativeFound = false, themedFound = false;
				const int native = invoke(false, set.type, set.ids[index], index, nativeFound);
				const int themed = invoke(true, set.type, set.ids[index], index, themedFound);
				CStringA row; row.Format("%s\t%u\t%d\t%d\t%d\t%d\r\n", set.name, set.ids[index], native, themed,
					nativeFound && themedFound ? 1 : 0, native == themed && native == static_cast<int>(set.ids[index]) ? 1 : 0);
				output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
			}
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"themed-message-partial-failure"))
	{
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		g_rejectThemedMessageButton = true;
		g_rejectedThemedMessageButton = false;
		HHOOK hook = ::SetWindowsHookExW(WH_CBT, RejectThemedMessageButtonForTest, NULL, ::GetCurrentThreadId());
		bool nativeFound = false;
		std::thread driver([&]()
		{
			HWND ok = NULL;
			for(int retry = 0; retry < 500 && ok == NULL; ++retry)
			{
				// The failed themed window briefly has the same caption. Wait
				// for the native fallback and for its button to be ready.
				HWND native = ::FindWindowW(L"#32770", L"FBE themed failure probe");
				if(native != NULL)
				{
					ok = ::GetDlgItem(native, IDOK);
					if(ok == NULL)
						::EnumChildWindows(native, [](HWND child, LPARAM context) -> BOOL
						{
							wchar_t className[32] = {};
							::GetClassNameW(child, className, _countof(className));
							if(_wcsicmp(className, L"Button") == 0 && ::IsWindowVisible(child) && ::IsWindowEnabled(child))
							{
								*reinterpret_cast<HWND*>(context) = child;
								return FALSE;
							}
							return TRUE;
						}, reinterpret_cast<LPARAM>(&ok));
				}
				if(ok == NULL) ::Sleep(10);
			}
			if(ok != NULL) { nativeFound = true; ::SendMessageW(ok, BM_CLICK, 0, 0); }
		});
		const int answer = hook != NULL ? ThemeManager::MessageBox(m_hWnd, L"Creation failure must clean up before native fallback.",
			L"FBE themed failure probe", MB_OK | MB_ICONERROR) : 0;
		driver.join();
		if(hook != NULL) ::UnhookWindowsHookEx(hook);
		g_rejectThemedMessageButton = false;
		CStringA row; row.Format("hook\t%d\r\nrejected\t%d\r\nnative\t%d\r\nanswer\t%d\r\nowner_enabled\t%d\r\nno_themed_hwnd\t%d\r\n",
			hook != NULL ? 1 : 0, g_rejectedThemedMessageButton ? 1 : 0, nativeFound ? 1 : 0,
			answer == IDOK ? 1 : 0, ::IsWindowEnabled(m_hWnd) ? 1 : 0,
			::FindWindowW(L"FBEThemedMessageDialog", L"FBE themed failure probe") == NULL ? 1 : 0);
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"export-html"))
	{
		// The plugin itself receives deterministic options through its test-only
		// environment hook; activation and Export still follow the normal FBE
		// local-COM production path.
		BOOL handled = FALSE;
		OnToolsExport(0, ID_EXPORT_BASE, m_hWnd, handled);
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}

	CStringA rows("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\tcommitted_bytes\treserved_bytes\tsource_bytes\tsource_lines\tundo_selection_history\r\n");
	const ULONGLONG start = ::GetTickCount64();
	auto appendSnapshot = [&](const char* phase)
	{
		const FbeSourceDiagnostics::ProcessMemorySnapshot memory = FbeSourceDiagnostics::GetProcessMemorySnapshot();
		const sptr_t sourceBytes = m_source.SendMessage(SCI_GETLENGTH);
		const sptr_t sourceLines = m_source.SendMessage(SCI_GETLINECOUNT);
		CStringA row;
		row.Format("%s\t%I64u\t%I64u\t%I64u\t%I64u\t%I64u\t%Id\t%Id\t%d\r\n", phase,
			::GetTickCount64() - start, static_cast<unsigned __int64>(memory.privateBytes),
			static_cast<unsigned __int64>(memory.workingSetBytes), static_cast<unsigned __int64>(memory.committedBytes),
			static_cast<unsigned __int64>(memory.reservedBytes), sourceBytes, sourceLines,
			AU::_ARGS.disable_undo_selection_history ? 0 : 1);
		rows += row;
	};

	appendSnapshot("document-open");
	auto appendShowSourceProfile = [&](const char* scenario)
	{
		for (const FbeSourceDiagnostics::SourceProfileSample& sample : FbeSourceDiagnostics::ProfileSamples())
		{
			const FbeSourceDiagnostics::ProcessMemorySnapshot memory = FbeSourceDiagnostics::GetProcessMemorySnapshot();
			CStringA phase("showsource-");
			phase += scenario;
			phase += ":";
			phase += sample.phase;
			CStringA row;
			row.Format("%s\t%.3f\t%I64u\t%I64u\t%I64u\t%I64u\t%Id\t%Id\t%d\r\n", phase.GetString(),
				sample.elapsedMilliseconds, static_cast<unsigned __int64>(memory.privateBytes),
				static_cast<unsigned __int64>(memory.workingSetBytes), static_cast<unsigned __int64>(memory.committedBytes),
				static_cast<unsigned __int64>(memory.reservedBytes), m_source.SendMessage(SCI_GETLENGTH),
				m_source.SendMessage(SCI_GETLINECOUNT), AU::_ARGS.disable_undo_selection_history ? 0 : 1);
			rows += row;
		}
	};
	auto appendTableSnapshot = [&](const char* phase)
	{
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		auto countTag = [&](const char* tag) -> long
		{
			long count = 0;
			for (const char* position = source.data(); (position = strstr(position, tag)) != NULL; ++position)
				++count;
			return count;
		};
		CStringA row;
		row.Format("%s:table=%ld;tr=%ld;td=%ld;th=%ld\t%I64u\t0\t0\t0\t0\t%Id\t%Id\t%d\r\n",
			phase, countTag("<table"), countTag("<tr"), countTag("<td"), countTag("<th"),
			::GetTickCount64() - start, sourceLength, m_source.SendMessage(SCI_GETLINECOUNT),
			AU::_ARGS.disable_undo_selection_history ? 0 : 1);
		rows += row;
	};
	ShowView(SOURCE);
	appendShowSourceProfile("first");
	appendTableSnapshot("table-source-first");
	for (int repeat = 1; repeat <= 5; ++repeat)
	{
		ShowView(BODY);
		ShowView(SOURCE);
		appendTableSnapshot("table-unchanged-body-source");
		CStringA scenario;
		scenario.Format("unchanged-%d", repeat);
		appendShowSourceProfile(scenario);
	}
	appendSnapshot("source-unchanged-body-source-5");
	m_source.SendMessage(SCI_COLOURISE, 0, -1);
	appendSnapshot("source-styled-wrap-word");
	SourceEditorConfig benchmarkConfig = BuildSourceEditorConfig();
	benchmarkConfig.wrap = false;
	m_source.ApplyConfiguration(benchmarkConfig);
	m_source.SendMessage(SCI_COLOURISE, 0, -1);
	appendSnapshot("source-styled-wrap-none");
	m_source.FoldAll();
	appendSnapshot("fold-all");
	m_source.FoldAll();
	appendSnapshot("expand-all");

	const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
	const sptr_t stride = max<sptr_t>(1, length / 997);
	const char* const sectionNeedle = "<section";
	sptr_t searchStart = 0;
	for (int iteration = 0; iteration < 1000; ++iteration)
	{
		m_source.SendMessage(SCI_SETTARGETSTART, searchStart);
		m_source.SendMessage(SCI_SETTARGETEND, length);
		const sptr_t found = m_source.SendMessage(SCI_SEARCHINTARGET, strlen(sectionNeedle),
			reinterpret_cast<LPARAM>(sectionNeedle));
		searchStart = found < 0 ? 0 : m_source.SendMessage(SCI_GETTARGETEND);
	}
	appendSnapshot("find-section-1000");
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	searchStart = 0;
	for (int iteration = 0; iteration < 100; ++iteration)
	{
		m_source.SendMessage(SCI_SETTARGETSTART, searchStart);
		m_source.SendMessage(SCI_SETTARGETEND, length);
		const sptr_t found = m_source.SendMessage(SCI_SEARCHINTARGET, strlen(sectionNeedle),
			reinterpret_cast<LPARAM>(sectionNeedle));
		if (found < 0) { searchStart = 0; continue; }
		m_source.SendMessage(SCI_REPLACETARGET, strlen(sectionNeedle), reinterpret_cast<LPARAM>(sectionNeedle));
		searchStart = m_source.SendMessage(SCI_GETTARGETEND);
	}
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	m_source.SendMessage(SCI_SETSAVEPOINT);
	appendSnapshot("replace-section-same-text-100");
	const sptr_t lineCount = m_source.SendMessage(SCI_GETLINECOUNT);
	for (int iteration = 0; iteration < 1000; ++iteration)
	{
		const sptr_t line = (static_cast<sptr_t>(iteration) * 37) % lineCount;
		m_source.SendMessage(SCI_SETCURRENTPOS, m_source.SendMessage(SCI_POSITIONFROMLINE, line));
	}
	appendSnapshot("navigate-source-lines-1000");
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	for (int iteration = 0; iteration < 10000; ++iteration)
	{
		const sptr_t position = length + iteration;
		m_source.SendMessage(SCI_SETSEL, position, position);
		m_source.SendMessage(SCI_INSERTTEXT, position, reinterpret_cast<LPARAM>(" "));
	}
	appendSnapshot("undo-selection-history-10000-edits");
	for (int iteration = 0; iteration < 10000 && m_source.SendMessage(SCI_CANUNDO); ++iteration)
		m_source.SendMessage(SCI_UNDO);
	appendSnapshot("undo-all-10000-edits");
	for (int iteration = 0; iteration < 10000 && m_source.SendMessage(SCI_CANREDO); ++iteration)
		m_source.SendMessage(SCI_REDO);
	appendSnapshot("redo-all-10000-edits");
	for (int iteration = 0; iteration < 10000 && m_source.SendMessage(SCI_CANUNDO); ++iteration)
		m_source.SendMessage(SCI_UNDO);
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	m_source.SendMessage(SCI_SETSAVEPOINT);

	auto runMatchedTags = [&](int first, int last)
	{
		for (int iteration = first; iteration < last; ++iteration)
		{
			const sptr_t position = (static_cast<sptr_t>(iteration) * stride) % length;
		// Stress the same caret/update lifecycle as keyboard navigation without
		// forcing every position through viewport scroll policy and layout cache.
		m_source.SendMessage(SCI_SETCURRENTPOS, position);
		m_source.UpdateTagHighlight({ true, _Settings.XmlSrcTagHighlightMode() ? XmlTagHighlightMode::FullTag : XmlTagHighlightMode::NameOnly, _Settings.XmlSrcTagHighlightAttributes(), _Settings.XmlSrcTagHighlightErrors() });
		}
	};
	runMatchedTags(0, 10000);
	appendSnapshot("matched-tags-10000-positions");
	runMatchedTags(10000, 50000);
	appendSnapshot("matched-tags-50000-positions");
	runMatchedTags(50000, 100000);
	appendSnapshot("matched-tags-100000-positions");
	if (AU::_ARGS.run_source_view_cycles)
	{
		for (int cycle = 1; cycle <= 100; ++cycle)
		{
			ShowView(BODY);
			ShowView(SOURCE);
			m_source.SendMessage(SCI_COLOURISE, 0, -1);
			if (cycle == 1 || cycle == 10 || cycle == 50 || cycle == 100)
			{
				CStringA phase;
				phase.Format("body-source-cycle-%d", cycle);
				const FbeSourceDiagnostics::ProcessMemorySnapshot memory = FbeSourceDiagnostics::GetProcessMemorySnapshot();
				const sptr_t sourceBytes = m_source.SendMessage(SCI_GETLENGTH);
				const sptr_t sourceLines = m_source.SendMessage(SCI_GETLINECOUNT);
				CStringA row;
				row.Format("%s\t%I64u\t%I64u\t%I64u\t%I64u\t%I64u\t%Id\t%Id\t%d\r\n", phase.GetString(),
					::GetTickCount64() - start, static_cast<unsigned __int64>(memory.privateBytes),
					static_cast<unsigned __int64>(memory.workingSetBytes), static_cast<unsigned __int64>(memory.committedBytes),
					static_cast<unsigned __int64>(memory.reservedBytes), sourceBytes, sourceLines,
					AU::_ARGS.disable_undo_selection_history ? 0 : 1);
				rows += row;
				appendTableSnapshot("table-body-source");
			}
		}
	}
	if (AU::_ARGS.save_benchmark_document)
	{
		ShowView(BODY);
		if (!m_doc->Save())
		{
			// A failed serialization transaction must leave a deterministic
			// diagnostic trail for the production safety test.  A second Save
			// verifies that the document has been fail-closed in memory.
			const bool secondSaveRejected = !m_doc->Save();
			CStringA row;
			row.Format("table-save-rejected:second-save-rejected=%d\t%I64u\t0\t0\t0\t0\t0\t0\t%d\r\n",
				secondSaveRejected ? 1 : 0, ::GetTickCount64() - start,
				AU::_ARGS.disable_undo_selection_history ? 0 : 1);
			rows += row;
			DWORD written = 0;
			output.Write(rows, static_cast<DWORD>(rows.GetLength()), &written);
			output.Close();
			// This is an internal benchmark failure, not an interactive close:
			// do not enter the dirty-document prompt after Save was rejected.
			::PostQuitMessage(1);
			return 0;
		}
		ShowView(SOURCE);
		appendTableSnapshot("table-after-save");
	}

	DWORD written = 0;
	output.Write(rows, static_cast<DWORD>(rows.GetLength()), &written);
	output.Close();
	// -b is an unattended contract.  WM_CLOSE routes through DiscardChanges(),
	// which may legitimately ask an interactive user about a dirty FBD document
	// after reopening it.  There is no user in batch mode, and the report was
	// already written, so end the message loop directly.
	::PostQuitMessage(0);
	return 0;
}
