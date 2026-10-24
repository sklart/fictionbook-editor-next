// Exercise the command bar's own popup lifecycle, including the modal settings pump.
    struct MenuRouteProbe {
        HWND frame, bar; HMENU root; CStringA* report; bool seen=false, done=false; int choose=-1; ULONGLONG selectedAt=0; unsigned options=0, refresh=0, enters=0, exits=0, depth=0, maxDepth=0; HWND owner=NULL, selectedMenu=NULL; bool timedOut=false, refreshedOpen=false, refreshWhileOpen=false; unsigned readyEvents=0, cancelEvents=0, deactivations=0; ULONGLONG enteredAt=0, exitedAt=0; UINT readyMessage=0; unsigned serial=0; HWND readyMenu=NULL;
        std::wstring stage;
        void Dump(const char* phase) {
            CStringA rows;rows.AppendFormat("stage=%ls phase=%s theme=%d\n",stage.c_str(),phase,ThemeManager::GetSelectedTheme());
            const HMENU menu=::GetSubMenu(root,5);
            for(int i=0;i<::GetMenuItemCount(menu);++i) {
                wchar_t text[1024]={};MENUITEMINFO info={};info.cbSize=sizeof(info);
                info.fMask=MIIM_STRING|MIIM_ID|MIIM_FTYPE|MIIM_STATE|MIIM_DATA|MIIM_BITMAP|MIIM_SUBMENU;
                info.dwTypeData=text;info.cch=_countof(text);::GetMenuItemInfoW(menu,i,TRUE,&info);
                rows.AppendFormat("pos=%d id=%u text=%s type=%x state=%x data=%p bitmap=%p bitmapType=%lu submenu=%p\n",i,info.wID,static_cast<LPCSTR>(CW2A(text,CP_UTF8)),info.fType,info.fState,reinterpret_cast<void*>(info.dwItemData),info.hbmpItem,info.hbmpItem?::GetObjectType(info.hbmpItem):0,info.hSubMenu);
            }
            report->Append(rows);
            // Flush every stage before the next menu interaction, even if it crashes.
            const std::wstring path=PluginOptionsProbe::captures+L"\\menu-route.txt";
            HANDLE file=::CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
            if(file!=INVALID_HANDLE_VALUE){DWORD written=0;::WriteFile(file,rows.GetString(),static_cast<DWORD>(rows.GetLength()),&written,NULL);::CloseHandle(file);}
        }
        static LRESULT CALLBACK MenuLoop(HWND window,UINT message,WPARAM wParam,LPARAM lParam,UINT_PTR,DWORD_PTR data) {
            auto* self=reinterpret_cast<MenuRouteProbe*>(data);
            if(window==self->owner) {
                if(message==WM_ENTERMENULOOP){self->enteredAt=::GetTickCount64();++self->enters;++self->depth;self->maxDepth=(std::max)(self->maxDepth,self->depth);}
                if(message==WM_EXITMENULOOP){self->exitedAt=::GetTickCount64();++self->exits;if(self->depth)--self->depth;}
                if(message==WM_CANCELMODE)++self->cancelEvents;
            }
            const LRESULT result = ::DefSubclassProc(window,message,wParam,lParam);
            // A real menu-ready notification is a stronger rendezvous than a
            // 150-ms polling timer, which can miss an entire short menu loop.
            if(window==self->owner && message==WM_ENTERIDLE && wParam==MSGF_MENU && self->depth && !self->done && !self->readyEvents) {
                ++self->readyEvents; self->readyMenu=reinterpret_cast<HWND>(lParam);
                ::PostMessageW(self->frame,self->readyMessage,self->serial,0);
            }
            return result;
        }
        static LRESULT CALLBACK Commands(HWND window,UINT message,WPARAM wParam,LPARAM lParam,UINT_PTR,DWORD_PTR data) {
            auto* self=reinterpret_cast<MenuRouteProbe*>(data);
            if(message==self->readyMessage) {
                if(wParam==self->serial && self->depth && !self->done) Tick(NULL,0,0,0);
                return 0;
            }
            if(message==WM_ACTIVATEAPP && !wParam)++self->deactivations;
            if(message==WM_CANCELMODE)++self->cancelEvents;
            if(message==WM_COMMAND) {
                if(LOWORD(wParam)==ID_VIEW_OPTIONS)++self->options;
                if(LOWORD(wParam)==ID_TOOLS_REFRESH_SCRIPTS)++self->refresh;
            }
            return ::DefSubclassProc(window,message,wParam,lParam);
        }
        static void CALLBACK Tick(HWND,UINT,UINT_PTR,DWORD) {
            auto* probe=reinterpret_cast<MenuRouteProbe*>(::GetPropW(PluginOptionsProbe::dialog,L"FBE.MenuRoute"));
            if(!probe || !probe->depth)return;
            if(!probe->readyEvents) {
                if(::GetTickCount64()-probe->enteredAt>2000){probe->timedOut=true;::EndMenu();probe->done=true;}
                return;
            }
            if(probe->selectedAt) {
                if(::IsWindowVisible(probe->selectedMenu)&&::GetTickCount64()-probe->selectedAt>2000){probe->timedOut=true;::EndMenu();probe->done=true;}
                return;
            }
            const HWND window=probe->readyMenu; auto* self=probe;
            wchar_t kind[32]={};::GetClassNameW(window,kind,_countof(kind));
                if(!wcscmp(kind,L"#32768")&&::IsWindowVisible(window)) {
                    self->seen=true;self->Dump("open");
                    if(self->refreshWhileOpen&&!self->refreshedOpen) {
                        self->refreshedOpen=true;::SendMessageW(self->frame,WM_COMMAND,ID_TOOLS_REFRESH_SCRIPTS,0);
                        self->Dump("refreshed-open");
                    }
                    PluginOptionsProbe::prefix=self->stage;
                    PluginOptionsProbe::Capture(window,L"commandbar");
                    if(self->choose>=0) {
                        self->selectedAt=::GetTickCount64();self->selectedMenu=window;
                        for(int i=0;i<=self->choose;++i)::PostMessageW(window,WM_KEYDOWN,VK_DOWN,0);
                        ::PostMessageW(window,WM_KEYDOWN,VK_RETURN,0);
                    } else ::EndMenu();
                    self->done=true;
                }
        }
    } probe={m_hWnd,m_MenuBar,m_MenuBar.GetMenu(),&report};
    probe.readyMessage=::RegisterWindowMessageW(L"FBE.Test.MenuRouteReady"); check("menu_ready_message",probe.readyMessage!=0);
    ::SetPropW(m_hWnd,L"FBE.MenuRoute",&probe);
    check("command_observer",::SetWindowSubclass(m_hWnd,MenuRouteProbe::Commands,0x4d525450,reinterpret_cast<DWORD_PTR>(&probe))!=FALSE);
    check("menu_loop_observers",::SetWindowSubclass(m_MenuBar,MenuRouteProbe::MenuLoop,0x4d524c50,reinterpret_cast<DWORD_PTR>(&probe))&&
        ::SetWindowSubclass(m_MenuBar.GetParent(),MenuRouteProbe::MenuLoop,0x4d524c50,reinterpret_cast<DWORD_PTR>(&probe)));
    auto route=[&](int item,const std::wstring& stage,int choose=-1,bool refreshOpen=false) {
        probe.owner=ThemeManager::IsDark()&&!ThemeManager::IsHighContrast()?static_cast<HWND>(m_MenuBar.GetParent()):m_MenuBar.m_hWnd;
        probe.enters=probe.exits=probe.depth=probe.maxDepth=0;probe.timedOut=false;probe.selectedMenu=NULL;
        ++probe.serial; probe.readyMenu=NULL; probe.readyEvents=probe.cancelEvents=probe.deactivations=0;probe.enteredAt=probe.exitedAt=0;
        probe.refreshWhileOpen=refreshOpen;probe.refreshedOpen=false;
        const unsigned options=probe.options, refresh=probe.refresh;
        probe.stage=stage;probe.seen=false;probe.done=false;probe.choose=choose;probe.selectedAt=0;probe.Dump("before");
        RECT itemRect={};check("commandbar_button",m_MenuBar.GetItemRect(item,&itemRect)!=FALSE);
        const UINT_PTR timer=::SetTimer(NULL,0,150,MenuRouteProbe::Tick);
        m_MenuBar.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM((itemRect.left+itemRect.right)/2,(itemRect.top+itemRect.bottom)/2));
        m_MenuBar.SendMessage(WM_LBUTTONUP,0,MAKELPARAM((itemRect.left+itemRect.right)/2,(itemRect.top+itemRect.bottom)/2));
        const ULONGLONG deadline=::GetTickCount64()+3000;
        while(!probe.done&&::GetTickCount64()<deadline) {
            MSG message={};if(::PeekMessage(&message,NULL,0,0,PM_REMOVE)){::TranslateMessage(&message);::DispatchMessage(&message);}
            else ::MsgWaitForMultipleObjects(0,NULL,FALSE,20,QS_ALLINPUT);
        }
        ::KillTimer(NULL,timer);check("single_menu_loop",probe.enters==1&&probe.exits==1&&probe.depth==0&&probe.maxDepth==1);
        check("menu_selection_no_timeout",!probe.timedOut);
        report.AppendFormat("menu_ready_events=%u;cancel_events=%u;deactivations=%u;loop_ms=%llu;foreground=%p\n",
            probe.readyEvents,probe.cancelEvents,probe.deactivations,
            probe.exitedAt>=probe.enteredAt?probe.exitedAt-probe.enteredAt:0,::GetForegroundWindow());
        check("menu_ready_observed",probe.readyEvents>0);
        check("commandbar_popup",probe.seen);probe.Dump("after");toolsMenu();
        const auto restored=metadata();
        check("commandbar_metadata_restored",restored.fType==original.fType&&restored.dwItemData==original.dwItemData&&restored.hbmpItem==original.hbmpItem);
        MSG pending={};while(::PeekMessage(&pending,NULL,WM_COMMAND,WM_COMMAND,PM_REMOVE))::DispatchMessage(&pending);
        if(choose==1)check("settings_selected_from_menu",probe.options==options+1);
        if(choose==2)check("refresh_selected_from_menu",probe.refresh==refresh+1);
    };
    wchar_t loopsText[16]={};::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MENU_CYCLES",loopsText,_countof(loopsText));
    const int loops=loopsText[0]?max(1,_wtoi(loopsText)):20;
    report.AppendFormat("commandbar_dpi=%u\n",UiMetrics::DpiForWindow(m_MenuBar));
    route(5,L"route-startup");
    route(5,L"refresh-while-open",-1,true);check("refresh_while_menu_open",probe.refreshedOpen);
    const int start=ThemeManager::GetSelectedTheme();
    for(int cycle=0;cycle<loops;++cycle) {
        for(int theme:{2,1,2}) {
            wchar_t value[4]={};_itow_s(theme,value,10);::SetEnvironmentVariableW(L"FBE_NEXT_TEST_MENU_THEME",value);
            route(5,L"settings-"+std::to_wstring(cycle)+L"-"+std::to_wstring(theme),1);::SetEnvironmentVariableW(L"FBE_NEXT_TEST_MENU_THEME",NULL);
            check("modal_theme_committed",ThemeManager::GetSelectedTheme()==theme);
            route(5,L"route-"+std::to_wstring(cycle)+L"-"+std::to_wstring(theme));
            route(4,L"style-"+std::to_wstring(cycle)+L"-"+std::to_wstring(theme));
            route(6,L"scripts-"+std::to_wstring(cycle)+L"-"+std::to_wstring(theme));
            route(5,L"refresh-"+std::to_wstring(cycle)+L"-"+std::to_wstring(theme),2);toolsMenu();
        }
    }
    ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_MENU_THEME",L"0");
    route(5,L"settings-automatic",1);::SetEnvironmentVariableW(L"FBE_NEXT_TEST_MENU_THEME",NULL);
    check("automatic_committed",ThemeManager::GetSelectedTheme()==INTERFACE_THEME_AUTOMATIC);
    route(5,L"route-automatic");
    ThemeManager::SetSelectedTheme(static_cast<InterfaceTheme>(start));ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());


