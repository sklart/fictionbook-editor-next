	CAtlFile output;
	if (FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS)))
		return 0;
	if (IsFbeTestScenario(L"archive-open-runtime"))
	{
		const bool archiveSource = m_document_session.Location().IsArchive();
		const bool fb2 = m_doc->GetDocumentFileType() == FictionBookFileType::Fb2;
		const bool fbd = m_doc->GetDocumentFileType() == FictionBookFileType::Fbd;
		const bool htmlReady = m_doc->m_body.Document() != NULL;
		const bool rar = m_document_session.Location().containerKind == DocumentContainerKind::Rar;
		// Runtime reports are consumed as UTF-8 by the PowerShell regressions.
		// Do not let the runner's ANSI code page convert a Unicode ZIP entry:
		// on an English Windows image that conversion can throw before the
		// archive-open diagnostic is written.
		const CStringA entryUtf8(CW2A(m_document_session.Location().entryPath, CP_UTF8));
		CStringA report;
		report.Format("archive=%d\nfb2=%d\nfbd=%d\nmshtml=%d\nrar=%d\nentry=%s\n", archiveSource, fb2, fbd, htmlReady, rar,
			static_cast<LPCSTR>(entryUtf8));
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(archiveSource && (fb2 || fbd) && htmlReady ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-runtime") || IsFbeTestScenario(L"archive-rar-save-runtime"))
	{
		const bool archiveSource = m_document_session.Location().IsArchive();
		const bool fb2 = m_doc->GetDocumentFileType() == FictionBookFileType::Fb2;
		const bool htmlReady = m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		const char* markerText = "ARCHIVE_RUNTIME_BEFORE";
		char* marker = strstr(source.data(), markerText);
		if (!marker)
		{
			// The checked-in RAR5 fixture is an existing valid FB2 regression
			// document.  Its stable author field is the edit target for Save As.
			markerText = "FBE Test";
			marker = strstr(source.data(), markerText);
		}
		const bool markerFound = marker != NULL;
		if (markerFound) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen(markerText)); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_AFTER")); }
		int sourceLine = 0, sourceColumn = 0;
		const bool sourceCommitted = markerFound && m_doc->SetXMLAndValidate(m_source, false, sourceLine, sourceColumn);
		ShowView(BODY);
		const bool bodyActive = !IsSourceActive();
		const bool readOnlyArchive = m_document_session.Location().containerKind == DocumentContainerKind::Rar;
		const bool saveAs = IsFbeTestScenario(L"archive-rar-save-runtime");
		const bool shouldSave = !readOnlyArchive || saveAs;
		const bool saved = sourceCommitted && bodyActive && markerFound && (!shouldSave || SaveFile(false) == OK);
		CStringA report;
		report.Format("archive=%d\nfb2=%d\nfbd=%d\nmshtml=%d\nrar=%d\nsave_as=%d\nsource_committed=%d\nbody_active=%d\nentry=%S\nsaved=%d\n", archiveSource, fb2,
			m_doc->GetDocumentFileType() == FictionBookFileType::Fbd, htmlReady, readOnlyArchive, saveAs,
			sourceCommitted, bodyActive, static_cast<LPCWSTR>(m_document_session.Location().entryPath), saved);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(saved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"body-source-transition-runtime"))
	{
		FB::Doc* const originalDocument = m_doc;
		const wchar_t* const unicodeMarker = L"\x0413\x0420\x0410\x041D\x042C_UNICODE";
		// Exercise the actual MSHTML selection -> Scintilla selection -> MSHTML
		// selection path before the legacy source-edit checks below.  The fixture
		// deliberately includes Unicode, repeated text and inline boundaries: a
		// source position alone cannot prove that these ranges survived a view
		// transition.
		auto selectBodyRange = [&](const wchar_t* elementId, const wchar_t* beginMarker, const wchar_t* endMarker, bool collapsed)
		{
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLDocument3Ptr document3(document);
			MSHTML::IHTMLBodyElementPtr body(document ? document->body : NULL);
			MSHTML::IHTMLElementPtr element(document3 ? document3->getElementById(elementId) : NULL);
			if (!body || !element) return false;
			MSHTML::IHTMLTxtRangePtr begin(body->createTextRange());
			MSHTML::IHTMLTxtRangePtr end(body->createTextRange());
			if (!begin || !end) return false;
			begin->moveToElementText(element); end->moveToElementText(element);
			const bool wholeElement = beginMarker == NULL || *beginMarker == L'\0';
			if (!wholeElement && !begin->findText(beginMarker, 0, 0)) return false;
			if (collapsed)
				begin->collapse(VARIANT_TRUE);
			else
			{
				if (!wholeElement && (!end->findText(endMarker, 0, 0) || FAILED(begin->setEndPoint(L"EndToEnd", end)))) return false;
			}
			begin->select();
			return true;
		};
		auto selectedBodyText = [&]()
		{
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLSelectionObjectPtr selection(document ? document->selection : NULL);
			MSHTML::IHTMLTxtRangePtr range(selection ? selection->createRange() : NULL);
			return range ? CString(static_cast<LPCWSTR>(range->text)) : CString();
		};
		CStringA selectionRoundTripDiagnostics;
		auto roundTripBodySelection = [&](const wchar_t* elementId, const wchar_t* beginMarker, const wchar_t* endMarker, const wchar_t* expectedText, bool collapsed)
		{
			const bool selected = selectBodyRange(elementId, beginMarker, endMarker, collapsed);
			if (!selected) { selectionRoundTripDiagnostics.AppendFormat("%S=select;", elementId); return false; }
			const CString before(selectedBodyText());
			ShowView(SOURCE);
			const sptr_t sourceStart = m_source.SendMessage(SCI_GETSELECTIONSTART);
			const sptr_t sourceEnd = m_source.SendMessage(SCI_GETSELECTIONEND);
			const bool sourceSelection = IsSourceActive() && m_editor_selection_state.BodySource().bodyToSourceTransferred &&
				sourceStart >= 0 && sourceEnd >= sourceStart && (collapsed ? sourceStart == sourceEnd : sourceEnd > sourceStart);
			const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
			m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
			const CStringA expectedUtf8(CW2A(expectedText, CP_UTF8));
			const bool sourceContainsExpected = collapsed || (sourceSelection && CStringA(source.data() + sourceStart,
				static_cast<int>(sourceEnd - sourceStart)).Find(expectedUtf8) >= 0);
			ShowView(BODY);
			const CString after(selectedBodyText());
			// IHTMLTxtRange::text omits inline markup content for an outer paragraph
			// range.  Its UTF-8 source range above is the authoritative check for
			// this case; the visual comparison still proves that the same range came
			// back after the round trip.
			const bool outerInlineRange = wcscmp(elementId, L"body-source-inline") == 0;
			const bool beforeMatches = collapsed ? before.IsEmpty() : (outerInlineRange ? !before.IsEmpty() : before.Find(expectedText) >= 0);
			const bool afterMatches = collapsed ? after.IsEmpty() : (outerInlineRange ? !after.IsEmpty() : after.Find(expectedText) >= 0);
			const bool exactRoundTrip = before == after;
			selectionRoundTripDiagnostics.AppendFormat("%S=%d/%d/%d/%d/%d/%d;", elementId, sourceSelection, sourceContainsExpected, beforeMatches, afterMatches, exactRoundTrip, !IsSourceActive());
			return sourceSelection && sourceContainsExpected && !IsSourceActive() && beforeMatches && afterMatches && exactRoundTrip;
		};
		const bool unicodeInlineSelection = roundTripBodySelection(L"body-source-unicode", L"UNICODE_BEGIN", L"UNICODE_END", unicodeMarker, false);
		const bool repeatedTextSelection = roundTripBodySelection(L"body-source-repeat", L"REPEAT_TOKEN", L"REPEAT_TOKEN_REPEAT_END", L"REPEAT_TOKEN middle REPEAT_TOKEN_REPEAT_END", false);
		const bool tagBoundarySelection = roundTripBodySelection(L"body-source-boundary", L"BOUNDARY_BEGIN", L"BOUNDARY_END", L"BOUNDARY_BEGIN", false);
		const bool collapsedCaretSelection = roundTripBodySelection(L"body-source-caret", L"CARET_UNICODE", L"CARET_UNICODE", L"", true);
		ShowView(SOURCE);
		const bool sourceActive = IsSourceActive();
		const sptr_t initialLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> initialSource(static_cast<size_t>(initialLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, initialLength + 1, reinterpret_cast<LPARAM>(initialSource.data()));
		const char* const originalMarker = "BODY_SOURCE_ORIGINAL";
		const bool sourceCurrent = strstr(initialSource.data(), originalMarker) != NULL;
		ShowView(BODY);
		const bool bodyWithoutChange = !IsSourceActive() && m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		char* marker = strstr(initialSource.data(), originalMarker);
		const bool markerFound = marker != NULL;
		if(markerFound)
		{
			const sptr_t position = static_cast<sptr_t>(marker - initialSource.data());
			m_source.SendMessage(SCI_SETSEL, position, position + strlen(originalMarker));
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("BODY_SOURCE_EDITED"));
		}
		ShowView(BODY);
		const bool validEditApplied = markerFound && !IsSourceActive() && m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		const sptr_t editedLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> editedSource(static_cast<size_t>(editedLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, editedLength + 1, reinterpret_cast<LPARAM>(editedSource.data()));
		const bool editedSourceCurrent = strstr(editedSource.data(), "BODY_SOURCE_EDITED") != NULL;
		bool cycles = true;
		for(int cycle = 0; cycle < 3; ++cycle) { ShowView(BODY); cycles = cycles && !IsSourceActive(); ShowView(SOURCE); cycles = cycles && IsSourceActive(); }
		const sptr_t preservedSelectionStart = m_source.SendMessage(SCI_GETSELECTIONSTART);
		const sptr_t preservedSelectionEnd = m_source.SendMessage(SCI_GETSELECTIONEND);
		m_source.SendMessage(SCI_SELECTALL);
		m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("<FictionBook><broken>"));
		const bool invalidRejected = !SourceToHTML();
		const bool invalidPreserved = invalidRejected && IsSourceActive() && m_doc == originalDocument &&
			m_source.SendMessage(SCI_GETLENGTH) > 0 && m_source.SendMessage(SCI_GETSELECTIONSTART) >= 0 && m_source.SendMessage(SCI_GETSELECTIONEND) >= 0;
		CStringA report;
		report.Format("source_active=%d\nsource_current=%d\nbody_without_change=%d\nvalid_edit=%d\nedited_source=%d\ncycles=%d\ninvalid_rejected=%d\ninvalid_preserved=%d\nselection_saved=%d\nunicode_inline_selection=%d\nrepeated_text_selection=%d\ntag_boundary_selection=%d\ncollapsed_caret_selection=%d\nselection_diagnostics=%s\n", sourceActive, sourceCurrent, bodyWithoutChange, validEditApplied, editedSourceCurrent, cycles, invalidRejected, invalidPreserved, preservedSelectionStart >= 0 && preservedSelectionEnd >= 0, unicodeInlineSelection, repeatedTextSelection, tagBoundarySelection, collapsedCaretSelection, static_cast<LPCSTR>(selectionRoundTripDiagnostics));
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(sourceActive && sourceCurrent && bodyWithoutChange && validEditApplied && editedSourceCurrent && cycles && invalidPreserved && unicodeInlineSelection && repeatedTextSelection && tagBoundarySelection && collapsedCaretSelection ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"settings-dialog-runtime") || IsFbeTestScenario(L"settings-dialog-runtime-verify"))
	{
		const bool verifyOnly = IsFbeTestScenario(L"settings-dialog-runtime-verify");
		if (!verifyOnly)
			SendMessage(WM_COMMAND, MAKEWPARAM(ID_TOOLS_OPTIONS, 0), 0);
		const bool backupPersisted = _Settings.GetCreateBackupFile();
		const bool nbspPersisted = _Settings.GetNBSPChar() == L"\x25AB";
		const bool wrapPersisted = _Settings.XmlSrcWrap();
		const bool persisted = backupPersisted && nbspPersisted && wrapPersisted;
		const bool applied = verifyOnly || (m_source.SendMessage(SCI_GETWRAPMODE) != SC_WRAP_NONE);
		CStringA report;
		report.Format("settings_dialog=%d\npersisted=%d\nbackup=%d\nnbsp=%d\nwrap=%d\napplied=%d\n", !verifyOnly, persisted, backupPersisted, nbspPersisted, wrapPersisted, applied);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(persisted && applied ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"editor-view-lifecycle-runtime"))
	{
		FB::Doc* const originalDocument = m_doc;
		const auto isBodyHostActive = [&]() { return m_view.GetActiveWnd() == m_doc->m_body; };
		const auto descriptionModeEnabled = [&]()
		{
			MSHTML::IHTMLDocument3Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr description = document ? document->getElementById(L"fbw_desc") : MSHTML::IHTMLElementPtr();
			return description && description->style && U::scmp(description->style->display, L"block") == 0;
		};
		ShowView(DESC);
		const bool bodyToDescription = m_editor_view_state.Current() == DESC && m_editor_view_state.Previous() == BODY && isBodyHostActive() && descriptionModeEnabled();
		ShowView(BODY);
		const bool descriptionToBody = m_editor_view_state.Current() == BODY && m_editor_view_state.Previous() == DESC && isBodyHostActive() && !descriptionModeEnabled();
		ShowView(SOURCE);
		const bool bodyToSource = m_editor_view_state.Current() == SOURCE && IsSourceActive() && m_editor_view_state.Previous() == BODY;
		ShowView(BODY);
		const bool sourceToBody = m_editor_view_state.Current() == BODY && !IsSourceActive() && m_editor_view_state.Previous() == SOURCE && isBodyHostActive();
		ShowView(DESC);
		ShowView(SOURCE);
		const bool descriptionToSource = m_editor_view_state.Current() == SOURCE && IsSourceActive() && m_editor_view_state.Previous() == DESC;
		ShowView(DESC);
		const bool sourceToDescription = m_editor_view_state.Current() == DESC && !IsSourceActive() && m_editor_view_state.Previous() == SOURCE && isBodyHostActive();
		ShowView(SOURCE);
		ShowView(BODY);
		ShowView(DESC);
		const bool sourceBodyDescription = m_editor_view_state.Current() == DESC && m_editor_view_state.Previous() == BODY && isBodyHostActive();
		ShowView(SOURCE);
		ShowView(DESC);
		ShowView(BODY);
		const bool sourceDescriptionBody = m_editor_view_state.Current() == BODY && m_editor_view_state.Previous() == DESC && isBodyHostActive();
		bool cycles = true;
		for(int cycle = 0; cycle < 3; ++cycle)
		{
			ShowView(DESC); cycles = cycles && m_editor_view_state.Current() == DESC && isBodyHostActive();
			ShowView(SOURCE); cycles = cycles && IsSourceActive();
			ShowView(BODY); cycles = cycles && m_editor_view_state.Current() == BODY && isBodyHostActive();
		}
		const bool documentPreserved = m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		CStringA report;
		report.Format("body_desc=%d\ndesc_body=%d\nbody_source=%d\nsource_body=%d\ndesc_source=%d\nsource_desc=%d\nsource_body_desc=%d\nsource_desc_body=%d\ncycles=%d\ndocument=%d\n", bodyToDescription, descriptionToBody, bodyToSource, sourceToBody, descriptionToSource, sourceToDescription, sourceBodyDescription, sourceDescriptionBody, cycles, documentPreserved);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(bodyToDescription && descriptionToBody && bodyToSource && sourceToBody && descriptionToSource && sourceToDescription && sourceBodyDescription && sourceDescriptionBody && cycles && documentPreserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-two-phase-runtime"))
	{
		wchar_t failedArchive[MAX_PATH] = {};
		const DWORD failedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_FAILURE_PATH", failedArchive, _countof(failedArchive));
		ShowView(SOURCE);
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_BEFORE");
		if (marker)
		{
			const size_t offset = static_cast<size_t>(marker - source.data());
			m_source.SendMessage(SCI_SETSEL, static_cast<WPARAM>(offset), static_cast<LPARAM>(offset + strlen("ARCHIVE_RUNTIME_BEFORE")));
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_UNSAVED"));
		}
		const CString filenameBefore(m_doc->m_filename);
		CString mruBefore;
		for (int index = 0; index < m_recentDocuments.List().m_arrDocs.GetSize(); ++index)
			mruBefore.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_recentDocuments.List().m_arrDocs[index].szDocName));
		const FILE_OP_STATUS result = failedLength && failedLength < _countof(failedArchive) ? LoadFile(failedArchive) : FAIL;
		const bool sourceStillModified = m_source.SendMessage(SCI_GETMODIFY) != 0;
		const bool sameDocument = CString(m_doc->m_filename) == filenameBefore && !m_document_session.Location().IsArchive();
		CString mruAfter;
		for (int index = 0; index < m_recentDocuments.List().m_arrDocs.GetSize(); ++index)
			mruAfter.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_recentDocuments.List().m_arrDocs[index].szDocName));
		const bool mruUnchanged = mruAfter == mruBefore;
		CStringA report;
		report.Format("open_cancelled=%d\nmodified=%d\nsame_document=%d\nmru_unchanged=%d\n", result == CANCELLED, sourceStillModified, sameDocument, mruUnchanged);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(result == CANCELLED && sourceStillModified && sameDocument && mruUnchanged ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"failed-open-runtime"))
	{
		wchar_t failedPath[MAX_PATH] = {};
		const DWORD failedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_FAILED_OPEN_PATH", failedPath, _countof(failedPath));
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		CString mruBefore;
		for (int index = 0; index < m_recentDocuments.List().m_arrDocs.GetSize(); ++index) mruBefore.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_recentDocuments.List().m_arrDocs[index].szDocName));
		const FILE_OP_STATUS result = failedLength && failedLength < _countof(failedPath) ? LoadFile(failedPath) : FAIL;
		CString mruAfter;
		for (int index = 0; index < m_recentDocuments.List().m_arrDocs.GetSize(); ++index) mruAfter.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_recentDocuments.List().m_arrDocs[index].szDocName));
		const bool preserved = result == FAIL && m_doc == original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath == originalLocation.storagePath && mruBefore == mruAfter;
		CStringA report; report.Format("failed=%d\nidentity=%d\nactive=%d\nsession=%d\nmru_unchanged=%d\n", result == FAIL, m_doc == original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == originalLocation.storagePath, mruBefore == mruAfter);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(preserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"save-as-failure-runtime"))
	{
		wchar_t destination[MAX_PATH] = {};
		const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SAVE_PATH", destination, _countof(destination));
		const CString filename(m_doc->m_filename), encoding(m_doc->m_encoding);
		const bool nameValid = m_doc->m_namevalid;
		const FictionBookFileType type = m_doc->GetDocumentFileType();
		const DocumentLocation location = m_document_session.Location();
		const FILE_OP_STATUS result = length && length < _countof(destination) ? SaveFile(true) : FAIL;
		const bool preserved = result == FAIL && CString(m_doc->m_filename) == filename && m_doc->m_namevalid == nameValid &&
			m_doc->GetDocumentFileType() == type && CString(m_doc->m_encoding) == encoding &&
			m_document_session.Location().storagePath == location.storagePath;
		CStringA report; report.Format("failed=%d\nfilename=%d\nnamevalid=%d\ntype=%d\nencoding=%d\nsession=%d\n", result == FAIL,
			CString(m_doc->m_filename) == filename, m_doc->m_namevalid == nameValid, m_doc->GetDocumentFileType() == type,
			CString(m_doc->m_encoding) == encoding, m_document_session.Location().storagePath == location.storagePath);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(preserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"malformed-source-fallback-runtime"))
	{
		wchar_t malformedPath[MAX_PATH] = {};
		const DWORD malformedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MALFORMED_SOURCE_PATH", malformedPath, _countof(malformedPath));
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		const FILE_OP_STATUS result = malformedLength && malformedLength < _countof(malformedPath) ? LoadFile(malformedPath) : FAIL;
		const bool sourceFallback = result == OK && m_doc == original && FB::Doc::m_active_doc == m_doc &&
			m_bad_xml && m_bad_filename == malformedPath && m_editor_view_state.Current() == SOURCE &&
			m_document_session.Location().storagePath == originalLocation.storagePath;
		CStringA report; report.Format("fallback=%d\nidentity=%d\nactive=%d\nsession=%d\nsource=%d\n", result == OK, m_doc == original,
			FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == originalLocation.storagePath, m_bad_xml && m_bad_filename == malformedPath && m_editor_view_state.Current() == SOURCE);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(sourceFallback ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"successful-open-runtime"))
	{
		wchar_t openedPath[MAX_PATH] = {};
		const DWORD openedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SUCCESSFUL_OPEN_PATH", openedPath, _countof(openedPath));
		FB::Doc* const original = m_doc;
		const FILE_OP_STATUS result = openedLength && openedLength < _countof(openedPath) ? LoadFile(openedPath) : FAIL;
		const bool opened = result == OK && m_doc != original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath == openedPath && m_doc->m_filename == openedPath && m_doc->m_body.Document() != NULL;
		CStringA report; report.Format("opened=%d\nidentity_changed=%d\nactive=%d\nsession=%d\nfilename=%d\nvalid=%d\n", result == OK,
			m_doc != original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == openedPath,
			m_doc->m_filename == openedPath, m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(opened ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"new-document-runtime"))
	{
		FB::Doc* const original = m_doc;
		BOOL handled = FALSE;
		OnFileNew(0, ID_FILE_NEW, NULL, handled);
		const bool created = m_doc != original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath.IsEmpty() && !m_document_session.Location().IsArchive() && m_doc->m_body.Document() != NULL;
		CStringA report; report.Format("created=%d\nidentity_changed=%d\nactive=%d\nsession_new=%d\nvalid=%d\n", created,
			m_doc != original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath.IsEmpty() && !m_document_session.Location().IsArchive(), m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(created ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"reload-success-runtime") || IsFbeTestScenario(L"reload-failure-runtime"))
	{
		const bool expectSuccess = IsFbeTestScenario(L"reload-success-runtime");
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		if (!expectSuccess) ::SetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", L"api-load-return-false");
		const bool result = ReloadFile();
		if (!expectSuccess) ::SetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", NULL);
		const bool reloadMatches = expectSuccess
			? result && m_doc != original && FB::Doc::m_active_doc == m_doc && m_doc->m_body.Document() != NULL &&
				m_document_session.Location().storagePath == originalLocation.storagePath
			: !result && m_doc == original && FB::Doc::m_active_doc == m_doc &&
				m_document_session.Location().storagePath == originalLocation.storagePath;
		CStringA report; report.Format("reloaded=%d\nidentity=%d\nactive=%d\nsession=%d\nvalid=%d\n", result,
			expectSuccess ? m_doc != original : m_doc == original, FB::Doc::m_active_doc == m_doc,
			m_document_session.Location().storagePath == originalLocation.storagePath, m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(reloadMatches ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-mru-runtime"))
	{
		wchar_t secondEntry[MAX_PATH] = {}, occurrenceText[16] = {}, normalEntries[8192] = {};
		const DWORD secondLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_ENTRY", secondEntry, _countof(secondEntry));
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_OCCURRENCE", occurrenceText, _countof(occurrenceText));
		const DWORD normalLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_NORMAL_ENTRIES", normalEntries, _countof(normalEntries));
		DocumentLocation first = m_document_session.Location(), second = first;
		unsigned int occurrence = 0;
		const bool secondValid = secondLength > 0 && secondLength < _countof(secondEntry) && FbeRecentDocuments::ParseArchiveMruUnsigned(occurrenceText, occurrence);
		second.entryPath = secondEntry; second.entryOccurrence = occurrence; second.documentType = DetectFictionBookFileType(second.entryPath);
		// This scenario exercises MRU routing, not the unsaved-changes prompt.
		// A freshly loaded MSHTML document can carry a transient form-change bit.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		ResolvedOpenDocument secondResolved; FbeArchive::Error secondError;
		const bool secondFound = secondValid && FbeArchiveUi::ResolveOpenRequest(second.storagePath, secondResolved, &second, &secondError);
		const FILE_OP_STATUS secondOpen = secondFound ? LoadFile(second.storagePath, &second) : CANCELLED;
		if (secondOpen == OK) FbeRecentDocuments::RememberArchiveMruRecord(m_recentDocuments.List(), m_document_session.Location());
		if (secondOpen == OK) { m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT); }
		std::vector<FbeRecentDocuments::ArchiveMruRecord> records; FbeRecentDocuments::ReadArchiveMruRecords(records);
		const CString firstKey = FbeRecentDocuments::ArchiveMruKey(first); WORD firstCommand = 0;
		for (int offset = 0; offset < m_recentDocuments.List().m_arrDocs.GetSize() && offset <= ID_FILE_MRU_LAST - ID_FILE_MRU_FIRST; ++offset) { CString key; const WORD candidate = MruCommandId(offset); if (m_recentDocuments.List().GetFromList(candidate, key) && key == firstKey) { firstCommand = candidate; break; } }
		DocumentLocation menuFirst;
		const bool menuLookup = firstCommand != 0 && FbeRecentDocuments::FindArchiveMruRecord(firstKey, menuFirst) && FbeRecentDocuments::SameArchiveMruIdentity(menuFirst, first);
		BOOL handled = FALSE;
		const LRESULT handlerResult = secondOpen == OK && menuLookup ? OnFileOpenMRU(0, firstCommand, NULL, handled) : 1;
		const FILE_OP_STATUS firstOpen = handlerResult == 0 && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), first) ? OK : FAIL;
		const bool reopenedFirst = firstOpen == OK && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), first);
		bool normalEntriesOpened = normalLength == 0;
		if (normalLength > 0 && normalLength < _countof(normalEntries))
		{
			int position = 0;
			while (position >= 0)
			{
				const CString normal = CString(normalEntries).Tokenize(L"|", position);
				if (normal.IsEmpty()) continue;
				m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
				if (LoadFile(normal) != OK) { normalEntriesOpened = false; break; }
				FbeRecentDocuments::RememberNormalMruRecord(m_recentDocuments.List(), normal);
				normalEntriesOpened = true;
			}
		}
		FbeRecentDocuments::ReadArchiveMruRecords(records);
		bool hasFirst = false, hasSecond = false;
		for (size_t index = 0; index < records.size(); ++index) { hasFirst = hasFirst || FbeRecentDocuments::SameArchiveMruIdentity(records[index].location, first); hasSecond = hasSecond || FbeRecentDocuments::SameArchiveMruIdentity(records[index].location, second); }
		DocumentLocation missing = first; missing.entryPath = L"missing.fb2"; missing.entryOccurrence = 0;
		ResolvedOpenDocument ignored; FbeArchive::Error missingError;
		const bool missingRejected = !FbeArchiveUi::ResolveOpenRequest(first.storagePath, ignored, &missing, &missingError) && missingError.code == FbeArchive::ErrorCode::EntryNotFound;
		int menuCount = 0, visibleArchiveCount = 0; bool menuClean = true;
		const HMENU mruMenu = m_recentDocuments.List().GetMenuHandle();
		if (mruMenu != NULL) for (int index = 0; index < ::GetMenuItemCount(mruMenu); ++index)
		{
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID;
			if (!::GetMenuItemInfo(mruMenu, index, TRUE, &item) || item.wID < ID_FILE_MRU_FIRST || item.wID > ID_FILE_MRU_LAST) continue;
			++menuCount; wchar_t caption[512] = {};
			::GetMenuString(mruMenu, item.wID, caption, _countof(caption), MF_BYCOMMAND);
			DocumentLocation captionLocation;
			if (FbeRecentDocuments::ParseArchiveMruKey(caption, captionLocation) || CString(caption).Left(2) == L"&1" || CString(caption).Left(2) == L"&2") menuClean = false;
			CString key; DocumentLocation keyLocation;
			if (m_recentDocuments.List().GetFromList(item.wID, key) && FbeRecentDocuments::ParseArchiveMruKey(key, keyLocation)) ++visibleArchiveCount;
		}
		const CString firstCaption = FbeRecentDocuments::ArchiveMruCaption(firstKey), secondCaption = FbeRecentDocuments::ArchiveMruCaption(FbeRecentDocuments::ArchiveMruKey(second));
		const bool captionsDifferent = firstCaption.Compare(secondCaption) != 0;
		const bool captionsDistinct = visibleArchiveCount < 2 || captionsDifferent;
		FbeRecentDocuments::WritePortableMru(m_recentDocuments.List());
		CStringA report; report.Format("first=%d\nsecond=%d\nmenu_lookup=%d\nsecond_found=%d\nsecond_error=%d\nsecond_open=%d\nfirst_open=%d\nreopened_first=%d\nmissing_entry=%d\narchive_records=%u\nnormal_entries=%d\nmenu_count=%d\nmenu_clean=%d\ncaption_diff=%d\ncaptions_distinct=%d\n", hasFirst, hasSecond, menuLookup, secondFound, static_cast<int>(secondError.code), secondOpen, firstOpen, reopenedFirst, missingRejected, static_cast<unsigned int>(records.size()), normalEntriesOpened, menuCount, menuClean, captionsDifferent, captionsDistinct);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(hasFirst && hasSecond && menuLookup && reopenedFirst && missingRejected && normalEntriesOpened && menuCount > 0 && menuCount <= 10 && menuClean && captionsDistinct ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-mru-restart-runtime"))
	{
		std::vector<CString> order; FbeRecentDocuments::ReadMruOrder(order);
		const int count = m_recentDocuments.List().m_arrDocs.GetSize(); bool exactOrder = count == 10 && order.size() == 10, cleanMenu = true; int menuCount = 0;
		for (int index = 0; index < count && exactOrder; ++index) exactOrder = CString(m_recentDocuments.List().m_arrDocs[index].szDocName) == order[order.size() - 1 - index];
		for (int index = 0; index < count; ++index) { DocumentLocation location; if (FbeRecentDocuments::ParseArchiveMruKey(CString(m_recentDocuments.List().m_arrDocs[index].szDocName), location) && !FbeRecentDocuments::FindArchiveMruRecord(CString(m_recentDocuments.List().m_arrDocs[index].szDocName), location)) cleanMenu = false; }
		std::vector<CString> captions; const HMENU menu = m_recentDocuments.List().GetMenuHandle(); int emptyCaption = 0, rawCaption = 0, numberedCaption = 0, duplicateCaption = 0, disabledCaption = 0; bool minimalFolderContexts = false;
		if (menu == NULL) cleanMenu = false; else for (int index = 0; index < ::GetMenuItemCount(menu); ++index)
		{
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID;
			if (!::GetMenuItemInfo(menu, index, TRUE, &item) || item.wID < ID_FILE_MRU_FIRST || item.wID > ID_FILE_MRU_LAST) continue;
			++menuCount; wchar_t text[512] = {}; ::GetMenuString(menu, index, text, _countof(text), MF_BYPOSITION); CString caption(text), parsedKey;
			item.fMask = MIIM_STATE; ::GetMenuItemInfo(menu, index, TRUE, &item); DocumentLocation parsed; if (caption == m_recentDocuments.List().m_szNoEntries) { ++emptyCaption; } if ((item.fState & (MFS_DISABLED | MFS_GRAYED)) != 0) { cleanMenu = false; ++disabledCaption; } if (FbeRecentDocuments::ParseArchiveMruKey(caption, parsed)) { cleanMenu = false; ++rawCaption; } if (caption.GetLength() > 1 && caption[0] == L'&' && caption[1] >= L'0' && caption[1] <= L'9') { cleanMenu = false; ++numberedCaption; }
			for (size_t previous = 0; previous < captions.size(); ++previous) if (captions[previous].CompareNoCase(caption) == 0) { cleanMenu = false; ++duplicateCaption; }
			captions.push_back(caption);
		}
		for (size_t i = 0; i < captions.size(); ++i) for (size_t j = i + 1; j < captions.size(); ++j)
			if ((captions[i].Find(L"A\\Books\\archive.zip") >= 0 && captions[j].Find(L"B\\Books\\archive.zip") >= 0) || (captions[i].Find(L"B\\Books\\archive.zip") >= 0 && captions[j].Find(L"A\\Books\\archive.zip") >= 0)) minimalFolderContexts = true;
		wchar_t path[MAX_PATH] = {}, entry[MAX_PATH] = {}, occurrenceText[16] = {}; ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_REOPEN_PATH", path, _countof(path)); ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_ENTRY", entry, _countof(entry)); ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_OCCURRENCE", occurrenceText, _countof(occurrenceText));
		unsigned int occurrence = 0; DocumentLocation target; target.containerKind = DetectDocumentContainerKind(path); target.storagePath = path; target.entryPath = entry; target.entryOccurrence = FbeRecentDocuments::ParseArchiveMruUnsigned(occurrenceText, occurrence) ? occurrence : 0; target.documentType = DetectFictionBookFileType(target.entryPath);
		WORD command = 0; const CString key = FbeRecentDocuments::ArchiveMruKey(target); for (int offset = 0; offset < count && offset <= ID_FILE_MRU_LAST - ID_FILE_MRU_FIRST; ++offset) { CString value; const WORD candidate = MruCommandId(offset); if (m_recentDocuments.List().GetFromList(candidate, value) && value == key) { command = candidate; break; } }
		BOOL handled = FALSE; const bool reopened = command != 0 && OnFileOpenMRU(0, command, NULL, handled) == 0 && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), target);
		auto getMruItem = [&](UINT id, CString& caption, UINT& state) -> bool
		{
			const int position = FindMenuPositionByCommand(menu, id);
			if (position < 0) return false;
			wchar_t text[512] = {}; if (::GetMenuString(menu, position, text, _countof(text), MF_BYPOSITION) <= 0) return false;
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_STATE;
			if (!::GetMenuItemInfo(menu, position, TRUE, &item)) return false;
			caption = text; state = item.fState; return true;
		};
		CString firstBefore, firstRussian; UINT firstBeforeState = 0, firstRussianState = 0;
		CString firstKeyBefore; const bool firstMappedBefore = m_recentDocuments.List().GetFromList(ID_FILE_MRU_FIRST, firstKeyBefore);
		const bool firstVisibleBefore = getMruItem(ID_FILE_MRU_FIRST, firstBefore, firstBeforeState);
		_Settings.SetInterfaceLanguage(FBE_INTERFACE_LANGUAGE_RUSSIAN);
		FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName()); FbeResetRuntimeLocalization(); RefreshLocalizedMainFrameUi();
		const bool russianLocaleSelected = _Settings.GetInterfaceLocaleName() == L"ru-RU";
		const CString russianLocalizedEmpty = FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.recent.empty", L"No Recent Files");
		CString firstKeyRussian; const bool firstMappedRussian = m_recentDocuments.List().GetFromList(ID_FILE_MRU_FIRST, firstKeyRussian);
		const bool firstVisibleRussian = getMruItem(ID_FILE_MRU_FIRST, firstRussian, firstRussianState);
		const bool nonEmptyMruLocalized = firstMappedBefore && firstMappedRussian && firstKeyBefore == firstKeyRussian && firstVisibleBefore && firstVisibleRussian && firstBefore == firstRussian && firstRussian != m_recentDocuments.List().m_szNoEntries && (firstRussianState & (MFS_DISABLED | MFS_GRAYED)) == 0;
		m_recentDocuments.List().m_arrDocs.RemoveAll();
		RefreshMruEmptyStateText(m_recentDocuments.List()); FbeRecentDocuments::RebuildMruMenu(m_recentDocuments.List());
		CString russianEmpty; UINT russianEmptyState = 0; int russianEmptyCount = 0;
		for (int index = 0; index < ::GetMenuItemCount(menu); ++index) { MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID; if (::GetMenuItemInfo(menu, index, TRUE, &item) && item.wID >= ID_FILE_MRU_FIRST && item.wID <= ID_FILE_MRU_LAST) ++russianEmptyCount; }
		const bool russianEmptyOk = russianEmptyCount == 1 && !russianLocalizedEmpty.IsEmpty() && getMruItem(ID_FILE_MRU_FIRST, russianEmpty, russianEmptyState) && russianEmpty == russianLocalizedEmpty && (russianEmptyState & (MFS_DISABLED | MFS_GRAYED)) != 0;
		_Settings.SetInterfaceLanguage(FBE_INTERFACE_LANGUAGE_ENGLISH);
		FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName()); FbeResetRuntimeLocalization(); RefreshLocalizedMainFrameUi();
		CString englishEmpty; UINT englishEmptyState = 0; int englishEmptyCount = 0;
		for (int index = 0; index < ::GetMenuItemCount(menu); ++index) { MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID; if (::GetMenuItemInfo(menu, index, TRUE, &item) && item.wID >= ID_FILE_MRU_FIRST && item.wID <= ID_FILE_MRU_LAST) ++englishEmptyCount; }
		const bool englishEmptyOk = englishEmptyCount == 1 && getMruItem(ID_FILE_MRU_FIRST, englishEmpty, englishEmptyState) && englishEmpty == L"No Recent Files" && (englishEmptyState & (MFS_DISABLED | MFS_GRAYED)) != 0;
		CStringA report; report.Format("count=%d\nmenu_count=%d\norder=%d\nclean=%d\nempty=%d\ndisabled=%d\nraw=%d\nnumbered=%d\nduplicates=%d\nfolders=%d\nreopened=%d\nlocalized_nonempty=%d\nrussian_empty=%d\nrussian_locale=%d\nenglish_empty=%d\n", count, menuCount, exactOrder, cleanMenu, emptyCaption, disabledCaption, rawCaption, numberedCaption, duplicateCaption, minimalFolderContexts, reopened, nonEmptyMruLocalized, russianEmptyOk, russianLocaleSelected, englishEmptyOk); DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(count == 10 && menuCount == 10 && exactOrder && cleanMenu && reopened && nonEmptyMruLocalized && russianEmptyOk && englishEmptyOk ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-create"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_BEFORE");
		if (marker) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen("ARCHIVE_RUNTIME_BEFORE")); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_RECOVERY")); }
		else
		{
			char* titleEnd = strstr(source.data(), "</book-title>");
			const size_t offset = titleEnd ? static_cast<size_t>(titleEnd - source.data()) : static_cast<size_t>(length);
			m_source.SendMessage(SCI_SETSEL, offset, offset);
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(" ARCHIVE_RUNTIME_RECOVERY"));
		}
		const bool saved = SourceToHTML() && SaveRecoveryNow(); CStringA report; report.Format("recovery_created=%d\n", saved); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(saved ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-verify"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		const bool archive = m_document_session.Location().IsArchive(); const bool fbd = m_doc->GetDocumentFileType() == FictionBookFileType::Fbd;
		const bool payload = strstr(source.data(), "ARCHIVE_RUNTIME_RECOVERY") != NULL;
		CStringA report; report.Format("archive=%d\nfbd=%d\nrecovery_payload=%d\n", archive, fbd, payload); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(archive && payload ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-external-verify"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_RECOVERY");
		if (marker) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen("ARCHIVE_RUNTIME_RECOVERY")); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_EXTERNAL")); }
		::SetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SAVE_ERROR", NULL);
		const bool blocked = marker != NULL && SourceToHTML() && SaveFile(false) == FAIL;
		wchar_t errorCode[16] = {};
		const bool modifiedExternally = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SAVE_ERROR", errorCode, _countof(errorCode)) > 0 &&
			_wtoi(errorCode) == static_cast<int>(FbeArchive::ErrorCode::ModifiedExternally);
		CStringA report; report.Format("archive=%d\nblocked=%d\nmodified_externally=%d\nerror_code=%S\n", m_document_session.Location().IsArchive(), blocked, modifiedExternally, errorCode); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(blocked && modifiedExternally ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"table-roundtrip"))
	{
		const ULONGLONG start = ::GetTickCount64();
		auto appendTablePhase = [&](const char* phase)
		{
			const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
			m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
			auto countTag = [&](const char* tag) -> long { long count = 0; for (const char* position = source.data(); (position = strstr(position, tag)) != NULL; ++position) ++count; return count; };
			const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
			CStringA row;
			row.Format("%s\t%I64u\t%I64u\t%I64u\t%ld\t%ld\t%ld\t%ld\r\n", phase,
				::GetTickCount64() - start, static_cast<unsigned __int64>(memory.privateBytes), static_cast<unsigned __int64>(memory.workingSetBytes),
				countTag("<table"), countTag("<tr"), countTag("<td"), countTag("<th"));
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		CStringA header("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\ttable_count\ttr_count\ttd_count\tth_count\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();
		appendTablePhase("open-complete");
		for (int cycle = 1; cycle <= 5; ++cycle)
		{
			CStringA phase; phase.Format("source-%d-start", cycle); appendTablePhase(phase);
			ShowView(SOURCE); phase.Format("source-%d-complete", cycle); appendTablePhase(phase);
			phase.Format("body-%d-start", cycle); appendTablePhase(phase);
			ShowView(BODY); phase.Format("body-%d-complete", cycle); appendTablePhase(phase);
		}
		appendTablePhase("save-1-start");
		if (!m_doc->Save())
		{
			appendTablePhase("save-1-failed;phase=save-1;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendTablePhase("save-1-complete");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"editor-background-runtime"))
	{
		StartupTrace::AppendTestStartupBreadcrumb("scenario-enter");
		CStringA header("phase\timage\tcss_url\trepeat\tposition\tsize\tattachment\tmodified\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		auto backgroundPathFromEnvironment = [](const wchar_t* name) -> CString
		{
			wchar_t value[1024] = {};
			const DWORD length = ::GetEnvironmentVariable(name, value, _countof(value));
			return length && length < _countof(value) ? CString(value) : CString();
		};
		auto appendBackgroundPhase = [&](const char* phase)
		{
			MSHTML::IHTMLStylePtr style(m_doc->m_body.Document() ? m_doc->m_body.Document()->body->style : MSHTML::IHTMLStylePtr());
			CString image, repeat, position, attachment, cssText, size(L"auto");
			CString cssUrl;
			if(style) {
				image = static_cast<LPCWSTR>(style->backgroundImage); repeat = static_cast<LPCWSTR>(style->backgroundRepeat);
				position = static_cast<LPCWSTR>(style->backgroundPosition); attachment = static_cast<LPCWSTR>(style->backgroundAttachment);
				cssText = static_cast<LPCWSTR>(style->cssText);
				if(cssText.Find(L"background-size: contain") >= 0) size = L"contain";
				else if(cssText.Find(L"background-size: cover") >= 0) size = L"cover";
				else { _variant_t sizeAttribute(style->getAttribute(L"background-size", 0)); if(sizeAttribute.vt == VT_BSTR && sizeAttribute.bstrVal) size = sizeAttribute.bstrVal; }
			}
			CString path;
			if(_Settings.GetEditorBackgroundKind() == L"builtin") EditorBackgrounds::ResolveBuiltIn(_Settings.GetEditorBackgroundId(), path);
			else if(_Settings.GetEditorBackgroundKind() == L"custom") path = _Settings.GetEditorBackgroundCustomPath();
			if(!path.IsEmpty()) { const CString uri = U::UrlFromPath(path); if(!uri.IsEmpty()) cssUrl.Format(L"url(\"%s\")", static_cast<LPCWSTR>(uri)); }
			CStringA imageA(CW2A(image, CP_UTF8)), repeatA(CW2A(repeat, CP_UTF8)), positionA(CW2A(position, CP_UTF8));
			CStringA cssUrlA(CW2A(cssUrl, CP_UTF8)), sizeA(CW2A(size, CP_UTF8)), attachmentA(CW2A(attachment, CP_UTF8));
			CStringA row; row.Format("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\r\n", phase,
				imageA.GetString(), cssUrlA.GetString(), repeatA.GetString(), positionA.GetString(), sizeA.GetString(), attachmentA.GetString(), m_doc->DocChanged() ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		_Settings.SetEditorBackgroundKind(L"none"); _Settings.SetEditorBackgroundId(CString()); _Settings.SetEditorBackgroundCustomPath(CString()); _Settings.SetEditorBackgroundLayout(L"tile");
		StartupTrace::AppendTestStartupBreadcrumb("none-start"); m_doc->ApplyConfChanges(); appendBackgroundPhase("none"); StartupTrace::AppendTestStartupBreadcrumb("none-complete");
		_Settings.SetEditorBackgroundKind(L"builtin"); _Settings.SetEditorBackgroundId(L"01_clean_white"); _Settings.SetEditorBackgroundLayout(L"tile");
		StartupTrace::AppendTestStartupBreadcrumb("builtin-tile-start"); m_doc->ApplyConfChanges(); appendBackgroundPhase("builtin-tile"); StartupTrace::AppendTestStartupBreadcrumb("builtin-tile-complete");
		for(const wchar_t* layout : { L"center", L"contain", L"cover" }) {
			_Settings.SetEditorBackgroundLayout(layout); m_doc->ApplyConfChanges();
			if(wcscmp(layout, L"center") == 0) { appendBackgroundPhase("builtin-center"); StartupTrace::AppendTestStartupBreadcrumb("builtin-center-complete"); }
			else if(wcscmp(layout, L"contain") == 0) { appendBackgroundPhase("builtin-contain"); StartupTrace::AppendTestStartupBreadcrumb("builtin-contain-complete"); }
			else { appendBackgroundPhase("builtin-cover"); StartupTrace::AppendTestStartupBreadcrumb("builtin-cover-complete"); }
		}
		StartupTrace::AppendTestStartupBreadcrumb("source-view-start"); ShowView(SOURCE); StartupTrace::AppendTestStartupBreadcrumb("source-view-complete"); StartupTrace::AppendTestStartupBreadcrumb("body-view-start"); ShowView(BODY); StartupTrace::AppendTestStartupBreadcrumb("body-view-complete");
		// Source-to-Body recreates the MSHTML document in this test harness.  Its
		// freshly loaded version is the baseline before checking that UI-only
		// background changes leave the document clean.
		m_doc->MarkSavePoint();
		m_doc->ApplyConfChanges(); appendBackgroundPhase("builtin-after-view-recreate"); StartupTrace::AppendTestStartupBreadcrumb("view-recreate-complete");
		_Settings.SetEditorBackgroundId(L"unknown-background"); m_doc->ApplyConfChanges(); appendBackgroundPhase("unknown-builtin"); StartupTrace::AppendTestStartupBreadcrumb("unknown-builtin-complete");
		_Settings.SetEditorBackgroundKind(L"custom"); _Settings.SetEditorBackgroundCustomPath(backgroundPathFromEnvironment(L"FBE_NEXT_TEST_BACKGROUND_MISSING_PATH")); m_doc->ApplyConfChanges(); appendBackgroundPhase("missing-custom"); StartupTrace::AppendTestStartupBreadcrumb("missing-custom-complete");
		_Settings.SetEditorBackgroundCustomPath(backgroundPathFromEnvironment(L"FBE_NEXT_TEST_BACKGROUND_PATH")); _Settings.SetEditorBackgroundLayout(L"contain"); m_doc->ApplyConfChanges(); appendBackgroundPhase("custom"); StartupTrace::AppendTestStartupBreadcrumb("custom-complete");
		_Settings.SetEditorBackgroundKind(L"builtin"); _Settings.SetEditorBackgroundId(L"01_clean_white"); _Settings.SetEditorBackgroundLayout(L"tile"); m_doc->ApplyConfChanges(); appendBackgroundPhase("before-save"); StartupTrace::AppendTestStartupBreadcrumb("before-save");
		StartupTrace::AppendTestStartupBreadcrumb("save-start");
		if(!m_doc->Save()) {
			CString saveFailure;
			saveFailure.Format(L"editor background runtime save failed; name-valid=%d", m_doc->m_namevalid ? 1 : 0);
			StartupTrace::HResult(L"test", L"TST202", m_doc->GetLastSaveError(), saveFailure);
			CStringA savePhase;
			savePhase.Format("save-failed-hr-0x%08lX", static_cast<unsigned long>(m_doc->GetLastSaveError()));
			appendBackgroundPhase(savePhase);
			StartupTrace::AppendTestStartupBreadcrumb(m_doc->m_namevalid ? "save-failed" : "save-failed-name-invalid");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		StartupTrace::AppendTestStartupBreadcrumb("save-complete");
		appendBackgroundPhase("after-save"); output.Flush(); StartupTrace::AppendTestStartupBreadcrumb("after-save"); StartupTrace::AppendTestStartupBreadcrumb("report-flush"); output.Close(); StartupTrace::AppendTestStartupBreadcrumb("report-closed"); StartupTrace::AppendTestStartupBreadcrumb("shutdown-requested"); ::PostQuitMessage(0); StartupTrace::AppendTestStartupBreadcrumb("shutdown-quit-posted"); return 0;
	}
	if (IsFbeTestScenario(L"structural-trace-contract"))
	{
		wchar_t tracePath[MAX_PATH] = {};
		const DWORD traceLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TRACE", tracePath, _countof(tracePath));
		FbeStructure::StructuralTrace trace(traceLength ? tracePath : nullptr, L"cite\ttrace", L"case\r\ntrace");
		trace.Before(L"phase\tone", L"TAB\tCR\rLF\n");
		trace.After(L"phase-two", L"complete");
		trace.Hr(L"failed-write", E_ACCESSDENIED, L"denied");
		CStringA row; row.Format("enabled\twrite_failed\tlast_error\tresult\r\n%d\t%d\t0x%08lX\tpass\r\n", trace.IsEnabled() ? 1 : 0, trace.HasWriteFailure() ? 1 : 0, static_cast<unsigned long>(trace.LastError()));
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(0); return 0;
	}
	if (IsFbeTestScenario(L"cite-poem-undo"))
	{
		wchar_t operation[16] = {};
		wchar_t target[16] = {};
		wchar_t selectionMode[16] = {};
		wchar_t tracePath[MAX_PATH] = {};
		wchar_t traceCase[64] = {};
		wchar_t route[16] = {}, citePoemFault[32] = {};
		const DWORD operationLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_OPERATION", operation, _countof(operation));
		const DWORD targetLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TARGET", target, _countof(target));
		const DWORD selectionModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_SELECTION_MODE", selectionMode, _countof(selectionMode));
		const DWORD traceLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TRACE", tracePath, _countof(tracePath));
		const DWORD traceCaseLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_CASE", traceCase, _countof(traceCase));
		const DWORD routeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_ROUTE", route, _countof(route));
		const DWORD citePoemFaultLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_CITE_POEM_FAULT", citePoemFault, _countof(citePoemFault));
		const bool cite = operationLength == 4 && wcscmp(operation, L"cite") == 0;
		const bool poem = operationLength == 4 && wcscmp(operation, L"poem") == 0;
		const wchar_t* targetClass = targetLength ? target : L"section";
		const CStringA targetName((CW2A(targetClass)));
		const bool selectCaret = selectionModeLength == 5 && wcscmp(selectionMode, L"caret") == 0;
		const CStringA selectionName(selectCaret ? "caret" : "selected");
		const bool repeat = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_REPEAT", nullptr, 0) != 0;
		const bool viaWrapper = routeLength == 7 && wcscmp(route, L"wrapper") == 0;
		CStringA header("operation\ttarget\tselection_mode\tselection_collapsed\tselection_text_utf16\tselection_html_utf16\tselection_parent_utf16\tselection_start_to_first_start\tselection_end_to_first_end\tcheck_allowed\tbefore_equals_undo\tafter_equals_redo\tsequential_cycle\tbefore_paragraphs\tafter_cites\tafter_poems\tafter_stanzas\tpoem_text_utf16\tempty_divs\tempty_paragraphs\tempty_stanzas\tsaved\tresult\tcheck_status\tapply_status\thresult\tdocument_changed\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		auto writeFailure = [&](const char* reason)
		{
			CStringA row; row.Format("%s\t%s\t%s\t0\t-\t-\t-\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t-\t0\t0\t0\t0\t%s\r\n", cite ? "cite" : poem ? "poem" : "unknown", (LPCSTR)targetName, (LPCSTR)selectionName, reason);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1);
		};
		if (!cite && !poem) { writeFailure("invalid-operation"); return 0; }
		MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr divs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr container;
		for (long index = 0; divs && index < divs->length; ++index) {
			MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
			if (div && U::scmp(div->className, targetClass) == 0) { container = div; break; }
		}
		MSHTML::IHTMLElementCollectionPtr paragraphs(container ? MSHTML::IHTMLElement2Ptr(container)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		if (!body || !container || !paragraphs || paragraphs->length == 0) { writeFailure("missing-paragraph"); return 0; }
		MSHTML::IHTMLElementPtr first(paragraphs->item(_variant_t(0L), _variant_t()));
		MSHTML::IHTMLElementPtr last(paragraphs->item(_variant_t(paragraphs->length - 1), _variant_t()));
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		MSHTML::IHTMLTxtRangePtr rangeEnd(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		if (!first || !last || !range || !rangeEnd) { writeFailure("selection-create"); return 0; }
		m_doc->m_body.SetFocus();
		range->moveToElementText(first); range->collapse(VARIANT_TRUE);
		// Keep the caret inside an empty P.  Moving one character from its start
		// makes MSHTML place the range after the P, in an editor-owned DIV.
		if (!CString((const wchar_t*)first->innerText).IsEmpty())
			range->move(L"character", 1);
		if (!selectCaret) {
			rangeEnd->moveToElementText(last);
			rangeEnd->collapse(VARIANT_FALSE); rangeEnd->move(L"character", -1);
			// Both boundaries must be inside their P elements. MSHTML otherwise
			// reports the enclosing DIV as parentElement(), and
			// The structural editor rejects this selection.
			range->setEndPoint(L"EndToEnd", rangeEnd);
		}
		auto utf16Summary = [](const CString& value) -> CStringA
		{
			if (value.IsEmpty()) return CStringA("-");
			CStringA summary;
			for (int index = 0; index < value.GetLength(); ++index) {
				if (index) summary += ',';
				CStringA codeUnit; codeUnit.Format("%04X", static_cast<unsigned int>(static_cast<unsigned short>(value[index])));
				summary += codeUnit;
			}
			return summary;
		};
		const CString selectionText((const wchar_t*)range->text), selectionHtml((const wchar_t*)range->htmlText);
		const bool selectionCollapsed = range->compareEndPoints(L"StartToEnd", range) == 0;
		MSHTML::IHTMLTxtRangePtr firstRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		firstRange->moveToElementText(first);
		MSHTML::IHTMLElementPtr selectionParent(range->parentElement());
		CString selectionParentText = selectionParent ? CString((const wchar_t*)selectionParent->tagName) + L":" + CString((const wchar_t*)selectionParent->className) : CString(L"-");
		const long selectionStartToFirstStart = range->compareEndPoints(L"StartToStart", firstRange);
		const long selectionEndToFirstEnd = range->compareEndPoints(L"EndToEnd", firstRange);
		const CStringA selectionTextSummary(utf16Summary(selectionText)), selectionHtmlSummary(utf16Summary(selectionHtml));
		const CStringA selectionParentSummary(utf16Summary(selectionParentText));
		range->select();
		auto countEmpty = [&](const wchar_t* tagName, const wchar_t* className = nullptr) -> long
		{
			long count = 0;
			MSHTML::IHTMLElementCollectionPtr elements(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(tagName));
			for (long index = 0; elements && index < elements->length; ++index) {
				MSHTML::IHTMLElementPtr element(elements->item(_variant_t(index), _variant_t()));
				MSHTML::IHTMLElementCollectionPtr children(element ? element->children : MSHTML::IHTMLElementCollectionPtr());
				if (element && (!className || U::scmp(element->className, className) == 0) && (!children || children->length == 0) && CString((const wchar_t*)element->innerHTML).Trim().IsEmpty()) ++count;
			}
			return count;
		};
		const long beforeEmptyDivs = countEmpty(L"DIV"), beforeEmptyParagraphs = countEmpty(L"P"), beforeEmptyStanzas = countEmpty(L"DIV", L"stanza");
		const CString before((const wchar_t*)body->innerHTML);
		const long beforeParagraphs = paragraphs->length;
		const bool dirtyBefore = m_doc->DocChanged();
		FbeStructure::StructuralTrace trace(traceLength ? tracePath : nullptr,
			cite ? L"cite" : L"poem", traceCaseLength ? traceCase : L"runtime");
		FbeStructure::BodyStructuralEditor extracted(m_doc->m_body.Document(), m_doc->m_body.MarkupServices(), trace.IsEnabled() ? &trace : nullptr);
		const FbeStructure::CitePoemFailurePoint failurePoint = citePoemFaultLength
			? (wcscmp(citePoemFault, L"after-insert") == 0 ? FbeStructure::CitePoemFailurePoint::AfterInsert
				: wcscmp(citePoemFault, L"before-selection") == 0 ? FbeStructure::CitePoemFailurePoint::BeforeSelection
				: FbeStructure::CitePoemFailurePoint::BeforeMutation)
			: FbeStructure::CitePoemFailurePoint::None;
		auto apply = [&](bool checkOnly) -> FbeStructure::StructuralOperationResult {
			if (viaWrapper)
				return cite ? m_doc->m_body.InsertCiteResult(checkOnly, failurePoint) : m_doc->m_body.InsertPoemResult(checkOnly, failurePoint);
			return cite ? extracted.InsertCite(checkOnly, failurePoint) : extracted.InsertPoem(checkOnly, failurePoint);
		};
		const FbeStructure::StructuralOperationResult checkResult = apply(true);
		MSHTML::IHTMLTxtRangePtr selectionAfterCheck(m_doc->m_body.Document()->selection->createRange());
		const bool checkDomUnchanged = before == CString((const wchar_t*)body->innerHTML);
		const bool checkSelectionUnchanged = selectionAfterCheck &&
			range->compareEndPoints(L"StartToStart", selectionAfterCheck) == 0 &&
			range->compareEndPoints(L"EndToEnd", selectionAfterCheck) == 0;
		const bool checkDirtyUnchanged = dirtyBefore == m_doc->DocChanged();
		const FbeStructure::StructuralOperationResult applyResult = apply(false);
		const bool checkAllowed = checkResult.IsApplied();
		const bool applied = applyResult.IsApplied();
		const CString after((const wchar_t*)body->innerHTML);
		if (citePoemFaultLength) {
			MSHTML::IHTMLTxtRangePtr selectionAfterApply(m_doc->m_body.Document()->selection->createRange());
			const bool selectionUnchanged = selectionAfterApply &&
				range->compareEndPoints(L"StartToStart", selectionAfterApply) == 0 &&
				range->compareEndPoints(L"EndToEnd", selectionAfterApply) == 0;
			const bool observedChanged = before != after;
			const bool dirtyChanged = dirtyBefore != m_doc->DocChanged();
			BOOL handled = FALSE;
			if (observedChanged) m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			const bool expectedChanged = wcscmp(citePoemFault, L"before-mutation") != 0;
			const bool restored = before == CString((const wchar_t*)body->innerHTML);
			const bool passed = checkResult.IsApplied() && checkDomUnchanged && checkSelectionUnchanged && checkDirtyUnchanged &&
				applyResult.HasTechnicalFailure() && applyResult.error == E_FAIL && observedChanged == expectedChanged &&
				applyResult.documentChanged == observedChanged && (expectedChanged ? restored : selectionUnchanged && !dirtyChanged && restored);
			CStringA row;
			row.Format("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%ld\t%ld\t%d\t%d\t1\t1\t%ld\t0\t0\t0\t-\t0\t0\t0\t0\t%s\t%s\t%s\t0x%08lX\t%d\r\n", cite ? "cite" : "poem", (LPCSTR)targetName, (LPCSTR)selectionName, selectionCollapsed, (LPCSTR)selectionTextSummary, (LPCSTR)selectionHtmlSummary, (LPCSTR)selectionParentSummary, selectionStartToFirstStart, selectionEndToFirstEnd, checkAllowed, restored, beforeParagraphs, passed ? "pass" : "fail", checkResult.IsApplied() ? "applied" : checkResult.HasTechnicalFailure() ? "failed" : "not-applicable", applyResult.IsApplied() ? "applied" : applyResult.HasTechnicalFailure() ? "failed" : "not-applicable", static_cast<unsigned long>(applyResult.error), applyResult.documentChanged ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
		}
		if (!applied || before == after) {
			MSHTML::IHTMLTxtRangePtr selectionAfterApply(m_doc->m_body.Document()->selection->createRange());
			const bool applySelectionUnchanged = selectionAfterApply &&
				range->compareEndPoints(L"StartToStart", selectionAfterApply) == 0 &&
				range->compareEndPoints(L"EndToEnd", selectionAfterApply) == 0;
			const bool applyDomUnchanged = before == after;
			const bool applyDirtyUnchanged = dirtyBefore == m_doc->DocChanged();
			const bool genuineNotApplicable = applyResult.status == FbeStructure::StructuralOperationStatus::NotApplicable &&
				checkDomUnchanged && checkSelectionUnchanged && checkDirtyUnchanged &&
				applyDomUnchanged && applySelectionUnchanged && applyDirtyUnchanged;
			const char* reason = genuineNotApplicable ? "not-applicable" : "operation-failed";
			CStringA row;
			row.Format("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%ld\t%ld\t%d\t0\t0\t0\t%ld\t0\t0\t0\t-\t0\t0\t0\t0\t%s\t%s\t%s\t0x%08lX\t%d\r\n", cite ? "cite" : "poem", (LPCSTR)targetName, (LPCSTR)selectionName, selectionCollapsed, (LPCSTR)selectionTextSummary, (LPCSTR)selectionHtmlSummary, (LPCSTR)selectionParentSummary, selectionStartToFirstStart, selectionEndToFirstEnd, checkAllowed, beforeParagraphs, reason, checkResult.IsApplied() ? "applied" : checkResult.HasTechnicalFailure() ? "failed" : "not-applicable", applyResult.IsApplied() ? "applied" : applyResult.HasTechnicalFailure() ? "failed" : "not-applicable", static_cast<unsigned long>(applyResult.error), applyResult.documentChanged ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0;
		}
		BOOL handled = FALSE;
		m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
		const CString undo((const wchar_t*)body->innerHTML);
		m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
		const CString redo((const wchar_t*)body->innerHTML);
		auto countClass = [&](const wchar_t* className) -> long
		{
			long count = 0;
			MSHTML::IHTMLElementCollectionPtr divs(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV"));
			for (long index = 0; divs && index < divs->length; ++index) {
				MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
				if (div && U::scmp(div->className, className) == 0) ++count;
			}
			return count;
		};
		const long citeCount = countClass(L"cite"), poemCount = countClass(L"poem"), stanzaCount = countClass(L"stanza");
		CString poemText;
		MSHTML::IHTMLElementCollectionPtr poemElements(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV"));
		for (long index = 0; poemElements && index < poemElements->length; ++index) {
			MSHTML::IHTMLElementPtr poemElement(poemElements->item(_variant_t(index), _variant_t()));
			if (poemElement && U::scmp(poemElement->className, L"poem") == 0) { poemText = (const wchar_t*)poemElement->innerText; break; }
		}
		const CStringA poemTextSummary(utf16Summary(poemText));
		const long emptyDivsAfterRedo = countEmpty(L"DIV"), emptyParagraphsAfterRedo = countEmpty(L"P"), emptyStanzasAfterRedo = countEmpty(L"DIV", L"stanza");
		m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
		const CString restored((const wchar_t*)body->innerHTML);
		const long undoEmptyDivs = countEmpty(L"DIV"), undoEmptyParagraphs = countEmpty(L"P"), undoEmptyStanzas = countEmpty(L"DIV", L"stanza");
		const long emptyDivs = (emptyDivsAfterRedo > undoEmptyDivs ? emptyDivsAfterRedo : undoEmptyDivs) - beforeEmptyDivs;
		const long emptyParagraphs = (emptyParagraphsAfterRedo > undoEmptyParagraphs ? emptyParagraphsAfterRedo : undoEmptyParagraphs) - beforeEmptyParagraphs;
		const long emptyStanzas = (emptyStanzasAfterRedo > undoEmptyStanzas ? emptyStanzasAfterRedo : undoEmptyStanzas) - beforeEmptyStanzas;
		const bool undone = before == undo && before == restored;
		const bool redone = after == redo;
		bool sequential = true;
		if (repeat) {
			range->select();
			const bool secondApplied = apply(false).IsApplied();
			const CString secondAfter((const wchar_t*)body->innerHTML);
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			const CString secondUndo((const wchar_t*)body->innerHTML);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
			const CString secondRedo((const wchar_t*)body->innerHTML);
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			const CString secondRestored((const wchar_t*)body->innerHTML);
			sequential = secondApplied && before == secondUndo && secondAfter == secondRedo && before == secondRestored;
		}
		const bool structure = cite ? citeCount == 1 && poemCount == 0 : poemCount == 1 && stanzaCount >= 1;
		// The final Undo proves restoration of the original DOM.  Persist the
		// operation result after a fresh Redo: an intentionally empty fixture is
		// not itself a valid FictionBook section and must not open a validation UI.
		m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
		const bool redoForSave = after == CString((const wchar_t*)body->innerHTML);
		int validationLine = 0, validationColumn = 0;
		const bool saved = redoForSave && m_doc->Validate(validationLine, validationColumn) && m_doc->Save();
		const bool passed = undone && redone && sequential && structure && emptyDivs == 0 && emptyParagraphs == 0 && emptyStanzas == 0 && saved;
		CStringA row;
		row.Format("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%ld\t%ld\t%d\t%d\t%d\t%d\t%ld\t%ld\t%ld\t%ld\t%s\t%ld\t%ld\t%ld\t%d\t%s\t%s\t%s\t0x%08lX\t%d\r\n", cite ? "cite" : "poem", (LPCSTR)targetName, (LPCSTR)selectionName, selectionCollapsed, (LPCSTR)selectionTextSummary, (LPCSTR)selectionHtmlSummary, (LPCSTR)selectionParentSummary, selectionStartToFirstStart, selectionEndToFirstEnd, checkAllowed, undone, redone, sequential,
			beforeParagraphs, citeCount, poemCount, stanzaCount, (LPCSTR)poemTextSummary, emptyDivs, emptyParagraphs, emptyStanzas, saved, passed ? "pass" : "fail", checkResult.IsApplied() ? "applied" : checkResult.HasTechnicalFailure() ? "failed" : "not-applicable", applyResult.IsApplied() ? "applied" : applyResult.HasTechnicalFailure() ? "failed" : "not-applicable", static_cast<unsigned long>(applyResult.error), applyResult.documentChanged ? 1 : 0);
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
	}
