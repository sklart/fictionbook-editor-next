if(IsFbeTestScenario(L"new-document-safety-runtime")) {
    bool passed=false;CStringA details="start";
    auto require=[&](bool ok,const char* label){details=label;if(!ok)CheckError(E_FAIL);};
    auto phase=[&](const char* label){CStringA row=CStringA("phase=")+label+"\r\n";DWORD n=0;output.Write(row,row.GetLength(),&n);output.Flush();};
    struct ObserverGuard {~ObserverGuard(){NewDocumentRuntimeProbe::Observer()={};::SetEnvironmentVariableW(L"FBE_NEXT_FAULT_INJECT",nullptr);}} observerGuard;
    try {
        const CString first=m_doc->m_filename;
        auto clean=[&](){m_doc->MarkSavePoint();m_source.SendMessage(SCI_SETSAVEPOINT);};
        auto xml=[&](){return CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));};
        unsigned prepared=0,detached=0,heldSpellerReferences=0;bool order=true;
        NewDocumentRuntimeProbe::Observer()=[&](bool released){
            if(!released){++prepared;heldSpellerReferences=(std::max)(heldSpellerReferences,NewDocumentRuntimeProbe::References(m_Speller));}
            else {
                ++detached;
                order=order&&FB::Doc::m_active_doc==m_doc&&m_doc->m_body.IsWindow()&&!NewDocumentRuntimeProbe::References(m_Speller)&&
                    !m_editor_selection_state.BodyRange()&&!m_editor_selection_state.DescriptionRange()&&
                    !m_selection_context.container&&!m_selection_context.structuralContainer&&!m_selection_context.image&&
                    !m_selection_context.section&&!m_selection_context.table&&!m_selection_context.tableCell&&!m_selection_context.anchor&&
                    !m_find_results_pane.AttachedView()&&m_document_tree.m_tree.m_tree.GetCount()==0;
            }
        };
        auto created=[&](FB::Doc* old){
            require(m_doc!=old&&m_document_session.HasDocument()&&FB::Doc::m_active_doc==m_doc,"new identity and active document");
            require(!m_doc->m_namevalid&&m_document_session.Location().storagePath.IsEmpty()&&!m_document_session.IsArchive(),"new unsaved session");
            require(m_doc->m_body.HasDoc()&&m_view.GetActiveWnd()==m_doc->m_body&&!IsSourceActive(),"new Body visible");
            require(order&&prepared==detached,"detach before destroying old host");
            require(m_source.SendMessage(SCI_GETLENGTH)==0&&!m_source.SendMessage(SCI_CANUNDO)&&!m_source.SendMessage(SCI_CANREDO),"new Source and history cleared");
            require(!m_doc->m_body.CheckCommand(ID_EDIT_UNDO)&&!m_doc->m_body.CheckCommand(ID_EDIT_REDO),"new Body history empty");
            require(xml().Find(L"NEW_OLD_UNIQUE")<0,"old content absent");
            require(m_document_tree.m_tree.m_tree.GetCount()>0,"new structure built");
            require(_Settings.GetUseSpellChecker()?m_Speller&&m_Speller->Enabled()&&NewDocumentRuntimeProbe::Attached(m_Speller,m_doc->m_body.Document()):!NewDocumentRuntimeProbe::References(m_Speller)&&!m_Speller->Enabled(),"spell references match new document or remain detached");
        };
        auto newCommand=[&](){FB::Doc* old=m_doc;SendMessage(WM_COMMAND,ID_FILE_NEW);created(old);};
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
            SendMessage(WM_COMMAND,command);stop=true;driver.join();require(!expectPrompt||found,"save decision answered");
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
        clean();require(LoadFile(first)==OK,"Source fixture open");
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
        ShowView(BODY);clean();
        require(!DocChanged(),"disposable book clean before EPUB import");
        phase("EPUB import then New");
        WORD importCommand=0;
        for(int i=0;i<m_plugins.ImportPlugins().GetSize();++i){const auto* plugin=m_plugins.Manager().FindPlugin(m_plugins.ImportPlugins()[i]);if(plugin&&plugin->id==L"import-epub")importCommand=static_cast<WORD>(ID_IMPORT_BASE+i);}
        require(importCommand!=0,"EPUB importer available");
        wchar_t epub[32768]={};::GetEnvironmentVariableW(L"FBE_NEXT_TEST_NEW_EPUB",epub,_countof(epub));
        ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",L"import-epub");::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH",epub);
        // Import may update the disposable template's metadata while converting
        // the EPUB, so answer its normal discard prompt if one is displayed.
        answerNew(IDNO,importCommand,false);
        ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",L"new-document-safety-runtime");::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH",nullptr);
        require(m_document_session.HasDocument()&&!m_doc->m_namevalid&&DocChanged(),"import prepared");clean();newCommand();
        require(order&&prepared==detached,"all New replacements detached exactly once");
        CStringA evidence;evidence.Format("prepared=%u;detached=%u;old_spell_refs=%u\r\n",prepared,detached,heldSpellerReferences);DWORD n=0;output.Write(evidence,evidence.GetLength(),&n);
        passed=true;details="new document preparation, detach, rollback and state PASS";
    }catch(const _com_error& error){CStringA code;code.Format(" hr=0x%08lX",static_cast<unsigned long>(error.Error()));details+=code;}
    catch(...){details="native exception";}
    CStringA report;report.Format("result=%s\r\ndetails=%s\r\n",passed?"PASS":"FAIL",details.GetString());DWORD n=0;output.Write(report,report.GetLength(),&n);output.Close();::PostQuitMessage(passed?0:1);return 0;
}
