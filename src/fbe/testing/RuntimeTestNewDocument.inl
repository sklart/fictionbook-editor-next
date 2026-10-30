if(IsFbeTestScenario(L"new-document-edit-state-runtime")) {
    bool passed = false; CStringA details = "start";
    auto require = [&](bool value, const char* label) {
        details = label; CStringA phase; phase.Format("phase=%s;ok=%d\r\n", label, value ? 1 : 0);
        DWORD written = 0; output.Write(phase, phase.GetLength(), &written); output.Flush();
        if (!value) CheckError(E_FAIL);
    };
    auto pump = []() {
        const ULONGLONG until = ::GetTickCount64() + 100;
        while (::GetTickCount64() < until) { MSG message = {}; while (::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessageW(&message); } ::Sleep(1); }
    };
    try {
        std::vector<wchar_t> path(32768);
        const DWORD length = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_NEW_EDIT_OUTPUT", path.data(), static_cast<DWORD>(path.size()));
        require(length && length < path.size(), "output path");
        wchar_t requestedLayout[16] = {};
        const DWORD layoutLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_DESCRIPTION_LAYOUT", requestedLayout, _countof(requestedLayout));
        require(layoutLength && layoutLength < _countof(requestedLayout) && _Settings.GetDescriptionLayout() == requestedLayout, "description layout applied");
        m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
        BOOL handled = FALSE; OnFileNew(0, ID_FILE_NEW, NULL, handled);
        const bool immediatelyClean = !DocChanged(); pump();
        const auto state = m_doc->CaptureUndoSavepoints();
        CString currentHtml;
        MSHTML::IHTMLElementPtr stateBody(m_doc->m_body.Document()->all->item(L"fbw_body"));
        if (stateBody) currentHtml = static_cast<LPCWSTR>(stateBody->innerHTML);
        CStringA evidence; evidence.Format("new_immediate_clean=%d;after_pump_clean=%d;namevalid=%d;saved_version=%ld;current_version=%ld;form_changed=%d;body_same=%d;has_saved=%d\r\n", immediatelyClean, !DocChanged(), m_doc->m_namevalid, state.savedVersion, m_doc->m_body.GetVersionNumber(), m_doc->m_body.IsFormChanged(), state.savedHtml == currentHtml, state.hasSaved);
        DWORD evidenceBytes = 0; output.Write(evidence, evidence.GetLength(), &evidenceBytes); output.Flush();
        require(!DocChanged() && !m_doc->m_namevalid, "new document clean");
        ::ShowWindow(m_hWnd, SW_SHOWNORMAL); ::SetForegroundWindow(m_hWnd);
        ShowView(DESC); pump();
        MSHTML::IHTMLDocument3Ptr descriptionHtmlDoc(m_doc->m_body.Document());
        MSHTML::IHTMLInputTextElementPtr undoTitle(descriptionHtmlDoc->getElementById(L"tiTitle"));
        MSHTML::IHTMLInputTextElementPtr undoKeywords(descriptionHtmlDoc->getElementById(L"tiKwd"));
        require(undoTitle && undoKeywords, "description undo controls");
        const CString initialTitle(static_cast<LPCWSTR>(undoTitle->value));
        const CString initialKeywords(static_cast<LPCWSTR>(undoKeywords->value));
        auto typeTitle = [&]() {
            MSHTML::IHTMLElement2Ptr titleElement(undoTitle); titleElement->focus(); undoTitle->select(); pump();
            const HWND focus = ::GetFocus(); require(focus != NULL, "description undo focus");
            ::SendMessageW(focus, WM_CHAR, L'X', 1); pump();
            require(CString(static_cast<LPCWSTR>(undoTitle->value)) != initialTitle && DocChanged(), "description typed dirty");
        };
        typeTitle();
        SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
        require(CString(static_cast<LPCWSTR>(undoTitle->value)) == initialTitle && !DocChanged(), "description undo restores clean state");
        SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
        require(CString(static_cast<LPCWSTR>(undoTitle->value)) != initialTitle && DocChanged(), "description redo restores dirty state");
        SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
        require(!DocChanged(), "description second undo clean");
        undoKeywords->value = L"unrelated pending keywords";
        typeTitle();
        SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
        require(CString(static_cast<LPCWSTR>(undoTitle->value)) == initialTitle && DocChanged(), "description undo preserves unrelated edit");
        undoKeywords->value = static_cast<LPCWSTR>(initialKeywords);
        typeTitle();
        SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
        require(!DocChanged(), "description undo clean after restoring unrelated edit");
        ShowView(BODY); m_doc->m_body.SetFocus();
        MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
        MSHTML::IHTMLElement2Ptr body(MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"fbw_body"));
        const auto divs = body->getElementsByTagName(L"DIV"); MSHTML::IHTMLElementPtr bookBody;
        for (long i = 0; divs && i < divs->length; ++i) {
            MSHTML::IHTMLElementPtr div(divs->item(_variant_t(i), _variant_t()));
            if (div && U::scmp(div->className, L"body") == 0) { bookBody = div; break; }
        }
        require(bookBody != nullptr, "book body element");
        auto paragraphs = MSHTML::IHTMLElement2Ptr(bookBody)->getElementsByTagName(L"P");
        MSHTML::IHTMLElementPtr paragraph(paragraphs->item(_variant_t(0L), _variant_t()));
        require(paragraph != nullptr, "initial paragraph");
        const CString original(static_cast<LPCWSTR>(paragraph->innerText));
        MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
        m_doc->m_body.SetFocus(); pump();
        range->moveToElementText(paragraph); range->collapse(VARIANT_TRUE); range->move(L"character", 3); range->select();
        const HWND focused = ::GetFocus(); require(focused != NULL, "body focus");
        wchar_t focusClass[128] = {}; ::GetClassNameW(focused, focusClass, _countof(focusClass));
        MSHTML::IHTMLTxtRangePtr activeRange(document->selection->createRange());
        MSHTML::IHTMLElementPtr activeParent(activeRange ? activeRange->parentElement() : nullptr);
        CStringA selectionEvidence; selectionEvidence.Format("focus_class=%S;focus_inside_view=%d;selection_tag=%S;selection_class=%S;selection_is_target=%d\r\n", focusClass,
            focused == m_doc->m_body.m_hWnd || ::IsChild(m_doc->m_body.m_hWnd, focused),
            activeParent ? static_cast<LPCWSTR>(activeParent->tagName) : L"", activeParent ? static_cast<LPCWSTR>(activeParent->className) : L"", activeParent == paragraph);
        DWORD selectionBytes = 0; output.Write(selectionEvidence, selectionEvidence.GetLength(), &selectionBytes); output.Flush();
        if (activeParent) {
            const CString selectedText(static_cast<LPCWSTR>(activeParent->innerText));
            CStringA selectionText; selectionText.Format("selection_length=%d;target_length=%d;selection_equals_target_text=%d\r\n", selectedText.GetLength(), original.GetLength(), selectedText == original);
            DWORD bytes = 0; output.Write(selectionText, selectionText.GetLength(), &bytes); output.Flush();
        }
        ::SendMessageW(focused, WM_CHAR, L'Z', 1); pump();
        const CString actual(static_cast<LPCWSTR>(paragraph->innerText));
        CStringA inputEvidence; inputEvidence.Format("typed_original_length=%d;typed_actual_length=%d;typed_z_delta=%d\r\n", original.GetLength(), actual.GetLength(), actual.GetLength() - original.GetLength());
        DWORD inputBytes = 0; output.Write(inputEvidence, inputEvidence.GetLength(), &inputBytes); output.Flush();
        const CString edited = original.Left(3) + L"Z" + original.Mid(3);
        require(CString(static_cast<LPCWSTR>(paragraph->innerText)) == edited, "body typed text");
        require(DocChanged(), "body edit dirty");
        SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
        CStringA undoEvidence; undoEvidence.Format("undo_text_matches=%d;undo_clean=%d;version=%ld\r\n", CString(static_cast<LPCWSTR>(paragraph->innerText)) == original, !DocChanged(), m_doc->m_body.GetVersionNumber());
        DWORD undoBytes = 0; output.Write(undoEvidence, undoEvidence.GetLength(), &undoBytes); output.Flush();
        require(CString(static_cast<LPCWSTR>(paragraph->innerText)) == original && !DocChanged(), "body undo restores clean state");
        SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
        require(CString(static_cast<LPCWSTR>(paragraph->innerText)) == edited && DocChanged(), "body redo restores dirty state");
        ShowView(DESC); pump();
        MSHTML::IHTMLInputElementPtr title(MSHTML::IHTMLDocument3Ptr(m_doc->m_body.Document())->getElementById(L"tiTitle"));
        require(title != nullptr, "description title field");
        MSHTML::IHTMLElement2Ptr titleElement(title); titleElement->focus(); pump();
        title->value = L"NEW_DESCRIPTION_EDIT"; pump();
        require(DocChanged(), "description edit dirty");
        const HRESULT metadata = m_doc->m_body.Document()->parentWindow->execScript(
            L"document.getElementById('tiGenre').getElementsByTagName('DIV')[0].all.genre.value='prose';"
            L"document.getElementById('tiAuthor').getElementsByTagName('DIV')[0].all.first.value='Test';"
            L"document.getElementById('tiAuthor').getElementsByTagName('DIV')[0].all.last.value='Author';"
            L"document.getElementById('diID').value='new-edit-state-test';"
            L"document.getElementById('diVersion').value='1.0';"
            L"document.getElementById('diDate').value='2026-01-01';"
            L"document.getElementById('diDateVal').value='2026-01-01';", L"JScript");
        require(SUCCEEDED(metadata), "valid description fixture");
        std::atomic<bool> stop(false); const DWORD thread = ::GetCurrentThreadId();
        std::thread dismiss([&]() {
            while (!stop.load()) {
                ::EnumThreadWindows(thread, [](HWND window, LPARAM) -> BOOL {
                    wchar_t className[32] = {}; ::GetClassNameW(window, className, _countof(className));
                    if (::IsWindowVisible(window) && wcscmp(className, L"#32770") == 0) ::PostMessageW(window, WM_CLOSE, 0, 0);
                    return TRUE;
                }, 0);
                ::Sleep(20);
            }
        });
        bool saved = false;
        try { saved = m_doc->Save(path.data()); }
        catch (...) { stop = true; dismiss.join(); throw; }
        stop = true; dismiss.join();
        require(saved, "save new document");
        require(!DocChanged(), "saved document clean");
        const FILE_OP_STATUS reopened = LoadFile(path.data()); pump();
        CStringA openEvidence; openEvidence.Format("reopen_result=%d;reopen_dirty=%d\r\n", static_cast<int>(reopened), DocChanged());
        DWORD openBytes = 0; output.Write(openEvidence, openEvidence.GetLength(), &openBytes); output.Flush();
        require(reopened == OK && !DocChanged(), "reopen saved document clean");
        const CString xml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
        require(xml.Find(L"NEW_DESCRIPTION_EDIT") >= 0 && xml.Find(edited) >= 0, "reopened body and description content");
        OnFileNew(0, ID_FILE_NEW, NULL, handled); pump();
        require(!DocChanged() && !m_doc->m_namevalid, "second new document clean");
        ShowView(DESC); pump();
        MSHTML::IHTMLInputElementPtr secondTitle(MSHTML::IHTMLDocument3Ptr(m_doc->m_body.Document())->getElementById(L"tiTitle"));
        require(secondTitle != nullptr, "new description title field");
        MSHTML::IHTMLElement2Ptr secondTitleElement(secondTitle); secondTitleElement->focus(); pump();
        secondTitle->value = L"DESCRIPTION_ONLY_EDIT"; pump();
        require(DocChanged(), "description only edit dirty");
        auto answerNew = [&](int answer) {
            std::atomic<bool> finished(false), found(false); const DWORD uiThread = ::GetCurrentThreadId();
            std::thread responder([&]() {
                const ULONGLONG until = ::GetTickCount64() + 8000;
                while (!finished.load() && ::GetTickCount64() < until) {
                    struct Prompt { HWND window; } prompt = {};
                    ::EnumThreadWindows(uiThread, [](HWND window, LPARAM data) -> BOOL {
                        auto& p = *reinterpret_cast<Prompt*>(data);
                        if (::IsWindowVisible(window) && ::GetDlgItem(window, IDYES) && ::GetDlgItem(window, IDNO) && ::GetDlgItem(window, IDCANCEL)) { p.window = window; return FALSE; }
                        return TRUE;
                    }, reinterpret_cast<LPARAM>(&prompt));
                    if (prompt.window && !found.exchange(true)) ::PostMessageW(prompt.window, WM_COMMAND, MAKEWPARAM(answer, BN_CLICKED), reinterpret_cast<LPARAM>(::GetDlgItem(prompt.window, answer)));
                    ::Sleep(10);
                }
                if (!finished.load()) ::PostThreadMessageW(uiThread, WM_QUIT, 93, 0);
            });
            SendMessage(WM_COMMAND, ID_FILE_NEW); finished = true; responder.join(); return found.load();
        };
        FB::Doc* const descriptionDocument = m_doc;
        require(answerNew(IDCANCEL), "description cancel prompt");
        require(m_doc == descriptionDocument && DocChanged() && CString(static_cast<LPCWSTR>(secondTitle->value)) == L"DESCRIPTION_ONLY_EDIT", "cancel preserves description change");
        require(answerNew(IDNO), "description discard prompt"); pump();
        require(m_doc != descriptionDocument && !DocChanged(), "discard creates clean document");
        if (RuntimeTests::IsScenario(L"new-document-edit-state-runtime")) {
            wchar_t diagnostic[2] = {};
            if (::GetEnvironmentVariableW(L"FBE_NEXT_TEST_DISABLE_NEW_BASELINE", diagnostic, _countof(diagnostic)) == 1 && diagnostic[0] == L'1') {
                m_doc->MarkSavePoint();
                const char note[] = "diagnostic_baseline_disabled=1\r\n"; DWORD bytes = 0; output.Write(note, sizeof(note) - 1, &bytes); output.Flush();
            }
        }
        ShowView(BODY); m_doc->m_body.SetFocus(); pump();
        MSHTML::IHTMLDocument2Ptr binaryDocument(m_doc->m_body.Document());
        MSHTML::IHTMLElement2Ptr binaryBody(MSHTML::IHTMLDocument3Ptr(binaryDocument)->getElementById(L"fbw_body"));
        auto binaryDivs = binaryBody->getElementsByTagName(L"DIV"); MSHTML::IHTMLElementPtr binaryBook;
        for (long i = 0; binaryDivs && i < binaryDivs->length; ++i) {
            MSHTML::IHTMLElementPtr div(binaryDivs->item(_variant_t(i), _variant_t()));
            if (div && U::scmp(div->className, L"body") == 0) { binaryBook = div; break; }
        }
        require(binaryBook != nullptr, "binary book body");
        const auto binaryParagraphs = MSHTML::IHTMLElement2Ptr(binaryBook)->getElementsByTagName(L"P");
        MSHTML::IHTMLElementPtr binaryParagraph(binaryParagraphs->item(_variant_t(0L), _variant_t()));
        require(binaryParagraph != nullptr, "binary insertion selection");
        MSHTML::IHTMLTxtRangePtr binaryRange(MSHTML::IHTMLBodyElementPtr(binaryDocument->body)->createTextRange());
        binaryRange->moveToElementText(binaryParagraph); binaryRange->collapse(VARIANT_TRUE); binaryRange->move(L"character", 3); binaryRange->select();
        std::vector<wchar_t> scriptPath(32768);
        const DWORD scriptLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_NEW_EDIT_SCRIPT", scriptPath.data(), static_cast<DWORD>(scriptPath.size()));
        require(scriptLength && scriptLength < scriptPath.size(), "binary script path");
        CString binaryPath(scriptPath.data());
        const int separator = binaryPath.ReverseFind(L'\\');
        require(separator >= 0, "binary input directory");
        binaryPath = binaryPath.Left(separator + 1) + L"new-test.png";
        CString binaryError;
        const HRESULT importResult = m_doc->ImportBinary(binaryPath, binaryError);
        CStringA nativeEvidence; nativeEvidence.Format("native_binary_import=0x%08lX\r\n", static_cast<unsigned long>(importResult));
        DWORD nativeBytes = 0; output.Write(nativeEvidence, nativeEvidence.GetLength(), &nativeBytes); output.Flush();
        require(SUCCEEDED(importResult), "native binary import succeeded");
        const auto nativeXml = m_doc->CreateDOMForAnalysis();
        require(nativeXml && CString(static_cast<LPCWSTR>(nativeXml->xml)).Find(L"new-test.png") >= 0, "native binary stored");
        require(DocChanged(), "native binary only edit dirty");
        require(answerNew(IDNO), "binary discard prompt"); pump();
        require(!DocChanged(), "clean document after binary discard");
        CComPtr<IOleUndoManager> undoManager; IServiceProviderPtr services(m_doc->m_body.Document());
        const HRESULT serviceResult = services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&undoManager));
        DWORD openState = 0; const HRESULT openResult = undoManager ? undoManager->GetOpenParentState(&openState) : E_NOINTERFACE;
        const auto all = m_doc->m_body.Document()->all;
        const bool hasTitleCover = all && IDispatchPtr(all->item(L"tiCover"));
        const bool hasSourceCover = all && IDispatchPtr(all->item(L"stiCover"));
        const bool hasBinaryContainer = all && IDispatchPtr(all->item(L"binobj"));
        CStringA binaryEvidence; binaryEvidence.Format("binary_updates_open=%d;undo_service=0x%08lX;undo_parent=0x%08lX;undo_state=%lu;title_cover=%d;source_cover=%d;binary_container=%d;active_doc=%d\r\n", FbeScripts::HasDocumentUpdates(), static_cast<unsigned long>(serviceResult), static_cast<unsigned long>(openResult), static_cast<unsigned long>(openState), hasTitleCover, hasSourceCover, hasBinaryContainer, FB::Doc::m_active_doc == m_doc);
        DWORD binaryBytes = 0; output.Write(binaryEvidence, binaryEvidence.GetLength(), &binaryBytes); output.Flush();
        { FbeScriptDiagnostics::ScopedDialogSuppression suppressDialogs; m_doc->RunScript(scriptPath.data()); }
        openState = 0; const HRESULT afterScript = undoManager ? undoManager->GetOpenParentState(&openState) : E_NOINTERFACE;
        CStringA afterEvidence; afterEvidence.Format("after_script_parent=0x%08lX;after_script_state=%lu\r\n", static_cast<unsigned long>(afterScript), static_cast<unsigned long>(openState));
        DWORD afterBytes = 0; output.Write(afterEvidence, afterEvidence.GetLength(), &afterBytes); output.Flush();
        CComVariant binaryResult;
        { FbeScripts::ScriptExecutionScope context(scriptPath.data()); CheckError(m_doc->InvokeFunc(L"addNewBinary", nullptr, 0, binaryResult, true)); }
        if (binaryResult.vt == VT_BSTR) { CStringA resultText(CW2A(binaryResult.bstrVal, CP_UTF8)); CStringA line = "binary_result=" + resultText + "\r\n"; DWORD bytes = 0; output.Write(line, line.GetLength(), &bytes); output.Flush(); }
        require(binaryResult.vt == VT_BSTR && CString(binaryResult.bstrVal) == L"new-test.png", "binary script succeeded");
        pump();
        const auto binaryXml = m_doc->CreateDOMForAnalysis();
        require(binaryXml && CString(static_cast<LPCWSTR>(binaryXml->xml)).Find(L"new-test.png") >= 0, "new binary stored");
        require(DocChanged(), "binary only edit dirty");
        ShowView(DESC); pump();
        MSHTML::IHTMLDocument3Ptr binaryDescription(m_doc->m_body.Document());
        MSHTML::IHTMLInputTextElementPtr binaryTitle(binaryDescription->getElementById(L"tiTitle"));
        require(binaryTitle != nullptr, "binary description title");
        const CString binaryOriginalTitle(static_cast<LPCWSTR>(binaryTitle->value));
        MSHTML::IHTMLElement2Ptr binaryTitleElement(binaryTitle); binaryTitleElement->focus(); binaryTitle->select(); pump();
        const HWND binaryFocus = ::GetFocus(); require(binaryFocus != NULL, "binary description focus");
        ::SendMessageW(binaryFocus, WM_CHAR, L'X', 1); pump();
        require(CString(static_cast<LPCWSTR>(binaryTitle->value)) != binaryOriginalTitle, "binary description typed");
        SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
        require(CString(static_cast<LPCWSTR>(binaryTitle->value)) == binaryOriginalTitle && DocChanged(), "description undo preserves pending binary");
        passed = true; details = "body typing undo redo, description cancel, binary dirty and save reopen PASS";
    } catch (const _com_error& error) { CStringA code; code.Format(" hr=0x%08lX", static_cast<unsigned long>(error.Error())); details += code; }
    catch (...) { details = "native exception"; }
    CStringA report; report.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString());
    DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
}
if(IsFbeTestScenario(L"new-document-safety-runtime")) {
    bool passed=false;CStringA details="start";
    auto require=[&](bool ok,const char* label){details=label;if(!ok)CheckError(E_FAIL);};
    auto bodyHistoryEmpty=[&](){
        IServiceProviderPtr provider(m_doc->m_body.Document()); CComPtr<IOleUndoManager> manager;
        if(!provider) return false;
        CheckError(provider->QueryService(SID_SOleUndoManager,IID_IOleUndoManager,reinterpret_cast<void**>(&manager)));
        if(!manager) return false;
        for(bool redo : {false,true}) {
            CComPtr<IEnumOleUndoUnits> units; CheckError(redo ? manager->EnumRedoable(&units) : manager->EnumUndoable(&units));
            if(!units) return false;
            CComPtr<IOleUndoUnit> unit; const HRESULT result=units->Next(1,&unit,NULL);
            if(result!=S_FALSE) { CheckError(result); return false; }
        }
        return true;
    };
    auto phase=[&](const char* label){CStringA row=CStringA("phase=")+label+"\r\n";DWORD n=0;output.Write(row,row.GetLength(),&n);output.Flush();};
    struct ObserverGuard {~ObserverGuard(){NewDocumentRuntimeProbe::Observer()={};::SetEnvironmentVariableW(L"FBE_NEXT_FAULT_INJECT",nullptr);}} observerGuard;
    extern CElementDescMnr _EDMnr;
    struct CleanupGuard {
        std::vector<std::unique_ptr<CElementDescriptor>> descriptors;
        ~CleanupGuard() {
            DocumentCleanupRuntimeProbe::OnDetach()={};DocumentCleanupRuntimeProbe::OnScript()={};
            for(const auto& descriptor:descriptors) DocumentCleanupRuntimeProbe::Remove(_EDMnr,descriptor.get());
        }
    } cleanupGuard;
    try {
        const CString first=m_doc->m_filename;
        auto clean=[&](){m_doc->MarkSavePoint();m_source.SendMessage(SCI_SETSAVEPOINT);};
        auto xml=[&](){return CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));};
#include "RuntimeTestDocumentCleanupContext.inl"
        unsigned prepared=0,detached=0,heldSpellerReferences=0;bool order=true;
        NewDocumentRuntimeProbe::Observer()=[&](bool released){
            if(!released){++prepared;heldSpellerReferences=(std::max)(heldSpellerReferences,NewDocumentRuntimeProbe::References(m_Speller));}
            else {
                ++detached;
                order=order&&FB::Doc::m_active_doc==(m_document_session.HasDocument()?m_doc:nullptr)&&m_doc->m_body.IsWindow()&&!NewDocumentRuntimeProbe::References(m_Speller)&&
                    !m_editor_selection_state.BodyRange()&&!m_editor_selection_state.DescriptionRange()&&
                    !m_selection_context.container&&!m_selection_context.structuralContainer&&!m_selection_context.image&&
                    !m_selection_context.section&&!m_selection_context.table&&!m_selection_context.tableCell&&!m_selection_context.anchor&&
                    !m_find_results_pane.AttachedView()&&m_document_tree.m_tree.m_tree.GetCount()==0;
            }
        };
        auto created=[&](FB::Doc* old){
            require(m_doc!=old&&m_document_session.HasDocument()&&FB::Doc::m_active_doc==m_doc,"new identity and active document");
            require(!m_doc->m_namevalid&&m_document_session.Location().storagePath.IsEmpty()&&!m_document_session.IsArchive(),"new unsaved session");
            require(!DocChanged(),"new document initially clean without resetting savepoint");
            require(m_doc->m_body.HasDoc()&&m_view.GetActiveWnd()==m_doc->m_body&&!IsSourceActive(),"new Body visible");
            require(order&&prepared==detached,"detach before destroying old host");
            require(m_source.SendMessage(SCI_GETLENGTH)==0&&!m_source.SendMessage(SCI_CANUNDO)&&!m_source.SendMessage(SCI_CANREDO),"new Source and history cleared");
            require(bodyHistoryEmpty(),"new Body history empty");
            require(xml().Find(L"NEW_OLD_UNIQUE")<0,"old content absent");
            require(m_document_tree.m_tree.m_tree.GetCount()>0,"new structure built");
            require(_Settings.GetUseSpellChecker()?m_Speller&&m_Speller->Enabled()&&NewDocumentRuntimeProbe::Attached(m_Speller,m_doc->m_body.Document()):!NewDocumentRuntimeProbe::References(m_Speller)&&!m_Speller->Enabled(),"spell references match new document or remain detached");
        };
        auto newCommand=[&](){FB::Doc* old=m_doc;SendMessage(WM_COMMAND,ID_FILE_NEW);created(old);};
        auto pumpMessages=[](){const ULONGLONG until=::GetTickCount64()+100;
            while(::GetTickCount64()<until){MSG message={};while(::PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){::TranslateMessage(&message);::DispatchMessageW(&message);}::Sleep(1);}
        };
        auto answerNew=[&](int answer,WORD command=ID_FILE_NEW,bool expectPrompt=true){
            std::atomic<bool> stop(false);bool found=false;const DWORD thread=::GetCurrentThreadId();
            std::thread driver([&](){const ULONGLONG until=::GetTickCount64()+8000;
                while(!stop.load()&&::GetTickCount64()<until){
                    struct Prompt{HWND window;int answer;} prompt={nullptr,answer};
                    ::EnumThreadWindows(thread,[](HWND window,LPARAM data)->BOOL{
                        auto& prompt=*reinterpret_cast<Prompt*>(data);
                        if(::IsWindowVisible(window)&&::GetDlgItem(window,IDYES)&&::GetDlgItem(window,IDNO)&&::GetDlgItem(window,IDCANCEL)) {prompt.window=window;return FALSE;}return TRUE;
                    },reinterpret_cast<LPARAM>(&prompt));
                    if(prompt.window&&!found){found=true;::PostMessageW(prompt.window,WM_COMMAND,MAKEWPARAM(answer,BN_CLICKED),reinterpret_cast<LPARAM>(::GetDlgItem(prompt.window,answer)));}
                    ::Sleep(10);
                }
                if(!stop.load())::PostThreadMessageW(thread,WM_QUIT,93,0);
            });
            SendMessage(WM_COMMAND,command);stop=true;driver.join();
            require(expectPrompt ? found : !found, expectPrompt ? "save decision answered" : "unexpected save decision");
        };
        phase("spell enabled: old interfaces observed and detached");
        _Settings.SetUseSpellChecker(true);ApplyConfChanges(false,false);clean();
        require(NewDocumentRuntimeProbe::References(m_Speller)>0&&m_Speller->Enabled()&&NewDocumentRuntimeProbe::Attached(m_Speller,m_doc->m_body.Document()),"old document has real spell COM references");
        m_editor_selection_state.BodyRange()=MSHTML::IHTMLBodyElementPtr(m_doc->m_body.Document()->body)->createTextRange();
        m_selection_context.container=m_doc->m_body.Document()->body;
        m_find_results_pane.Attach(&m_doc->m_body);
        // Exercise the actual File/New handler. The legacy default Ctrl+N
        // belongs to Add Epigraph; do not change the user's shortcut mapping.
        newCommand();
        require(heldSpellerReferences>0,"instrumented old spell interfaces before cleanup");
        phase("repeat open-new-open-close with spelling");
        for(int cycle=0;cycle<2;++cycle){clean();require(LoadFile(first)==OK,"open before New");clean();newCommand();clean();require(LoadFile(first)==OK,"open after New");clean();SendMessage(WM_COMMAND,ID_FILE_CLOSE);require(!m_document_session.HasDocument()&&!NewDocumentRuntimeProbe::References(m_Speller),"close after New");newCommand();}
        phase("spell disabled; consecutive new books");
        _Settings.SetUseSpellChecker(false);ApplyConfChanges(false,false);clean();newCommand();clean();newCommand();
        phase("untouched New after message pump then New and Open");
        pumpMessages();require(!DocChanged(),"untouched New remains clean after message pump");
        { FB::Doc* previous=m_doc;answerNew(IDNO,ID_FILE_NEW,false);created(previous); }
        pumpMessages();require(!DocChanged(),"second untouched New remains clean after message pump");
        require(LoadFile(first)==OK,"open after untouched New without save prompt");
        _Settings.SetUseSpellChecker(true);ApplyConfChanges(false,false);ShowView(SOURCE);
        const auto sourceBeforeEdit=FbeScripts::SourceBytes(m_source);
        const size_t editAt=sourceBeforeEdit.find("NEW_OLD_UNIQUE");require(editAt!=std::string::npos,"Source edit target");
        m_source.SendMessage(SCI_SETSEL,editAt,editAt+strlen("NEW_OLD_UNIQUE"));
        m_source.SendMessage(SCI_REPLACESEL,0,reinterpret_cast<LPARAM>("NEW_SOURCE_SAVED"));
        FB::Doc* old=m_doc;const auto oldSource=FbeScripts::SourceBytes(m_source);const CString oldXml=xml(),oldPath=m_document_session.Location().storagePath;
        const unsigned beforeCancel=prepared;const auto oldSpell=m_Speller;
        phase("dirty Source cancel");answerNew(IDCANCEL);
        require(m_doc==old&&FB::Doc::m_active_doc==old&&m_document_session.Location().storagePath==oldPath&&IsSourceActive()&&DocChanged()&&xml()==oldXml&&FbeScripts::SourceBytes(m_source)==oldSource&&m_source.SendMessage(SCI_CANUNDO)&&prepared==beforeCancel&&m_Speller==oldSpell&&NewDocumentRuntimeProbe::Attached(m_Speller,m_doc->m_body.Document()),"cancel preserves Source, history, identity and spell attachment");
        phase("failed preparation preserves old state and interfaces");
        for(LPCWSTR fault:{L"api-load-return-false",L"api-load-exception"}) {
            const auto source=FbeScripts::SourceBytes(m_source);const CString content=xml();
            const auto range=MSHTML::IHTMLBodyElementPtr(m_doc->m_body.Document()->body)->createTextRange();
            m_editor_selection_state.BodyRange()=range;
            m_find_results_pane.Attach(&m_doc->m_body);const UINT treeCount=m_document_tree.m_tree.m_tree.GetCount();
            ::SetEnvironmentVariableW(L"FBE_NEXT_FAULT_INJECT",fault);
            answerNew(IDNO);::SetEnvironmentVariableW(L"FBE_NEXT_FAULT_INJECT",nullptr);
            require(m_doc==old&&FB::Doc::m_active_doc==old&&m_document_session.Location().storagePath==oldPath&&xml()==content&&FbeScripts::SourceBytes(m_source)==source&&IsSourceActive()&&DocChanged()&&m_source.SendMessage(SCI_CANUNDO)&&prepared==beforeCancel&&m_editor_selection_state.BodyRange()==range&&m_find_results_pane.AttachedView()==&m_doc->m_body&&m_document_tree.m_tree.m_tree.GetCount()==treeCount&&NewDocumentRuntimeProbe::Attached(m_Speller,m_doc->m_body.Document()),"failed preparation rollback");
        }
        // Save the same dirty named document through the normal decision.
        phase("dirty Source save then create");old=m_doc;answerNew(IDYES);created(old);
        clean();require(LoadFile(first)==OK,"reopen saved old file");ShowView(SOURCE);
        require(FbeScripts::SourceBytes(m_source).find("NEW_SOURCE_SAVED")!=std::string::npos,"New saved original changes");
        clean();newCommand();ShowView(SOURCE);require(FbeScripts::SourceBytes(m_source).find("NEW_SOURCE_SAVED")==std::string::npos&&!m_source.SendMessage(SCI_CANUNDO),"Source rebuilt from new document");
        ShowView(BODY);
        pumpMessages();
        require(!DocChanged(),"disposable book clean before EPUB import");
        phase("EPUB import then New");
        WORD importCommand=0;
        for(int i=0;i<m_plugins.ImportPlugins().GetSize();++i){const auto* plugin=m_plugins.Manager().FindPlugin(m_plugins.ImportPlugins()[i]);if(plugin&&plugin->id==L"import-epub")importCommand=static_cast<WORD>(ID_IMPORT_BASE+i);}
        require(importCommand!=0,"EPUB importer available");
        wchar_t epub[32768]={};::GetEnvironmentVariableW(L"FBE_NEXT_TEST_NEW_EPUB",epub,_countof(epub));
        ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",L"import-epub");::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH",epub);
        // An untouched new document must not prompt before importing EPUB.
        answerNew(IDNO,importCommand,false);
        ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",L"new-document-safety-runtime");::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH",nullptr);
        require(m_document_session.HasDocument()&&!m_doc->m_namevalid&&DocChanged(),"import prepared");clean();newCommand();
        require(order&&prepared==detached,"all New replacements detached exactly once");
        if(cleanupContext) {
            phase("cleanup exceptions, reload and RAII unwind");
            clean();cleanupThrowScript=true;newCommand();cleanupThrowScript=false;
            phase("script exception completed without a modal message");
            clean();cleanupThrowNative=true;newCommand();cleanupThrowNative=false;
            phase("native cleanup exception contained");
            clean();DocumentLifecycleController reload(*this,m_doc,m_document_session,m_view);
            require(reload.ReloadNormal(first).Succeeded(),"reload cleanup context");AttachDocument(m_doc);
            clean();SendMessage(WM_COMMAND,ID_FILE_CLOSE);
            require(LoadFile(first)==OK,"closed host to Open context");
            clean();SendMessage(WM_COMMAND,ID_FILE_CLOSE);
            ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",L"import-epub");::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH",epub);
            answerNew(IDNO,importCommand,false);
            ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",L"new-document-safety-runtime");::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH",nullptr);
            require(m_document_session.HasDocument()&&!m_doc->m_namevalid,"closed host to EPUB import context");
            clean();FB::Doc* before=m_doc;const CString beforeXml=xml();const auto beforeRefs=NewDocumentRuntimeProbe::References(m_Speller);
            cleanupUnwind=true;bool caught=false;
            try { DocumentLifecycleController lifecycle(*this,m_doc,m_document_session,m_view);lifecycle.NewDocument(); }
            catch(const _com_error&) {caught=true;}
            cleanupUnwind=false;
            require(caught&&m_doc==before&&FB::Doc::m_active_doc==before&&xml()==beforeXml&&NewDocumentRuntimeProbe::References(m_Speller)==beforeRefs,"RAII restores old active document on unwind");
            require(cleanupCorrect&&cleanupBooks>0&&cleanupClosed>=3&&cleanupScriptFailures==2&&cleanupNativeFailures==1,"all cleanup contexts, closed hosts and exceptions checked");
            CStringA contextEvidence;contextEvidence.Format("cleanup_books=%u;closed_hosts=%u;script_failures=%u;native_failures=%u\r\n",cleanupBooks,cleanupClosed,cleanupScriptFailures,cleanupNativeFailures);DWORD bytes=0;output.Write(contextEvidence,contextEvidence.GetLength(),&bytes);
        }
        CStringA evidence;evidence.Format("prepared=%u;detached=%u;old_spell_refs=%u\r\n",prepared,detached,heldSpellerReferences);DWORD n=0;output.Write(evidence,evidence.GetLength(),&n);
        passed=true;details="new document preparation, detach, rollback and state PASS";
    }catch(const _com_error& error){CStringA code;code.Format(" hr=0x%08lX",static_cast<unsigned long>(error.Error()));details+=code;}
    catch(...){details="native exception";}
    CStringA report;report.Format("result=%s\r\ndetails=%s\r\n",passed?"PASS":"FAIL",details.GetString());DWORD n=0;output.Write(report,report.GetLength(),&n);output.Close();::PostQuitMessage(passed?0:1);return 0;
}
