// Regression for caller-owned bitmaps destroyed/replaced during a native popup.
{
    // Compare the actual WTL command/image pairing with the native registry.
    // Inspect DIB pixels as well as handles: an existing empty bitmap is not
    // evidence that an icon can be seen.
    unsigned mapped=0, missing=0, empty=0, invalidAlpha=0, magenta=0, scripts=0, folders=0, plugins=0, dpiFailures=0;
    const int imageCount=(std::min)(m_MenuBar.m_arrCommand.GetSize(),::ImageList_GetImageCount(m_MenuBar.m_hImageList));
    for(int index=0;index<imageCount;++index) {
        const UINT command=m_MenuBar.m_arrCommand[index];
        const HBITMAP bitmap=ThemeManager::NativeMenuBitmap(command);
        const bool script=command>=ID_SCRIPT_BASE&&command<=ID_SCRIPT_BASE+SCRIPT_COMMAND_COUNT;
        const bool folder=command>=SCRIPT_FOLDER_MENU_ID_BASE&&command<=SCRIPT_FOLDER_MENU_ID_BASE+SCRIPT_FOLDER_MENU_ID_COUNT;
        if(script)++scripts; if(folder)++folders;
        if((command>=ID_PLUGIN_IMPORT_FIRST&&command<=ID_PLUGIN_IMPORT_LAST)||(command>=ID_PLUGIN_EXPORT_FIRST&&command<=ID_PLUGIN_EXPORT_LAST))++plugins;
        ++mapped;
        if(!bitmap||::GetObjectType(bitmap)!=OBJ_BITMAP){++missing;continue;}
        for(UINT dpi : {96u,144u,192u}) {
            HICON icon=::ImageList_GetIcon(m_MenuBar.m_hImageList,index,ILD_TRANSPARENT);
            const int size=UiMetrics::ScaleForDpi(16,dpi);
            HBITMAP scaled=FbeScripts::Detail::CreateMenuBitmap(icon,size);
            if(icon)::DestroyIcon(icon);
            DIBSECTION scaledInfo={};
            if(!scaled||::GetObject(scaled,sizeof(scaledInfo),&scaledInfo)!=sizeof(scaledInfo)||
                !scaledInfo.dsBm.bmBits||scaledInfo.dsBm.bmWidth!=size||abs(scaledInfo.dsBmih.biHeight)!=size) ++dpiFailures;
            else {
                const DWORD* data=static_cast<const DWORD*>(scaledInfo.dsBm.bmBits);bool hasAlpha=false, valid=true;
                for(int px=0;px<size*size;++px) {
                    const DWORD p=data[px],a=p>>24;if(a)hasAlpha=true;
                    if(((p>>16)&255)>a||((p>>8)&255)>a||(p&255)>a||(a&&(p&0xffffff)==0xff00ff))valid=false;
                }
                if(!hasAlpha||!valid)++dpiFailures;
            }
            if(scaled)::DeleteObject(scaled);
        }
        DIBSECTION info={};
        if(::GetObject(bitmap,sizeof(info),&info)!=sizeof(info)||!info.dsBm.bmBits||info.dsBm.bmBitsPixel!=32){++empty;continue;}
        const DWORD* pixels=static_cast<const DWORD*>(info.dsBm.bmBits);const int count=info.dsBm.bmWidth*abs(info.dsBmih.biHeight);unsigned visible=0;
        for(int pixel=0;pixel<count;++pixel){const DWORD p=pixels[pixel],a=p>>24;if(a)++visible;if(a&&(p&0xffffff)==0xff00ff)++magenta;if(((p>>16)&255)>a||((p>>8)&255)>a||(p&255)>a)++invalidAlpha;}
        if(!visible)++empty;
    }
    report.AppendFormat("native_menu_mapped=%u\nnative_menu_missing=%u\nnative_menu_empty=%u\nnative_menu_invalid_alpha=%u\nnative_menu_magenta=%u\nnative_menu_scripts=%u\nnative_menu_folders=%u\nnative_menu_plugins=%u\nnative_menu_dpi_failures=%u\n",mapped,missing,empty,invalidAlpha,magenta,scripts,folders,plugins,dpiFailures);
    check("native_menu_all_wtl_commands_mapped",mapped>=35&&missing==0);
    check("native_menu_visible_pixels",empty==0&&invalidAlpha==0&&magenta==0);
    check("native_menu_bitmap_dpi_100_150_200",dpiFailures==0);
    check("native_menu_script_and_folder_mapped",scripts>0&&folders>0);
    wchar_t requirePluginIcons[4]={};
    if(::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MENU_PLUGIN_ICONS",requirePluginIcons,_countof(requirePluginIcons)))
        check("native_menu_plugin_icons_mapped",plugins>=2);
    const UINT actualDpi=UiMetrics::DpiForWindow(m_hWnd);
    RebuildOwnedNativeMenuBitmaps(ATL::_AtlBaseModule.GetModuleInstance(),actualDpi);
    RegisterCommandBarNativeMenuBitmaps(m_MenuBar,actualDpi);
    const DWORD rebuildBefore=::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS);
    for(int cycle=0;cycle<12;++cycle) {
        const UINT dpi=cycle%3==0?96u:(cycle%3==1?144u:192u);
        RebuildOwnedNativeMenuBitmaps(ATL::_AtlBaseModule.GetModuleInstance(),dpi);
        RegisterCommandBarNativeMenuBitmaps(m_MenuBar,dpi);
    }
    RebuildOwnedNativeMenuBitmaps(ATL::_AtlBaseModule.GetModuleInstance(),actualDpi);
    RegisterCommandBarNativeMenuBitmaps(m_MenuBar,actualDpi);
    const DWORD rebuildAfter=::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS);
    report.AppendFormat("native_menu_rebuild_gdi_before=%lu\nnative_menu_rebuild_gdi_after=%lu\n",rebuildBefore,rebuildAfter);
    check("native_menu_rebuild_gdi_balanced",rebuildBefore==rebuildAfter);
    const DWORD replaceBefore=::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS);
    RegisterOwnedNativeMenuBitmap(ATL::_AtlBaseModule.GetModuleInstance(),IDB_PASTE_TEXT,ID_EDIT_PASTE_TEXT,actualDpi);
    RegisterOwnedNativeMenuBitmap(ATL::_AtlBaseModule.GetModuleInstance(),IDB_PASTE_TEXT,ID_EDIT_PASTE_TEXT,actualDpi);
    const DWORD replaceAfter=::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS);
    check("native_menu_replacement_gdi_balanced",replaceBefore==replaceAfter&&::GetObjectType(ThemeManager::NativeMenuBitmap(ID_EDIT_PASTE_TEXT))==OBJ_BITMAP);
    struct BitmapLifetimeProbe {
        HWND frame;HMENU menu,child;UINT command;HBITMAP previous,source,replacement;
        bool called=false,copyAlive=false,replacementAlive=false,folderCopyAlive=false,scaledOnPopup=false,darkOnlyCorrect=false,scaledAlphaCorrect=false;
        static HBITMAP Bitmap() {
            BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=4;info.bmiHeader.biHeight=-4;
            info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* pixels=NULL;
            HBITMAP bitmap=::CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);
            if(bitmap&&pixels)for(int i=0;i<16;++i)static_cast<DWORD*>(pixels)[i]=i==0?0:0xff48a080;
            return bitmap;
        }
        static void CALLBACK Tick(HWND,UINT,UINT_PTR timer,DWORD) {
            ::KillTimer(NULL,timer);
            auto* self=reinterpret_cast<BitmapLifetimeProbe*>(::GetPropW(PluginOptionsProbe::dialog,L"FBE.BitmapLifetime"));
            if(!self){::EndMenu();return;}
            MENUITEMINFO info={};info.cbSize=sizeof(info);info.fMask=MIIM_BITMAP;
            ::GetMenuItemInfoW(self->menu,0,TRUE,&info);const HBITMAP displayed=info.hbmpItem;
            self->copyAlive=displayed!=self->source&&::GetObjectType(displayed)==OBJ_BITMAP;
            BITMAP displayedSize={};
            const int expectedWidth=ThemeManager::IsDark()&&!ThemeManager::IsHighContrast()?UiMetrics::ScaleForDpi(16,UiMetrics::DpiForWindow(self->frame)):4;
            self->scaledOnPopup=::GetObjectW(displayed,sizeof(displayedSize),&displayedSize)==sizeof(displayedSize)&&displayedSize.bmWidth==expectedWidth;
            DIBSECTION scaled={};
            if(::GetObjectW(displayed,sizeof(scaled),&scaled)==sizeof(scaled)&&scaled.dsBm.bmBits&&scaled.dsBm.bmBitsPixel==32) {
                const DWORD* pixels=static_cast<const DWORD*>(scaled.dsBm.bmBits);const int count=scaled.dsBm.bmWidth*abs(scaled.dsBmih.biHeight);
                bool transparent=false,visible=false,valid=true;
                for(int i=0;i<count;++i) {const DWORD p=pixels[i],a=p>>24;transparent|=a==0;visible|=a!=0;if(((p>>16)&255)>a||((p>>8)&255)>a||(p&255)>a)valid=false;}
                self->scaledAlphaCorrect=transparent&&visible&&valid;
            }
            MENUITEMINFO folder={};folder.cbSize=sizeof(folder);folder.fMask=MIIM_BITMAP;
            self->folderCopyAlive=::GetMenuItemInfoW(self->menu,1,TRUE,&folder)&&folder.hbmpItem&&::GetObjectType(folder.hbmpItem)==OBJ_BITMAP;
            MENUITEMINFO darkOnly={};darkOnly.cbSize=sizeof(darkOnly);darkOnly.fMask=MIIM_BITMAP;
            const bool darkActive=ThemeManager::IsDark()&&!ThemeManager::IsHighContrast();
            self->darkOnlyCorrect=::GetMenuItemInfoW(self->menu,2,TRUE,&darkOnly)&&
                (darkActive ? darkOnly.hbmpItem&&::GetObjectType(darkOnly.hbmpItem)==OBJ_BITMAP : darkOnly.hbmpItem==NULL);
            ThemeManager::UnregisterNativeMenuBitmap(self->command);
            ::DeleteObject(self->previous);self->previous=NULL;::DeleteObject(self->source);self->source=NULL;
            self->replacement=Bitmap();ThemeManager::RegisterNativeMenuBitmap(self->command,self->replacement);
            // Destroy/recreate an unshown submenu while the root popup is active.
            ::DeleteMenu(self->menu,1,MF_BYPOSITION);self->child=::CreatePopupMenu();
            ::AppendMenuW(self->child,MF_STRING,self->command,L"New script");
            info.hbmpItem=self->replacement;::SetMenuItemInfoW(self->child,0,TRUE,&info);
            ::AppendMenuW(self->menu,MF_POPUP,reinterpret_cast<UINT_PTR>(self->child),L"Scripts");
            self->replacementAlive=::GetObjectType(displayed)==OBJ_BITMAP;
            self->called=true;::EndMenu();
        }
    };
    // Warm up USER/UxTheme's first native-popup caches before measuring our ownership.
    HMENU warmup=::CreatePopupMenu();::AppendMenuW(warmup,MF_STRING,0xdff1,L"Warm up");
    const UINT_PTR warmTimer=::SetTimer(NULL,0,100,BitmapLifetimeProbe::Tick);
    RECT warmBounds={};GetWindowRect(&warmBounds);ThemeManager::TrackPopupMenu(warmup,TPM_NONOTIFY,warmBounds.left+40,warmBounds.top+40,m_hWnd);
    ::KillTimer(NULL,warmTimer);::DestroyMenu(warmup);
    const DWORD before=::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS);
    BitmapLifetimeProbe lifetime={m_hWnd,::CreatePopupMenu(),::CreatePopupMenu(),0xdff0,BitmapLifetimeProbe::Bitmap(),BitmapLifetimeProbe::Bitmap(),NULL};
    check("bitmap_lifetime_setup",lifetime.menu&&lifetime.child&&lifetime.previous&&lifetime.source);
    ::AppendMenuW(lifetime.menu,MF_STRING,lifetime.command,L"Refresh lifetime probe");
    ::AppendMenuW(lifetime.child,MF_STRING,lifetime.command,L"Old script");
    ::AppendMenuW(lifetime.menu,MF_POPUP,reinterpret_cast<UINT_PTR>(lifetime.child),L"Scripts");
    ::AppendMenuW(lifetime.menu,MF_STRING,0xdff3,L"Dark-only bridge");
    ThemeManager::RegisterNativeMenuBitmap(0xdff3,lifetime.source,true);
    MENUITEMINFO folderItem={};folderItem.cbSize=sizeof(folderItem);folderItem.fMask=MIIM_ID;folderItem.wID=0xdff2;
    ::SetMenuItemInfoW(lifetime.menu,1,TRUE,&folderItem);ThemeManager::RegisterNativeMenuBitmap(folderItem.wID,lifetime.source);
    MENUITEMINFO item={};item.cbSize=sizeof(item);item.fMask=MIIM_BITMAP;item.hbmpItem=lifetime.previous;
    ::SetMenuItemInfoW(lifetime.menu,0,TRUE,&item);ThemeManager::RegisterNativeMenuBitmap(lifetime.command,lifetime.source);
    ::SetPropW(m_hWnd,L"FBE.BitmapLifetime",&lifetime);
    const UINT_PTR timer=::SetTimer(NULL,0,100,BitmapLifetimeProbe::Tick);check("bitmap_lifetime_timer",timer!=0);
    RECT bounds={};GetWindowRect(&bounds);ThemeManager::TrackPopupMenu(lifetime.menu,TPM_NONOTIFY,bounds.left+40,bounds.top+40,m_hWnd);
    ::KillTimer(NULL,timer);::RemovePropW(m_hWnd,L"FBE.BitmapLifetime");
    check("bitmap_survives_source_destruction",lifetime.called&&lifetime.copyAlive&&lifetime.replacementAlive);
    check("folder_popup_bitmap_applied",lifetime.folderCopyAlive);
    check("native_popup_scales_bitmap",lifetime.scaledOnPopup);
    check("native_popup_scaled_alpha",lifetime.scaledAlphaCorrect);
    check("native_bridge_preserves_light_menu",lifetime.darkOnlyCorrect);
    ::GetMenuItemInfoW(lifetime.menu,0,TRUE,&item);check("destroyed_bitmap_not_restored",item.hbmpItem==NULL);
    ::GetMenuItemInfoW(lifetime.menu,1,TRUE,&item);check("shifted_item_bitmap_restored",item.hbmpItem==NULL);
    ::GetMenuItemInfoW(lifetime.child,0,TRUE,&item);check("rebuilt_submenu_bitmap_preserved",item.hbmpItem==lifetime.replacement&&::GetObjectType(item.hbmpItem)==OBJ_BITMAP);
    ThemeManager::UnregisterNativeMenuBitmap(lifetime.command);ThemeManager::UnregisterNativeMenuBitmap(0xdff2);ThemeManager::UnregisterNativeMenuBitmap(0xdff3);::DestroyMenu(lifetime.menu);
    if(lifetime.previous)::DeleteObject(lifetime.previous);if(lifetime.source)::DeleteObject(lifetime.source);if(lifetime.replacement)::DeleteObject(lifetime.replacement);
    const DWORD after=::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS);
    report.AppendFormat("bitmap_gdi_before=%lu\nbitmap_gdi_after=%lu\n",before,after);
    check("bitmap_lifetime_gdi_balanced",before==after);
}
