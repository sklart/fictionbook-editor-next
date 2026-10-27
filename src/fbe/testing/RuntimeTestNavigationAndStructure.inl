	if (IsFbeTestScenario(L"drop-native-manual"))
	{
		// This fixture intentionally contains a BR. Recovery serialization after
		// two minutes would normalize it while waiting for the human gesture and
		// add an unrelated undo unit to the history under observation.
		KillTimer(RECOVERY_TIMER_ID);
		// No fireEvent or replacement editing command: wait for an actual mouse
		// move in this disposable FBE window, then inspect its native result.
		static CMainFrame* frame;
		static CString initial, before, neighbor, path;
		static ULONGLONG changedAt;
		static CComPtr<IOleUndoManager> nativeManager;
		static auto undoSummary = [](IOleUndoManager* manager) {
			CStringA text;
			DWORD state = 0; const HRESULT open = manager->GetOpenParentState(&state);
			text.AppendFormat("open=0x%08lX state=%lu", static_cast<unsigned long>(open), state);
			CComPtr<IEnumOleUndoUnits> units;
			if (SUCCEEDED(manager->EnumUndoable(&units)) && units) {
				for (unsigned count = 0; count < 20; ++count) {
					CComPtr<IOleUndoUnit> unit;
					if (units->Next(1, &unit, nullptr) != S_OK) break;
					CComBSTR name; unit->GetDescription(&name);
					text += " | "; text += CStringA(CW2A(name ? name.m_str : L"?", CP_UTF8));
				}
			}
			return text;
		};
		frame = this; changedAt = 0; path = AU::_ARGS.source_memory_benchmark_path;
		MSHTML::IHTMLDocument2Ptr dom(m_doc->m_body.Document());
		nativeManager.Release();
		IServiceProviderPtr services(dom);
		CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&nativeManager)));
		dom->parentWindow->execScript(L"document.all.fbw_body.innerHTML=\"<DIV class='body'><DIV class='section' id='source'><P id='drag'>MOVE</P><P>Source remains.</P></DIV><DIV class='section' id='target'><P id='drop'>Target text here.</P></DIV><DIV class='section' id='untouched'><P id='kept'>Keep<BR><EM>this formatting</EM> and <A href='#kept'>link</A>.</P></DIV></DIV>\";", L"JScript");
		MSHTML::IHTMLElementPtr editable(dom->all->item(L"fbw_body"));
		initial = static_cast<LPCWSTR>(editable->innerHTML);
		m_doc->MarkSavePoint();
		dom->parentWindow->execScript(L"window.external.BeginUndoUnit(document,'prior native edit');document.all.drop.insertAdjacentHTML('beforeEnd',' prior');window.external.EndUndoUnit(document);", L"JScript");
		before = static_cast<LPCWSTR>(editable->innerHTML);
		neighbor = static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(dom->all->item(L"untouched"))->outerHTML);
		::ShowWindow(m_hWnd, SW_RESTORE); ::SetForegroundWindow(m_hWnd); m_doc->m_body.SetFocus();
		dom->parentWindow->execScript(L"var dragRange=document.body.createTextRange();dragRange.moveToElementText(document.all.drag);dragRange.collapse(true);dragRange.moveEnd('character',4);dragRange.select();", L"JScript");
		// Observe through script variables only: writing diagnostic attributes to
		// the live DOM here would itself invalidate MSHTML's native undo history.
		dom->parentWindow->execScript(L"window.fbeNativeDropSeen=false;document.body.attachEvent('ondrop',function(){window.fbeNativeBefore=document.all.fbw_body.innerHTML;window.fbeNativeNeighbor=document.all.untouched.outerHTML;window.fbeNativeDropSeen=true;});document.body.attachEvent('onkeydown',function(){window.event.returnValue=false;});", L"JScript");
		const CStringA ready(CStringA("ready: use the mouse to drag MOVE into Target text here.\r\n") + undoSummary(nativeManager) + "\r\n");
		DWORD written = 0; output.Write(ready, ready.GetLength(), &written); output.Flush(); output.Close();
		::SetTimer(nullptr, 0, 200, [](HWND, UINT, UINT_PTR timer, DWORD) {
			MSHTML::IHTMLDocument2Ptr document(frame->m_doc->m_body.Document());
			CComDispatchDriver observer(frame->m_doc->m_body.Script());
			_variant_t observed, observedBefore, observedNeighbor;
			if (FAILED(observer.GetPropertyByName(L"fbeNativeDropSeen", &observed)) || observed.vt != VT_BOOL || observed.boolVal != VARIANT_TRUE) return;
			if (FAILED(observer.GetPropertyByName(L"fbeNativeBefore", &observedBefore)) || FAILED(observer.GetPropertyByName(L"fbeNativeNeighbor", &observedNeighbor))) return;
			before = static_cast<LPCWSTR>(_bstr_t(observedBefore));
			neighbor = static_cast<LPCWSTR>(_bstr_t(observedNeighbor));
			MSHTML::IHTMLElementPtr body(document->all->item(L"fbw_body"));
			CString after(static_cast<LPCWSTR>(body->innerHTML));
			if (after == before) { changedAt = 0; return; }
			if (!changedAt) { changedAt = ::GetTickCount64(); return; }
			if (::GetTickCount64() - changedAt < 2000) return;
			::KillTimer(nullptr, timer);
			const CString source(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(document->all->item(L"source"))->innerText));
			const CString target(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(document->all->item(L"target"))->innerText));
			const bool moved = source.Find(L"MOVE") < 0 && target.Find(L"MOVE") >= 0;
			const bool scoped = CString(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(document->all->item(L"untouched"))->outerHTML)) == neighbor;
			const bool dirty = frame->m_doc->DocChanged();
			const CStringA beforeUndo(undoSummary(nativeManager));
			frame->m_doc->m_body.SetFocus();
			IOleCommandTargetPtr commands(frame->m_doc->m_body.Browser());
			const HRESULT firstUndo = commands->Exec(&CGID_MSHTML, IDM_UNDO, 0, nullptr, nullptr);
			const CString undoHtml(static_cast<LPCWSTR>(body->innerHTML));
			const bool undo = undoHtml == before;
			frame->m_doc->m_body.ExecCommand(IDM_UNDO);
			const CString previousHtml(static_cast<LPCWSTR>(body->innerHTML));
			const bool previous = previousHtml == initial;
			frame->m_doc->m_body.ExecCommand(IDM_REDO);
			const bool redoPrevious = CString(static_cast<LPCWSTR>(body->innerHTML)) == before;
			frame->m_doc->m_body.ExecCommand(IDM_REDO);
			const bool redo = CString(static_cast<LPCWSTR>(body->innerHTML)) == after;
			const bool passed = moved && scoped && dirty && undo && previous && redoPrevious && redo;
			CStringA report; report.Format("native-drop=%s\r\nmoved=%d scoped=%d dirty=%d undo=%d previous=%d redo-previous=%d redo=%d\r\n", passed ? "pass" : "fail", moved, scoped, dirty, undo, previous, redoPrevious, redo);
			report += "history-before-undo="; report += beforeUndo;
			report.AppendFormat("\r\nundo-command=0x%08lX\r\n", static_cast<unsigned long>(firstUndo));
			if (!passed) {
				report += "before="; report += CStringA(CW2A(before, CP_UTF8));
				report += "\r\nafter="; report += CStringA(CW2A(after, CP_UTF8));
				report += "\r\nundo="; report += CStringA(CW2A(undoHtml, CP_UTF8));
				report += "\r\nprevious="; report += CStringA(CW2A(previousHtml, CP_UTF8)); report += "\r\n";
			}
			WritePortableStateTestText(path, report);
			::PostQuitMessage(passed ? 0 : 1);
		});
		return 0;
	}
	if (IsFbeTestScenario(L"drop-files-runtime") || IsFbeTestScenario(L"undo-atomic-failures"))
	{
		LegacyTodoTest::MessageProbe messages;
		const CString originalFile(m_doc->m_filename);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		CComPtr<IOleUndoManager> undoManager;
		IServiceProviderPtr undoProvider(document);
		CheckError(undoProvider->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&undoManager)));
		auto closedGroup = [&]() { DWORD state = 0; return undoManager->GetOpenParentState(&state) == S_FALSE; };
		auto drop = [&](const std::vector<CString>& files) {
			size_t chars = 1;
			for (const CString& file : files) chars += static_cast<size_t>(file.GetLength()) + 1;
			HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPFILES) + chars * sizeof(wchar_t));
			DROPFILES* data = memory ? static_cast<DROPFILES*>(::GlobalLock(memory)) : nullptr;
			if (!data) { if (memory) ::GlobalFree(memory); return false; }
			data->pFiles = sizeof(DROPFILES); data->fWide = TRUE;
			wchar_t* target = reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(data) + sizeof(DROPFILES));
			for (const CString& file : files) { wcscpy_s(target, file.GetLength() + 1, file); target += file.GetLength() + 1; }
			::GlobalUnlock(memory);
			BOOL handled = FALSE; OnDropFiles(WM_DROPFILES, reinterpret_cast<WPARAM>(memory), 0, handled);
			return true; // OnDropFiles owns DragFinish, including all early returns.
		};
		auto bodyHtml = [&]() { return CString(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(document->all->item(L"fbw_body"))->innerHTML)); };
		auto binaryState = [&]() {
			CString state;
			MSHTML::IHTMLDOMNodePtr parent(document->all->item(L"binobj"));
			for (MSHTML::IHTMLDOMNodePtr node(parent->firstChild); node; node = node->nextSibling) {
				MSHTML::IHTMLElementPtr element(node);
				if (!element || U::scmp(element->tagName, L"DIV") != 0) continue;
				MSHTML::IHTMLElementCollectionPtr fields(element->all);
				MSHTML::IHTMLInputElementPtr id(fields->item(L"id")), type(fields->item(L"type"));
				state += CString(static_cast<LPCWSTR>(id->value)) + L"|" + CString(static_cast<LPCWSTR>(type->value)) + L"|";
				CComDispatchDriver dispatch(element);
				_variant_t data; CheckError(dispatch.GetPropertyByName(L"base64data", &data));
				if (data.vt != (VT_ARRAY | VT_UI1) || !data.parray || SafeArrayGetDim(data.parray) != 1) return CString(L"<invalid-data>");
				long first = 0, last = -1;
				CheckError(SafeArrayGetLBound(data.parray, 1, &first)); CheckError(SafeArrayGetUBound(data.parray, 1, &last));
				const long count = last - first + 1;
				if (count < 0 || count > INT_MAX / 2) return CString(L"<invalid-size>");
				CString hex; wchar_t* text = hex.GetBuffer(count * 2);
				BYTE* bytes = nullptr; CheckError(SafeArrayAccessData(data.parray, reinterpret_cast<void**>(&bytes)));
				const wchar_t* digits = L"0123456789abcdef";
				for (long i = 0; i < count; ++i) { text[2 * i] = digits[bytes[i] >> 4]; text[2 * i + 1] = digits[bytes[i] & 15]; }
				CheckError(SafeArrayUnaccessData(data.parray)); hex.ReleaseBuffer(count * 2);
				state += hex + L"\n";
			}
			return state;
		};
		auto imageOrder = [&]() {
			CString order;
			MSHTML::IHTMLElementPtr body(document->all->item(L"fbw_body"));
			MSHTML::IHTMLElementCollectionPtr elements(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV"));
			for (long i = 0; elements && i < elements->length; ++i) {
				MSHTML::IHTMLElementPtr element(elements->item(i));
				if (element && U::scmp(element->className, L"image") == 0) order += CString(static_cast<LPCWSTR>(_bstr_t(element->getAttribute(L"href", 2)))) + L"|";
			}
			return order;
		};
		auto prepareCaret = [&]() {
			m_doc->m_body.SetFocus();
			MSHTML::IHTMLElementPtr editable(document->all->item(L"fbw_body"));
			MSHTML::IHTMLElementCollectionPtr paragraphs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"P"));
			MSHTML::IHTMLElementPtr paragraph(paragraphs->item(paragraphs->length - 1));
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
			range->moveToElementText(paragraph); range->collapse(VARIANT_TRUE);
			range->moveStart(L"character", 1); range->collapse(VARIANT_TRUE); range->select();
		};
		bool passed = messages.timer != 0;
		CStringA details;
		auto check = [&](const char* name, bool result) { CStringA row; row.Format("%s=%d\r\n", name, result); details += row; passed = result && passed; };
		if (IsFbeTestScenario(L"undo-atomic-failures")) {
			KillTimer(RECOVERY_TIMER_ID);
			MSHTML::IHTMLWindow2Ptr window(document->parentWindow);
			auto script = [&](LPCWSTR value) { CheckError(window->execScript(value, L"JScript")); };
			auto state = [&]() {
				CString result(bodyHtml());
				MSHTML::IHTMLElementCollectionPtr elements(document->all);
				for (long i = 0; i < elements->length; ++i) {
					IDispatchPtr element(elements->item(i));
					CComQIPtr<MSHTML::IHTMLInputElement> input(element); CComQIPtr<MSHTML::IHTMLSelectElement> select(element);
					if (input) result += L"\n" + CString(static_cast<LPCWSTR>(input->value));
					if (select) result += L"\n" + CString(static_cast<LPCWSTR>(select->value));
				}
				// Derived cover options are compared by text and target, not by
				// MSHTML's implicit/default-selected serialization flags.
				MSHTML::IHTMLElementCollectionPtr options(MSHTML::IHTMLElement2Ptr(document->body)->getElementsByTagName(L"OPTION"));
				for (long i = 0; i < options->length; ++i) {
					MSHTML::IHTMLOptionElementPtr option(options->item(i));
					result += L"\n" + CString(static_cast<LPCWSTR>(option->value)) + L"|" + CString(static_cast<LPCWSTR>(option->text));
				}
				return result + L"\n" + binaryState();
			};
			auto history = [&]() {
				std::vector<CComPtr<IUnknown>> entries;
				for (int redo = 0; redo < 2; ++redo) {
					CComPtr<IEnumOleUndoUnits> list;
					CheckError(redo ? undoManager->EnumRedoable(&list) : undoManager->EnumUndoable(&list));
					entries.push_back(nullptr);
					for (;;) { CComPtr<IOleUndoUnit> unit; const HRESULT hr = list->Next(1, &unit, nullptr); if (hr == S_FALSE) break; CheckError(hr); CComPtr<IUnknown> identity; CheckError(unit->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity))); entries.push_back(identity); }
				}
				return entries;
			};
			script(L"document.all.fbw_body.innerHTML=\"<DIV class='body'><DIV class='section' id='owner'><P id='prior'>Prior</P><P>Anchor text</P></DIV></DIV>\";");
			const CString initial(state());
			m_doc->MarkSavePoint(); m_doc->MarkDocCP();
			script(L"window.external.BeginUndoUnit(document,'prior one');document.all.prior.insertAdjacentHTML('beforeEnd',' one');window.external.EndUndoUnit(document);window.external.BeginUndoUnit(document,'prior two');document.all.prior.insertAdjacentHTML('beforeEnd',' two');window.external.EndUndoUnit(document);");
			const CString priorState(state());
			prepareCaret(); check("image-one", SUCCEEDED(m_doc->m_body.AddImage(originalFile + L".first.png")));
			const CString firstState(state());
			prepareCaret(); check("image-two", SUCCEEDED(m_doc->m_body.AddImage(originalFile + L".second.png")));
			const CString secondState(state());
			m_doc->m_body.ExecCommand(IDM_UNDO); check("seed-existing-redo", state() == firstState);
			m_doc->MarkSavePoint(); m_doc->MarkDocCP();
            // The helper must accept NULL even with retained entries, without
            // executing them, recording inverses or changing either stack.
            {
                CComObject<FbeUndo::RestoreRedo>* raw = nullptr;
                CheckError(CComObject<FbeUndo::RestoreRedo>::CreateInstance(&raw));
                CComPtr<IOleUndoUnit> replay(raw);
                const auto oldHistory = history();
                const CString oldState(state());
                check("restore-redo-null-empty", replay->Do(nullptr) == S_OK);
                for (int redo = 0; redo < 2; ++redo) {
                    CComPtr<IEnumOleUndoUnits> list;
                    CheckError(redo ? undoManager->EnumRedoable(&list) : undoManager->EnumUndoable(&list));
                    for (;;) { CComPtr<IOleUndoUnit> unit; const HRESULT hr = list->Next(1, &unit, nullptr); if (hr == S_FALSE) break; CheckError(hr); raw->units.push_back(unit); }
                }
                const auto retained = raw->units;
                check("restore-redo-null-retained", retained.size() > 1 && replay->Do(nullptr) == S_OK &&
                    replay->Do(nullptr) == S_OK && raw->units == retained &&
                    history() == oldHistory && state() == oldState && closedGroup());
            }
            auto failure = [&](LPCWSTR point, bool redo) {
				script(L"var caret=document.body.createTextRange();caret.moveToElementText(document.all.prior);caret.collapse(true);caret.moveEnd('character',3);caret.select();");
				const CString oldState(state()), oldFull(static_cast<LPCWSTR>(document->body->outerHTML)); const auto oldHistory = history();
				const bool dirty = m_doc->DocChanged(), recovery = m_doc->DocRelChanged();
				::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", point);
				const HRESULT hr = static_cast<HRESULT>(m_doc->m_body.ExecCommand(redo ? IDM_REDO : IDM_UNDO));
				wchar_t unused[80] = {}; const bool consumed = !::GetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", unused, _countof(unused));
				::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", nullptr);
				CStringA label{CW2A(point)}; label += redo ? "-redo" : "-undo";
				const bool intact = FAILED(hr) && consumed && state() == oldState && history() == oldHistory && closedGroup() &&
					m_doc->DocChanged() == dirty && m_doc->DocRelChanged() == recovery &&
					CString(static_cast<LPCWSTR>(MSHTML::IHTMLTxtRangePtr(document->selection->createRange())->text)) == L"Pri";
				check(label, intact);
				details.AppendFormat("command-hr=0x%08lX\r\n", static_cast<unsigned long>(hr));
				if (!intact) {
					details.AppendFormat("failure-hr=0x%08lX consumed=%d document=%d history=%d closed=%d dirty=%d recovery=%d\r\n", static_cast<unsigned long>(hr), consumed, state() == oldState, history() == oldHistory, closedGroup(), m_doc->DocChanged() == dirty, m_doc->DocRelChanged() == recovery);
					if (consumed) {
						WritePortableStateTestText(originalFile + L".before.html", CStringA(CW2A(oldFull, CP_UTF8)));
						WritePortableStateTestText(originalFile + L".after.html", CStringA(CW2A(document->body->outerHTML, CP_UTF8)));
					}
				}
			};
			const wchar_t* shared[] = {L"binary-rebuild", L"binary-covers", L"binary-close", L"binary-add", L"html-close", L"html-add"};
			for (LPCWSTR point : shared) failure(point, false);
            const wchar_t* memoryFailures[] = {L"binary-oom", L"binary-com-oom", L"binary-atl-oom", L"binary-covers-oom"};
            auto binaryExceptions = [&](bool redo) {
                for (LPCWSTR point : memoryFailures) failure(point, redo);
                failure(L"binary-exception", redo);
            };
            binaryExceptions(false);
			failure(L"html-remove", false);
			// Real IDispatch exceptions after cache mutation, not only injected HRESULTs.
			for (LPCWSTR function : {L"RebuildImagesInfo", L"FillCoverList"}) {
				CString code; code.Format(L"var savedCacheFunction=%s;%s=(function(original){var first=true;return function(){original();if(first){first=false;throw new Error('undo cache failure');}};})(savedCacheFunction);", function, function); script(code);
				const CString oldState(state()); const auto oldHistory = history();
				const HRESULT hr = static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO));
				code.Format(L"%s=savedCacheFunction;", function); script(code);
				check("actual-cache-COM-failure", FAILED(hr) && state() == oldState && history() == oldHistory && closedGroup());
			}
			m_doc->m_body.ExecCommand(IDM_UNDO); check("retry-undo", state() == priorState);
			for (LPCWSTR point : shared) failure(point, true);
            binaryExceptions(true);
			failure(L"html-insert", true);
			m_doc->m_body.ExecCommand(IDM_REDO); check("retry-redo-one", state() == firstState);
			m_doc->m_body.ExecCommand(IDM_REDO); check("retry-redo-two", state() == secondState);
			m_doc->m_body.ExecCommand(IDM_UNDO); m_doc->m_body.ExecCommand(IDM_UNDO);
			m_doc->m_body.ExecCommand(IDM_UNDO); m_doc->m_body.ExecCommand(IDM_UNDO);
			check("previous-native-history", state() == initial);
			for (int i = 0; i < 4; ++i) m_doc->m_body.ExecCommand(IDM_REDO);
			check("complete-redo-history", state() == secondState);
			script(L"window.external.BeginUndoUnit(document,'normalizer fixture');document.all.owner.insertAdjacentHTML('beforeEnd','<P><SPAN>one</SPAN><SPAN>two</SPAN><BR>three</P>');window.external.EndUndoUnit(document);");
			for (LPCWSTR point : {L"normalize-remove", L"normalize-merge", L"normalize-structure", L"normalize-links", L"normalize-live", L"normalize-close", L"normalize-add"}) {
				const CString oldState(state()); const auto oldHistory = history(); const bool dirty = m_doc->DocChanged();
				::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", point);
				HRESULT hr = S_OK;
				try { m_doc->m_body.NormalizeScope(MSHTML::IHTMLDOMNodePtr(document->all->item(L"owner"))); } catch (const _com_error& error) { hr = error.Error(); }
				::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", nullptr);
				check(CStringA(CW2A(point)), FAILED(hr) && state() == oldState && history() == oldHistory && closedGroup() && m_doc->DocChanged() == dirty);
				details.AppendFormat("normalize-hr=0x%08lX document=%d history=%d closed=%d\r\n", static_cast<unsigned long>(hr), state() == oldState, history() == oldHistory, closedGroup());
				if (state() != oldState) { WritePortableStateTestText(originalFile + L".normalize-before.html", CStringA(CW2A(oldState, CP_UTF8))); WritePortableStateTestText(originalFile + L".normalize-after.html", CStringA(CW2A(state(), CP_UTF8))); }
			}
			const CString savedBinaries(binaryState());
			int line = 0, column = 0; check("validate", m_doc->Validate(line, column));
			const CString savedXml(static_cast<LPCWSTR>(m_doc->CreateDOM(L"utf-8")->xml));
			check("save", m_doc->Save()); check("reopen", LoadFile(originalFile) == OK);
			document = m_doc->m_body.Document();
			check("reopened-complete-document", CString(static_cast<LPCWSTR>(m_doc->CreateDOM(L"utf-8")->xml)) == savedXml);
			check("reopened-binary-bytes", binaryState() == savedBinaries);
			check("reopened-image-links", imageOrder().Find(L".first.png") >= 0 && imageOrder().Find(L".second.png") >= 0);
			CStringA report(passed ? "undo-atomic=pass\r\n" : "undo-atomic=fail\r\n"); report += details;
			DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Flush(); output.Close();
			::PostQuitMessage(passed ? 0 : 1); return 0;
		}
		// Establish a real text caret as in the existing import runtime harness.
		::ShowWindow(m_hWnd, SW_RESTORE);
		::SetForegroundWindow(m_hWnd);
		prepareCaret();
		const CString before(bodyHtml());
		const CString binariesBefore(binaryState());
		m_doc->MarkSavePoint(); m_doc->MarkDocCP();
		check("two-files-dispatch", drop({originalFile + L".first.png", originalFile + L".second.png"}));
		check("batch-group-closed", closedGroup());
		CString order(imageOrder());
		check("order", order.Find(L".first.png") >= 0 && order.Find(L".second.png") > order.Find(L".first.png"));
		const CString after(bodyHtml());
		const CString binariesAfter(binaryState());
		check("two-new-binaries", binariesAfter != binariesBefore && binariesAfter.Find(L".first.png") >= 0 && binariesAfter.Find(L".second.png") >= 0);
		m_doc->m_body.ExecCommand(IDM_UNDO); check("undo", bodyHtml() == before && binaryState() == binariesBefore);
		if (bodyHtml() != before || binaryState() != binariesBefore) {
			details += "undo-html: "; details += CStringA(CW2A(bodyHtml(), CP_UTF8)); details += "\r\nundo-binary: "; details += CStringA(CW2A(binaryState(), CP_UTF8)); details += "\r\nexpected-html: "; details += CStringA(CW2A(before, CP_UTF8)); details += "\r\n";
		}
		check("clean-after-undo", !m_doc->DocChanged());
		m_doc->m_body.ExecCommand(IDM_REDO); check("redo", bodyHtml() == after && binaryState() == binariesAfter);
		check("dirty-after-redo", m_doc->DocChanged());
		m_doc->MarkSavePoint();
		m_doc->m_body.ExecCommand(IDM_UNDO); check("undo-from-savepoint", m_doc->DocChanged() && bodyHtml() == before && binaryState() == binariesBefore);
		m_doc->m_body.ExecCommand(IDM_REDO); check("redo-to-savepoint", !m_doc->DocChanged() && bodyHtml() == after && binaryState() == binariesAfter);
		// Exercise failures after binary creation, including a partially changed
		// paragraph before the script throws. The next image must still succeed.
		MSHTML::IHTMLWindow2Ptr window(document->parentWindow);
		const wchar_t* failures[] = { L"return null;", L"throw new Error('before insert');", L"original(false,id); return null;", L"original(false,id); throw new Error('after insert');" };
		for (const wchar_t* failure : failures) {
			prepareCaret();
			const CString oldHtml(bodyHtml()), oldBinaries(binaryState());
			CString js; js.Format(L"var savedInsImage=InsImage;InsImage=(function(original){return function(check,id){if(check)return original(check,id);%s};})(savedInsImage);", failure);
			window->execScript(_bstr_t(js), L"JScript");
			MSHTML::IHTMLElementPtr inserted;
			const HRESULT failed = m_doc->m_body.AddImage(originalFile + L".first.png", false, std::addressof(inserted));
			window->execScript(L"InsImage=savedInsImage;", L"JScript");
			check("insert-failure-atomic", FAILED(failed) && !inserted && bodyHtml() == oldHtml && binaryState() == oldBinaries);
			check("error-group-closed", closedGroup());
			if (bodyHtml() != oldHtml || binaryState() != oldBinaries) details.AppendFormat("failure: html=%d binary=%d\r\n", bodyHtml() == oldHtml, binaryState() == oldBinaries);
		}
		// Fail only the middle member of a real shell batch, after native DOM
		// insertion; successful members and their binary data stay ordered.
		prepareCaret();
		window->execScript(L"var savedInsImage=InsImage;InsImage=function(check,id){if(check)return savedInsImage(check,id);var image=savedInsImage(check,id);if(String(id).indexOf('.failed.png')>=0)throw new Error('middle image');return image;};", L"JScript");
		const CString preBatchHtml(bodyHtml()), preBatchBinaries(binaryState());
		drop({originalFile + L".first.png", originalFile + L".failed.png", originalFile + L".second.png"});
		window->execScript(L"InsImage=savedInsImage;", L"JScript");
		const CString postBatchHtml(bodyHtml()), postBatchBinaries(binaryState());
		check("middle-failure-no-orphan", postBatchBinaries.Find(L".failed.png") < 0 && postBatchHtml.Find(L".failed.png") < 0 && postBatchBinaries != preBatchBinaries);
		m_doc->m_body.ExecCommand(IDM_UNDO); check("partial-batch-undo", bodyHtml() == preBatchHtml && binaryState() == preBatchBinaries);
		m_doc->m_body.ExecCommand(IDM_REDO); check("partial-batch-redo", bodyHtml() == postBatchHtml && binaryState() == postBatchBinaries);
		// Return to the first successful batch before the original contour.
		m_doc->m_body.ExecCommand(IDM_UNDO);
		const int warnings = LegacyTodoTest::messageCount;
		check("unsupported", drop({originalFile + L".unsupported.txt"}) && bodyHtml() == after && LegacyTodoTest::messageCount == warnings + 1);
		prepareCaret();
		check("bad-then-valid", drop({originalFile + L".bad.png", originalFile + L".third.png"}) && LegacyTodoTest::messageCount == warnings + 2 && imageOrder().Find(L".third.png") >= 0);
		prepareCaret();
		check("single", drop({originalFile + L".single.png"}) && imageOrder().Find(L".single.png") >= 0);
		const CString beforeMixed(bodyHtml());
		check("mixed", drop({originalFile, originalFile + L".first.png"}) && bodyHtml() == beforeMixed && m_doc->m_filename == originalFile && LegacyTodoTest::messageCount == warnings + 3);
		int validationLine = 0, validationColumn = 0;
		check("validate", m_doc->Validate(validationLine, validationColumn));
		const CString savedBinaries(binaryState());
		check("save", m_doc->Save());
		check("reopen", LoadFile(originalFile) == OK);
		document = m_doc->m_body.Document();
		check("reopened-binary-data", binaryState() == savedBinaries);
		order = imageOrder();
		check("saved-images", order.Find(L".first.png") >= 0 && order.Find(L".second.png") > order.Find(L".first.png") && order.Find(L".third.png") >= 0 && order.Find(L".single.png") >= 0);
		// Exercise the established singleton open route from a clean book;
		// the normal dirty-book confirmation remains owned by LoadFile.
		m_doc->MarkSavePoint();
		check("single-fb2", drop({originalFile + L".other.fb2"}) && m_doc->m_filename == originalFile + L".other.fb2");
		CStringA report; report.Format("drop-files=%s\r\nmessages=%d\r\n", passed ? "pass" : "fail", LegacyTodoTest::messageCount);
		report += details; report += CStringA(CW2A(LegacyTodoTest::messageText, CP_UTF8));
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"source-regex-diagnostics-runtime"))
	{
		ShowView(SOURCE);
		const bool passed = RunSourceRegexDiagnosticsForTest(&m_doc->m_body, m_source);
		const CStringA report(passed ? "source-regex=pass\r\n" : "source-regex=fail\r\n");
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"drop-normalization-runtime"))
	{
		LegacyTodoTest::MessageProbe messages;
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLWindow2Ptr window(document->parentWindow);
		auto script = [&](LPCWSTR text) { return SUCCEEDED(window->execScript(_bstr_t(text), _bstr_t(L"JScript"))); };
		auto html = [&]() { return CString(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(document->all->item(L"fbw_body"))->innerHTML)); };
		auto finish = [&]() {
			MSG message = {};
			while (::PeekMessage(&message, m_doc->m_body.m_hWnd, AU::WM_FINALIZE_DROP, AU::WM_FINALIZE_DROP, PM_REMOVE))
				::DispatchMessage(&message);
		};
		CStringA report("case\tresult\r\n");
		bool passed = CFBEView::VerifyDropUndoContractForTest();
		report += passed ? "ole-parent-contract\tpass\r\n" : "ole-parent-contract\tfail\r\n";
		for (int test = 0; test < 7; ++test)
		{
			const bool changes = test < 3 || test >= 5;
			script(L"document.all.fbw_body.innerHTML=\"<DIV class='body'><DIV class='section' id='source'><P id='drag'>move</P><P>remain</P></DIV><DIV class='section' id='target'><P id='drop'>target</P></DIV><DIV class='section' id='untouched'><P>keep<BR>this</P></DIV></DIV>\";");
			const CString untouchedBefore(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(document->all->item(L"untouched"))->outerHTML));
			const CString initial(html());
			m_doc->MarkSavePoint();
			script(L"window.external.BeginUndoUnit(document,'previous edit');var prior=document.body.createTextRange();prior.moveToElementText(document.all.drop);prior.collapse(false);prior.select();prior.pasteHTML(' prior');window.external.EndUndoUnit(document);");
			const CString before(html());
			const long version = m_doc->m_body.GetVersionNumber();
			// Real MSHTML event dispatch exercises the production connection point.
			// The controlled DOM mutation stands in for the browser's default drop;
			// the separate GUI probe establishes the native event ordering.
			if (test != 2) script(L"var r=document.body.createTextRange();r.moveToElementText(document.all.drag);r.select();document.all.drag.fireEvent('ondragstart');");
			if (test != 3)
			{
				if (test == 2) script(L"document.all.drop.fireEvent('ondragenter');document.all.drop.fireEvent('ondragover');");
				script(test == 0 ? L"document.all.drag.fireEvent('ondrop');" : L"document.all.drop.fireEvent('ondrop');");
				if (changes)
				{
					// A native browser drop creates an editing undo unit. Give the
					// controlled stand-in the same contract; unwrapped script DOM
					// mutations are deliberately not undoable in MSHTML.
					const CString mutation = test == 0
						? L"document.all.source.insertAdjacentHTML('beforeEnd','<P>new<BR>line</P>');"
						: L"document.all.target.insertAdjacentHTML('beforeEnd','<P>new<BR>line</P>');";
					CString operation(L"window.external.BeginUndoUnit(document,'controlled drop');");
					operation += mutation;
					if (test == 1) operation += L"document.all.drag.removeNode(true);";
					if (test == 5) operation += L"document.all.source.removeNode(true);";
					if (test == 6) operation += L"document.all.source.insertAdjacentHTML('beforeEnd','<P>source<BR>line</P>');";
					operation += L"window.external.EndUndoUnit(document);";
					script(operation);
				}
			}
			if (changes) script(test == 0 ? L"var p=document.all.source.lastChild;var r=document.body.createTextRange();r.moveToElementText(p);r.collapse(true);r.moveEnd('character',3);r.select();" : L"var p=document.all.target.lastChild;var r=document.body.createTextRange();r.moveToElementText(p);r.collapse(true);r.moveEnd('character',3);r.select();");
			const CString selectedBefore(static_cast<LPCWSTR>(MSHTML::IHTMLTxtRangePtr(document->selection->createRange())->text));
			const CString unnormalized(html());
			if (test == 6) ::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", L"normalize-target");
			if (test != 2) script(L"document.body.fireEvent('ondragend');");
			const CString selectedAfterEvent(static_cast<LPCWSTR>(MSHTML::IHTMLTxtRangePtr(document->selection->createRange())->text));
			finish();
			wchar_t fault[80] = {};
			const bool consumed = !::GetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", fault, _countof(fault));
			::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", nullptr);
			const CString after(html());
			if (test == 6) {
				bool ok = consumed && after == before;
				m_doc->m_body.ExecCommand(IDM_UNDO); ok = ok && html() == initial;
				m_doc->m_body.ExecCommand(IDM_REDO); ok = ok && html() == before;
				report += ok ? "normalization-two-owner-failure\tpass\r\n" : "normalization-two-owner-failure\tfail\r\n";
				passed = passed && ok; continue;
			}
			bool ok = true;
			bool normalized = true, selectionKept = true, scoped = true, undoOk = true, redoOk = true;
			if (changes)
			{
				const CString ownerHtml(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(document->all->item(test == 0 ? L"source" : L"target"))->innerHTML));
				const CString untouched(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(document->all->item(L"untouched"))->innerHTML));
				normalized = test == 6 ? consumed && after == unnormalized : ownerHtml.Find(L"<BR") < 0 && ownerHtml.Find(L"new") >= 0 && ownerHtml.Find(L"line") >= 0;
				ok = normalized;
				selectionKept = CString(static_cast<LPCWSTR>(MSHTML::IHTMLTxtRangePtr(document->selection->createRange())->text)) == selectedBefore;
				if (!selectionKept) report.AppendFormat("selection-before=%S event=%S after=%S\r\n", static_cast<LPCWSTR>(selectedBefore), static_cast<LPCWSTR>(selectedAfterEvent), static_cast<LPCWSTR>(MSHTML::IHTMLTxtRangePtr(document->selection->createRange())->text));
				ok = ok && selectionKept;
				scoped = untouched.Find(L"<BR") >= 0 &&
					CString(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(document->all->item(L"untouched"))->outerHTML)) == untouchedBefore;
				ok = ok && scoped;
				m_doc->m_body.ExecCommand(IDM_UNDO);
				undoOk = html() == before; ok = ok && undoOk;

				if (!undoOk) {
					report += "undo-before: "; report += CStringA(before); report += "\r\nundo-after: "; report += CStringA(html()); report += "\r\n";
				}
				m_doc->m_body.ExecCommand(IDM_REDO);
				redoOk = html() == after; ok = ok && redoOk;
				m_doc->m_body.ExecCommand(IDM_UNDO);
				m_doc->m_body.ExecCommand(IDM_UNDO);
				const bool history = html() == initial;
				m_doc->m_body.ExecCommand(IDM_REDO);
				const bool priorRestored = html() == before;
				m_doc->m_body.ExecCommand(IDM_REDO);
				ok = ok && history && priorRestored && html() == after;
				report.AppendFormat("previous-history=%d redo-history=%d dirty=%d\r\n", history, priorRestored, m_doc->DocChanged());
				ok = ok && m_doc->DocChanged();
				const long finishedVersion = m_doc->m_body.GetVersionNumber();
				finish();
				ok = ok && m_doc->m_body.GetVersionNumber() == finishedVersion;
			}
			else {
				ok = after == before && m_doc->m_body.GetVersionNumber() == version;
				// A cancelled external hover must discard its empty OLE parent and
				// leave the existing undo/redo stacks intact.
				script(L"document.all.drop.fireEvent('ondragenter');document.all.drop.fireEvent('ondragover');document.all.drop.fireEvent('ondragleave');");
				CComPtr<IOleUndoManager> manager;
				IServiceProviderPtr services(document);
				DWORD state = 0;
				ok = ok && SUCCEEDED(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager))) && manager->GetOpenParentState(&state) == S_FALSE;
				m_doc->m_body.ExecCommand(IDM_UNDO);
				ok = ok && html() == initial;
				script(L"var r=document.body.createTextRange();r.moveToElementText(document.all.drag);r.select();document.all.drag.fireEvent('ondragstart');");
				if (test == 4) script(L"document.all.drop.fireEvent('ondrop');");
				script(L"document.body.fireEvent('ondragend');"); finish();
				m_doc->m_body.ExecCommand(IDM_REDO);
				ok = ok && html() == before;
			}
			const char* names[] = { "local", "cross-section", "external", "cancel", "unchanged", "detached-source", "normalization-two-owner-failure" };
			CStringA row; row.Format("%s\t%s\r\n", names[test], ok ? "pass" : "fail"); report += row;
			CStringA detail; detail.Format("checks: normalize=%d selection=%d scope=%d undo=%d redo=%d version-before=%ld version-after=%ld\r\n", normalized, selectionKept, scoped, undoOk, redoOk, version, m_doc->m_body.GetVersionNumber()); report += detail;
			passed = passed && ok;
		}
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"visual-dom-normalizer"))
	{
		CStringA header("case\tparagraphs\tempty_divs\tbrs\texact_paragraphs\tempty_line\tnbsp\tformatting\tscope_isolated\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr body(document ? MSHTML::IHTMLElementPtr(document->body) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr editable(document ? MSHTML::IHTMLElementPtr(document->all->item(L"fbw_body")) : MSHTML::IHTMLElementPtr());
		if (!body || !editable)
		{
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		const CString originalHtml((const wchar_t*)editable->innerHTML);
		// The adjacent section deliberately contains a BR. A body-wide paste
		// normalization would rewrite it; a scoped paste must not.
		editable->innerHTML = L"<DIV class='section'><P>Before paste</P></DIV><DIV class='section'><P>Adjacent<BR>untouched</P></DIV>";
		const CString pastePayload(L"paste-alpha\u00A0bold\r\npaste-beta\r\n\r\npaste-gamma");
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
				MSHTML::IHTMLElementPtr initialParagraph(initialParagraphs && initialParagraphs->length ? MSHTML::IHTMLElementPtr(initialParagraphs->item(0L)) : MSHTML::IHTMLElementPtr());
				if (initialParagraph) { MSHTML::IHTMLTxtRangePtr pasteRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange()); pasteRange->moveToElementText(initialParagraph); pasteRange->collapse(VARIANT_FALSE); pasteRange->select(); BOOL pasteHandled = FALSE; m_doc->m_body.OnPaste(0, ID_EDIT_PASTE, 0, pasteHandled); }
				editable = document->all->item(L"fbw_body");
				const CString pastedHtml(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
				const CString pastedText(editable ? static_cast<LPCWSTR>(editable->innerText) : L"");
				const int alpha = pastedText.Find(L"paste-alpha"), beta = pastedText.Find(L"paste-beta"), gamma = pastedText.Find(L"paste-gamma");
				MSHTML::IHTMLElementCollectionPtr sections(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV"));
				MSHTML::IHTMLElementPtr pastedSection(sections && sections->length ? MSHTML::IHTMLElementPtr(sections->item(0L)) : MSHTML::IHTMLElementPtr());
				MSHTML::IHTMLElementPtr adjacent(sections && sections->length > 1 ? MSHTML::IHTMLElementPtr(sections->item(1L)) : MSHTML::IHTMLElementPtr());
				CString pastedSectionHtml(pastedSection ? static_cast<LPCWSTR>(pastedSection->innerHTML) : L""); pastedSectionHtml.MakeUpper();
				CString adjacentHtml(adjacent ? static_cast<LPCWSTR>(adjacent->innerHTML) : L""); adjacentHtml.MakeUpper();
				const bool scopeIsolated = adjacent && adjacentHtml.Find(L"<BR") >= 0 && pastedSection && pastedSectionHtml.Find(L"<BR") < 0;
				pasteNormalized = alpha >= 0 && beta > alpha && gamma > beta && (pastedHtml.Find(L"&nbsp;") >= 0 || pastedHtml.Find(L"\x00a0") >= 0) && scopeIsolated;
			}
			::OleSetClipboard(originalClipboard);
		}
		CStringA pasteRow; pasteRow.Format("paste-normal\t0\t0\t0\t%d\t%d\t%d\t%d\t%d\t%s\r\n", pasteNormalized ? 1 : 0, pasteNormalized ? 1 : 0, pasteNormalized ? 1 : 0, 1, pasteNormalized ? 1 : 0, pasteNormalized ? "pass" : "fail");
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
			row.Format("%S\t%ld\t%ld\t%ld\t%d\t%d\t%d\t%d\t1\t%s\r\n", testCase.name, countElements(L"P"), emptyDivs, countElements(L"BR"), exactParagraphs, emptyLine, nbsp, formatting, passed ? "pass" : "fail");
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
		}
		// Title containers have their own structural invariant: a title cannot
		// contain a title and a section cannot have competing direct titles.
		// Exercise the production normalizer twice to keep this rule idempotent.
		struct TitleNormalizerCase { const wchar_t* name; const wchar_t* html; const wchar_t* first; const wchar_t* second; };
		const TitleNormalizerCase titleCases[] = {
			{ L"title-single", L"<DIV class='section'><DIV class='title'><P>Single <STRONG>strong</STRONG> <EM>em</EM> <A href='#single'>link</A></P></DIV><P>Body</P></DIV>", L"Single strong em link", L"" },
			{ L"title-nested", L"<DIV class='section'><DIV class='title'><P>First <STRONG>strong</STRONG></P><DIV class='title'><P>Second <EM>em</EM> <A href='#nested'>link</A></P></DIV></DIV><P>Body</P></DIV>", L"First strong", L"Second em link" },
			{ L"title-direct-merge", L"<DIV class='section'><DIV class='title'><P>First <STRONG>strong</STRONG></P></DIV><DIV class='title'><P>Second <EM>em</EM> <A href='#direct'>link</A></P></DIV><P>Body</P></DIV>", L"First strong", L"Second em link" }
		};
		for (const TitleNormalizerCase& testCase : titleCases)
		{
			editable->innerHTML = testCase.html;
			MSHTML::IHTMLElementCollectionPtr sections(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV"));
			MSHTML::IHTMLElementPtr section(sections && sections->length ? MSHTML::IHTMLElementPtr(sections->item(0L)) : MSHTML::IHTMLElementPtr());
			FbeVisualDom::NormalizeStructure(document, MSHTML::IHTMLDOMNodePtr(section));
			editable = document->all->item(L"fbw_body");
			section = editable ? MSHTML::IHTMLElementPtr(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV")->item(0L)) : MSHTML::IHTMLElementPtr();
			const CString once(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
			FbeVisualDom::NormalizeStructure(document, MSHTML::IHTMLDOMNodePtr(section));
			editable = document->all->item(L"fbw_body");
			const CString twice(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
			section = editable ? MSHTML::IHTMLElementPtr(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV")->item(0L)) : MSHTML::IHTMLElementPtr();
			long directTitles = 0, nestedTitles = 0;
			for (MSHTML::IHTMLDOMNodePtr child(section ? MSHTML::IHTMLDOMNodePtr(section)->firstChild : MSHTML::IHTMLDOMNodePtr()); child; child = child->nextSibling) {
				MSHTML::IHTMLElementPtr element(child); if (element && U::scmp(element->tagName, L"DIV") == 0 && U::scmp(element->className, L"title") == 0) ++directTitles;
			}
			MSHTML::IHTMLElementCollectionPtr titles(section ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(section)->getElementsByTagName(L"DIV")) : MSHTML::IHTMLElementCollectionPtr());
			for (long index = 0; titles && index < titles->length; ++index) {
				MSHTML::IHTMLElementPtr title(titles->item(_variant_t(index), _variant_t())); MSHTML::IHTMLElementPtr parent(title ? MSHTML::IHTMLElementPtr(title->parentElement) : MSHTML::IHTMLElementPtr());
				if (title && parent && U::scmp(title->className, L"title") == 0 && U::scmp(parent->className, L"title") == 0) ++nestedTitles;
			}
			MSHTML::IHTMLElementPtr title(titles && titles->length ? MSHTML::IHTMLElementPtr(titles->item(0L)) : MSHTML::IHTMLElementPtr());
			const CString text(title ? static_cast<LPCWSTR>(title->innerText) : L"");
			const bool order = text.Find(testCase.first) >= 0 && (!*testCase.second || text.Find(testCase.second) > text.Find(testCase.first));
			const bool formatting = title && MSHTML::IHTMLElement2Ptr(title)->getElementsByTagName(L"STRONG")->length == 1 && MSHTML::IHTMLElement2Ptr(title)->getElementsByTagName(L"EM")->length == 1 && MSHTML::IHTMLElement2Ptr(title)->getElementsByTagName(L"A")->length == 1;
			const bool idempotent = once == twice;
			const bool passed = directTitles == 1 && nestedTitles == 0 && order && formatting && idempotent;
			allPassed = allPassed && passed;
			CStringA row; row.Format("%S\t0\t0\t0\t%d\t%d\t%d\t%d\t%d\t%s\r\n", testCase.name, order ? 1 : 0, nestedTitles == 0 ? 1 : 0, idempotent ? 1 : 0, formatting ? 1 : 0, directTitles == 1 ? 1 : 0, passed ? "pass" : "fail");
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
		}
		// Restore the fixture before exercising the ordinary production save path.
		editable->innerHTML = originalHtml.AllocSysString();
		int validationLine = 0, validationColumn = 0;
		const bool saved = m_doc->Validate(validationLine, validationColumn) && m_doc->Save();
		output.Flush(); output.Close();
		::PostQuitMessage(allPassed && saved ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"html-paste-runtime"))
	{
		DWORD written = 0;
		CStringA header("case\ttext\tformatting\tparagraphs\tneighbor\tclipboard\tundo\tredo\tselection\tdirty\trecovery\tvalidate\tsave\tresult\r\n");
		output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		wchar_t caseName[32] = {};
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_HTML_PASTE_CASE", caseName, _countof(caseName));
        const bool queueCase = wcsncmp(caseName, L"queue-", 6) == 0;
        const bool cleanQueueFailure = wcscmp(caseName, L"queue-post-clean-failure") == 0;
		const bool singleLine = wcscmp(caseName, L"plain-single") == 0;
		const bool middle = wcscmp(caseName, L"plain-middle") == 0;
		const bool internalEnd = wcscmp(caseName, L"internal-end") == 0;
		const bool preDirty = wcscmp(caseName, L"internal-dirty") == 0;
		const bool historyMiddle = wcscmp(caseName, L"internal-history-middle") == 0;
		const bool priorHistory = wcscmp(caseName, L"internal-history") == 0 || historyMiddle || (queueCase && !cleanQueueFailure);
		const bool missingLink = wcscmp(caseName, L"internal-missing-link") == 0;
		const bool missingImage = wcscmp(caseName, L"internal-missing-image") == 0;
		const bool internal = wcscmp(caseName, L"internal") == 0 || internalEnd || preDirty || priorHistory || missingLink || missingImage;
		const bool legacy = wcscmp(caseName, L"legacy-unmarked") == 0;
		const bool plain = wcscmp(caseName, L"plain") == 0 || singleLine || middle;
		const bool title = wcscmp(caseName, L"title") == 0;
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr body(document ? MSHTML::IHTMLElementPtr(document->body) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr editable(document ? MSHTML::IHTMLElementPtr(document->all->item(L"fbw_body")) : MSHTML::IHTMLElementPtr());
		bool textOk = false, formatting = false, paragraphs = false, neighborOk = false;
		bool clipboardOk = false, undoOk = false, redoOk = false, selectionOk = false, dirtyOk = false, recoveryOk = false, validated = false, saved = false;
		bool dirtyAfterPaste = false, cleanAfterUndo = false, dirtyAfterRedo = false;
		bool twoPasteOk = true, safetyOk = true;
		CStringA twoPasteTrace;
		CComPtr<IDataObject> originalClipboard;
		::OleGetClipboard(&originalClipboard);
		try
		{
			if (!body || !editable) throw _com_error(E_FAIL);
			if (internal) {
				MSHTML::IHTMLElementPtr sourceOwner(document->all->item(L"paste-target"));
				if (!sourceOwner) throw _com_error(E_FAIL);
				sourceOwner->insertAdjacentHTML(L"beforeEnd", L"<p id='paste-internal-target'>Paste target</p><p><span class='code'>Internal code</span> <a href='#paste-neighbor'>internal link</a></p>");
				if (missingLink) sourceOwner->insertAdjacentHTML(L"beforeEnd", L"<p><a class='note' href='#absent-note'>Missing note</a></p>");
				if (missingImage) sourceOwner->insertAdjacentHTML(L"beforeEnd", L"<p><span class='image' href='#absent-image'><img src='fbw-internal:#absent-image'></span></p>");
			}
			m_doc->MarkSavePoint();
			m_doc->MarkDocCP();
			const long initialVersion = m_doc->m_body.GetVersionNumber();
			if (m_doc->DocChanged() || m_doc->DocRelChanged()) throw _com_error(E_FAIL);
			CString before(static_cast<LPCWSTR>(editable->innerHTML));
			MSHTML::IHTMLElementPtr target(document->all->item(L"paste-target"));
			MSHTML::IHTMLElementPtr neighbor(document->all->item(L"paste-neighbor"));
			const CString neighborBefore(neighbor ? static_cast<LPCWSTR>(neighbor->outerHTML) : L"");
			MSHTML::IHTMLElementCollectionPtr ps(target ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(target)->getElementsByTagName(L"P")) : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr pasteTarget(internal ? MSHTML::IHTMLElementPtr(document->all->item(L"paste-internal-target")) :
				ps && ps->length > (title ? 0 : 1) ? MSHTML::IHTMLElementPtr(ps->item(title ? 0L : 1L)) : MSHTML::IHTMLElementPtr());
			if (!pasteTarget) throw _com_error(E_FAIL);
			if (preDirty) {
				pasteTarget->insertAdjacentText(L"beforeEnd", L" Prior change");
				if (!m_doc->DocChanged() || !m_doc->DocRelChanged()) throw _com_error(E_FAIL);
				before = static_cast<LPCWSTR>(editable->innerHTML);
			}
			CString beforeAfterEdit;
			if (priorHistory) {
				m_doc->m_body.MarkupServices()->BeginUndoUnit(L"Prior edit");
				pasteTarget->insertAdjacentText(L"beforeEnd", L" Prior change");
				m_doc->m_body.MarkupServices()->EndUndoUnit();
				beforeAfterEdit = static_cast<LPCWSTR>(editable->innerHTML);
				if (beforeAfterEdit == before || !m_doc->DocChanged() || !m_doc->DocRelChanged()) throw _com_error(E_FAIL);
			}
			const CStringA fragment(legacy ?
				"<div class='section'><p><a class='note' href='file:///C:/OldFBE/main.html#note1'>Legacy link</a></p></div>" :
				"<p class='MsoNormal' style='margin-left:36pt'><span style='font-weight:700;font-style:italic;color:red'>Bold italic</span>&nbsp;one</p><p>Second</p>");
			if (internal) {
				MSHTML::IHTMLElementPtr source(ps && ps->length ? MSHTML::IHTMLElementPtr(ps->item(ps->length - 1)) : MSHTML::IHTMLElementPtr());
				if (!source) throw _com_error(E_FAIL);
				MSHTML::IHTMLTxtRangePtr copyRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
				m_doc->m_body.SetFocus(); ::SetFocus(m_doc->m_body);
				copyRange->moveToElementText(source); copyRange->select();
				BOOL copied = FALSE;
				m_doc->m_body.OnCopy(0, ID_EDIT_COPY, 0, copied);
				CString copiedFragment;
				const bool fragmentRead = FbeClipboard::HtmlPasteSanitizer::ReadClipboardFragment(m_doc->m_body, copiedFragment);
				const bool safe = FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(
					FbeClipboard::HtmlPasteSanitizer::NormalizeInternalLinks(copiedFragment,
						U::UrlFromPath(U::GetProgDirFile(L"main.html"))));
				const bool marked = FbeClipboard::HtmlPasteSanitizer::HasInternalCopyMarker(m_doc->m_body);
				if (!fragmentRead || !safe || !marked) {
					CStringA diagnostic;
					diagnostic.Format("internal-copy-check: read=%d safe=%d marked=%d url=%S fragment=%S\r\n",
						fragmentRead, safe, marked, static_cast<LPCWSTR>(document->url), static_cast<LPCWSTR>(copiedFragment));
					output.Write(diagnostic, static_cast<DWORD>(diagnostic.GetLength()), &written);
					throw _com_error(E_FAIL);
				}
			}
			else {
			const CStringA prefix("Version:0.9\r\nStartFragment:00000000\r\nEndFragment:00000000\r\n");
			CStringA headerBytes;
			headerBytes.Format("Version:0.9\r\nStartFragment:%08u\r\nEndFragment:%08u\r\n", static_cast<unsigned>(prefix.GetLength()), static_cast<unsigned>(prefix.GetLength() + fragment.GetLength()));
			const CStringA payload(headerBytes + fragment);
			const CString plainText(singleLine ? L" Ж Ω" : L"Plain one\r\n\r\nPlain three");
			if (!::OpenClipboard(NULL)) throw _com_error(E_FAIL);
			::EmptyClipboard();
			const UINT format = ::RegisterClipboardFormat(L"HTML Format");
			HGLOBAL htmlMemory = ::GlobalAlloc(GMEM_MOVEABLE, payload.GetLength() + 1);
			HGLOBAL textMemory = ::GlobalAlloc(GMEM_MOVEABLE, (plainText.GetLength() + 1) * sizeof(wchar_t));
			char* htmlBytes = htmlMemory ? static_cast<char*>(::GlobalLock(htmlMemory)) : NULL;
			wchar_t* textBytes = textMemory ? static_cast<wchar_t*>(::GlobalLock(textMemory)) : NULL;
			if (htmlBytes) { memcpy(htmlBytes, static_cast<LPCSTR>(payload), payload.GetLength() + 1); ::GlobalUnlock(htmlMemory); }
			if (textBytes) { wcscpy_s(textBytes, plainText.GetLength() + 1, plainText); ::GlobalUnlock(textMemory); }
			const bool htmlReady = htmlBytes && ::SetClipboardData(format, htmlMemory);
			const bool textReady = textBytes && ::SetClipboardData(CF_UNICODETEXT, textMemory);
			if (!htmlReady && htmlMemory) ::GlobalFree(htmlMemory);
			if (!textReady && textMemory) ::GlobalFree(textMemory);
			::CloseClipboard();
			if (!htmlReady || !textReady) throw _com_error(E_FAIL);
			}
			const DWORD sequence = ::GetClipboardSequenceNumber();
			m_doc->m_body.SetFocus(); ::SetFocus(m_doc->m_body);
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
			range->moveToElementText(pasteTarget); range->collapse(VARIANT_FALSE);
			if (internal && !internalEnd) range->move(L"character", -1);
			if (middle || historyMiddle) range->move(L"character", -3);
			range->select();
			BOOL handled = FALSE;
			CComPtr<IOleUndoManager> pasteManager;
			IServiceProviderPtr services(document);
			CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&pasteManager)));
			auto closed = [&]() { DWORD state = 0; return pasteManager->GetOpenParentState(&state) == S_FALSE; };
			auto selectionState = [&]() {
				MSHTML::IHTMLTxtRangePtr selected(document->selection->createRange());
				MSHTML::IHTMLTxtRangePtr prefix(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
				prefix->moveToElementText(editable); prefix->setEndPoint(L"EndToStart", selected);
				const long start = _bstr_t(prefix->text).length();
				prefix->moveToElementText(editable); prefix->setEndPoint(L"EndToEnd", selected);
				CString result; result.Format(L"%ld:%u", start, _bstr_t(prefix->text).length());
				return result;
			};
			// The batch frame is hidden: GetActiveWnd's visibility fallback
			// selects Source. Deliver commands to the real Body message map.
			auto edit = [&](UINT command) {
				m_doc->m_body.SendMessage(WM_COMMAND, MAKEWPARAM(command, 0), 0);
				safetyOk = closed() && safetyOk;
			};
			auto paste = [&](WORD command) {
				wchar_t route[4] = {};
				const bool realPaste = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_HTML_REAL_PASTE", route, _countof(route)) == 1 && route[0] == L'1';
				if (!realPaste || command == ID_EDIT_PASTE_TEXT) m_doc->m_body.OnPaste(0, command, 0, handled);
				else {
					document->execCommand(L"Paste", VARIANT_FALSE, _variant_t());
					bool routed = false;
					MSG pending = {};
					while (::PeekMessage(&pending, m_doc->m_body, AU::WM_FINALIZE_HTML_PASTE, AU::WM_FINALIZE_HTML_PASTE, PM_REMOVE)) {
						routed = true;
						::DispatchMessage(&pending);
					}
					safetyOk = routed && safetyOk;
				}
				safetyOk = closed() && safetyOk;
			};
			auto saveReopen = [&]() {
				const CString file(m_doc->m_filename);
				if (!m_doc->Save()) return false;
				const CString xml(static_cast<LPCWSTR>(m_doc->CreateDOM(L"utf-8")->xml));
				if (LoadFile(file) != OK) return false;
				return CString(static_cast<LPCWSTR>(m_doc->CreateDOM(L"utf-8")->xml)) == xml;
			};
            auto undoHistory = [&]() {
                CComPtr<IEnumOleUndoUnits> list; CheckError(pasteManager->EnumUndoable(&list));
                std::vector<CComPtr<IOleUndoUnit>> units;
                for (;;) { CComPtr<IOleUndoUnit> unit; const HRESULT hr = list->Next(1, &unit, nullptr); if (hr == S_FALSE) break; CheckError(hr); units.push_back(unit); }
                return units;
            };
            const auto previousUndo = undoHistory();
			const CString beforeCaret(selectionState());
            if (queueCase) {
                auto redoHistory = [&]() {
                    CComPtr<IEnumOleUndoUnits> list; CheckError(pasteManager->EnumRedoable(&list));
                    std::vector<CComPtr<IOleUndoUnit>> units;
                    for (;;) { CComPtr<IOleUndoUnit> unit; const HRESULT hr = list->Next(1, &unit, nullptr); if (hr == S_FALSE) break; CheckError(hr); units.push_back(unit); }
                    return units;
                };
                auto setInput = [&](int index) {
                    const bool textOnly = wcscmp(caseName, L"queue-plain-two") == 0;
                    CStringA html; html.Format("<strong>Queue%d</strong>", index);
                    const CStringA prefix("Version:0.9\r\nStartFragment:00000000\r\nEndFragment:00000000\r\n");
                    CStringA bytes; bytes.Format("Version:0.9\r\nStartFragment:%08u\r\nEndFragment:%08u\r\n",
                        static_cast<unsigned>(prefix.GetLength()), static_cast<unsigned>(prefix.GetLength() + html.GetLength()));
                    bytes += html;
                    CString text; text.Format(textOnly ? L"Queue%d\r\n\r\nTail" : L"Queue%d", index);
                    CheckError(::OpenClipboard(m_doc->m_body) ? S_OK : E_FAIL);
                    ::EmptyClipboard();
                    HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE, bytes.GetLength() + 1);
                    HGLOBAL t = ::GlobalAlloc(GMEM_MOVEABLE, (text.GetLength() + 1) * sizeof(wchar_t));
                    char* hp = h ? static_cast<char*>(::GlobalLock(h)) : nullptr;
                    wchar_t* tp = t ? static_cast<wchar_t*>(::GlobalLock(t)) : nullptr;
                    if (hp) { memcpy(hp, static_cast<LPCSTR>(bytes), bytes.GetLength() + 1); ::GlobalUnlock(h); }
                    if (tp) { memcpy(tp, static_cast<LPCWSTR>(text), (text.GetLength() + 1) * sizeof(wchar_t)); ::GlobalUnlock(t); }
                    const bool htmlSet = !textOnly && hp && ::SetClipboardData(::RegisterClipboardFormat(L"HTML Format"), h);
                    const bool textSet = tp && ::SetClipboardData(CF_UNICODETEXT, t);
                    if (!htmlSet && h) ::GlobalFree(h);
                    if (!textSet && t) ::GlobalFree(t);
                    ::CloseClipboard(); CheckError((textOnly || htmlSet) && textSet ? S_OK : E_FAIL);
                };
                auto event = [&]() { document->execCommand(L"Paste", VARIANT_FALSE, _variant_t()); };
                auto drain = [&]() {
                    int count = 0; MSG message = {};
                    while (::PeekMessage(&message, m_doc->m_body, AU::WM_FINALIZE_HTML_PASTE, AU::WM_FINALIZE_HTML_PASTE, PM_REMOVE)) {
                        ++count; ::DispatchMessage(&message);
                    }
                    safetyOk = closed() && safetyOk;
                    return count;
                };
                // Leave both a previous native Undo and a native Redo. A
                // rejected browser event must retain their exact COM objects.
                m_doc->m_body.MarkupServices()->BeginUndoUnit(L"Future edit");
                pasteTarget->insertAdjacentText(L"beforeEnd", L" Future");
                m_doc->m_body.MarkupServices()->EndUndoUnit();
                edit(ID_EDIT_UNDO);
                range->select();
                const auto previousRedo = redoHistory();
                const auto baseline = FbeUndo::DocumentState(document);
                const CString baselineHtml(static_cast<LPCWSTR>(editable->innerHTML)), baselineCaret(selectionState());
                const bool baselineDirty = m_doc->DocChanged(), baselineRecovery = m_doc->DocRelChanged();
                const auto baselineSavepoints = m_doc->CaptureUndoSavepoints();
                bool valid = !previousRedo.empty() && undoHistory() == previousUndo;
                auto traceQueue = [&](LPCSTR stage) {
                    CStringA message; message.Format("queue-stage: %s valid=%d undo=%u redo=%u caret=%S html=%S\r\n", stage, valid,
                        static_cast<unsigned>(undoHistory().size()), static_cast<unsigned>(redoHistory().size()),
                        static_cast<LPCWSTR>(selectionState()), static_cast<LPCWSTR>(editable->innerHTML));
                    output.Write(message, static_cast<DWORD>(message.GetLength()), &written);
                };
                const bool failPost = cleanQueueFailure || wcscmp(caseName, L"queue-post-failure") == 0 || wcscmp(caseName, L"queue-failure-retry") == 0;
                if (failPost) {
                    setInput(7);
                    ::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", L"paste-post");
                    event();
                    const bool notPosted = drain() == 0;
                    _Module.GetMessageLoop()->OnIdle(0); // actual fallback, after the MSHTML event returned
                    const bool restored = FbeUndo::DocumentState(document) == baseline &&
                        selectionState() == baselineCaret && undoHistory() == previousUndo && redoHistory() == previousRedo &&
                        m_doc->DocChanged() == baselineDirty && m_doc->DocRelChanged() == baselineRecovery && closed();
                    const auto recovered = m_doc->CaptureUndoSavepoints();
                    const bool savepoints = recovered.savedVersion == (baselineSavepoints.clean ? m_doc->m_body.GetVersionNumber() : baselineSavepoints.savedVersion) &&
                        recovered.recoveryVersion == (baselineSavepoints.recoveryClean ? m_doc->m_body.GetVersionNumber() : baselineSavepoints.recoveryVersion) && recovered.savedHtml == baselineSavepoints.savedHtml &&
                        recovered.recoveryHtml == baselineSavepoints.recoveryHtml && recovered.hasSaved == baselineSavepoints.hasSaved &&
                        recovered.hasRecovery == baselineSavepoints.hasRecovery && recovered.reconcile == baselineSavepoints.reconcile;
                    valid = valid && notPosted && restored && savepoints;
                    CStringA trace; trace.Format("queue-failure: posted=%d restored=%d\r\n", !notPosted, restored);
                    if (!valid) output.Write(trace, static_cast<DWORD>(trace.GetLength()), &written);
                }
                const int count = (wcscmp(caseName, L"queue-three") == 0 || wcscmp(caseName,L"queue-reentrant")==0) ? 3 : (wcscmp(caseName, L"queue-two") == 0 || wcscmp(caseName, L"queue-plain-two") == 0) ? 2 :
                    (wcscmp(caseName, L"queue-post-failure") == 0 || cleanQueueFailure) ? 0 : 1;
                std::vector<CString> htmls{baselineHtml}, carets{baselineCaret};
                std::vector<unsigned long long> states{baseline};
                // Sequential real events supply an independent ordering and
                // caret oracle for the burst, with a different clipboard each time.
                for (int i = 1; i <= count; ++i) {
                    setInput(i); m_doc->m_body.OnPaste(0, ID_EDIT_PASTE, 0, handled);
                    htmls.push_back(CString(static_cast<LPCWSTR>(editable->innerHTML)));
                    carets.push_back(selectionState()); states.push_back(FbeUndo::DocumentState(document));
                    CString marker; marker.Format(L"Queue%d", i);
                    valid = htmls.back().Find(marker) >= 0 && undoHistory().size() == previousUndo.size() + i && closed() && valid;
                    if (!valid) traceQueue("sequential");
                }
                for (int i = count - 1; i >= 0; --i) {
                    edit(ID_EDIT_UNDO);
                    valid = FbeUndo::DocumentState(document) == states[i] && selectionState() == carets[i] && valid;
                    if (!valid) traceQueue("sequential-undo");
                }
                valid = undoHistory() == previousUndo && valid;
                ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_PASTE_REENTERED",NULL);
                const bool reentrantPostFailure=wcscmp(caseName,L"queue-reentrant-post-failure")==0;
                for (int i = 1; i <= count+(reentrantPostFailure ? 1 : 0); ++i) { setInput(i); event(); }
                if(wcscmp(caseName,L"queue-reentrant")==0 || reentrantPostFailure) {
                    wchar_t reentered[2]={};
                    valid=::GetEnvironmentVariableW(L"FBE_NEXT_TEST_PASTE_REENTERED",reentered,_countof(reentered))==1 && reentered[0]==L'1' && valid;
                }
                setInput(9); // queued requests must not read this newer clipboard
                const DWORD queuedClipboard = ::GetClipboardSequenceNumber();
                valid = drain() == (reentrantPostFailure ? 0 : (count ? 1 : 0)) && valid;
                if(reentrantPostFailure) _Module.GetMessageLoop()->OnIdle(0); // replay the first accepted request
                valid = FbeUndo::DocumentState(document) == states.back() && selectionState() == carets.back() &&
                    undoHistory().size() == previousUndo.size() + count && ::GetClipboardSequenceNumber() == queuedClipboard && valid;
                if (!valid) traceQueue("burst");
                for (int i = count - 1; i >= 0; --i) {
                    edit(ID_EDIT_UNDO);
                    valid = FbeUndo::DocumentState(document) == states[i] && selectionState() == carets[i] &&
                        m_doc->DocChanged() == baselineDirty && m_doc->DocRelChanged() == baselineRecovery && valid;
                }
                valid = undoHistory() == previousUndo && valid;
                if (count) {
                    edit(ID_EDIT_UNDO);
                    valid = CString(static_cast<LPCWSTR>(editable->innerHTML)) == before && !m_doc->DocChanged() && !m_doc->DocRelChanged() && valid;
                    edit(ID_EDIT_REDO);
                    valid = FbeUndo::DocumentState(document) == baseline && valid;
                }
                for (int i = 1; i <= count; ++i) {
                    edit(ID_EDIT_REDO);
                    valid = FbeUndo::DocumentState(document) == states[i] && selectionState() == carets[i] &&
                        m_doc->DocChanged() && m_doc->DocRelChanged() && valid;
                }
                int line = 0, column = 0;
                valid = closed() && safetyOk && valid;
                const bool validation = m_doc->Validate(line, column), reopen = saveReopen();
                valid = validation && reopen && valid;
                CStringA row; row.Format("%S\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\r\n",
                    caseName, valid, valid, valid, valid, valid, valid, valid, valid, valid, valid, validation, reopen, valid ? "pass" : "fail");
                output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
                if (originalClipboard) ::OleSetClipboard(originalClipboard);
                output.Flush(); output.Close(); ::PostQuitMessage(valid ? 0 : 1); return 0;
            }
			CString middleLeft, middleRight;
			if (middle) {
				MSHTML::IHTMLTxtRangePtr split(range->duplicate());
				split->moveToElementText(pasteTarget); split->setEndPoint(L"EndToStart", range); middleLeft = static_cast<LPCWSTR>(split->text);
				split->moveToElementText(pasteTarget); split->setEndPoint(L"StartToStart", range); middleRight = static_cast<LPCWSTR>(split->text);
			}
			paste(plain ? ID_EDIT_PASTE_TEXT : ID_EDIT_PASTE);
			const CString afterCaret(selectionState());
			if (priorHistory) {
				const CString afterPaste(static_cast<LPCWSTR>(editable->innerHTML));
				const bool pasted = afterPaste != beforeAfterEdit && m_doc->DocChanged() && m_doc->DocRelChanged();
				edit(ID_EDIT_UNDO);
				const CString afterUndo1(static_cast<LPCWSTR>(editable->innerHTML));
				const bool firstUndo = afterUndo1 == beforeAfterEdit && undoHistory() == previousUndo &&
					document->all->item(L"paste-internal-target") == IDispatchPtr(pasteTarget) && selectionState() == beforeCaret &&
					m_doc->DocChanged() && m_doc->DocRelChanged();
				edit(ID_EDIT_UNDO);
				const CString afterUndo2(static_cast<LPCWSTR>(editable->innerHTML));
				const long afterUndo2Version = m_doc->m_body.GetVersionNumber();
				const bool afterUndo2Changed = m_doc->DocChanged(), afterUndo2Recovery = m_doc->DocRelChanged();
				const bool secondUndo = afterUndo2 == before &&
					!afterUndo2Changed && !afterUndo2Recovery;
				edit(ID_EDIT_REDO);
				const CString afterRedo1(static_cast<LPCWSTR>(editable->innerHTML));
				const bool firstRedo = afterRedo1 == beforeAfterEdit &&
					m_doc->DocChanged() && m_doc->DocRelChanged();
				edit(ID_EDIT_REDO);
				const bool secondRedo = CString(static_cast<LPCWSTR>(editable->innerHTML)) == afterPaste &&
					selectionState() == afterCaret &&
					m_doc->DocChanged() && m_doc->DocRelChanged();
				int line = 0, column = 0;
				const bool valid = pasted && firstUndo && secondUndo && firstRedo && secondRedo &&
					m_doc->Validate(line, column) && safetyOk && saveReopen();
				if (!valid) {
					CStringA trace;
					trace.Format("history: paste=%d undo1=%d undo2=%d redo1=%d redo2=%d initialVersion=%ld undo2Version=%ld undo2Changed=%d undo2Recovery=%d; before=%S; edit=%S; undo1=%S; undo2=%S; redo1=%S; final=%S\r\n",
						pasted, firstUndo, secondUndo, firstRedo, secondRedo, initialVersion, afterUndo2Version, afterUndo2Changed, afterUndo2Recovery,
						static_cast<LPCWSTR>(before), static_cast<LPCWSTR>(beforeAfterEdit), static_cast<LPCWSTR>(afterUndo1), static_cast<LPCWSTR>(afterUndo2), static_cast<LPCWSTR>(afterRedo1), static_cast<LPCWSTR>(editable->innerHTML));
					output.Write(trace, static_cast<DWORD>(trace.GetLength()), &written);
				}
				CStringA row; row.Format("%S\t%d\t1\t1\t1\t1\t%d\t%d\t1\t%d\t%d\t%d\t%d\t%s\r\n",
					caseName, pasted, firstUndo && secondUndo, firstRedo && secondRedo, secondUndo && secondRedo,
					secondUndo && secondRedo, valid, valid, valid ? "pass" : "fail");
				output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
				if (originalClipboard) ::OleSetClipboard(originalClipboard);
				output.Flush(); output.Close(); ::PostQuitMessage(valid ? 0 : 1); return 0;
			}
			if (missingLink || missingImage) {
				editable = document->all->item(L"fbw_body");
				const bool unchanged = undoHistory() == previousUndo && editable && CString(static_cast<LPCWSTR>(editable->innerHTML)) == before &&
					!m_doc->DocChanged() && !m_doc->DocRelChanged();
				const bool clipboardUnchanged = ::GetClipboardSequenceNumber() == sequence;
				CStringA row; row.Format("%S\t%d\t0\t0\t0\t%d\t0\t0\t0\t%d\t%d\t0\t0\t%s\r\n",
					caseName, unchanged, clipboardUnchanged, unchanged, unchanged,
					unchanged && clipboardUnchanged ? "pass" : "fail");
				output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
				output.Flush(); output.Close(); ::PostQuitMessage(unchanged && clipboardUnchanged ? 0 : 1); return 0;
			}
			if (legacy) {
				editable = document->all->item(L"fbw_body");
				const bool unchanged = undoHistory() == previousUndo && editable && CString(static_cast<LPCWSTR>(editable->innerHTML)) == before &&
					!m_doc->DocChanged() && !m_doc->DocRelChanged();
				int line = 0, column = 0;
				const bool valid = unchanged && m_doc->Validate(line, column) && safetyOk && saveReopen();
				const bool clipboardUnchanged = ::GetClipboardSequenceNumber() == sequence;
				CStringA row; row.Format("%S\t%d\t0\t0\t0\t%d\t0\t0\t0\t0\t0\t%d\t%d\t%s\r\n",
					caseName, unchanged, clipboardUnchanged, valid, valid, unchanged && clipboardUnchanged && valid ? "pass" : "fail");
				output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
				output.Flush(); output.Close(); ::PostQuitMessage(unchanged && clipboardUnchanged && valid ? 0 : 1); return 0;
			}
			dirtyAfterPaste = m_doc->DocChanged();
			const bool recoveryAfterPaste = m_doc->DocRelChanged();
			clipboardOk = ::GetClipboardSequenceNumber() == sequence;
			editable = document->all->item(L"fbw_body");
			const CString after(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
			const CString afterText(editable ? static_cast<LPCWSTR>(editable->innerText) : L"");
			textOk = internal ? (afterText.Find(L"Internal code", afterText.Find(L"Internal code") + 1) > 0 && afterText.Find(L"internal link") > afterText.Find(L"Internal code")) : singleLine ? (afterText.Find(L"Ж") >= 0 && afterText.Find(L"Ω") > afterText.Find(L"Ж")) : plain ? (afterText.Find(L"Plain one") >= 0 && afterText.Find(L"Plain three") > afterText.Find(L"Plain one")) :
				(afterText.Find(L"Bold italic") >= 0 && afterText.Find(L"Second") > afterText.Find(L"Bold italic"));
			MSHTML::IHTMLElementPtr targetAfter(document->all->item(L"paste-target"));
			const CString targetHtml(targetAfter ? static_cast<LPCWSTR>(targetAfter->innerHTML) : L"");
			formatting = internal ? (targetHtml.Find(L"class=code") >= 0 && targetHtml.Find(L"href=\"#paste-neighbor\"") >= 0) : plain ? targetHtml.Find(L"<STRONG") < 0 :
				(targetHtml.Find(L"<STRONG") >= 0 && targetHtml.Find(L"<EM") >= 0 && targetHtml.Find(L"MsoNormal") < 0 && targetHtml.Find(L"color") < 0);
			MSHTML::IHTMLElementCollectionPtr paragraphsAfter(targetAfter ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(targetAfter)->getElementsByTagName(L"P")) : MSHTML::IHTMLElementCollectionPtr());
			paragraphs = paragraphsAfter && (singleLine ? paragraphsAfter->length == 2 : paragraphsAfter->length >= (plain ? 4 : 3));
			if (middle && paragraphsAfter && paragraphsAfter->length == 6) {
				auto text = [&](long index) { return CString(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(paragraphsAfter->item(index))->innerText)); };
				CString empty(text(3)); empty.Remove(L'\r'); empty.Remove(L'\n'); empty.Remove(L' '); empty.Remove(0x00a0);
				safetyOk = text(1) == middleLeft && text(2) == L"Plain one" && empty.IsEmpty() &&
					text(4) == L"Plain three" && text(5) == middleRight && safetyOk;
			} else if (middle) safetyOk = false;
			neighbor = document->all->item(L"paste-neighbor");
			neighborOk = neighbor && CString(static_cast<LPCWSTR>(neighbor->outerHTML)) == neighborBefore;
			const bool twoPasteCase = internal && !internalEnd;
			auto selectionInTarget = [&]() -> bool {
			MSHTML::IHTMLTxtRangePtr selected(document->selection->createRange());
			for (MSHTML::IHTMLElementPtr element(selected ? MSHTML::IHTMLElementPtr(selected->parentElement()) : MSHTML::IHTMLElementPtr()); element; element = element->parentElement)
				if (U::scmp(element->id, L"paste-target") == 0) return true;
			return false;
		};
			if (!twoPasteCase) {
				m_doc->m_body.SetFocus(); ::SetFocus(m_doc->m_body);
				edit(ID_EDIT_UNDO);
				editable = document->all->item(L"fbw_body");
				undoOk = editable && CString(static_cast<LPCWSTR>(editable->innerHTML)) == before && undoHistory() == previousUndo;
				if (!undoOk) {
					WritePortableStateTestText(m_doc->m_filename + L".before.html", CStringA(CW2A(before, CP_UTF8)));
					WritePortableStateTestText(m_doc->m_filename + L".undo.html", CStringA(CW2A(editable->innerHTML, CP_UTF8)));
				}
				cleanAfterUndo = preDirty ? m_doc->DocChanged() : !m_doc->DocChanged();
				const bool recoveryCleanAfterUndo = preDirty ? m_doc->DocRelChanged() : !m_doc->DocRelChanged();
				const bool undoSelection = selectionInTarget() && selectionState() == beforeCaret;
				m_doc->m_body.SetFocus(); ::SetFocus(m_doc->m_body);
				edit(ID_EDIT_REDO);
				editable = document->all->item(L"fbw_body");
				redoOk = editable && CString(static_cast<LPCWSTR>(editable->innerHTML)) == after;
				dirtyAfterRedo = m_doc->DocChanged();
				dirtyOk = dirtyAfterPaste && cleanAfterUndo && dirtyAfterRedo;
				recoveryOk = recoveryAfterPaste && recoveryCleanAfterUndo && m_doc->DocRelChanged();
				selectionOk = undoSelection && selectionInTarget() && selectionState() == afterCaret;
			}
			int line = 0, column = 0;
			if (twoPasteCase) {
				twoPasteOk = false;
				editable = document->all->item(L"fbw_body");
				const CString afterFirst(after);
				MSHTML::IHTMLElementPtr secondTarget(document->all->item(L"paste-internal-target"));
				MSHTML::IHTMLTxtRangePtr secondRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
				secondRange->moveToElementText(secondTarget); secondRange->collapse(VARIANT_FALSE); secondRange->select();
				m_doc->m_body.SetFocus(); ::SetFocus(m_doc->m_body);
				paste(ID_EDIT_PASTE);
				editable = document->all->item(L"fbw_body");
				const CString afterSecond(static_cast<LPCWSTR>(editable->innerHTML));
				const bool secondDirty = m_doc->DocChanged() && m_doc->DocRelChanged();
				twoPasteOk = afterSecond != afterFirst && secondDirty;
				twoPasteTrace.Format("second=%d,dirty2=%d", afterSecond != afterFirst, secondDirty);
				edit(ID_EDIT_UNDO);
				editable = document->all->item(L"fbw_body");
				const bool firstUndo = editable && CString(static_cast<LPCWSTR>(editable->innerHTML)) == afterFirst &&
					m_doc->DocChanged() && m_doc->DocRelChanged();
				twoPasteTrace.AppendFormat(",undo1_dirty=%d", firstUndo);
				undoOk = firstUndo;
				twoPasteOk = twoPasteOk && firstUndo;
				edit(ID_EDIT_UNDO);
				editable = document->all->item(L"fbw_body");
				const bool secondHtml = editable && CString(static_cast<LPCWSTR>(editable->innerHTML)) == before;
				const bool secondChanged = m_doc->DocChanged(), secondRecovery = m_doc->DocRelChanged();
				const bool secondUndo = secondHtml && secondChanged == preDirty && secondRecovery == preDirty;
				CComPtr<IOleUndoManager> remainingUndo;
				IServiceProviderPtr undoProvider(document);
				if (undoProvider) undoProvider->QueryService(SID_SOleUndoManager, IID_IOleUndoManager,
					reinterpret_cast<void**>(&remainingUndo));
				CComBSTR priorDescription;
				const bool priorUndoAvailable = !preDirty ||
					(remainingUndo && SUCCEEDED(remainingUndo->GetLastUndoDescription(&priorDescription)));
				twoPasteTrace.AppendFormat(",undo2_html=%d,dirty=%d,recovery=%d", secondHtml, secondChanged, secondRecovery);
				dirtyOk = dirtyAfterPaste && secondDirty && firstUndo && secondUndo;
				recoveryOk = recoveryAfterPaste && secondDirty && firstUndo && secondUndo;
				selectionOk = selectionInTarget();
				twoPasteOk = twoPasteOk && secondUndo && priorUndoAvailable;
				edit(ID_EDIT_REDO);
				edit(ID_EDIT_REDO);
				editable = document->all->item(L"fbw_body");
				const bool bothRedo = editable && CString(static_cast<LPCWSTR>(editable->innerHTML)) == afterSecond &&
					m_doc->DocChanged() && m_doc->DocRelChanged();
				twoPasteTrace.AppendFormat(",redo2_dirty=%d", bothRedo);
				redoOk = bothRedo;
				selectionOk = selectionOk && selectionInTarget();
				twoPasteOk = twoPasteOk && bothRedo;
			}
			validated = redoOk && twoPasteOk && m_doc->Validate(line, column);
			saved = validated && safetyOk && saveReopen();
		}
		catch (const _com_error&) {}
		if (originalClipboard) ::OleSetClipboard(originalClipboard);
		if (!twoPasteOk) {
			CStringA diagnostic("two-paste-check: " + twoPasteTrace + "\r\n");
			output.Write(diagnostic, static_cast<DWORD>(diagnostic.GetLength()), &written);
		}
		const bool passed = textOk && formatting && paragraphs && neighborOk && clipboardOk && undoOk && redoOk && selectionOk && dirtyOk && recoveryOk && validated && saved && twoPasteOk && safetyOk;
		CStringA row; row.Format("%S\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\r\n", caseName, textOk, formatting, paragraphs, neighborOk, clipboardOk, undoOk, redoOk, selectionOk, dirtyOk, recoveryOk, validated, saved, passed ? "pass" : "fail");
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"clipboard-title-paste"))
	{
		CStringA header("case\tnormalized_nested_title\tnormalized_direct_titles\ttext_order\tformatting\tneighbor_unchanged\tundo\tredo\tvalidate\tvalidation_line\tvalidation_column\tsave\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr body(document ? MSHTML::IHTMLElementPtr(document->body) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr editable(document ? MSHTML::IHTMLElementPtr(document->all->item(L"fbw_body")) : MSHTML::IHTMLElementPtr());
		if (!body || !editable) { output.Close(); ::PostQuitMessage(1); return 0; }
		ShowView(BODY);
		wchar_t pasteCaseBuffer[64] = {};
		const DWORD pasteCaseLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TITLE_PASTE_CASE", pasteCaseBuffer, _countof(pasteCaseBuffer));
		const CString pasteCase(pasteCaseLength && pasteCaseLength < _countof(pasteCaseBuffer) ? pasteCaseBuffer : L"full-title-nonempty");

		// The loaded FB2 fixture has three sections and keeps strong/em/link
		// children in the source title.  This tests the real document conversion,
		// rather than a synthetic innerHTML assignment which cannot prove Save.
		auto byId = [&](const wchar_t* id) -> MSHTML::IHTMLElementPtr { return document->all->item(id); };
		auto selectElement = [&](MSHTML::IHTMLElementPtr element, bool collapseAtEnd) -> bool {
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
			if (!range || !element) return false;
			range->moveToElementText(element); if (collapseAtEnd) range->collapse(VARIANT_FALSE); range->select(); return true;
		};
		auto countNestedTitle = [&](MSHTML::IHTMLElementPtr root) -> long {
			long count = 0; MSHTML::IHTMLElementCollectionPtr divs(root ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(root)->getElementsByTagName(L"DIV")) : MSHTML::IHTMLElementCollectionPtr());
			for (long index = 0; divs && index < divs->length; ++index) {
				MSHTML::IHTMLElementPtr current(divs->item(_variant_t(index), _variant_t()));
				MSHTML::IHTMLElementPtr parent(current ? MSHTML::IHTMLElementPtr(current->parentElement) : MSHTML::IHTMLElementPtr());
				if (current && parent && U::scmp(current->className, L"title") == 0 && U::scmp(parent->tagName, L"DIV") == 0 && U::scmp(parent->className, L"title") == 0) ++count;
			}
			return count;
		};
		auto countDirectTitles = [&](MSHTML::IHTMLElementPtr section) -> long {
			MSHTML::IHTMLDOMNodePtr sectionNode(section);
			long count = 0; for (MSHTML::IHTMLDOMNodePtr child(sectionNode ? MSHTML::IHTMLDOMNodePtr(sectionNode->firstChild) : MSHTML::IHTMLDOMNodePtr()); child; child = child->nextSibling) {
				MSHTML::IHTMLElementPtr element(child); if (element && U::scmp(element->tagName, L"DIV") == 0 && U::scmp(element->className, L"title") == 0) ++count;
			}
			return count;
		};
		bool normalizedNested = false, normalizedDuplicate = false;
		bool textOrder = false, formatting = false, neighborUnchanged = false, undo = false, redo = false, validated = false, saved = false;
		int validationLine = 0, validationColumn = 0;
		try {
			MSHTML::IHTMLElementPtr sourceTitle(byId(L"title-source") ? MSHTML::IHTMLElementPtr(MSHTML::IHTMLElement2Ptr(byId(L"title-source"))->getElementsByTagName(L"DIV")->item(0L)) : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr sourceParagraphs(sourceTitle ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(sourceTitle)->getElementsByTagName(L"P")) : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr sourceFirst(sourceParagraphs && sourceParagraphs->length ? MSHTML::IHTMLElementPtr(sourceParagraphs->item(0L)) : MSHTML::IHTMLElementPtr());
			const bool emptyTarget = pasteCase == L"full-title-empty-target";
			const bool multipleParagraphs = pasteCase == L"multiple-title-paragraphs" || pasteCase == L"full-title-empty-target" || pasteCase == L"full-title-nonempty" || pasteCase == L"ctrl-v-command";
			MSHTML::IHTMLElementPtr targetSection(byId(emptyTarget ? L"title-target-empty" : L"title-target"));
			MSHTML::IHTMLElementPtr targetTitle(targetSection ? MSHTML::IHTMLElementPtr(MSHTML::IHTMLElement2Ptr(targetSection)->getElementsByTagName(L"DIV")->item(0L)) : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementPtr neighbor(byId(L"title-neighbor"));
			const CString before(static_cast<LPCWSTR>(editable->innerHTML));
			const CString neighborBefore(neighbor ? static_cast<LPCWSTR>(neighbor->outerHTML) : L"");
			BOOL handled = FALSE;
			m_doc->m_body.SetFocus();
			if (!sourceTitle || !sourceFirst) throw _com_error(E_FAIL);
			if (pasteCase == L"text-only" || pasteCase == L"text-paragraph-end") {
				MSHTML::IHTMLTxtRangePtr sourceRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
				if (!sourceRange) throw _com_error(E_FAIL);
				sourceRange->moveToElementText(sourceFirst);
				if (pasteCase == L"text-only") sourceRange->moveEnd(L"character", -1);
				else sourceRange->moveStart(L"character", 1);
				sourceRange->select();
			} else if (!selectElement(multipleParagraphs ? sourceTitle : sourceFirst, false)) throw _com_error(E_FAIL);
			MSHTML::IHTMLTxtRangePtr copiedRange(document->selection->createRange());
			const CString copiedText(copiedRange ? static_cast<LPCWSTR>(copiedRange->text) : L"");
			if (copiedText.IsEmpty()) throw _com_error(E_FAIL);
			m_doc->m_body.OnCopy(0, ID_EDIT_COPY, m_doc->m_body, handled);
			if (!selectElement(targetTitle, true)) throw _com_error(E_FAIL);
			// Exercise the normal Ctrl+V route: MSHTML raises onpaste, OnRealPaste
			// routes it through WM_COMMAND, and the editor runs CFBEView::OnPaste.
			document->execCommand(L"Paste", VARIANT_FALSE, _variant_t());
			MSG pendingPaste = {};
			while (::PeekMessage(&pendingPaste, m_doc->m_body, WM_COMMAND, WM_COMMAND, PM_REMOVE))
				::DispatchMessage(&pendingPaste);
			editable = document->all->item(L"fbw_body"); targetSection = byId(emptyTarget ? L"title-target-empty" : L"title-target");
			normalizedNested = countNestedTitle(targetSection) > 0;
			normalizedDuplicate = countDirectTitles(targetSection) > 1;
			MSHTML::IHTMLElementPtr normalizedTitle(targetSection ? MSHTML::IHTMLElementPtr(MSHTML::IHTMLElement2Ptr(targetSection)->getElementsByTagName(L"DIV")->item(0L)) : MSHTML::IHTMLElementPtr());
			const CString titleText(normalizedTitle ? static_cast<LPCWSTR>(normalizedTitle->innerText) : L"");
			const int copiedAt = titleText.Find(copiedText);
			textOrder = copiedAt >= 0 && (emptyTarget || copiedAt > titleText.Find(L"Target existing")) && (!multipleParagraphs || titleText.Find(L"Source second") > titleText.Find(L"Source strong em link"));
			formatting = normalizedTitle && MSHTML::IHTMLElement2Ptr(normalizedTitle)->getElementsByTagName(L"STRONG")->length == 1 && MSHTML::IHTMLElement2Ptr(normalizedTitle)->getElementsByTagName(L"EM")->length == 1 && MSHTML::IHTMLElement2Ptr(normalizedTitle)->getElementsByTagName(L"A")->length == 1;
			neighbor = byId(L"title-neighbor"); neighborUnchanged = neighbor && CString(static_cast<LPCWSTR>(neighbor->outerHTML)) == neighborBefore;
			const CString normalized(static_cast<LPCWSTR>(editable->innerHTML));
			const CString normalizedTrace(AU::_ARGS.source_memory_benchmark_path + L".after-normalization.html");
			const CStringA normalizedTraceUtf8(CW2A(normalized, CP_UTF8));
			WritePortableStateTestText(normalizedTrace, normalizedTraceUtf8);
			m_doc->m_body.SetFocus();
			::SetFocus(m_doc->m_body);
			SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_UNDO, 0), 0);
			MSHTML::IHTMLElementPtr undoEditable(document->all->item(L"fbw_body"));
			undo = undoEditable && CString(static_cast<LPCWSTR>(undoEditable->innerHTML)) == before;
			const CString undoTrace(AU::_ARGS.source_memory_benchmark_path + L".after-undo.html");
			const CStringA undoTraceUtf8(CW2A(undoEditable ? static_cast<LPCWSTR>(undoEditable->innerHTML) : L"", CP_UTF8));
			WritePortableStateTestText(undoTrace, undoTraceUtf8);
			m_doc->m_body.SetFocus();
			::SetFocus(m_doc->m_body);
			SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_REDO, 0), 0);
			MSHTML::IHTMLElementPtr redoEditable(document->all->item(L"fbw_body"));
			redo = redoEditable && CString(static_cast<LPCWSTR>(redoEditable->innerHTML)) == normalized;
			validated = redo && m_doc->Validate(validationLine, validationColumn); saved = validated && m_doc->Save();
			ShowView(SOURCE);
			const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
			m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
			WritePortableStateTestText(AU::_ARGS.source_memory_benchmark_path + L".serialized.xml", source.data());
			ShowView(BODY);
		}
		catch (const _com_error&) {}
		const bool passed = !normalizedNested && !normalizedDuplicate && textOrder && formatting && neighborUnchanged && undo && redo && validated && saved;
		CStringA row; row.Format("%S\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\r\n", static_cast<LPCWSTR>(pasteCase), normalizedNested ? 1 : 0, normalizedDuplicate ? 1 : 0, textOrder ? 1 : 0, formatting ? 1 : 0, neighborUnchanged ? 1 : 0, undo ? 1 : 0, redo ? 1 : 0, validated ? 1 : 0, validationLine, validationColumn, saved ? 1 : 0, passed ? "pass" : "fail");
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"link-navigation-runtime"))
	{
		CStringA header("nested\ttarget\tsame_document\tbroken\treturned_second\tinserted_navigate\tinserted_same_unique\tinserted_returned\tinserted_returned_origin\tinserted_before\tdeleted_origin_fallback\tother_document_opened\tunchanged\tsecond_dom_unchanged\tsecond_dirty_unchanged\torigin_dom_unchanged\torigin_dirty_unchanged\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr editable(FBELinkNavigation::GetEditableBody(document));
		MSHTML::IHTMLElementCollectionPtr links(editable ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"A")) : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr internal(links && links->length > 0 ? MSHTML::IHTMLElementPtr(links->item(0L)) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr second(links && links->length > 1 ? MSHTML::IHTMLElementPtr(links->item(1L)) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr broken(links && links->length > 2 ? MSHTML::IHTMLElementPtr(links->item(2L)) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr strongs(internal ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(internal)->getElementsByTagName(L"STRONG")) : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr nested(strongs && strongs->length ? MSHTML::IHTMLElementPtr(strongs->item(0L)) : MSHTML::IHTMLElementPtr());
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
		MSHTML::IHTMLElementPtr returnedLink(returnedRange ? MSHTML::IHTMLElementPtr(FBELinkNavigation::FindNearestLinkElement(returnedRange->parentElement(), editable)) : MSHTML::IHTMLElementPtr());
		const bool returnedSecond = secondNavigated && returnedLink == second;
		const CString brokenTargetId(FBELinkNavigation::GetInternalLinkTargetId(document, broken));
		const bool brokenInternal = !brokenTargetId.IsEmpty() && !FBELinkNavigation::FindTargetElement(document, brokenTargetId);
		MSHTML::IHTMLElementPtr inserted;
		MSHTML::IHTMLDOMNodePtr secondNode(second);
		MSHTML::IHTMLDOMNodePtr secondParent(secondNode ? MSHTML::IHTMLDOMNodePtr(secondNode->parentNode) : MSHTML::IHTMLDOMNodePtr());
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
			MSHTML::IHTMLElementPtr link(links->length == 1 ? MSHTML::IHTMLElementPtr(links->item(0L)) : MSHTML::IHTMLElementPtr());
			return link && CString(static_cast<LPCWSTR>(link->innerText)) == L"manual-link";
		};
		auto hasUnlinkedTypedText = [&]() -> bool {
			MSHTML::IHTMLDocument2Ptr current(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr typed(current && current->all ? MSHTML::IHTMLElementPtr(current->all->item(L"auto-url-typed")) : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr links(typed ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(typed)->getElementsByTagName(L"A")) : MSHTML::IHTMLElementCollectionPtr());
			return typed && CString(static_cast<LPCWSTR>(typed->innerText)) == L"\\\\слово" && (!links || links->length == 0);
		};
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		bool enteredByUserInput = false;
		try
		{
			MSHTML::IHTMLElementPtr inputTarget(document && document->all ? MSHTML::IHTMLElementPtr(document->all->item(L"auto-url-typed")) : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementPtr focusTarget(document && document->all ? MSHTML::IHTMLElementPtr(document->all->item(L"auto-url-focus")) : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLTxtRangePtr inputRange(inputTarget ? MSHTML::IHTMLTxtRangePtr(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange()) : MSHTML::IHTMLTxtRangePtr());
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
			MSHTML::IHTMLElementPtr manualTarget(document && document->all ? MSHTML::IHTMLElementPtr(document->all->item(L"auto-url-manual")) : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLTxtRangePtr range(manualTarget ? MSHTML::IHTMLTxtRangePtr(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange()) : MSHTML::IHTMLTxtRangePtr());
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
		struct PathApiValues { CString path, name, directory; bool pathCalled = false, nameCalled = false, directoryCalled = false; };
		auto readEnvironmentPath = [](const wchar_t* name) -> CString {
			const DWORD length = ::GetEnvironmentVariable(name, NULL, 0);
			if (!length) return CString();
			std::vector<wchar_t> value(static_cast<size_t>(length));
			return ::GetEnvironmentVariable(name, value.data(), length) ? CString(value.data()) : CString();
		};
		auto readExternal = [&]() -> PathApiValues {
			PathApiValues values;
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLWindow2Ptr window(document ? MSHTML::IHTMLWindow2Ptr(document->parentWindow) : MSHTML::IHTMLWindow2Ptr());
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
			values.pathCalled = invoke(L"GetDocumentFilePath", values.path);
			values.nameCalled = invoke(L"GetDocumentFileName", values.name);
			values.directoryCalled = invoke(L"GetDocumentDirectory", values.directory);
			return values;
		};
		auto matches = [](const PathApiValues& actual, const CString& expected) -> bool {
			const int separator = expected.ReverseFind(L'\\');
			const CString expectedName = separator >= 0 ? expected.Mid(separator + 1) : expected;
			const CString expectedDirectory = separator == 2 && expected.GetLength() >= 3 && expected[1] == L':' ? expected.Left(3) : separator > 0 ? expected.Left(separator) : CString();
			const bool absolute = expected.GetLength() >= 2 && expected[0] == L'\\' && expected[1] == L'\\' || expected.GetLength() >= 3 && ((expected[0] >= L'A' && expected[0] <= L'Z') || (expected[0] >= L'a' && expected[0] <= L'z')) && expected[1] == L':' && expected[2] == L'\\';
			return actual.pathCalled && actual.nameCalled && actual.directoryCalled && absolute && actual.path == expected && actual.name == expectedName && actual.directory == expectedDirectory;
		};
		const CString first(readEnvironmentPath(L"FBE_NEXT_TEST_DOCUMENT_PATH_FIRST"));
		const CString saveAs(readEnvironmentPath(L"FBE_NEXT_TEST_SAVE_PATH"));
		const CString second(readEnvironmentPath(L"FBE_NEXT_TEST_DOCUMENT_PATH_SECOND"));
		const CString unc(readEnvironmentPath(L"FBE_NEXT_TEST_DOCUMENT_PATH_UNC"));
		const CString longPath(readEnvironmentPath(L"FBE_NEXT_TEST_DOCUMENT_PATH_LONG"));
		const PathApiValues untitled(readExternal());
		const bool unsaved = untitled.pathCalled && untitled.nameCalled && untitled.directoryCalled && untitled.path.IsEmpty() && untitled.name.IsEmpty() && untitled.directory.IsEmpty();
		// This verifies the document-path API, not the disposable startup
		// document's transient MSHTML dirty bit.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		const bool opened = !first.IsEmpty() && LoadFile(first) == OK && matches(readExternal(), first);
		const bool savedAs = opened && !saveAs.IsEmpty() && SaveFile(true) == OK && matches(readExternal(), saveAs);
		const PathApiValues beforeSecond(readExternal());
		// Saving establishes the document state, but MSHTML may complete an
		// asynchronous form-state notification before the next test-only open.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		const bool otherOpened = savedAs && !second.IsEmpty() && LoadFile(second) == OK && matches(readExternal(), second) && beforeSecond.path != second;
		const CString currentFilename(m_doc->m_filename); const bool currentNameValid = m_doc->m_namevalid;
		m_doc->m_filename = unc; m_doc->m_namevalid = true;
		const bool uncPath = !unc.IsEmpty() && matches(readExternal(), unc);
		m_doc->m_filename = longPath; m_doc->m_namevalid = true;
		const PathApiValues longValues(readExternal());
		const bool longDocumentPath = longPath.GetLength() > MAX_PATH && matches(longValues, longPath);
		const int longSeparator = longPath.ReverseFind(L'\\');
		const CString longName = longSeparator >= 0 ? longPath.Mid(longSeparator + 1) : longPath;
		const CString longDirectory = longSeparator > 0 ? longPath.Left(longSeparator) : CString();
		m_doc->m_filename = currentFilename; m_doc->m_namevalid = currentNameValid;
		const bool unicode = first.Find(L"путь") >= 0 && first.Find(L"книга") >= 0 && second.Find(L"путь") >= 0 && second.Find(L"книга") >= 0;
		const bool passed = unsaved && opened && savedAs && otherOpened && unicode && uncPath && longDocumentPath;
		CStringA report;
		report.Format("unsaved=%d\nopened=%d\nsave_as=%d\nother_opened=%d\nunicode=%d\nunc=%d\nlong_path=%d\nlong_calls=%d\nlong_actual_length=%d\nlong_expected_length=%d\nlong_full=%d\nlong_name=%d\nlong_directory=%d\nresult=%s\n", unsaved, opened, savedAs, otherOpened, unicode, uncPath, longDocumentPath, longValues.pathCalled && longValues.nameCalled && longValues.directoryCalled, longValues.path.GetLength(), longPath.GetLength(), longValues.path == longPath, longValues.name == longName, longValues.directory == longDirectory, passed ? "pass" : "fail");
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		// Loading the disposable fixture can leave MSHTML transiently dirty while
		// it finishes initialization.  The scenario is complete at this point;
		// suppress only its exit-time Save prompt so the runtime host can quit.
		m_doc->MarkSavePoint();
		if (IsSourceActive()) m_source.SendMessage(SCI_SETSAVEPOINT);
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"reference-navigation-runtime"))
	{
		CStringA header("footnote_check\tfootnote_target\treference_check\treference_target\tcheck_unchanged\tdom_unchanged\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr editable(FBELinkNavigation::GetEditableBody(document));
		MSHTML::IHTMLElementCollectionPtr links(editable ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"A")) : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr link(links && links->length ? MSHTML::IHTMLElementPtr(links->item(0L)) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr strongs(link ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(link)->getElementsByTagName(L"STRONG")) : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr nested(strongs && strongs->length ? MSHTML::IHTMLElementPtr(strongs->item(0L)) : MSHTML::IHTMLElementPtr());
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
		MSHTML::IHTMLTxtRangePtr expectedReference(link && document && document->body ? MSHTML::IHTMLTxtRangePtr(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange()) : MSHTML::IHTMLTxtRangePtr());
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
