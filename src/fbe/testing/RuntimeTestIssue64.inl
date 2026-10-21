LRESULT CMainFrame::RunIssue64Runtime(CAtlFile& output) {
    bool passed=true;CStringA report;const auto originalTheme=ThemeManager::GetSelectedTheme();
    auto check=[&](const char* label,bool value){report.AppendFormat("%s=%d\n",label,value);passed=value&&passed;};
    std::vector<wchar_t> artifacts(32768);::GetEnvironmentVariableW(L"FBE_NEXT_TEST_ARTIFACT_DIR",artifacts.data(),static_cast<DWORD>(artifacts.size()));
    PluginOptionsProbe::captures=artifacts.data();PluginOptionsProbe::prefix=L"issue64";
    std::map<std::pair<UINT,UINT>,std::vector<bool>> silhouettes,coreStrokes;
    std::map<std::pair<UINT,UINT>,std::vector<DWORD>> coloredIcons;
    DWORD warmedGdi=0,warmedUser=0;
    auto pixelsFor=[](HIMAGELIST images,int index,int size) {
        std::vector<DWORD> result(static_cast<size_t>(size)*size),white(result.size());
        BITMAPINFO dib={};dib.bmiHeader.biSize=sizeof(dib.bmiHeader);dib.bmiHeader.biWidth=size;dib.bmiHeader.biHeight=-size;dib.bmiHeader.biPlanes=1;dib.bmiHeader.biBitCount=32;
        void* bits=NULL;HBITMAP bitmap=::CreateDIBSection(NULL,&dib,DIB_RGB_COLORS,&bits,NULL,0);HDC dc=::CreateCompatibleDC(NULL);
        if(!bitmap||!dc||!bits){if(bitmap)::DeleteObject(bitmap);if(dc)::DeleteDC(dc);return std::vector<DWORD>();}
        HGDIOBJ old=::SelectObject(dc,bitmap);bool ready=true;
        for(int pass=0;pass<2;++pass){std::fill_n(static_cast<DWORD*>(bits),result.size(),pass?0:0xFFFFFFFF);ready=::ImageList_Draw(images,index,dc,0,0,ILD_TRANSPARENT)!=FALSE&&ready;::GdiFlush();std::copy_n(static_cast<DWORD*>(bits),result.size(),pass?result.begin():white.begin());}
        ::SelectObject(dc,old);::DeleteDC(dc);::DeleteObject(bitmap);
        for(size_t i=0;i<result.size();++i){const DWORD alpha=255-(((white[i]>>16)&255)-((result[i]>>16)&255));result[i]=(result[i]&0xFFFFFF)|(alpha<<24);}
        if(!ready)result.clear();return result;
    };    CString analysisError;const auto initialDom=m_doc->CreateDOMForAnalysis(&analysisError);
    if(!initialDom) {report.AppendFormat("initial_dom_error=%s\nresult=FAIL\n",CStringA(analysisError).GetString());DWORD bytes=0;output.Write(report,report.GetLength(),&bytes);output.Close();::PostQuitMessage(1);return 0;}
    const CString beforeXml(static_cast<LPCWSTR>(initialDom->xml));const bool beforeDirty=DocChanged();
    const DWORD gdiBefore=::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS),userBefore=::GetGuiResources(::GetCurrentProcess(),GR_USEROBJECTS);
    ShowWindow(SW_SHOWNORMAL);UpdateWindow();
    RECT startupButton={};m_CmdToolbar.GetItemRect(0,&startupButton);report.AppendFormat("startup_buttons=%d;first_rect=%ld,%ld,%ld,%ld\n",m_CmdToolbar.GetButtonCount(),startupButton.left,startupButton.top,startupButton.right,startupButton.bottom);
    check("startup_toolbar_images_present",::ImageList_GetImageCount(m_CmdToolbar.GetImageList())>=30);
    check("startup_toolbar_visible",::IsWindowVisible(m_CmdToolbar)!=FALSE);
    check("startup_toolbar_capture",PluginOptionsProbe::Capture(m_CmdToolbar,L"startup-toolbar"));
    auto untouched=[](HWND window) {
        bool clean=::GetPropW(window,L"FBE.AppliedThemeGeneration")==NULL;
        ::EnumChildWindows(window,[](HWND child,LPARAM state)->BOOL {
            auto& clean=*reinterpret_cast<bool*>(state);clean=clean&&::GetPropW(child,L"FBE.AppliedThemeGeneration")==NULL;return TRUE;
        },reinterpret_cast<LPARAM>(&clean));return clean;
    };
    // This is also exercised while Show() lives in a separate plugin DLL.
    struct NativeProbe {
        static void CALLBACK Tick(HWND,UINT,UINT_PTR,DWORD) {
            bool options=false;
            ::EnumThreadWindows(::GetCurrentThreadId(),[](HWND window,LPARAM state)->BOOL {
                HWND owner=::GetWindow(window,GW_OWNER);
                if(::GetPropW(window,L"FBE.OwnedWindow")&&owner&&::GetPropW(owner,L"FBE.NativeFileDialog")&&::IsWindowVisible(window)) {
                    *reinterpret_cast<bool*>(state)=true;
                    const bool previous=PluginOptionsProbe::expectedDarkTabs;PluginOptionsProbe::expectedDarkTabs=ThemeManager::IsDark();
                    HWND tab=PluginOptionsProbe::Tab(window);PluginOptionsProbe::Check(tab!=NULL,__LINE__);
                    if(tab)for(int page=0;page<TabCtrl_GetItemCount(tab);++page) {
                        PluginOptionsProbe::Select(window,tab,page);
                        PluginOptionsProbe::Check(PluginOptionsProbe::Capture(window,L"nested-options-"+std::to_wstring(page)),__LINE__);
                    }
                    PluginOptionsProbe::expectedDarkTabs=previous;
                    ::SetPropW(owner,L"FBE.Issue64.SettingsOpened",reinterpret_cast<HANDLE>(2));
                    ::PostMessageW(window,WM_COMMAND,IDCANCEL,0);return FALSE;
                }return TRUE;
            },reinterpret_cast<LPARAM>(&options));
            if(options)return;
            ::EnumThreadWindows(::GetCurrentThreadId(),[](HWND window,LPARAM)->BOOL {
                if(::GetPropW(window,L"FBE.NativeFileDialog")&&!::GetPropW(window,L"FBE.OwnedWindow")&&::IsWindowVisible(window)) {
                    ::SetPropW(window,L"FBE.Issue64.ScopeSeen",reinterpret_cast<HANDLE>(ModernFileDialog::IsShowingOnCurrentThread()?1:2));
                    ThemeManager::ApplyToWindow(window);ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
                    bool clean=::GetPropW(window,L"FBE.AppliedThemeGeneration")==NULL;
                    ::EnumChildWindows(window,[](HWND child,LPARAM state)->BOOL {auto& clean=*reinterpret_cast<bool*>(state);clean=clean&&::GetPropW(child,L"FBE.AppliedThemeGeneration")==NULL;return TRUE;},reinterpret_cast<LPARAM>(&clean));
                    PluginOptionsProbe::Check(clean&&ModernFileDialog::IsShowingOnCurrentThread(),__LINE__);
                    if(::IsWindowEnabled(window)&&!::GetPropW(window,L"FBE.Issue64.SettingsOpened")) {
                        HWND settings=NULL;
                        ::EnumChildWindows(window,[](HWND child,LPARAM state)->BOOL {
                            wchar_t kind[64]={},text[512]={};::GetClassNameW(child,kind,_countof(kind));::GetWindowTextW(child,text,_countof(text));
                            if(!wcscmp(kind,L"Button")&&(wcsstr(text,L"Настройки")||wcsstr(text,L"Параметры")||wcsstr(text,L"settings"))) {*reinterpret_cast<HWND*>(state)=child;return FALSE;}return TRUE;
                        },reinterpret_cast<LPARAM>(&settings));
                        if(settings) {::SetPropW(window,L"FBE.Issue64.SettingsOpened",reinterpret_cast<HANDLE>(1));::PostMessageW(settings,BM_CLICK,0,0);return FALSE;}
                    }
                    if(!::IsWindowEnabled(window))return FALSE;
                    if(::GetPropW(window,L"FBE.Issue64.SettingsOpened")==reinterpret_cast<HANDLE>(1))return FALSE;
                    PluginOptionsProbe::dialog=window;
                    PluginOptionsProbe::Check(PluginOptionsProbe::Capture(window,L"shell"),__LINE__);
                    ::PostMessageW(window,WM_CLOSE,0,0);return FALSE;
                }return TRUE;
            },0);
        }
    };
    for(InterfaceTheme theme:{INTERFACE_THEME_LIGHT,INTERFACE_THEME_DARK,INTERFACE_THEME_AUTOMATIC}) {
        ThemeManager::SetSelectedTheme(theme);ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
        check("actual_selected_theme",ThemeManager::GetSelectedTheme()==theme);
        const auto palette=ThemeManager::ControlColor();
        {
            ModernFileDialog::ShowingScope outer(m_hWnd);check("scope_enter",outer.Valid()&&ModernFileDialog::IsShowingOnCurrentThread());
            try { ModernFileDialog::ShowingScope nested(NULL);check("nested_null_owner",ModernFileDialog::CurrentOwnerOnThread()==NULL);throw 1;}catch(int){}
            check("nested_restore",ModernFileDialog::CurrentOwnerOnThread()==m_hWnd);
            HWND shell=::CreateWindowExW(0,L"STATIC",L"protected subtree",WS_POPUP,0,0,1,1,m_hWnd,NULL,_Module.GetModuleInstance(),NULL);
            ::SetPropW(shell,L"FBE.NativeFileDialog",reinterpret_cast<HANDLE>(1));
            HWND child=::CreateWindowExW(0,L"STATIC",L"early child",WS_CHILD,0,0,1,1,shell,NULL,_Module.GetModuleInstance(),NULL);
            ThemeManager::ApplyToWindow(child);ThemeManager::ApplyToWindow(shell);check("early_child_excluded",untouched(shell));
            ::SetPropW(shell,L"FBE.OwnedWindow",reinterpret_cast<HANDLE>(1));::RemovePropW(shell,L"FBE.NativeFileDialog");
            ThemeManager::ApplyToWindow(shell);check("owned_dialog_not_excluded",::GetPropW(child,L"FBE.AppliedThemeGeneration")!=NULL);::DestroyWindow(shell);
        }
        check("scope_exit",!ModernFileDialog::IsShowingOnCurrentThread()&&ThemeManager::ControlColor()==palette);
        for(int plugin=-1;plugin<4;++plugin) {
            PluginOptionsProbe::dialog=NULL;PluginOptionsProbe::failedLine=0;
            PluginOptionsProbe::prefix=L"issue64-theme"+std::to_wstring(theme)+L"-plugin"+std::to_wstring(plugin);
            const UINT_PTR timer=::SetTimer(NULL,0,250,NativeProbe::Tick);check("shell_timer",timer!=0);
            HRESULT result=E_FAIL;
            if(plugin<0) {ModernFileDialog::Request request;request.title=L"Issue64 native open";const auto outcome=ModernFileDialog::Show(m_hWnd,request);result=outcome.error;}
            else {
                const wchar_t* ids[]={L"{C3098839-EF69-4DE5-B27D-1E80051CA843}",L"{09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56}",L"{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}",L"{36FCFB2D-C3D8-4B81-ABC1-5A09CA846515}"};
                CLSID clsid={};::CLSIDFromString(ids[plugin],&clsid);IUnknownPtr instance;
                result=m_plugins.Manager().CreateInstance(clsid,instance);
                CComPtr<IFBEPluginHost> host;if(SUCCEEDED(result))result=FbePluginApiV2::CreateHost(m_hWnd,L"ru-RU",&host);
                if(SUCCEEDED(result)&&plugin==2) {CComQIPtr<IFBEImportPlugin2> importer(instance);CComBSTR name;CComPtr<IStream> stream;result=importer?importer->Import(host,&name,&stream):E_NOINTERFACE;}
                else if(SUCCEEDED(result)) {CComQIPtr<IFBEExportPlugin2> exporter(instance);CComPtr<IFBEDocumentSnapshot> snapshot;result=FbePluginApiV2::CreateSnapshot(m_doc->CreateDOMForAnalysis(),L"fixture.fb2",L"utf-8",&snapshot);if(SUCCEEDED(result))result=exporter?exporter->Export(host,CComBSTR(L"fixture.fb2"),snapshot):E_NOINTERFACE;}
            }
            ::KillTimer(NULL,timer);
            check("native_dialog_untouched",PluginOptionsProbe::dialog&&!PluginOptionsProbe::failedLine);
            check("native_cancel",result==HRESULT_FROM_WIN32(ERROR_CANCELLED)||result==E_ABORT||result==S_FALSE);
            check("plugin_scope_removed",!ModernFileDialog::IsShowingOnCurrentThread());
        }
        for(UINT dpi:{96u,120u,144u,192u}) {
            check("toolbar_rebuilt",RebuildCommandToolbarImages(dpi));
            const int size=ToolbarFactory::CommandToolbarImageSize(dpi);
            check("toolbar_images_present",::ImageList_GetImageCount(m_CmdToolbar.GetImageList())>=30);
            for(UINT command:{ID_EDIT_BOLD,ID_EDIT_ITALIC,ID_EDIT_STRIK,ID_EDIT_SUP,ID_EDIT_SUB,ID_EDIT_CODE}) {
                const int index=static_cast<int>(m_CmdToolbar.SendMessage(TB_GETBITMAP,command,0));
                const auto pixels=pixelsFor(m_CmdToolbar.GetImageList(),index,size);check("format_glyph_pixels",!pixels.empty());
                unsigned strokes=0,bad=0,maxAlpha=0,minRed=255; for(DWORD p:pixels){maxAlpha=(std::max)(maxAlpha,static_cast<unsigned>(p>>24));if(p>>24)minRed=(std::min)(minRed,static_cast<unsigned>((p>>16)&255));} report.AppendFormat("pixel_max_alpha=%u;min_red=%u\n",maxAlpha,minRed);
                std::vector<bool> silhouette,core;
                auto luminance=[](COLORREF c) {auto channel=[](BYTE v){double s=v/255.0;return s<=0.04045?s/12.92:pow((s+0.055)/1.055,2.4);};return 0.2126*channel(GetRValue(c))+0.7152*channel(GetGValue(c))+0.0722*channel(GetBValue(c));};
                const double background=luminance(ThemeManager::ControlColor());
                const auto key=std::make_pair(dpi,command);
                for(size_t position=0;position<pixels.size();++position) {
                    const DWORD pixel=pixels[position],alpha=pixel>>24;silhouette.push_back(alpha!=0);
                    // The legacy artwork has partially transparent core pixels. Use high-coverage strokes; keep the 3:1 contrast requirement.
                    core.push_back(alpha>=192&&((pixel>>16)&255)<96&&((pixel>>8)&255)<96&&(pixel&255)<96);
                    const bool stroke=theme==INTERFACE_THEME_LIGHT?core.back():position<coreStrokes[key].size()&&coreStrokes[key][position];
                    if(!stroke)continue;++strokes;
                    const COLORREF surface=ThemeManager::ControlColor();
                    const COLORREF composed=RGB(((pixel>>16)&255)+GetRValue(surface)*(255-alpha)/255,((pixel>>8)&255)+GetGValue(surface)*(255-alpha)/255,(pixel&255)+GetBValue(surface)*(255-alpha)/255);
                    const double value=luminance(composed);if(((std::max)(value,background)+0.05)/((std::min)(value,background)+0.05)<3.0)++bad;
                }
                check("visible_core_strokes_contrast_3",strokes>0&&bad==0);
                report.AppendFormat("glyph_theme=%d;dpi=%u;command=%u;index=%d;core=%u;bad=%u\n",theme,dpi,command,index,strokes,bad);
                if(theme==INTERFACE_THEME_LIGHT){silhouettes[key]=silhouette;coreStrokes[key]=core;}else check("format_alpha_silhouette_preserved",silhouettes[key]==silhouette);
            }
            for(UINT command:{ID_FILE_OPEN,ID_FILE_SAVE}) {
                const auto pixels=pixelsFor(m_CmdToolbar.GetImageList(),static_cast<int>(m_CmdToolbar.SendMessage(TB_GETBITMAP,command,0)),size);const auto key=std::make_pair(dpi,command);
                if(theme==INTERFACE_THEME_LIGHT)coloredIcons[key]=pixels;else check("colored_artwork_unchanged",!pixels.empty()&&pixels==coloredIcons[key]);
            }
            PluginOptionsProbe::prefix=L"issue64-theme"+std::to_wstring(theme)+L"-dpi"+std::to_wstring(dpi);
            std::vector<std::pair<UINT,LRESULT>> states;
            for(UINT command:{ID_EDIT_BOLD,ID_EDIT_ITALIC,ID_EDIT_STRIK,ID_EDIT_SUP,ID_EDIT_SUB,ID_EDIT_CODE})states.emplace_back(command,m_CmdToolbar.SendMessage(TB_GETSTATE,command,0));
            const wchar_t* stateNames[]={L"normal",L"hover",L"pressed",L"checked",L"disabled"};
            for(int state=0;state<5;++state) {
                for(const auto& button:states) {
                    LRESULT value=button.second&~(TBSTATE_ENABLED|TBSTATE_CHECKED|TBSTATE_PRESSED);
                    if(state!=4)value|=TBSTATE_ENABLED;if(state==2)value|=TBSTATE_PRESSED;if(state==3)value|=TBSTATE_CHECKED;
                    m_CmdToolbar.SendMessage(TB_SETSTATE,button.first,MAKELPARAM(value,0));
                }
                m_CmdToolbar.SendMessage(TB_SETHOTITEM,state==1?m_CmdToolbar.SendMessage(TB_COMMANDTOINDEX,ID_EDIT_BOLD,0):-1,0);
                check("toolbar_state_capture",PluginOptionsProbe::Capture(m_CmdToolbar,stateNames[state]));
            }
            for(const auto& button:states)m_CmdToolbar.SendMessage(TB_SETSTATE,button.first,MAKELPARAM(button.second,0));
            m_CmdToolbar.SendMessage(TB_SETHOTITEM,static_cast<WPARAM>(-1),0);
        }
        const DWORD gdi=::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS),user=::GetGuiResources(::GetCurrentProcess(),GR_USEROBJECTS);
        report.AppendFormat("theme=%d;gdi=%lu;user=%lu\n",theme,gdi,user);
        if(theme==INTERFACE_THEME_LIGHT){warmedGdi=gdi;warmedUser=user;}
    }
    ThemeManager::SetSelectedTheme(originalTheme);ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
    check("document_unchanged",beforeXml==CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml))&&beforeDirty==DocChanged());
    const int gdiDelta=static_cast<int>(::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS))-static_cast<int>(gdiBefore),userDelta=static_cast<int>(::GetGuiResources(::GetCurrentProcess(),GR_USEROBJECTS))-static_cast<int>(userBefore);
    report.AppendFormat("gdi_delta=%d\nuser_delta=%d\nwarmed_gdi_delta=%ld\nwarmed_user_delta=%ld\n",gdiDelta,userDelta,static_cast<long>(::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS))-static_cast<long>(warmedGdi),static_cast<long>(::GetGuiResources(::GetCurrentProcess(),GR_USEROBJECTS))-static_cast<long>(warmedUser));
    check("resources_bounded_after_shell_warmup",::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS)<=warmedGdi+30&&::GetGuiResources(::GetCurrentProcess(),GR_USEROBJECTS)<=warmedUser+10);
    report.AppendFormat("result=%s\n",passed?"PASS":"FAIL");DWORD bytes=0;output.Write(report,report.GetLength(),&bytes);output.Close();::PostQuitMessage(passed?0:1);return 0;
}
