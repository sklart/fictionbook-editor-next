// Regression for caller-owned bitmaps destroyed/replaced during a native popup.
{
    struct BitmapLifetimeProbe {
        HWND frame;HMENU menu,child;UINT command;HBITMAP previous,source,replacement;
        bool called=false,copyAlive=false,replacementAlive=false;
        static HBITMAP Bitmap() {
            BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=4;info.bmiHeader.biHeight=-4;
            info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* pixels=NULL;
            HBITMAP bitmap=::CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);
            if(bitmap&&pixels)for(int i=0;i<16;++i)static_cast<DWORD*>(pixels)[i]=0xff48a080;
            return bitmap;
        }
        static void CALLBACK Tick(HWND,UINT,UINT_PTR timer,DWORD) {
            ::KillTimer(NULL,timer);
            auto* self=reinterpret_cast<BitmapLifetimeProbe*>(::GetPropW(PluginOptionsProbe::dialog,L"FBE.BitmapLifetime"));
            if(!self){::EndMenu();return;}
            MENUITEMINFO info={};info.cbSize=sizeof(info);info.fMask=MIIM_BITMAP;
            ::GetMenuItemInfoW(self->menu,0,TRUE,&info);const HBITMAP displayed=info.hbmpItem;
            self->copyAlive=displayed!=self->source&&::GetObjectType(displayed)==OBJ_BITMAP;
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
    MENUITEMINFO item={};item.cbSize=sizeof(item);item.fMask=MIIM_BITMAP;item.hbmpItem=lifetime.previous;
    ::SetMenuItemInfoW(lifetime.menu,0,TRUE,&item);ThemeManager::RegisterNativeMenuBitmap(lifetime.command,lifetime.source);
    ::SetPropW(m_hWnd,L"FBE.BitmapLifetime",&lifetime);
    const UINT_PTR timer=::SetTimer(NULL,0,100,BitmapLifetimeProbe::Tick);check("bitmap_lifetime_timer",timer!=0);
    RECT bounds={};GetWindowRect(&bounds);ThemeManager::TrackPopupMenu(lifetime.menu,TPM_NONOTIFY,bounds.left+40,bounds.top+40,m_hWnd);
    ::KillTimer(NULL,timer);::RemovePropW(m_hWnd,L"FBE.BitmapLifetime");
    check("bitmap_survives_source_destruction",lifetime.called&&lifetime.copyAlive&&lifetime.replacementAlive);
    ::GetMenuItemInfoW(lifetime.menu,0,TRUE,&item);check("destroyed_bitmap_not_restored",item.hbmpItem==NULL);
    ::GetMenuItemInfoW(lifetime.child,0,TRUE,&item);check("rebuilt_submenu_bitmap_preserved",item.hbmpItem==lifetime.replacement&&::GetObjectType(item.hbmpItem)==OBJ_BITMAP);
    ThemeManager::UnregisterNativeMenuBitmap(lifetime.command);::DestroyMenu(lifetime.menu);
    if(lifetime.previous)::DeleteObject(lifetime.previous);if(lifetime.source)::DeleteObject(lifetime.source);if(lifetime.replacement)::DeleteObject(lifetime.replacement);
    const DWORD after=::GetGuiResources(::GetCurrentProcess(),GR_GDIOBJECTS);
    report.AppendFormat("bitmap_gdi_before=%lu\nbitmap_gdi_after=%lu\n",before,after);
    check("bitmap_lifetime_gdi_balanced",before==after);
}
