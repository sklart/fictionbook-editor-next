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
		// Keep the test caret inside the paragraph rather than on its boundary;
		// InsImage then resolves its enclosing section just like a UI insertion.
		if (range->move(L"character", 1) != 1)
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
		appendImportPhase("save-start");
		if (!m_doc->Save())
		{
			appendImportPhase("save-failed;phase=save;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendImportPhase("save-complete");
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
		report.Format("view\t%s\r\nidle_cycles\t%I64u\r\nelapsed_ms\t%I64u\r\ncommand_state_updates\t%I64u\r\nselection_context_builds\t%I64u\r\ntoolbar_updates\t%I64u\r\nfile_fingerprint_checks\t%I64u\r\nclipboard_checks\t%I64u\r\ncheck_command_calls\t%I64u\r\nselection_container_queries\t%I64u\r\nselection_struct_con_queries\t%I64u\r\nselection_struct_table_con_queries\t%I64u\r\njs_com_calls\t%I64u\r\n",
			sourceIdle ? "source" : (descriptionIdle ? "description" : "body"), g_idleProfile.count - idleBefore, ::GetTickCount64() - started,
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
		report.Format("caret_moves\t%d\r\nselection_changes\t%d\r\ntyping_edits\t%d\r\ninteraction_idle_cycles\t%I64u\r\ninteraction_elapsed_ms\t%I64u\r\ncommand_state_updates\t%I64u\r\nselection_context_builds\t%I64u\r\ntoolbar_updates\t%I64u\r\nfile_fingerprint_checks\t%I64u\r\nclipboard_checks\t%I64u\r\ncheck_command_calls\t%I64u\r\njs_com_calls\t%I64u\r\nstable_idle_cycles\t%I64u\r\nstable_command_state_updates\t%I64u\r\nstable_selection_context_builds\t%I64u\r\nstable_toolbar_updates\t%I64u\r\nstable_check_command_calls\t%I64u\r\nstable_js_com_calls\t%I64u\r\n",
			caretMoves, selectionChanges, typingEdits, interactionIdleCycles, interactionElapsed,
			g_idleProfile.commandUpdates - commandBefore, g_idleProfile.selectionUpdates - selectionBefore,
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
