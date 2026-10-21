if (IsFbeTestScenario(L"ui-defects-runtime")) {
    bool passed = true; CStringA report;
    auto check = [&](const char* name, bool ok) {report.AppendFormat("%s=%d\n",name,ok);passed=ok&&passed;};
    ShowWindow(SW_SHOWNORMAL);
    auto toolsMenu = [&]() {
        HMENU menu=::GetSubMenu(m_MenuBar.GetMenu(),5);
        int count=0,position=-1;
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
        return menu;
    };
    HMENU tools=toolsMenu();
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
    report.AppendFormat("result=%s\n",passed?"PASS":"FAIL");DWORD written=0;output.Write(report,report.GetLength(),&written);output.Close();::PostQuitMessage(passed?0:1);return 0;
}
