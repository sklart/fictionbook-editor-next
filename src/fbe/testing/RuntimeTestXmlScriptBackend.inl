// Dedicated frame for XML Undo/COM lifetime probes.
__declspec(noinline) LRESULT CMainFrame::RunXmlScriptBackendRuntime(CAtlFile& output)
{
		CString original;
		const bool read = GetInternalXmlScriptSourceText(original);
		const bool originalDirty = read && m_doc->DocChanged();
		const XmlScriptDiagnostic valid = read ? ValidateInternalXmlScriptText(original) : XmlScriptDiagnostic();
		const XmlScriptDiagnostic invalid = read ? ValidateInternalXmlScriptText(original + L"<") : XmlScriptDiagnostic();
		const XmlScriptDiagnostic noOp = read ? ApplyInternalXmlScriptText(original, L"XML no-op runtime test") : XmlScriptDiagnostic();
		const bool noOpPreserved = noOp.valid && m_doc->DocChanged() == originalDirty;
		CString changed(original);
		const bool prepared = changed.Replace(L"XML_API_BEFORE", L"XML_API_AFTER") == 1;
		const XmlScriptDiagnostic applied = prepared ? ApplyInternalXmlScriptText(changed, L"XML scripting runtime test") : XmlScriptDiagnostic();
		CString afterApply;
		const bool changedInDocument = applied.valid && GetInternalXmlScriptSourceText(afterApply) && afterApply.Find(L"XML_API_AFTER") >= 0;
		const bool dirty = applied.valid && m_doc->DocChanged();
		if(applied.valid) { m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_UNDO, 0), 0); }
		CString afterUndo;
		const bool restored = applied.valid && GetInternalXmlScriptSourceText(afterUndo) && afterUndo.Find(L"XML_API_BEFORE") >= 0;
		const bool undoRestoredDirtyState = restored && m_doc->DocChanged() == originalDirty;
		if(restored) { m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_REDO, 0), 0); }
		CString afterRedo;
		const bool redone = restored && GetInternalXmlScriptSourceText(afterRedo) && afterRedo.Find(L"XML_API_AFTER") >= 0;
		if(redone) { m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_UNDO, 0), 0); }
		CString afterSecondUndo;
		const bool secondUndo = redone && GetInternalXmlScriptSourceText(afterSecondUndo) && afterSecondUndo.Find(L"XML_API_BEFORE") >= 0;
		if(secondUndo) { m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, MAKEWPARAM(ID_EDIT_REDO, 0), 0); }
		CString afterSecondRedo;
		const bool secondRedo = secondUndo && GetInternalXmlScriptSourceText(afterSecondRedo) && afterSecondRedo.Find(L"XML_API_AFTER") >= 0;
		bool historyChain = false;
		CStringA historyDetails;
		try {
			MSHTML::IHTMLDocument2Ptr dom(m_doc->m_body.Document());
			CheckError(dom->parentWindow->execScript(L"window.external.BeginUndoUnit(document,'manual before Apply');var prior=document.body.createTextRange();prior.moveToElementText(document.getElementById('xml-prior'));prior.collapse(false);prior.select();prior.pasteHTML(' MANUAL');window.external.EndUndoUnit(document);", L"JScript"));
			CString manual; const bool manualCaptured = GetInternalXmlScriptSourceText(manual) && manual.Find(L"MANUAL") >= 0;
			CString first(manual); first.Replace(L"XML_API_AFTER", L"XML_APPLY_ONE");
			CString second(first); second.Replace(L"XML_APPLY_ONE", L"XML_APPLY_TWO");
			first.Replace(L"AQID", L"BAUG"); second.Replace(L"AQID", L"BwgJ");
			const bool applyOne=manualCaptured && ApplyInternalXmlScriptText(first,L"Apply 1").valid;
			const bool applyTwo=applyOne && ApplyInternalXmlScriptText(second,L"Apply 2").valid;
			historyChain = manualCaptured && applyOne && applyTwo;
			historyDetails.AppendFormat("manual=%d\napply_one=%d\napply_two=%d\n",manualCaptured,applyOne,applyTwo);
			auto transition = [&](WORD command, const wchar_t* text, bool manualExpected, const wchar_t* binaryText) {
				m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, MAKEWPARAM(command,0),0);
				CString current; if (!GetInternalXmlScriptSourceText(current)) return false;
				const bool match=current.Find(text) >= 0 && (current.Find(L"MANUAL") >= 0) == manualExpected && current.Find(binaryText) >= 0;
				historyDetails.AppendFormat("transition_%u_%S=%d;manual=%d;binary=%d\n",command,text,match,current.Find(L"MANUAL")>=0,current.Find(binaryText)>=0);
				return match;
			};
			historyChain = historyChain && transition(ID_EDIT_UNDO,L"XML_APPLY_ONE",true,L"BAUG") && transition(ID_EDIT_UNDO,L"XML_API_AFTER",true,L"AQID") && transition(ID_EDIT_UNDO,L"XML_API_AFTER",false,L"AQID") && transition(ID_EDIT_REDO,L"XML_API_AFTER",true,L"AQID") && transition(ID_EDIT_REDO,L"XML_APPLY_ONE",true,L"BAUG") && transition(ID_EDIT_REDO,L"XML_APPLY_TWO",true,L"BwgJ");
		} catch (const _com_error&) { historyChain = false; }
		const bool bodyReady = m_doc->m_body.Document() != NULL;
		const bool treeReady = !_Settings.ViewDocumentTree() || m_document_tree.m_tree.m_tree.GetCount() > 0;
		bool payloadLifetime = false, detachedHistory = false;
		try {
			m_doc->ReconcileBinaryStore();
			std::weak_ptr<const CComVariant> oldPayload;
			{
				CComPtr<IDispatch> element(m_doc->m_binaryStore.Element(L"runtime.bin"));
				oldPayload = m_doc->m_binaryStore.payloads.Snapshot(element);
			}
			CComPtr<IOleUndoUnit> oldApply;
			CheckError(RuntimeTests::CreateXmlUndoLifetimeProbe(&m_xml_script_backend, m_doc, &oldApply));
			if (oldPayload.expired()) CheckError(E_FAIL);
			CString saved(m_doc->m_filename);
			saved = saved.Left(saved.ReverseFind(L'\\') + 1) + L"xml-c3-lifetime.fb2";
			if (!m_doc->Save(saved) || LoadFile(saved) != OK) CheckError(E_FAIL);
			payloadLifetime = oldPayload.expired();
			CString liveBefore, liveAfter;
			const bool captured = GetInternalXmlScriptSourceText(liveBefore);
			CComPtr<IOleUndoManager> liveManager;
			IServiceProviderPtr services(m_doc->m_body.Document());
			CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&liveManager)));
			const HRESULT detached = oldApply->Do(liveManager);
			detachedHistory = captured && detached == E_UNEXPECTED && GetInternalXmlScriptSourceText(liveAfter) && liveAfter == liveBefore;
		} catch (const _com_error&) { }
		const bool passed = read && valid.valid && !invalid.valid && noOpPreserved && prepared && changedInDocument && dirty && restored && undoRestoredDirtyState && redone && secondUndo && secondRedo && bodyReady && treeReady && historyChain && payloadLifetime && detachedHistory;
		CStringA report;
		report.Format("read=%d\nvalidate=%d\ninvalid_rejected=%d\nno_op=%d\napply=%d\ndirty=%d\nundo=%d\nundo_dirty_restored=%d\nredo=%d\nundo_second=%d\nredo_second=%d\nbody=%d\ntree=%d\nresult=%s\n",
			read, valid.valid, !invalid.valid, noOpPreserved, changedInDocument, dirty, restored, undoRestoredDirtyState, redone, secondUndo, secondRedo, bodyReady, treeReady, passed ? "pass" : "fail");
		report.AppendFormat("history_chain=%d\n",historyChain);
		report.AppendFormat("payload_lifetime=%d\ndetached_history=%d\n",payloadLifetime,detachedHistory);
		report+=historyDetails;
		DWORD written = 0;
		output.Write(report, static_cast<DWORD>(report.GetLength()), &written);
		output.Close();
		::PostQuitMessage(passed ? 0 : 1);
		return 0;
}
