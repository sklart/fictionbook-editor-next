if (IsFbeTestScenario(L"ui-defects-runtime")) {
    bool passed = true; CStringA report;
    auto check = [&](const char* name, bool ok) {report.AppendFormat("%s=%d\n",name,ok);passed=ok&&passed;};
    ShowWindow(SW_SHOWNORMAL);
    wchar_t requested[16]={};::GetEnvironmentVariableW(L"FBE_NEXT_TEST_REQUESTED_THEME",requested,_countof(requested));
    check("requested_theme_applied",requested[0]&&ThemeManager::GetSelectedTheme()==_wtoi(requested));
    const CString initialXml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));const bool initialDirty=DocChanged();
    std::vector<wchar_t> captures(32768);::GetEnvironmentVariableW(L"FBE_NEXT_TEST_ARTIFACT_DIR",captures.data(),static_cast<DWORD>(captures.size()));
    PluginOptionsProbe::captures=captures.data();PluginOptionsProbe::dialog=m_hWnd;
    auto showTools=[&](HMENU menu,const std::wstring& name) {
        PluginOptionsProbe::prefix=name;::RemovePropW(m_hWnd,L"FBE.MenuCapture");
        struct MenuProbe {static void CALLBACK Tick(HWND,UINT,UINT_PTR,DWORD) {
            ::EnumThreadWindows(::GetCurrentThreadId(),[](HWND window,LPARAM)->BOOL {
                wchar_t kind[32]={};::GetClassNameW(window,kind,_countof(kind));
                if(!wcscmp(kind,L"#32768")&&::IsWindowVisible(window)) {
                    if(PluginOptionsProbe::Capture(window,L"tools"))::SetPropW(PluginOptionsProbe::dialog,L"FBE.MenuCapture",reinterpret_cast<HANDLE>(1));return FALSE;
                }return TRUE;
            },0);::EndMenu();
        }};
        const UINT_PTR timer=::SetTimer(NULL,0,120,MenuProbe::Tick);check("menu_timer",timer!=0);
        RECT bounds={};m_MenuBar.GetWindowRect(&bounds);
        ThemeManager::TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,bounds.left,bounds.bottom,m_hWnd);
        ::KillTimer(NULL,timer);check("menu_display_capture",::GetPropW(m_hWnd,L"FBE.MenuCapture")!=NULL);
    };
    auto toolsMenu = [&]() {
        HMENU menu=::GetSubMenu(m_MenuBar.GetMenu(),5);
        int count=0,options=0,position=-1;
        for(int i=0;i<::GetMenuItemCount(menu);++i)if(::GetMenuItemID(menu,i)==ID_VIEW_OPTIONS)++options;
        check("settings_once",options==1);
        for(int i=0;i<::GetMenuItemCount(menu);++i)if(::GetMenuItemID(menu,i)==ID_TOOLS_REFRESH_SCRIPTS){++count;position=i;}
        check("refresh_once",count==1);
        check("refresh_neighbors",position==2&&::GetMenuItemID(menu,position-1)==ID_VIEW_OPTIONS&&::GetMenuItemID(menu,position+2)==ID_TOOLS_SPELLCHECK);
        for(int i=0;i<::GetMenuItemCount(menu);++i){
            wchar_t text[1024]={};MENUITEMINFO info={};info.cbSize=sizeof(info);info.fMask=MIIM_FTYPE|MIIM_STRING|MIIM_ID;info.dwTypeData=text;info.cch=_countof(text);
            check("menu_info",::GetMenuItemInfo(menu,i,TRUE,&info)!=FALSE);
            if(!(info.fType&MFT_SEPARATOR))check("tools_caption",text[0]!=0&&!(info.fType&MFT_OWNERDRAW));
        }
        wchar_t caption[1024]={};::GetMenuString(menu,ID_TOOLS_REFRESH_SCRIPTS,caption,_countof(caption),MF_BYCOMMAND);
        check("refresh_translation",CString(caption).Find(FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.tools.refresh_scripts"))==0);
        for(const auto& neighbor:{std::make_pair(ID_TOOLS_WORDS,L"fbe.menu.idr_mainframe.tools.words"),std::make_pair(ID_VIEW_OPTIONS,L"fbe.menu.idr_mainframe.tools.options"),std::make_pair(ID_TOOLS_SPELLCHECK,L"fbe.menu.idr_mainframe.tools.spellcheck")}) {
            wchar_t text[1024]={};::GetMenuStringW(menu,neighbor.first,text,_countof(text),MF_BYCOMMAND);
            check("neighbor_translation",CString(text).Find(FbeLoadRuntimeStringByKey(neighbor.second))==0);
        }
        return menu;
    };
    HMENU tools=toolsMenu();
    auto metadata=[&]() {MENUITEMINFO info={};info.cbSize=sizeof(info);info.fMask=MIIM_BITMAP|MIIM_STATE|MIIM_ID|MIIM_FTYPE|MIIM_DATA|MIIM_SUBMENU;check("refresh_metadata_read",::GetMenuItemInfoW(tools,ID_TOOLS_REFRESH_SCRIPTS,FALSE,&info)!=FALSE);return info;};
    const auto original=metadata();
#include "RuntimeTestMainMenu.inl"
    showTools(tools,L"startup");
    {
        auto& tree=m_document_tree.m_tree.m_tree;
        tree.SetScriptMode(true);
        check("scripts_classic_before_theme",::GetWindowTheme(tree)==NULL);
        ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
        check("scripts_classic_after_theme",::GetWindowTheme(tree)==NULL);
        report.AppendFormat("script_theme_after_refresh=%p\n",::GetWindowTheme(tree));
        tree.SetScriptMode(false);
        check("structure_themed_after_mode_switch",!::IsAppThemed()||::GetWindowTheme(tree)!=NULL);
        m_document_tree.GetDocumentStructure(m_doc->m_body.Document());
    }
    const InterfaceTheme sequence[]={INTERFACE_THEME_LIGHT,INTERFACE_THEME_DARK,INTERFACE_THEME_LIGHT,INTERFACE_THEME_DARK,INTERFACE_THEME_LIGHT,INTERFACE_THEME_DARK,INTERFACE_THEME_AUTOMATIC,INTERFACE_THEME_LIGHT,INTERFACE_THEME_DARK,INTERFACE_THEME_AUTOMATIC};
    const DWORD savedLanguage=_Settings.GetInterfaceLanguageID();
    for(int iteration=0;iteration<40;++iteration) {
        if(iteration%2==0)SendMessage(WM_COMMAND,ID_TOOLS_REFRESH_SCRIPTS);
        ThemeManager::SetSelectedTheme(sequence[iteration%_countof(sequence)]);ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
        _Settings.SetInterfaceLanguage(iteration%3==1?1033:1049);FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName());FbeResetRuntimeLocalization();RefreshLocalizedMainFrameUi();RebuildRuntimeHotkeys();
        if(iteration%2)SendMessage(WM_COMMAND,ID_TOOLS_REFRESH_SCRIPTS);
        toolsMenu();const auto now=metadata();
        check("menu_metadata_preserved",now.wID==original.wID&&now.fType==original.fType&&now.dwItemData==original.dwItemData&&now.hSubMenu==original.hSubMenu&&now.hbmpItem==original.hbmpItem);
        check("refresh_enabled",!(now.fState&(MFS_DISABLED|MFS_GRAYED)));
        if(iteration<10)showTools(tools,L"cycle-"+std::to_wstring(iteration));
    }
    _Settings.SetInterfaceLanguage(savedLanguage);FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName());FbeResetRuntimeLocalization();RefreshLocalizedMainFrameUi();
    ThemeManager::SetSelectedTheme(static_cast<InterfaceTheme>(_wtoi(requested)));ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
    check("theme_document_preserved",initialXml==CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml))&&initialDirty==DocChanged());
    // Localization must preserve the command's state and bitmap metadata.
    ::EnableMenuItem(tools,ID_TOOLS_REFRESH_SCRIPTS,MF_BYCOMMAND|MF_GRAYED);
    RefreshLocalizedMainFrameUi();
    check("localization_preserves_state",(::GetMenuState(tools,ID_TOOLS_REFRESH_SCRIPTS,MF_BYCOMMAND)&MF_GRAYED)!=0);
    ::EnableMenuItem(tools,ID_TOOLS_REFRESH_SCRIPTS,MF_BYCOMMAND|MF_ENABLED);
    RebuildRuntimeHotkeys();toolsMenu();
    for(int cycle=0;cycle<2;++cycle) {
        m_MenuBar.SendMessage(WM_INITMENUPOPUP,reinterpret_cast<WPARAM>(tools),0);
        toolsMenu();
        SendMessage(WM_COMMAND,ID_TOOLS_REFRESH_SCRIPTS);toolsMenu();
    }
    const DWORD language=_Settings.GetInterfaceLanguageID();
    _Settings.SetInterfaceLanguage(language==1049?1033:1049);
    FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName());
    FbeResetRuntimeLocalization();RefreshLocalizedMainFrameUi();RebuildRuntimeHotkeys();toolsMenu();
    _Settings.SetInterfaceLanguage(language);FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName());FbeResetRuntimeLocalization();RefreshLocalizedMainFrameUi();
    for(int reopen=0;reopen<2;++reopen) {
        ::RemovePropW(m_hWnd,L"FBE_UI_DEFECTS_SETTINGS");
        BOOL handled=FALSE;OnViewOptions(0,ID_VIEW_OPTIONS,NULL,handled);
        check("settings_caret",::GetPropW(m_hWnd,L"FBE_UI_DEFECTS_SETTINGS")==reinterpret_cast<HANDLE>(1));
    }
    m_doc->MarkSavePoint();m_source.SendMessage(SCI_SETSAVEPOINT);
    SendMessage(WM_COMMAND,ID_FILE_CLOSE);OnIdle();
    check("closed_session",!m_document_session.HasDocument());
    for(UINT command:{ID_VIEW_DESC,ID_VIEW_BODY,ID_VIEW_SOURCE}) {
        const LRESULT state=m_CmdToolbar.SendMessage(TB_GETSTATE,command,0);
        check("closed_mode_state",state!=-1&&(state&(TBSTATE_ENABLED|TBSTATE_CHECKED|TBSTATE_PRESSED))==0);
        CClientDC display(m_CmdToolbar);CDC paint;paint.CreateCompatibleDC(display);
        CBitmap bitmap;bitmap.CreateCompatibleBitmap(display,32,32);HBITMAP old=paint.SelectBitmap(bitmap);paint.FillSolidRect(0,0,32,32,RGB(255,255,255));
        NMTBCUSTOMDRAW draw={};draw.nmcd.hdr.hwndFrom=m_CmdToolbar;draw.nmcd.dwDrawStage=CDDS_ITEMPREPAINT;
        draw.nmcd.hdc=paint;draw.nmcd.rc={0,0,32,32};draw.nmcd.dwItemSpec=command;draw.nmcd.uItemState=CDIS_DISABLED;
        BOOL handled=FALSE;check("disabled_mode_renderer",OnCommandToolbarCustomDraw(0,&draw.nmcd.hdr,handled)==CDRF_SKIPDEFAULT);
        bool gray=true;for(int y=0;y<32;++y)for(int x=0;x<32;++x){const COLORREF pixel=paint.GetPixel(x,y);gray=gray&&GetRValue(pixel)==GetGValue(pixel)&&GetGValue(pixel)==GetBValue(pixel);}
        check("disabled_mode_grayscale",gray);paint.SelectBitmap(old);
        SendMessage(WM_COMMAND,command);
        check("closed_mode_inert",!m_document_session.HasDocument()&&m_view.GetActiveWnd()==NULL);
    }
    toolsMenu();check("refresh_without_book",!(::GetMenuState(tools,ID_TOOLS_REFRESH_SCRIPTS,MF_BYCOMMAND)&MF_GRAYED));
    PluginOptionsProbe::dialog=m_hWnd;showTools(tools,L"closed");
    route(5,L"closed-commandbar");::RemovePropW(m_hWnd,L"FBE.MenuRoute");::RemoveWindowSubclass(m_hWnd,MenuRouteProbe::Commands,0x4d525450);
    report.AppendFormat("result=%s\n",passed?"PASS":"FAIL");DWORD written=0;output.Write(report,report.GetLength(),&written);output.Close();::PostQuitMessage(passed?0:1);return 0;
}
