if (IsFbeTestScenario(L"file-close-runtime")) {
    bool passed=false;CStringA details="start";
    auto require=[&](bool ok,const char* label){details=label;if(!ok)CheckError(E_FAIL);};
    auto phase=[&](const char* label){CStringA row=CStringA("phase=")+label+"\r\n";DWORD n=0;output.Write(row,row.GetLength(),&n);output.Flush();};
    try {
        const CString first=m_document_session.Location().storagePath;
        std::vector<wchar_t> second(32768);::GetEnvironmentVariableW(L"FBE_NEXT_TEST_CLOSE_SECOND",second.data(),static_cast<DWORD>(second.size()));
        const bool extensive=!m_document_session.IsArchive();
        auto recent=[&](){std::vector<CString> names;for(int i=0;i<m_recentDocuments.List().m_arrDocs.GetSize();++i)names.emplace_back(m_recentDocuments.List().m_arrDocs[i].szDocName);return names;};
        auto clean=[&](){m_doc->MarkSavePoint();m_source.SendMessage(SCI_SETSAVEPOINT);};
        auto closed=[&](){
            OnIdle();CString title;GetWindowText(title);
            require(::IsWindow(m_hWnd)&&!m_document_session.HasDocument()&&!DocChanged(),"closed session/window");
            require(title==L"FictionBook Editor Next"&&FB::Doc::m_active_doc==nullptr,"closed title/active");
            require(m_view.GetActiveWnd()==nullptr&&!m_source.IsWindowVisible()&&!m_doc->m_body.IsWindowVisible(),"closed panes");
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
        {PendingDocument pending(*this,m_doc);pending.Rollback();}
        require(FB::Doc::m_active_doc==nullptr&&!m_document_session.HasDocument(),"rollback preserves closed state");
        if(extensive){phase("settings without a document");askClose(IDCANCEL,false,ID_VIEW_OPTIONS);closed();}
        require(LoadFile(second.data())==OK&&m_document_session.HasDocument(),"reopen after close");
        require(!m_doc->m_body.CheckCommand(ID_EDIT_UNDO)&&!m_doc->m_body.CheckCommand(ID_EDIT_REDO),"reopen history empty");
        const ACCEL originalClose=closeKey->m_accel;closeKey->m_accel.key='Q';_Settings.SaveHotkeyGroups();_Settings.LoadHotkeyGroups();RebuildRuntimeHotkeys();
        fileGroup=_Settings.GetGroupByName(L"File");closeKey=fileGroup?_Settings.GetHotkeyByName(L"Close",*fileGroup):nullptr;
        require(closeKey&&closeKey->m_accel.key=='Q',"Close remap persisted");
        std::vector<ACCEL> bindings(static_cast<size_t>(::CopyAcceleratorTableW(m_hAccel,nullptr,0)));
        ::CopyAcceleratorTableW(m_hAccel,bindings.data(),static_cast<int>(bindings.size()));
        require(std::any_of(bindings.begin(),bindings.end(),[](const ACCEL& item){return item.cmd==ID_FILE_CLOSE&&item.key=='Q';}),"remap in accelerator table");
        closeKey->m_accel=originalClose;_Settings.SaveHotkeyGroups();RebuildRuntimeHotkeys();
        HMENU fileMenu=::GetSubMenu(m_MenuBar.GetMenu(),0);wchar_t caption[256]={};::GetMenuStringW(fileMenu,ID_FILE_CLOSE,caption,_countof(caption),MF_BYCOMMAND);
        const CString localized=FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.file.close",L"&Close");
        require(CString(caption).Find(localized)==0,"localized Close menu");
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
