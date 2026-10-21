if (IsFbeTestScenario(L"file-close-runtime")) {
    extern CElementDescMnr _EDMnr;
    bool passed=false;CStringA details="start";
    auto require=[&](bool ok,const char* label){details=label;if(!ok)CheckError(E_FAIL);};
    auto phase=[&](const char* label){CStringA row=CStringA("phase=")+label+"\r\n";DWORD n=0;output.Write(row,row.GetLength(),&n);output.Flush();};
    try {
        const CString first=m_document_session.Location().storagePath;
        std::vector<wchar_t> second(32768);::GetEnvironmentVariableW(L"FBE_NEXT_TEST_CLOSE_SECOND",second.data(),static_cast<DWORD>(second.size()));
        const bool extensive=!m_document_session.IsArchive();
        // Deliberately keep both selected and unselected filters across close.
        for (int index : {0, 2}) {
            const bool selected = index == 2;
            if (_EDMnr.GetStED(index)->ViewInTree() != selected)
                m_document_tree.m_tree.SendMessage(WM_COMMAND, IDC_TREE_ST_BASE + index);
        }
        std::vector<bool> elementFilters;
        for (int i=0;i<_EDMnr.GetStEDsCount();++i) elementFilters.push_back(_EDMnr.GetStED(i)->ViewInTree());
        auto filtersPreserved=[&](){
            for (int i=0;i<_EDMnr.GetStEDsCount();++i) {
                const auto descriptor=_EDMnr.GetStED(i);
                require(descriptor->ViewInTree()==elementFilters[i],"close preserved element filters");
                require(_Settings.GetDocTreeItemState(descriptor->GetCaption(),!elementFilters[i])==elementFilters[i],"close preserved saved element preferences");
            }
        };
        auto recent=[&](){std::vector<CString> names;for(int i=0;i<m_recentDocuments.List().m_arrDocs.GetSize();++i)names.emplace_back(m_recentDocuments.List().m_arrDocs[i].szDocName);return names;};
        auto clean=[&](){m_doc->MarkSavePoint();m_source.SendMessage(SCI_SETSAVEPOINT);};
        auto closed=[&](){
            filtersPreserved();
            OnIdle();CString title;GetWindowText(title);
            require(::IsWindow(m_hWnd)&&!m_document_session.HasDocument()&&!DocChanged(),"closed session/window");
            require(title==L"FictionBook Editor Next"&&FB::Doc::m_active_doc==nullptr,"closed title/active");
            require(m_view.GetActiveWnd()==nullptr&&!m_source.IsWindowVisible()&&!m_doc->m_body.IsWindowVisible(),"closed panes");
            for(UINT command:{ID_VIEW_DESC,ID_VIEW_BODY,ID_VIEW_SOURCE})
                require((m_CmdToolbar.SendMessage(TB_GETSTATE,command,0)&(TBSTATE_ENABLED|TBSTATE_CHECKED|TBSTATE_PRESSED))==0,"closed mode buttons disabled and unchecked");
            CClientDC display(m_view);CDC paint;paint.CreateCompatibleDC(display);
            CBitmap bitmap;bitmap.CreateCompatibleBitmap(display,4,4);HBITMAP old=paint.SelectBitmap(bitmap);
            m_view.SendMessage(WM_ERASEBKGND,reinterpret_cast<WPARAM>(paint.m_hDC));const COLORREF background=paint.GetPixel(1,1);paint.SelectBitmap(old);
            require(background==ThemeManager::WindowColor(),"empty workspace theme background");
            require(m_source.SendMessage(SCI_GETLENGTH)==0&&!m_source.SendMessage(SCI_CANUNDO)&&!m_source.SendMessage(SCI_CANREDO),"closed Source/history");
            require(m_document_tree.m_tree.m_tree.GetCount()==0&&!m_find_results_pane.AttachedView(),"closed tree/results");
            require(CString(static_cast<LPCWSTR>(m_doc->m_body.Document()->body->innerText)).Find(L"CLOSE_FIRST_UNIQUE")<0,"old content released");
            HMENU file=::GetSubMenu(m_MenuBar.GetMenu(),0);
            require((::GetMenuState(file,ID_FILE_SAVE,MF_BYCOMMAND)&MF_GRAYED)&&(::GetMenuState(file,ID_FILE_CLOSE,MF_BYCOMMAND)&MF_GRAYED)&&
                !(::GetMenuState(file,ID_FILE_NEW,MF_BYCOMMAND)&MF_GRAYED)&&!(::GetMenuState(file,ID_FILE_OPEN,MF_BYCOMMAND)&MF_GRAYED),"closed menu");
            require(::SendMessageW(m_MenuBar,TB_ISBUTTONENABLED,0,0)!=FALSE,"File menu available");
            const UINT panes[]={ID_DEFAULT_PANE,ID_PANE_POSITION,ID_PANE_SELECTION,ID_PANE_CHAR,ID_PANE_ENCODING,ID_PANE_VALIDATION,ID_PANE_INS};
            for(const auto pane:panes){CString text;m_status.GetPaneText(pane,text);require(text.IsEmpty(),"closed status");}
            FB::Doc* host=m_doc;SendMessage(WM_COMMAND,ID_FILE_CLOSE);SendMessage(WM_COMMAND,ID_FILE_SAVE);
            require(m_doc==host&&!m_document_session.HasDocument(),"closed commands inert");
        };
        auto askClose=[&](int answer,bool cancelSaveAs=false,WORD command=ID_FILE_CLOSE){
            std::atomic<bool> stop(false);bool found=false;const DWORD thread=::GetCurrentThreadId();
            std::thread driver([&](){
                HWND previous=nullptr;unsigned responses=0;const ULONGLONG until=::GetTickCount64()+8000;
                while(!stop.load()&&::GetTickCount64()<until){
                    if(previous&&!::IsWindow(previous))previous=nullptr;
                    struct Probe{HWND previous;HWND window;int answer;}probe{previous,nullptr,responses?(cancelSaveAs?IDCANCEL:IDOK):answer};
                    ::EnumThreadWindows(thread,[](HWND window,LPARAM data)->BOOL{
                        auto& probe=*reinterpret_cast<Probe*>(data);wchar_t name[64]={};::GetClassNameW(window,name,_countof(name));
                        if(window!=probe.previous&&::IsWindowVisible(window)&&(!wcscmp(name,L"#32770")||!wcscmp(name,L"FBEThemedMessageDialog"))&&::GetDlgItem(window,probe.answer)){
                            probe.window=window;return FALSE;
                        }return TRUE;
                    },reinterpret_cast<LPARAM>(&probe));
                    if(probe.window){HWND button=::GetDlgItem(probe.window,probe.answer);if(::IsWindowEnabled(button)){
                        ::PostMessageW(probe.window,WM_COMMAND,MAKEWPARAM(probe.answer,BN_CLICKED),reinterpret_cast<LPARAM>(button));previous=probe.window;++responses;found=true;
                    }}::Sleep(10);
                }if(!stop.load())::PostThreadMessageW(thread,WM_QUIT,91,0);
            });
            SendMessage(WM_COMMAND,command);stop=true;driver.join();require(found,"dialog responded");
        };
        auto dirtySource=[&](){ShowView(SOURCE);const auto text=FbeScripts::SourceBytes(m_source);
            const size_t at=text.find("CLOSE_SECOND_UNIQUE");require(at!=std::string::npos,"Source fixture marker");
            m_source.SendMessage(SCI_SETSEL,at,at+strlen("CLOSE_SECOND_UNIQUE"));
            m_source.SendMessage(SCI_REPLACESEL,0,reinterpret_cast<LPARAM>("CLOSE_UNSAVED"));require(DocChanged(),"Source dirty");};
        clean();phase("clean/hotkey close");const auto recentBefore=recent();
        auto* fileGroup=_Settings.GetGroupByName(L"File");auto* closeKey=fileGroup?_Settings.GetHotkeyByName(L"Close",*fileGroup):nullptr;
        require(closeKey&&closeKey->m_accel.key=='W'&&closeKey->m_accel.cmd==ID_FILE_CLOSE&&(closeKey->m_accel.fVirt&(FCONTROL|FSHIFT))==(FCONTROL|FSHIFT),"default Close hotkey");
        auto* styleGroup=_Settings.GetGroupByName(L"Style");auto* noteKey=styleGroup?_Settings.GetHotkeyByName(L"Note",*styleGroup):nullptr;
        require(noteKey&&noteKey->m_accel.key=='W'&&(noteKey->m_accel.fVirt&(FCONTROL|FSHIFT))==FCONTROL,"Ctrl+W remains Note");
        std::array<BYTE,256> keyboard={},keys={};::GetKeyboardState(keyboard.data());keys=keyboard;keys[VK_CONTROL]=0x80;keys[VK_SHIFT]=0x80;::SetKeyboardState(keys.data());
        MSG key={};key.hwnd=m_hWnd;key.message=WM_KEYDOWN;key.wParam='W';key.lParam=1;
        const BOOL translated=::TranslateAcceleratorW(m_hWnd,m_hAccel,&key);::SetKeyboardState(keyboard.data());require(translated!=FALSE,"Ctrl+Shift+W translated");closed();
        require(recentBefore==recent(),"close preserved MRU");
        phase("closed tree hide/show and mode preservation");
        for(bool scripts:{false,true}) {
            m_document_tree.m_tree.SendMessage(WM_COMMAND,scripts?ID_DOCUMENT_TREE_MODE_SCRIPTS:ID_DOCUMENT_TREE_MODE_STRUCTURE);
            for(int toggle=0;toggle<2;++toggle) {
                const bool visible=_Settings.ViewDocumentTree();
                SendMessage(WM_COMMAND,ID_VIEW_TREE);
                require(_Settings.ViewDocumentTree()!=visible,"closed tree panel toggled");
                require(m_document_tree.m_tree.m_tree.IsScriptMode()==scripts,"panel preserved tree mode");
                if(!scripts) require(m_document_tree.m_tree.m_tree.GetCount()==0,"no old or blank DOM nodes after showing tree");
                else require(m_document_tree.m_tree.m_tree.GetCount()>0,"script catalog survives panel toggle");
            }
        }
        m_document_tree.m_tree.SendMessage(WM_COMMAND,ID_DOCUMENT_TREE_MODE_STRUCTURE);closed();
        require(LoadFile(second.data())==OK,"tree scenario opened another book");
        require(m_document_tree.m_tree.m_tree.GetCount()>0,"other book structure restored");
        {
            auto& tree=m_document_tree.m_tree.m_tree;
            bool secondBook=false;
            for(HTREEITEM item=tree.GetRootItem();item;item=tree.GetNextSiblingItem(item)) {
                MSHTML::IHTMLElementPtr root(reinterpret_cast<MSHTML::IHTMLElement*>(tree.GetItemData(item)));
                if(root&&CString(static_cast<LPCWSTR>(root->outerHTML)).Find(L"CLOSE_SECOND_UNIQUE")>=0) secondBook=true;
            }
            require(secondBook,"tree belongs to second book");
        }
        clean();SendMessage(WM_COMMAND,ID_FILE_CLOSE);closed();
        {PendingDocument pending(*this,m_doc);pending.Rollback();}
        require(FB::Doc::m_active_doc==nullptr&&!m_document_session.HasDocument(),"rollback preserves closed state");
        if(extensive){
            phase("settings Apply without a document");askClose(IDOK,false,ID_VIEW_OPTIONS);closed();
            const bool spell=_Settings.GetUseSpellChecker();
            _Settings.SetUseSpellChecker(true);ApplyConfChanges(true,false);closed();
            _Settings.SetUseSpellChecker(false);ApplyConfChanges(false,false);closed();
            _Settings.SetUseSpellChecker(spell);
            phase("theme/background/source settings without a document");
            const InterfaceTheme theme=_Settings.GetInterfaceTheme();
            _Settings.SetInterfaceTheme(theme==INTERFACE_THEME_DARK?INTERFACE_THEME_LIGHT:INTERFACE_THEME_DARK,true);
            ApplyEditorBackgroundChanges(false);closed();ApplyXmlSourceEditorChanges(false);closed();
            _Settings.SetInterfaceTheme(theme,true);closed();
            phase("language without a document");
            const DWORD language=_Settings.GetInterfaceLanguageID();
            _Settings.SetInterfaceLanguage(language==1049?1033:1049,true);
            FbeResetRuntimeLocalization();RefreshLocalizedMainFrameUi();closed();
            _Settings.SetInterfaceLanguage(language,true);
            FbeResetRuntimeLocalization();RefreshLocalizedMainFrameUi();closed();
            phase("DPI message without a document");
            const UINT dpi=m_current_dpi;
            SendMessage(WM_DPICHANGED,MAKEWPARAM(dpi==144?96:144,dpi==144?96:144),0);closed();
            SendMessage(WM_DPICHANGED,MAKEWPARAM(dpi,dpi),0);closed();
            phase("script catalog/structure without a document");
            require(m_document_tree.IsWindowEnabled()!=FALSE,"navigation enabled");
            m_document_tree.m_tree.SendMessage(WM_COMMAND,ID_DOCUMENT_TREE_MODE_SCRIPTS);
            require(m_document_tree.m_tree.m_tree.IsScriptMode(),"script catalog enabled");
            SendMessage(WM_COMMAND,ID_TOOLS_REFRESH_SCRIPTS);
            require(m_document_tree.m_tree.m_tree.GetCount()>0&&RequiresOpenDocument(ID_SCRIPT_BASE+1),"catalog refreshed; execution requires a book");
            SendMessage(WM_COMMAND,ID_SCRIPT_BASE+1);SendMessage(WM_COMMAND,ID_LAST_SCRIPT);
            SendMessage(WM_COMMAND,ID_PLUGIN_EXPORT_FIRST);SendMessage(WM_COMMAND,ID_VIEW_SOURCE);
            m_document_tree.m_tree.SendMessage(WM_COMMAND,ID_DOCUMENT_TREE_MODE_STRUCTURE);closed();
            require(!m_document_tree.m_tree.m_tree.IsScriptMode(),"empty structure selected");
            // Also close with the script catalog selected; no DOM references may survive it.
            SendMessage(WM_COMMAND,ID_FILE_NEW);clean();
            m_document_tree.m_tree.SendMessage(WM_COMMAND,ID_DOCUMENT_TREE_MODE_SCRIPTS);
            SendMessage(WM_COMMAND,ID_FILE_CLOSE);
            m_document_tree.m_tree.SendMessage(WM_COMMAND,ID_DOCUMENT_TREE_MODE_STRUCTURE);closed();
        }
        fileGroup=_Settings.GetGroupByName(L"File");closeKey=fileGroup?_Settings.GetHotkeyByName(L"Close",*fileGroup):nullptr;
        require(closeKey!=nullptr,"Close hotkey after settings");
        const ACCEL originalClose=closeKey->m_accel;closeKey->m_accel.key='Q';_Settings.SaveHotkeyGroups();_Settings.LoadHotkeyGroups();RebuildRuntimeHotkeys();
        fileGroup=_Settings.GetGroupByName(L"File");closeKey=fileGroup?_Settings.GetHotkeyByName(L"Close",*fileGroup):nullptr;
        require(closeKey&&closeKey->m_accel.key=='Q',"Close remap persisted");
        std::vector<ACCEL> bindings(static_cast<size_t>(::CopyAcceleratorTableW(m_hAccel,nullptr,0)));
        ::CopyAcceleratorTableW(m_hAccel,bindings.data(),static_cast<int>(bindings.size()));
        require(std::any_of(bindings.begin(),bindings.end(),[](const ACCEL& item){return item.cmd==ID_FILE_CLOSE&&item.key=='Q';}),"remap in accelerator table");
        closeKey->m_accel=originalClose;_Settings.SaveHotkeyGroups();RebuildRuntimeHotkeys();
        closed();
        if(extensive){
            phase("EPUB import cancel/error/success after close");
            WORD importCommand=0;
            for(int i=0;i<m_plugins.ImportPlugins().GetSize();++i){
                const auto* plugin=m_plugins.Manager().FindPlugin(m_plugins.ImportPlugins()[i]);
                if(plugin&&plugin->id==L"import-epub")importCommand=static_cast<WORD>(ID_IMPORT_BASE+i);
            }
            require(importCommand!=0&&!RequiresOpenDocument(importCommand),"EPUB import available");
            std::vector<wchar_t> epub(32768);::GetEnvironmentVariableW(L"FBE_NEXT_TEST_CLOSE_EPUB",epub.data(),static_cast<DWORD>(epub.size()));
            require(epub[0]!=0,"EPUB fixture supplied");
            struct ScenarioGuard { ~ScenarioGuard(){::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",L"file-close-runtime");::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_CANCEL",nullptr);::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH",nullptr);} } scenarioGuard;
            ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",L"import-epub");
            ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_CANCEL",L"1");
            SendMessage(WM_COMMAND,importCommand);closed();
            ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_CANCEL",nullptr);
            ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH",L"Z:\\fbe-missing-close-test.epub");
            // -b suppresses U::ReportError; a failed import must return normally.
            SendMessage(WM_COMMAND,importCommand);closed();
            ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH",epub.data());
            SendMessage(WM_COMMAND,importCommand);
            require(m_document_session.HasDocument()&&FB::Doc::m_active_doc==m_doc&&m_view.GetActiveWnd()==m_doc->m_body,"import committed/presented");
            require(DocChanged()&&!m_doc->m_body.CheckCommand(ID_EDIT_UNDO)&&!m_doc->m_body.CheckCommand(ID_EDIT_REDO),"import dirty with empty history");
            require(m_source.SendMessage(SCI_GETLENGTH)==0&&!m_source.SendMessage(SCI_CANUNDO),"import Source reset");
            require(CString(static_cast<LPCWSTR>(m_doc->m_body.Document()->body->innerText)).Find(L"CLOSE_FIRST_UNIQUE")<0,"import has no previous content");
            auto bytes=[&](const CString& path){
                CAtlFile file;CheckError(file.Create(path,GENERIC_READ,FILE_SHARE_READ,OPEN_EXISTING));
                ULONGLONG length=0;CheckError(file.GetSize(length));require(length<MAXDWORD,"small import fixture");
                std::vector<BYTE> data(static_cast<size_t>(length));
                if(length) CheckError(file.Read(data.data(),static_cast<DWORD>(length)));
                return data;
            };
            const CString epubPath(epub.data()),existing=epubPath.Left(epubPath.GetLength()-5)+L".fb2";
            const auto existingBytes=bytes(existing),epubBytes=bytes(epubPath);
            auto nativeSave=[&](const CString& target,bool cancel,int overwriteAnswer,WORD command=ID_FILE_SAVE){
                auto& probe=FileCloseSaveDialogProbe::Current();
                probe.active=true;probe.ready=false;probe.target=target;probe.suggestion.Empty();probe.options=0;probe.overwrites=0;
                std::atomic<bool> stop(false);bool found=false,confirmation=false;CStringA dialogTrace;const DWORD thread=::GetCurrentThreadId();
                std::thread driver([&](){
                    HWND save=nullptr,answered=nullptr;bool submitted=false,rejected=false;
                    std::set<HWND> observed;
                    const ULONGLONG until=::GetTickCount64()+15000;
                    while(!stop.load()&&::GetTickCount64()<until){
                        struct Dialogs{HWND save;HWND confirm;HWND yes;HWND no;CStringA* trace;std::set<HWND>* observed;} dialogs={nullptr,nullptr,nullptr,nullptr,&dialogTrace,&observed};
                        ::EnumThreadWindows(thread,[](HWND window,LPARAM data)->BOOL{
                            auto& dialogs=*reinterpret_cast<Dialogs*>(data);wchar_t name[64]={};::GetClassNameW(window,name,_countof(name));
                            if(::IsWindowVisible(window)&&(!wcscmp(name,L"#32770")||!wcscmp(name,L"FBEThemedMessageDialog"))) {
                                struct Buttons{HWND yes;HWND no;} buttons={nullptr,nullptr};
                                ::EnumChildWindows(window,[](HWND child,LPARAM data)->BOOL{
                                    auto& buttons=*reinterpret_cast<Buttons*>(data);
                                    if(::GetDlgCtrlID(child)==IDYES)buttons.yes=child;
                                    if(::GetDlgCtrlID(child)==IDNO)buttons.no=child;
                                    // Windows 11's native DirectUI confirmation exposes
                                    // proxy buttons with ID 0 rather than IDYES/IDNO.
                                    wchar_t name[32]={},text[128]={};::GetClassNameW(child,name,_countof(name));
                                    if(!wcscmp(name,L"Button")) {
                                        ::GetWindowTextW(child,text,_countof(text));CString caption(text);caption.Remove(L'&');caption.Trim();
                                        if(caption.CompareNoCase(L"Yes")==0||caption==L"Да")buttons.yes=child;
                                        if(caption.CompareNoCase(L"No")==0||caption==L"Нет")buttons.no=child;
                                    }
                                    return TRUE;
                                },reinterpret_cast<LPARAM>(&buttons));
                                if(buttons.yes&&buttons.no){dialogs.confirm=window;dialogs.yes=buttons.yes;dialogs.no=buttons.no;}
                                else if(::GetDlgItem(window,IDOK)&&::GetDlgItem(window,IDCANCEL)) dialogs.save=window;
                                if(dialogs.observed->insert(window).second){
                                    dialogs.trace->AppendFormat("dialog=%ls\r\n",name);
                                    ::EnumChildWindows(window,[](HWND child,LPARAM data)->BOOL{
                                        auto& trace=*reinterpret_cast<CStringA*>(data);wchar_t name[64]={},text[128]={};::GetClassNameW(child,name,_countof(name));::GetWindowTextW(child,text,_countof(text));
                                        trace.AppendFormat("control=%d,%ls,%ls\r\n",::GetDlgCtrlID(child),name,text);return TRUE;
                                    },reinterpret_cast<LPARAM>(dialogs.trace));
                                }
                            }return TRUE;
                        },reinterpret_cast<LPARAM>(&dialogs));
                        if(dialogs.save&&probe.ready.load()&&!submitted){
                            save=dialogs.save;found=true;submitted=true;
                            const int action=cancel?IDCANCEL:IDOK;
                            ::PostMessageW(save,WM_COMMAND,MAKEWPARAM(action,BN_CLICKED),reinterpret_cast<LPARAM>(::GetDlgItem(save,action)));
                        }
                        if(dialogs.confirm&&dialogs.confirm!=answered){
                            answered=dialogs.confirm;confirmation=true;rejected=overwriteAnswer==IDNO;
                            const HWND button=overwriteAnswer==IDYES?dialogs.yes:dialogs.no;
                            ::PostMessageW(button,BM_CLICK,0,0);
                        }
                        if(rejected&&!dialogs.confirm&&save&&::IsWindow(save)){
                            ::PostMessageW(save,WM_COMMAND,MAKEWPARAM(IDCANCEL,BN_CLICKED),reinterpret_cast<LPARAM>(::GetDlgItem(save,IDCANCEL)));rejected=false;
                        }
                        ::Sleep(10);
                    }
                    if(!stop.load()) ::PostThreadMessageW(thread,WM_QUIT,92,0);
                });
                SendMessage(WM_COMMAND,command);stop=true;driver.join();probe.active=false;
                if(!found||(probe.overwrites&&!confirmation)){DWORD written=0;output.Write(dialogTrace,dialogTrace.GetLength(),&written);output.Flush();}
                require(found&&probe.ready.load(),"ordinary Save reached native Save As");
                require((probe.options&FOS_OVERWRITEPROMPT)!=0,"native overwrite prompt enabled");
                if(!cancel&&::GetFileAttributesW(target)!=INVALID_FILE_ATTRIBUTES&&probe.overwrites)
                    require(confirmation,"native overwrite confirmation displayed");
                return confirmation;
            };
            for(int replacement=0;replacement<2;++replacement){
                phase(replacement?"import replaces open book: native first save":"import after close: native first save");
                if(replacement){
                    require(LoadFile(second.data())==OK,"book before replacement import");
                    ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",L"import-epub");SendMessage(WM_COMMAND,importCommand);
                }
                ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",L"file-close-runtime");
                require(m_document_session.HasDocument()&&m_document_session.Location().storagePath.IsEmpty()&&!m_document_session.IsArchive(),"import has new pathless session");
                require(!m_doc->m_namevalid&&m_doc->m_filename==existing&&DocChanged(),"import suggestion is not confirmed identity");
                const CString imported(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
                nativeSave(CString(),true,IDNO);
                // The Shell can hide the registered extension in its edit field.
                require(FileCloseSaveDialogProbe::Current().suggestion.Find(L"import-source")>=0,"Save As uses plugin suggestion");
                require(!m_doc->m_namevalid&&DocChanged()&&m_document_session.Location().storagePath.IsEmpty()&&bytes(existing)==existingBytes,"cancel preserved new identity and existing FB2 bytes");
                require(nativeSave(existing,false,IDNO),"existing FB2 prompts before overwrite");
                require(bytes(existing)==existingBytes&&!m_doc->m_namevalid&&DocChanged(),"declined overwrite preserved FB2 and import");
                const CString saved=U::GetProgDir()+(replacement?L"import-replaced-saved.fb2":L"import-closed-saved.fb2");
                nativeSave(saved,false,IDYES);
                require(m_doc->m_namevalid&&m_doc->m_filename==saved&&!DocChanged()&&m_doc->GetDocumentFileType()==FictionBookFileType::Fb2,"saved imported document identity/type/clean");
                require(m_document_session.Location().storagePath==saved&&m_document_session.Location().documentType==FictionBookFileType::Fb2&&!m_document_session.IsArchive(),"saved imported session matches document");
                require(!m_doc->m_body.CheckCommand(ID_EDIT_UNDO)&&!m_doc->m_body.CheckCommand(ID_EDIT_REDO),"saved import has empty history");
                require(bytes(existing)==existingBytes&&bytes(epubPath)==epubBytes,"source EPUB and original FB2 unchanged");
                require(LoadFile(saved)==OK&&CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml))==imported&&!DocChanged(),"saved import reopens with matching content");
                const CString overwrite=U::GetProgDir()+(replacement?L"overwrite-replaced.fb2":L"overwrite-closed.fb2");
                require(::CopyFileW(existing,overwrite,TRUE)!=FALSE,"create explicit overwrite target");
                require(nativeSave(overwrite,false,IDYES,ID_FILE_SAVE_AS),"explicit overwrite confirmed");
                require(bytes(overwrite)!=existingBytes&&m_doc->m_filename==overwrite&&!DocChanged(),"confirmed overwrite saved import");
            }
            clean();SendMessage(WM_COMMAND,ID_FILE_CLOSE);closed();
        }
        require(LoadFile(second.data())==OK&&m_document_session.HasDocument(),"reopen after close");
        filtersPreserved();
        require(m_document_tree.m_tree.m_tree.GetCount()>0,"reopen restored document elements");
        require(!m_doc->m_body.CheckCommand(ID_EDIT_UNDO)&&!m_doc->m_body.CheckCommand(ID_EDIT_REDO),"reopen history empty");
        HMENU fileMenu=::GetSubMenu(m_MenuBar.GetMenu(),0);wchar_t caption[256]={};::GetMenuStringW(fileMenu,ID_FILE_CLOSE,caption,_countof(caption),MF_BYCOMMAND);
        const CString localized=FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.file.close",L"&Close");
        require(CString(caption).Find(localized)==0,"localized Close menu");
        int closePosition=-1,restartPosition=-1;
        for(int i=0;i<::GetMenuItemCount(fileMenu);++i) {
            const UINT command=::GetMenuItemID(fileMenu,i);
            if(command==ID_FILE_CLOSE) closePosition=i;
            if(command==ID_FILE_RESTART) restartPosition=i;
        }
        require(closePosition>=0&&restartPosition==closePosition+1,"Close directly above Restart FBE");
        if(extensive){
            phase("Source cancel/recovery");dirtySource();require(SaveRecoveryNow(),"recovery created");
            const CString recovery=m_recovery.SnapshotPath(),path=m_doc->m_filename;const auto source=FbeScripts::SourceBytes(m_source);FB::Doc* original=m_doc;
            askClose(IDCANCEL);require(m_doc==original&&m_document_session.HasDocument()&&DocChanged()&&FbeScripts::SourceBytes(m_source)==source&&::GetFileAttributesW(recovery)!=INVALID_FILE_ATTRIBUTES,"cancel preserved Source/recovery");
            phase("Save As cancel");m_doc->m_namevalid=false;askClose(IDYES,true);m_doc->m_namevalid=true;
            require(m_document_session.HasDocument()&&m_doc->m_filename==path&&DocChanged()&&FbeScripts::SourceBytes(m_source)==source,"Save As cancel retained changes");
            phase("save failure");CHandle lock(::CreateFileW(path,GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));require(lock!=INVALID_HANDLE_VALUE,"lock file");askClose(IDYES);lock.Close();
            require(m_document_session.HasDocument()&&m_doc->m_filename==path&&DocChanged()&&FbeScripts::SourceBytes(m_source)==source,"save failure retained changes");
            phase("save and close");askClose(IDYES);closed();require(::GetFileAttributesW(recovery)==INVALID_FILE_ATTRIBUTES,"close removed recovery");
            require(LoadFile(second.data())==OK,"reopen saved file");ShowView(SOURCE);require(FbeScripts::SourceBytes(m_source).find("CLOSE_UNSAVED")!=std::string::npos,"Source saved");
            phase("invalid Source/save refused");m_source.SendMessage(SCI_SETTEXT,0,reinterpret_cast<LPARAM>("<invalid>"));askClose(IDYES);
            require(m_document_session.HasDocument()&&DocChanged()&&FbeScripts::SourceBytes(m_source)=="<invalid>","invalid Source retained");
            phase("discard invalid Source");askClose(IDNO);closed();
            require(LoadFile(second.data())==OK,"second reopen");ShowView(DESC);
            MSHTML::IHTMLInputElementPtr title(m_doc->m_body.Document()->all->item(L"tiTitle"));title->value=L"CLOSE_DESCRIPTION_DIRTY";m_doc->ResetSavePoint();
            require(DocChanged(),"Description dirty");phase("discard Description");askClose(IDNO);closed();
            BOOL handled=FALSE;OnFileNew(0,ID_FILE_NEW,nullptr,handled);require(m_document_session.HasDocument()&&!m_doc->m_namevalid,"new differs from closed");
            m_doc->ResetSavePoint();phase("discard new book");askClose(IDNO);closed();
        }else{
            clean();SendMessage(WM_COMMAND,ID_FILE_CLOSE);closed();require(LoadFile(first)==OK,"reopen archive");ShowView(SOURCE);
            const char change[]="<!-- discarded archive Source -->";m_source.SendMessage(SCI_APPENDTEXT,sizeof(change)-1,reinterpret_cast<LPARAM>(change));
            const auto source=FbeScripts::SourceBytes(m_source);phase("archive Source cancel/discard");askClose(IDCANCEL);
            require(m_document_session.IsArchive()&&DocChanged()&&FbeScripts::SourceBytes(m_source)==source,"archive cancel retained Source");askClose(IDNO);closed();
        }
        if(extensive){
            phase("open malformed Source after close");CString malformed(second.data());malformed=malformed.Left(malformed.ReverseFind(L'\\')+1)+L"malformed.fb2";
            require(LoadToScintilla(malformed)&&m_document_session.HasDocument()&&IsSourceActive()&&FbeScripts::SourceBytes(m_source).find("<invalid>")!=std::string::npos,"malformed Source opens after close");
            clean();SendMessage(WM_COMMAND,ID_FILE_CLOSE);closed();
        }
        require(LoadFile(first)==OK,"original reopen");clean();passed=true;details="close lifecycle, save decisions, empty UI and reopen PASS";
    }catch(const _com_error& error){CStringA code;code.Format(" hr=0x%08lX",static_cast<unsigned long>(error.Error()));details+=code;}
    catch(...){details="native exception";}
    CStringA report;report.Format("result=%s\r\ndetails=%s\r\n",passed?"PASS":"FAIL",details.GetString());DWORD written=0;output.Write(report,report.GetLength(),&written);output.Close();::PostQuitMessage(passed?0:1);return 0;
}
