// Exercise the command bar's own popup lifecycle, including the modal settings pump.
    struct MenuRouteProbe {
        HWND frame, bar; HMENU root; CStringA* report; bool seen=false, done=false, geometryOk=true, paletteColor=false, paletteGutter=false, paletteBorder=false, paletteSelected=false, paletteSelectedText=false, paletteHover=false, mnemonicOk=false, snapshotText=false, accessibleText=false, nestedAttempt=false, nestedSeen=false, nestedPainted=false, nestedAccessible=false, refreshFixtureCreated=false, stateUpdated=false, stateSynced=false, stateDisabledPaint=false, stateDisabledKey=false, stateDisabledAccessible=false; int choose=-1; ULONGLONG selectedAt=0; unsigned options=0, refresh=0, enters=0, exits=0, depth=0, maxDepth=0; HWND owner=NULL, selectedMenu=NULL; bool timedOut=false, refreshedOpen=false, refreshWhileOpen=false; unsigned readyEvents=0, cancelEvents=0, deactivations=0; ULONGLONG enteredAt=0, exitedAt=0; UINT readyMessage=0; unsigned serial=0; HWND readyMenu=NULL, nestedWindow=NULL;
        std::wstring stage; bool hoverPending=false, hoverCursorSaved=false; POINT hoverOriginalCursor={}; ULONGLONG hoverStarted=0;
        void Dump(const char* phase) {
            CStringA rows;rows.AppendFormat("stage=%ls phase=%s theme=%d\n",stage.c_str(),phase,ThemeManager::GetSelectedTheme());
            const int menuIndex=stage.find(L"scripts-")==0?6:stage.find(L"style-")==0?4:5;
            const HMENU menu=::GetSubMenu(root,menuIndex);
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
                if(message==WM_INITMENUPOPUP && self->nestedAttempt) {
                    const HMENU scripts=::GetSubMenu(self->root,6);
                    const HMENU child=reinterpret_cast<HMENU>(wParam);
                    const int minimum=self->stage==L"scripts-nested-after-refresh"?2:1;
                    if(scripts && child==::GetSubMenu(scripts,0) && ::GetMenuItemCount(child)>=minimum) self->nestedSeen=true;
                }
                if(message==WM_ENTERMENULOOP){self->enteredAt=::GetTickCount64();++self->enters;++self->depth;self->maxDepth=(std::max)(self->maxDepth,self->depth);}
                if(message==WM_EXITMENULOOP){self->exitedAt=::GetTickCount64();++self->exits;if(self->depth)--self->depth;}
                if(message==WM_CANCELMODE)++self->cancelEvents;
            }
            const LRESULT result = ::DefSubclassProc(window,message,wParam,lParam);
            if(window==self->owner && message==WM_INITMENUPOPUP && self->stage==L"style-state-sync" && reinterpret_cast<HMENU>(wParam)==::GetSubMenu(self->root,4))
                self->stateUpdated=::EnableMenuItem(reinterpret_cast<HMENU>(wParam),0,MF_BYPOSITION|MF_GRAYED)!=static_cast<UINT>(-1);
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
        static bool AccessibleItem(HWND window,long index,const wchar_t* expectedName,bool requireUnavailable) {
            HMODULE library=::LoadLibraryW(L"oleacc.dll"); if(!library) return false;
            using AccessibleFromWindow=HRESULT (WINAPI*)(HWND,DWORD,REFIID,void**);
            auto getAccessible=reinterpret_cast<AccessibleFromWindow>(::GetProcAddress(library,"AccessibleObjectFromWindow"));
            IAccessible* accessible=NULL; bool result=false;
            if(getAccessible && SUCCEEDED(getAccessible(window,static_cast<DWORD>(OBJID_CLIENT),__uuidof(IAccessible),reinterpret_cast<void**>(&accessible))) && accessible) {
                VARIANT child={}; child.vt=VT_I4; child.lVal=index; VARIANT state={}; BSTR name=NULL;
                const bool nameOk=SUCCEEDED(accessible->get_accName(child,&name)) && name && (!expectedName || wcsstr(name,expectedName));
                const bool stateOk=SUCCEEDED(accessible->get_accState(child,&state)) && state.vt==VT_I4 && (!requireUnavailable || (state.lVal&STATE_SYSTEM_UNAVAILABLE));
                result=nameOk&&stateOk; if(name) ::SysFreeString(name); ::VariantClear(&state); accessible->Release();
            }
            ::FreeLibrary(library); return result;
        }
        static BOOL CALLBACK FindNested(HWND window,LPARAM data) {
            auto* self=reinterpret_cast<MenuRouteProbe*>(data); wchar_t kind[32]={};
            if(window!=self->readyMenu && ::IsWindowVisible(window) && ::GetClassNameW(window,kind,_countof(kind)) && !wcscmp(kind,L"#32768")) { self->nestedWindow=window; return FALSE; }
            return TRUE;
        }
        static void CALLBACK Tick(HWND,UINT,UINT_PTR,DWORD) {
            auto* probe=reinterpret_cast<MenuRouteProbe*>(::GetPropW(PluginOptionsProbe::dialog,L"FBE.MenuRoute"));
            if(!probe || !probe->depth)return;
            if(probe->hoverPending) {
                HMENU menu=reinterpret_cast<HMENU>(::GetPropW(probe->owner,L"FBE.Test.MenuSnapshotRoot"));
                MENUITEMINFOW info={}; info.cbSize=sizeof(info); info.fMask=MIIM_STATE; RECT item={}, bounds={};
                if(menu && ::GetMenuItemInfoW(menu,1,TRUE,&info) && (info.fState&MFS_HILITE) && ::GetMenuItemRect(probe->owner,menu,1,&item) && ::GetWindowRect(probe->readyMenu,&bounds)) {
                    const int width=bounds.right-bounds.left, height=bounds.bottom-bounds.top, y=(item.top+item.bottom)/2-bounds.top; CImage image;
                    if(width>8 && height>0 && image.Create(width,height,32)) { HDC dc=image.GetDC();
                        if(dc) { if(::PrintWindow(probe->readyMenu,dc,0)) probe->paletteHover=::GetPixel(dc,width-8,y)==ThemeManager::SelectionBackgroundColor(); image.ReleaseDC(); }
                    }
                }
                if(probe->paletteHover || ::GetTickCount64()-probe->hoverStarted>2000) {
                    if(probe->hoverCursorSaved) ::SetCursorPos(probe->hoverOriginalCursor.x,probe->hoverOriginalCursor.y);
                    probe->hoverPending=false; ::EndMenu(); probe->done=true;
                }
                return;
            }
            if(probe->nestedAttempt) {
                if(probe->nestedSeen && !probe->nestedWindow) ::EnumThreadWindows(::GetCurrentThreadId(),FindNested,reinterpret_cast<LPARAM>(probe));
                if(probe->nestedWindow) {
                    RECT bounds={}; ::GetWindowRect(probe->nestedWindow,&bounds); CImage image;
                    if(bounds.right>bounds.left && bounds.bottom>bounds.top && image.Create(bounds.right-bounds.left,bounds.bottom-bounds.top,32)) {
                        HDC dc=image.GetDC(); if(dc) { probe->nestedPainted=::PrintWindow(probe->nestedWindow,dc,0)!=FALSE; image.ReleaseDC(); }
                    }
                    PluginOptionsProbe::prefix=probe->stage; PluginOptionsProbe::Capture(probe->nestedWindow,L"submenu");
                    probe->nestedAccessible=AccessibleItem(probe->nestedWindow,1,probe->stage==L"scripts-nested-after-refresh"?L"AddedDuringMenu":L"NestedProbe",false);
                }
                if(probe->nestedPainted || ::GetTickCount64()-probe->enteredAt>2000) {
                    if(!probe->nestedPainted) probe->timedOut=true;
                    ::EndMenu(); probe->done=true;
                }
                return;
            }
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
                        if(self->stage==L"scripts-refresh-open") {
                            CString path=_Settings.GetScriptsFolder(); path+=L"\\Nested\\AddedDuringMenu.js";
                            HANDLE file=::CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
                            if(file!=INVALID_HANDLE_VALUE) { const char script[]="function Run() {}"; DWORD written=0;
                                self->refreshFixtureCreated=::WriteFile(file,script,sizeof(script)-1,&written,NULL)&&written==sizeof(script)-1; ::CloseHandle(file); }
                        }
                        self->refreshedOpen=true;::SendMessageW(self->frame,WM_COMMAND,ID_TOOLS_REFRESH_SCRIPTS,0);
                        self->Dump("refreshed-open");
                    }
                    RECT popupBounds={}; ::GetWindowRect(window,&popupBounds);
                    if(self->stage.find(L"scripts-")==0 || self->stage==L"palette-navy" || self->stage==L"palette-sepia")
                        self->report->AppendFormat("menu_measure_%ls_messages=%Iu matched=%Iu thread=%Iu target=%p owner=%p active=%Iu\n",self->stage.c_str(),reinterpret_cast<UINT_PTR>(::GetPropW(self->owner,L"FBE.Test.MenuMeasureMessages")),reinterpret_cast<UINT_PTR>(::GetPropW(self->owner,L"FBE.Test.MenuMeasureMatched")),reinterpret_cast<UINT_PTR>(::GetPropW(self->owner,L"FBE.Test.MenuMeasureThreadCount")),::GetPropW(self->owner,L"FBE.Test.MenuMeasureTarget"),self->owner,reinterpret_cast<UINT_PTR>(::GetPropW(self->owner,L"FBE.Test.MenuRenderActive")));
                    if(self->stage.find(L"scripts-")==0 || self->stage==L"palette-navy" || self->stage==L"palette-sepia")
                        self->report->AppendFormat("menu_snapshot_%ls_type=%Ix data=%p\n",self->stage.c_str(),reinterpret_cast<UINT_PTR>(::GetPropW(self->owner,L"FBE.Test.MenuSnapshotType")),::GetPropW(self->owner,L"FBE.Test.MenuSnapshotData"));
                    if(self->stage.find(L"scripts-")==0 || self->stage.find(L"style-")==0) {
                        const int minWidth=UiMetrics::ScaleForDpi(75,UiMetrics::DpiForWindow(self->frame));
                        self->geometryOk=popupBounds.right-popupBounds.left>=minWidth;
                        self->report->AppendFormat("menu_geometry_%ls=%ldx%ld\n",self->stage.c_str(),popupBounds.right-popupBounds.left,popupBounds.bottom-popupBounds.top);
                    }
                    if(self->stage == L"palette-navy" || self->stage == L"palette-sepia") {
                        HMENU snapshot=reinterpret_cast<HMENU>(::GetPropW(self->owner,L"FBE.Test.MenuSnapshotRoot"));
                        if(snapshot) {
                            const HMENU source=::GetSubMenu(self->root,4); wchar_t caption[256]={};
                            if(::GetMenuStringW(source,0,caption,_countof(caption),MF_BYPOSITION)>0) {
                                wchar_t copyCaption[256]={};
                                self->snapshotText=::GetMenuStringW(snapshot,0,copyCaption,_countof(copyCaption),MF_BYPOSITION)>0 && wcscmp(caption,copyCaption)==0;
                                HMODULE oleacc=::LoadLibraryW(L"oleacc.dll");
                                if(oleacc) {
                                    using AccessibleFromWindow=HRESULT (WINAPI*)(HWND,DWORD,REFIID,void**);
                                    auto getAccessible=reinterpret_cast<AccessibleFromWindow>(::GetProcAddress(oleacc,"AccessibleObjectFromWindow"));
                                    IAccessible* accessible=NULL;
                                    if(getAccessible && SUCCEEDED(getAccessible(window,static_cast<DWORD>(OBJID_CLIENT),__uuidof(IAccessible),reinterpret_cast<void**>(&accessible))) && accessible) {
                                        VARIANT child={}; child.vt=VT_I4; child.lVal=1; BSTR name=NULL;
                                        if(SUCCEEDED(accessible->get_accName(child,&name)) && name) {
                                            std::wstring expected=caption; const size_t tab=expected.find(L'\t'); if(tab!=std::wstring::npos) expected.resize(tab);
                                            expected.erase(std::remove(expected.begin(),expected.end(),L'&'),expected.end());
                                            self->accessibleText=!expected.empty() && wcsstr(name,expected.c_str())!=NULL;
                                            ::SysFreeString(name);
                                        }
                                        accessible->Release();
                                    }
                                    ::FreeLibrary(oleacc);
                                }
                                const wchar_t* marker=wcschr(caption,L'&');
                                if(marker && marker[1] && marker[1]!=L'&') {
                                    const LRESULT answer=::SendMessageW(self->owner,WM_MENUCHAR,MAKEWPARAM(marker[1],MF_POPUP),reinterpret_cast<LPARAM>(snapshot));
                                    self->mnemonicOk=LOWORD(answer)==0 && HIWORD(answer)==MNC_EXECUTE;
                                }
                            }
                        }
                        RECT client={}; ::GetWindowRect(window,&client); CImage image;
                        const int width=client.right-client.left, height=client.bottom-client.top;
                        COLORREF pixel=CLR_INVALID, gutter=CLR_INVALID, border=CLR_INVALID, innerBorder=CLR_INVALID;
                        if(width>8 && height>0 && image.Create(width,height,32)) {
                            HDC dc=image.GetDC();
                            if(dc && ::PrintWindow(window,dc,0)) {
                                pixel=::GetPixel(dc,width-8,height/2);
                                gutter=::GetPixel(dc,UiMetrics::ScaleForDpi(5,UiMetrics::DpiForWindow(self->frame)),UiMetrics::ScaleForDpi(10,UiMetrics::DpiForWindow(self->frame)));
                                border=::GetPixel(dc,0,height/2);
                                innerBorder=::GetPixel(dc,1,height/2);
                            }
                            if(dc) image.ReleaseDC();
                        }
                        self->paletteColor=pixel==ThemeManager::ControlColor();
                        self->paletteGutter=gutter==ThemeManager::ControlColor();
                        self->paletteBorder=border==ThemeManager::Color(THEME_COLOR_BORDER) && innerBorder==ThemeManager::Color(THEME_COLOR_BORDER);
                        self->report->AppendFormat("palette_menu_actual=%lu expected=%lu\n",static_cast<unsigned long>(pixel),static_cast<unsigned long>(ThemeManager::ControlColor()));
                        self->report->AppendFormat("palette_menu_gutter_actual=%lu expected=%lu\n",static_cast<unsigned long>(gutter),static_cast<unsigned long>(ThemeManager::ControlColor()));
                        self->report->AppendFormat("palette_menu_border_actual=%lu expected=%lu\n",static_cast<unsigned long>(border),static_cast<unsigned long>(ThemeManager::Color(THEME_COLOR_BORDER)));
                        self->report->AppendFormat("palette_menu_border_inner=%lu\n",static_cast<unsigned long>(innerBorder));
                        if(snapshot) {
                            ::SendMessageW(window,WM_KEYDOWN,VK_DOWN,0); ::UpdateWindow(window);
                            for(int index=0; index<::GetMenuItemCount(snapshot); ++index) {
                                MENUITEMINFOW info={}; info.cbSize=sizeof(info); info.fMask=MIIM_STATE; RECT item={};
                                if(!::GetMenuItemInfoW(snapshot,index,TRUE,&info) || !(info.fState&MFS_HILITE) || !::GetMenuItemRect(self->owner,snapshot,index,&item)) continue;
                                CImage selected; if(selected.Create(width,height,32)) { HDC dc=selected.GetDC();
                                    if(dc) { if(::PrintWindow(window,dc,0)) {
                                        const COLORREF actual=::GetPixel(dc,width-8,(item.top+item.bottom)/2-client.top);
                                        self->paletteSelected=actual==ThemeManager::SelectionBackgroundColor();
                                        unsigned textPixels=0;
                                        for(int y=(std::max)(0,static_cast<int>(item.top-client.top)); y<(std::min)(height,static_cast<int>(item.bottom-client.top)); ++y)
                                            for(int x=UiMetrics::ScaleForDpi(35,UiMetrics::DpiForWindow(self->frame)); x<width-UiMetrics::ScaleForDpi(25,UiMetrics::DpiForWindow(self->frame)); ++x)
                                                if(::GetPixel(dc,x,y)==ThemeManager::SelectionTextColor()) ++textPixels;
                                        self->paletteSelectedText=textPixels>0;
                                        self->report->AppendFormat("palette_menu_selected_text_pixels=%u\n",textPixels);
                                        self->report->AppendFormat("palette_menu_selected_actual=%lu expected=%lu\n",static_cast<unsigned long>(actual),static_cast<unsigned long>(ThemeManager::SelectionBackgroundColor()));
                                    } selected.ReleaseDC(); }
                                }
                                break;
                            }
                            RECT hoverItem={};
                            if(::GetMenuItemRect(self->owner,snapshot,1,&hoverItem)) {
                                const int x=(hoverItem.left+hoverItem.right)/2-client.left, y=(hoverItem.top+hoverItem.bottom)/2-client.top;
                                self->hoverCursorSaved=::GetCursorPos(&self->hoverOriginalCursor)!=FALSE;
                                if(self->hoverCursorSaved && ::SetCursorPos((hoverItem.left+hoverItem.right)/2,(hoverItem.top+hoverItem.bottom)/2)) {
                                    self->hoverPending=true; self->hoverStarted=::GetTickCount64(); ::PostMessageW(window,WM_MOUSEMOVE,0,MAKELPARAM(x,y));
                                }
                            }
                        }
                    }
                    if(self->stage==L"style-state-sync") {
                        HMENU snapshot=reinterpret_cast<HMENU>(::GetPropW(self->owner,L"FBE.Test.MenuSnapshotRoot"));
                        MENUITEMINFOW info={}; info.cbSize=sizeof(info); info.fMask=MIIM_STATE;
                        self->stateSynced=snapshot&&::GetMenuItemInfoW(snapshot,0,TRUE,&info)&&(info.fState&MFS_DISABLED);
                        self->stateDisabledAccessible=AccessibleItem(window,1,NULL,true);
                        RECT item={}; CImage image; const int width=popupBounds.right-popupBounds.left, height=popupBounds.bottom-popupBounds.top;
                        if(self->stateSynced && ::GetMenuItemRect(self->owner,snapshot,0,&item) && width>0 && height>0 && image.Create(width,height,32)) {
                            HDC dc=image.GetDC(); unsigned pixels=0;
                            if(dc) { if(::PrintWindow(window,dc,0)) {
                                for(int y=(std::max)(0,static_cast<int>(item.top-popupBounds.top)); y<(std::min)(height,static_cast<int>(item.bottom-popupBounds.top)); ++y)
                                    for(int x=UiMetrics::ScaleForDpi(35,UiMetrics::DpiForWindow(self->frame)); x<width-UiMetrics::ScaleForDpi(25,UiMetrics::DpiForWindow(self->frame)); ++x)
                                        if(::GetPixel(dc,x,y)==ThemeManager::DisabledTextColor()) ++pixels;
                            } image.ReleaseDC(); }
                            self->stateDisabledPaint=pixels>0; self->report->AppendFormat("palette_menu_disabled_text_pixels=%u\n",pixels);
                            wchar_t caption[256]={};
                            if(::GetMenuStringW(::GetSubMenu(self->root,4),0,caption,_countof(caption),MF_BYPOSITION)>0) {
                                const wchar_t* marker=wcschr(caption,L'&');
                                if(marker && marker[1] && marker[1]!=L'&') self->stateDisabledKey=HIWORD(::SendMessageW(self->owner,WM_MENUCHAR,MAKEWPARAM(marker[1],MF_POPUP),reinterpret_cast<LPARAM>(snapshot)))==MNC_IGNORE;
                            }
                        }
                    }
                    PluginOptionsProbe::prefix=self->stage;
                    PluginOptionsProbe::Capture(window,L"commandbar");
                    if(self->hoverPending) return;
                    if(self->stage==L"scripts-nested" || self->stage==L"scripts-nested-after-refresh") {
                        self->nestedAttempt=true;
                        ::PostMessageW(window,WM_KEYDOWN,VK_DOWN,0);
                        ::PostMessageW(window,WM_KEYDOWN,VK_RIGHT,0);
                        return;
                    }
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
        probe.owner=ThemeManager::UsesPaletteSurface()?static_cast<HWND>(m_MenuBar.GetParent()):m_MenuBar.m_hWnd;
        probe.enters=probe.exits=probe.depth=probe.maxDepth=0;probe.timedOut=false;probe.selectedMenu=NULL;
        ++probe.serial; probe.readyMenu=NULL; probe.readyEvents=probe.cancelEvents=probe.deactivations=0;probe.enteredAt=probe.exitedAt=0;
        probe.refreshWhileOpen=refreshOpen;probe.refreshedOpen=false;
        const unsigned options=probe.options, refresh=probe.refresh;
        probe.stage=stage;probe.seen=false;probe.done=false;probe.geometryOk=true;probe.choose=choose;probe.selectedAt=0;probe.Dump("before");
        RECT itemRect={};check("commandbar_button",m_MenuBar.GetItemRect(item,&itemRect)!=FALSE);
        const UINT_PTR timer=::SetTimer(NULL,0,150,MenuRouteProbe::Tick);
        m_MenuBar.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM((itemRect.left+itemRect.right)/2,(itemRect.top+itemRect.bottom)/2));
        m_MenuBar.SendMessage(WM_LBUTTONUP,0,MAKELPARAM((itemRect.left+itemRect.right)/2,(itemRect.top+itemRect.bottom)/2));
        const ULONGLONG deadline=::GetTickCount64()+3000;
        while(!probe.done&&::GetTickCount64()<deadline) {
            MSG message={};if(::PeekMessage(&message,NULL,0,0,PM_REMOVE)){::TranslateMessage(&message);::DispatchMessage(&message);}
            else ::MsgWaitForMultipleObjects(0,NULL,FALSE,20,QS_ALLINPUT);
        }
        if(probe.hoverPending && probe.hoverCursorSaved) ::SetCursorPos(probe.hoverOriginalCursor.x,probe.hoverOriginalCursor.y);
        probe.hoverPending=false;
        ::KillTimer(NULL,timer);check("single_menu_loop",probe.enters==1&&probe.exits==1&&probe.depth==0&&probe.maxDepth==1);
        check("menu_selection_no_timeout",!probe.timedOut);
        report.AppendFormat("menu_ready_events=%u;cancel_events=%u;deactivations=%u;loop_ms=%llu;foreground=%p\n",
            probe.readyEvents,probe.cancelEvents,probe.deactivations,
            probe.exitedAt>=probe.enteredAt?probe.exitedAt-probe.enteredAt:0,::GetForegroundWindow());
        check("menu_ready_observed",probe.readyEvents>0);
        check("commandbar_popup",probe.seen);check("popup_geometry",probe.geometryOk);probe.Dump("after");toolsMenu();
        bool mappingsAlive=true;
        for(int i=0;i<m_MenuBar.m_arrCommand.GetSize();++i) {
            const HBITMAP bitmap=ThemeManager::NativeMenuBitmap(m_MenuBar.m_arrCommand[i]);
            if(!bitmap||::GetObjectType(bitmap)!=OBJ_BITMAP){mappingsAlive=false;break;}
        }
        check("native_icon_mappings_after_route",mappingsAlive);
        const auto restored=metadata();
        check("commandbar_metadata_restored",restored.fType==original.fType&&restored.dwItemData==original.dwItemData&&restored.hbmpItem==original.hbmpItem);
        MSG pending={};while(::PeekMessage(&pending,NULL,WM_COMMAND,WM_COMMAND,PM_REMOVE))::DispatchMessage(&pending);
        if(choose==1)check("settings_selected_from_menu",probe.options==options+1);
        if(choose==2)check("refresh_selected_from_menu",probe.refresh==refresh+1);
        if(refreshOpen)check("refresh_open_command_once",probe.refresh==refresh+1);
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
    wchar_t nestedRenderer[2]={};
    if(::GetEnvironmentVariableW(L"FBE_NEXT_TEST_PALETTE_MENU_RENDER",nestedRenderer,_countof(nestedRenderer))) {
        ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK); ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
        HMENU styleMenu=::GetSubMenu(probe.root,4); MENUITEMINFOW initialState={}; initialState.cbSize=sizeof(initialState); initialState.fMask=MIIM_STATE;
        const bool savedState=::GetMenuItemInfoW(styleMenu,0,TRUE,&initialState)!=FALSE;
        probe.stateUpdated=false; probe.stateSynced=false; probe.stateDisabledPaint=false; probe.stateDisabledKey=false; probe.stateDisabledAccessible=false; route(4,L"style-state-sync");
        check("palette_menu_original_init_notification",probe.stateUpdated);
        check("palette_menu_updated_state_synced",probe.stateSynced);
        check("palette_menu_disabled_text_color",probe.stateDisabledPaint);
        check("palette_menu_disabled_mnemonic_ignored",probe.stateDisabledKey);
        check("palette_menu_disabled_accessible_state",probe.stateDisabledAccessible);
        if(savedState) ::SetMenuItemInfoW(styleMenu,0,TRUE,&initialState);
        probe.nestedAttempt=false; probe.nestedSeen=false; probe.nestedPainted=false; probe.nestedAccessible=false; probe.nestedWindow=NULL; route(6,L"scripts-nested");
        check("palette_menu_keyboard_submenu",probe.nestedSeen);
        check("palette_menu_submenu_painted",probe.nestedPainted);
        check("palette_menu_submenu_accessible",probe.nestedAccessible);
        probe.nestedAttempt=false;
        route(6,L"scripts-refresh-open",-1,true);
        check("palette_menu_scripts_refresh_while_open",probe.refreshedOpen);
        check("palette_menu_refresh_fixture_created",probe.refreshFixtureCreated);
        check("palette_menu_refresh_closed_stale_popup",!::IsWindowVisible(probe.readyMenu));
        probe.nestedAttempt=false; probe.nestedSeen=false; probe.nestedPainted=false; probe.nestedAccessible=false; probe.nestedWindow=NULL;
        route(6,L"scripts-nested-after-refresh");
        check("palette_menu_reopened_updated_submenu",probe.nestedSeen&&probe.nestedPainted);
        check("palette_menu_reopened_accessible",probe.nestedAccessible);
        probe.nestedAttempt=false;
    }
    if(ThemeManager::IsDarkModeSupported()) {
        const InterfacePalette savedPalette=ThemeManager::GetNamedPalette(); ThemeManager::CustomPalette custom;
        const bool hadCustom=ThemeManager::GetCustomPalette(custom);
        ThemeManager::SetNamedPalette(INTERFACE_PALETTE_NAVY); ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
        probe.paletteColor=false; probe.paletteGutter=false; route(4,L"palette-navy"); check("palette_menu_color",probe.paletteColor);
        wchar_t requireGutter[2] = {};
        if(::GetEnvironmentVariableW(L"FBE_NEXT_TEST_REQUIRE_PALETTE_MENU_GUTTER",requireGutter,_countof(requireGutter)))
            check("palette_menu_gutter_color",probe.paletteGutter);
        wchar_t requireRenderer[2] = {};
        if(::GetEnvironmentVariableW(L"FBE_NEXT_TEST_PALETTE_MENU_RENDER",requireRenderer,_countof(requireRenderer)))
            {
                check("palette_menu_mnemonic",probe.mnemonicOk); check("palette_menu_snapshot_text",probe.snapshotText); check("palette_menu_accessible_text",probe.accessibleText); check("palette_menu_border_color",probe.paletteBorder); check("palette_menu_selected_color",probe.paletteSelected); check("palette_menu_selected_text_color",probe.paletteSelectedText); check("palette_menu_mouse_hover",probe.paletteHover);
                ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT); ThemeManager::SetNamedPalette(INTERFACE_PALETTE_SEPIA); ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
                probe.paletteColor=probe.paletteGutter=probe.paletteBorder=probe.paletteSelected=probe.paletteSelectedText=probe.paletteHover=probe.mnemonicOk=probe.snapshotText=probe.accessibleText=false;
                route(4,L"palette-sepia");
                check("palette_menu_light_body",probe.paletteColor); check("palette_menu_light_gutter",probe.paletteGutter); check("palette_menu_light_border",probe.paletteBorder);
                check("palette_menu_light_selection",probe.paletteSelected&&probe.paletteSelectedText&&probe.paletteHover);
                check("palette_menu_light_accessible",probe.snapshotText&&probe.accessibleText&&probe.mnemonicOk);
            }
        if(hadCustom) ThemeManager::SetCustomPalette(custom); else ThemeManager::SetNamedPalette(savedPalette);
    }
    ThemeManager::SetSelectedTheme(static_cast<InterfaceTheme>(start));ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());


