#pragma once
#include <gdiplus.h>
#include <algorithm>
#include <map>
#include <vector>
#include <string>

namespace TreeMarkerProbe {
// Settings.xml redirection alone does not isolate U::SaveDocPosition(), which
// opens HKCU by path. Keep this runtime scenario's save/reopen inside its hive.
struct RegistryIsolation {
    CRegKey key;
    std::wstring path;
    bool overridden=false,ready=true;
    RegistryIsolation() {
        if(!DeploymentContext::RegistryPersistenceAllowed())return;
        const wchar_t* appearancePath=L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
        CRegKey appearance;DWORD appsLight=1,systemLight=1;
        if(appearance.Open(HKEY_CURRENT_USER,appearancePath,KEY_READ)==ERROR_SUCCESS) {
            appearance.QueryDWORDValue(L"AppsUseLightTheme",appsLight);
            appearance.QueryDWORDValue(L"SystemUsesLightTheme",systemLight);
        }
        GUID id={};wchar_t text[40]={};
        ready=SUCCEEDED(::CoCreateGuid(&id))&&::StringFromGUID2(id,text,_countof(text))!=0;
        if(!ready)return;
        path=L"Software\\FBETreeMarkerTest-"+std::wstring(text);
        ready=key.Create(HKEY_CURRENT_USER,path.c_str())==ERROR_SUCCESS;
        CRegKey isolatedAppearance;
        ready=ready&&isolatedAppearance.Create(key,appearancePath)==ERROR_SUCCESS;
        ready=ready&&isolatedAppearance.SetDWORDValue(L"AppsUseLightTheme",appsLight)==ERROR_SUCCESS;
        ready=ready&&isolatedAppearance.SetDWORDValue(L"SystemUsesLightTheme",systemLight)==ERROR_SUCCESS;
        if(ready)overridden=::RegOverridePredefKey(HKEY_CURRENT_USER,key)==ERROR_SUCCESS;
        ready=overridden;
    }
    ~RegistryIsolation() {
        if(overridden)::RegOverridePredefKey(HKEY_CURRENT_USER,NULL);
        key.Close();
        if(!path.empty())::RegDeleteTreeW(HKEY_CURRENT_USER,path.c_str());
    }
};
// Only this process's fixture window is presented. Do not activate it or
// alter another window: the user can retain keyboard focus in their app.
struct WindowPresentation {
    HWND window=NULL;bool changed=false,ready=true;
    WindowPresentation(HWND value,bool background):window(value) {
        if(background)return;
        const bool wasTopmost=(::GetWindowLongPtr(window,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
        RECT bounds={};MONITORINFO monitor={sizeof(MONITORINFO)};
        ready=::GetWindowRect(window,&bounds)&&::GetMonitorInfo(::MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
        if(!ready)return;
        const int width=(std::min)(bounds.right-bounds.left,monitor.rcWork.right-monitor.rcWork.left);
        const int height=(std::min)(bounds.bottom-bounds.top,monitor.rcWork.bottom-monitor.rcWork.top);
        const int left=(std::max)(monitor.rcWork.left,(std::min)(bounds.left,monitor.rcWork.right-width));
        const int top=(std::max)(monitor.rcWork.top,(std::min)(bounds.top,monitor.rcWork.bottom-height));
        ready=::SetWindowPos(window,HWND_TOPMOST,left,top,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE;
        changed=ready&&!wasTopmost;
    }
    ~WindowPresentation() {
        if(changed&&::IsWindow(window))::SetWindowPos(window,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    }
};
// BitBlt ignores the source clip region. Require the entire client to be
// visible before using a screen snapshot in incremental/full comparisons.
inline bool EntireClientVisible(HDC dc,RECT client) {
    HRGN actual=::CreateRectRgn(0,0,0,0),expected=::CreateRectRgn(client.left,client.top,client.right,client.bottom);
    POINT origin={};bool visible=false;
    if(actual&&expected&&::GetDCOrgEx(dc,&origin)&&::GetRandomRgn(dc,actual,SYSRGN)==1) {
        ::OffsetRgn(actual,-origin.x,-origin.y);
        visible=::EqualRgn(actual,expected)!=FALSE;
    }
    if(actual)::DeleteObject(actual);if(expected)::DeleteObject(expected);
    return visible;
}
struct Frame {
    int width=0,height=0;std::vector<DWORD> pixels;
    bool ReadDc(HDC source,int w,int h) {
        width=w;height=h;
        if(width<=0||height<=0)return false;
        HDC memory=::CreateCompatibleDC(source);
        BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        void* data=NULL;HBITMAP image=::CreateDIBSection(source,&info,DIB_RGB_COLORS,&data,NULL,0);
        if(!source||!memory||!image||!data){if(image)::DeleteObject(image);if(memory)::DeleteDC(memory);return false;}
        HGDIOBJ old=::SelectObject(memory,image);
        const bool copied=::BitBlt(memory,0,0,width,height,source,0,0,SRCCOPY)!=FALSE;::GdiFlush();
        if(copied)pixels.assign(static_cast<DWORD*>(data),static_cast<DWORD*>(data)+static_cast<size_t>(width)*height);
        ::SelectObject(memory,old);::DeleteObject(image);::DeleteDC(memory);return copied;
    }
    bool Read(HWND window) {RECT r={};::GetClientRect(window,&r);HDC source=::GetDC(window);const bool result=ReadDc(source,r.right,r.bottom);::ReleaseDC(window,source);return result;}
    COLORREF At(int x,int y)const {if(x<0||y<0||x>=width||y>=height||pixels.empty())return CLR_INVALID;const DWORD p=pixels[static_cast<size_t>(y)*width+x];return RGB((p>>16)&255,(p>>8)&255,p&255);}
    Frame Crop(RECT rect,int scale=1)const {
        if(pixels.empty())return Frame();
        rect.left=(std::max)(0L,rect.left);rect.top=(std::max)(0L,rect.top);rect.right=(std::min)(LONG(width),rect.right);rect.bottom=(std::min)(LONG(height),rect.bottom);
        Frame result;result.width=(std::max)(0L,rect.right-rect.left)*scale;result.height=(std::max)(0L,rect.bottom-rect.top)*scale;
        result.pixels.resize(static_cast<size_t>(result.width)*result.height);
        for(int y=0;y<result.height;++y)for(int x=0;x<result.width;++x)result.pixels[static_cast<size_t>(y)*result.width+x]=pixels[static_cast<size_t>(rect.top+y/scale)*width+rect.left+x/scale];
        return result;
    }
    bool Save(const std::wstring& path)const {
        if(pixels.empty())return false;
        Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;if(Gdiplus::GdiplusStartup(&token,&input,NULL)!=Gdiplus::Ok)return false;
        bool saved=false;{Gdiplus::Bitmap bitmap(width,height,width*4,PixelFormat32bppRGB,reinterpret_cast<BYTE*>(const_cast<DWORD*>(pixels.data())));const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};saved=bitmap.Save(path.c_str(),&png,NULL)==Gdiplus::Ok;}
        Gdiplus::GdiplusShutdown(token);return saved;
    }
};
// Observe the real control paint, not WM_PRINT/PrintWindow. A window DC is
// clipped by other desktop windows and GetPixel then returns CLR_INVALID.
// This cache is diagnostic only: Windows can defer an occluded control's
// paint, leaving a preceding frame here. Never use it as the pixel oracle or
// as evidence that the latest document selection has actually been painted.
struct PaintObserver {
    struct Event {UINT stage=0;HTREEITEM item=NULL;RECT marker={},clip={},update={};COLORREF first=CLR_INVALID,middle=CLR_INVALID,last=CLR_INVALID;int region=0,rank=0;unsigned bad=0;DWORD dcType=0;POINT viewport={},origin={};};
    enum {Capacity=32};CTreeView& tree;bool active=false,trace=false;std::vector<Event> events;unsigned count=0;
    Frame nativeFrame;std::vector<BYTE> valid;HRGN updateRegion=NULL;unsigned completed=0; DWORD dcType=0; bool captureFailed=false; unsigned painting=0;
    void Capture(HDC dc) {
        RECT dirty={};
        const int regionKind=::GetRgnBox(updateRegion,&dirty);
        if(regionKind==NULLREGION)return;
        if(regionKind==ERROR){captureFailed=true;return;}
        dcType=::GetObjectType(dc);
        if(dcType!=OBJ_MEMDC){captureFailed=true;return;}
        RECT client={};tree.GetClientRect(&client);Frame image;if(!image.ReadDc(dc,client.right,client.bottom)){captureFailed=true;return;}
        if(nativeFrame.width!=image.width||nativeFrame.height!=image.height){nativeFrame=image;std::fill(nativeFrame.pixels.begin(),nativeFrame.pixels.end(),0);valid.assign(image.pixels.size(),0);}
        const DWORD bytes=::GetRegionData(updateRegion,0,NULL);std::vector<BYTE> data(bytes);
        const bool readRegion=bytes&&::GetRegionData(updateRegion,bytes,reinterpret_cast<RGNDATA*>(data.data()))!=0;
        if(readRegion){
            const auto* region=reinterpret_cast<const RGNDATA*>(data.data());const auto* rectangles=reinterpret_cast<const RECT*>(region->Buffer);
            for(DWORD r=0;r<region->rdh.nCount;++r){RECT box=rectangles[r];box.left=(std::max)(0L,box.left);box.top=(std::max)(0L,box.top);box.right=(std::min)(LONG(image.width),box.right);box.bottom=(std::min)(LONG(image.height),box.bottom);
                for(int y=box.top;y<box.bottom;++y)if(box.right>box.left){const size_t index=static_cast<size_t>(y)*image.width+box.left;const size_t n=static_cast<size_t>(box.right-box.left);std::copy_n(image.pixels.begin()+index,n,nativeFrame.pixels.begin()+index);std::fill_n(valid.begin()+index,n,BYTE(1));}
            }
        }
        if(!readRegion)captureFailed=true;
        Record(CDDS_POSTPAINT,tree.CurrentStructureItem(),dc,dirty);
        ++completed;
    }
    explicit PaintObserver(CTreeView& value):tree(value) {
        wchar_t flag[2]={};
        trace=::GetEnvironmentVariableW(L"FBE_NEXT_TEST_TREE_TRACE_PAINT",flag,_countof(flag))&&flag[0]==L'1';
        if(!trace)return;
        events.resize(Capacity);
        updateRegion=::CreateRectRgn(0,0,0,0);
        active=updateRegion&&::SetWindowSubclass(tree,Proc,0x6403,reinterpret_cast<DWORD_PTR>(this))!=FALSE;
    }
    ~PaintObserver() {
        if(active)::RemoveWindowSubclass(tree,Proc,0x6403);
        if(updateRegion)::DeleteObject(updateRegion);
    }
    void Record(UINT stage,HTREEITEM item,HDC dc,RECT update) {
        RECT marker={};if(!tree.StructureMarkerRect(item,marker))return;
        Event e;e.stage=stage;e.item=item;e.marker=marker;e.update=update;e.region=::GetClipBox(dc,&e.clip);e.rank=tree.StructureHighlight(item);e.dcType=::GetObjectType(dc);::GetViewportOrgEx(dc,&e.viewport);::GetWindowOrgEx(dc,&e.origin);
        const bool selected=(tree.GetItemState(item,TVIS_SELECTED)&TVIS_SELECTED)!=0;
        const COLORREF expected=ThemeManager::IsHighContrast()?::GetSysColor(selected?COLOR_HIGHLIGHTTEXT:COLOR_WINDOWTEXT):(selected?ThemeManager::SelectionTextColor():(e.rank==2?ThemeManager::AccentColor():ThemeManager::SecondaryTextColor()));
        for(int row=marker.top;row<marker.bottom;++row)for(int x=marker.left;x<marker.right;++x)if(::GetPixel(dc,x,row)!=expected)++e.bad;
        const int y=marker.bottom-1;e.first=::GetPixel(dc,marker.left,y);e.middle=::GetPixel(dc,(marker.left+marker.right)/2,y);e.last=::GetPixel(dc,marker.right-1,y);
        events[count++%Capacity]=e;
    }
    static LRESULT CALLBACK Proc(HWND window,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data) {
        auto& observer=*reinterpret_cast<PaintObserver*>(data);RECT update={};if(message==WM_PAINT){::GetUpdateRect(window,&update,FALSE);::GetUpdateRgn(window,observer.updateRegion,FALSE);++observer.painting;}
        NMTVCUSTOMDRAW* draw=NULL;
        if(message==OCM_NOTIFY&&l&&reinterpret_cast<NMHDR*>(l)->code==NM_CUSTOMDRAW)draw=reinterpret_cast<NMTVCUSTOMDRAW*>(l);
        LRESULT result=::DefSubclassProc(window,message,w,l);
        if(draw&&draw->nmcd.dwDrawStage==CDDS_PREPAINT)result|=CDRF_NOTIFYPOSTPAINT;
        if(observer.painting&&draw&&draw->nmcd.dwDrawStage==CDDS_POSTPAINT)observer.Capture(draw->nmcd.hdc);
        if(draw&&draw->nmcd.dwDrawStage==CDDS_ITEMPOSTPAINT)observer.Record(CDDS_ITEMPOSTPAINT,reinterpret_cast<HTREEITEM>(draw->nmcd.dwItemSpec),draw->nmcd.hdc,update);
        if(message==WM_PAINT&&observer.trace){HDC dc=::GetDC(window);observer.Record(WM_PAINT,observer.tree.CurrentStructureItem(),dc,update);::ReleaseDC(window,dc);}
        if(message==WM_PAINT)--observer.painting;
        return result;
    }
    COLORREF At(int x,int y)const {
        if(captureFailed||x<0||y<0||x>=nativeFrame.width||y>=nativeFrame.height)return CLR_INVALID;
        const size_t i=static_cast<size_t>(y)*nativeFrame.width+x;
        return i<valid.size()&&valid[i]?nativeFrame.At(x,y):CLR_INVALID;
    }
    bool Complete()const {return !captureFailed&&!valid.empty()&&std::find(valid.begin(),valid.end(),BYTE(0))==valid.end();}
    void Compare(CStringA& report,RECT r,COLORREF expected)const {
        unsigned bad=0,missing=0;for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x){const COLORREF actual=At(x,y);if(actual!=expected)++bad;if(actual==CLR_INVALID)++missing;}
        report.AppendFormat("marker_native_bad=%u;missing=%u;expected=0x%08lX\n",bad,missing,expected);
        const auto root=RootForObserver();if(!root.empty())nativeFrame.Crop(r,8).Save(root+L"\\native-marker-x8.png");
    }
    static std::wstring RootForObserver(){std::vector<wchar_t> b(32768);const DWORD n=::GetEnvironmentVariableW(L"FBE_NEXT_TEST_ARTIFACT_DIR",b.data(),static_cast<DWORD>(b.size()));return n&&n<b.size()?std::wstring(b.data(),n):std::wstring();}
    void Dump(CStringA& report)const {
        report.AppendFormat("marker_trace_active=%d;events=%u;completed=%u;valid=%u/%u;dc_type=%lu;capture_failed=%d\n",active,count,completed,static_cast<unsigned>(std::count(valid.begin(),valid.end(),BYTE(1))),static_cast<unsigned>(valid.size()),dcType,captureFailed);
        const auto root=RootForObserver();if(!root.empty()&&!nativeFrame.pixels.empty())nativeFrame.Save(root+L"\\native-completed-paint.png");
        const unsigned begin=count>Capacity?count-Capacity:0;
        for(unsigned i=begin;i<count;++i){const Event& e=events[i%Capacity];report.AppendFormat("marker_paint_event=%u;stage=0x%X;item=%p;rect=%ld,%ld,%ld,%ld;clip=%ld,%ld,%ld,%ld;region=%d;update=%ld,%ld,%ld,%ld;colors=0x%08lX,0x%08lX,0x%08lX;rank=%d;bad=%u;dc_type=%lu;viewport=%ld,%ld;origin=%ld,%ld\n",i,e.stage,e.item,e.marker.left,e.marker.top,e.marker.right,e.marker.bottom,e.clip.left,e.clip.top,e.clip.right,e.clip.bottom,e.region,e.update.left,e.update.top,e.update.right,e.update.bottom,e.first,e.middle,e.last,e.rank,e.bad,e.dcType,e.viewport.x,e.viewport.y,e.origin.x,e.origin.y);}
    }
};
inline std::wstring Root() {std::vector<wchar_t> text(32768);const DWORD n=::GetEnvironmentVariableW(L"FBE_NEXT_TEST_ARTIFACT_DIR",text.data(),static_cast<DWORD>(text.size()));return n&&n<text.size()?std::wstring(text.data(),n):std::wstring();}
inline void Rect(CStringA& report,const char* name,const RECT& r){report.AppendFormat("%s=%ld,%ld,%ld,%ld\n",name,r.left,r.top,r.right,r.bottom);}
inline void Diagnose(CStringA& report,HWND tree,HTREEITEM item,const CStringA& phase,const CString& id,const CString& text,int rank,RECT marker,RECT label,RECT client,RECT expectedRect,COLORREF expected,COLORREF first,const std::vector<COLORREF>& samples,const Frame& early,unsigned serial) {
    Frame before=early;if(before.pixels.empty())before.Read(tree);Frame after=before;
    report.AppendFormat("marker_snapshot_before_scan=%d\n",!early.pixels.empty());
    const HWND focus=::GetFocus();DWORD foregroundPid=0;::GetWindowThreadProcessId(::GetForegroundWindow(),&foregroundPid);
    CRegKey appearance;DWORD light=1;const LONG appearanceResult=appearance.Open(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",KEY_READ);if(appearanceResult==ERROR_SUCCESS)appearance.QueryDWORDValue(L"AppsUseLightTheme",light);
    report.AppendFormat("marker_diagnostic=begin\nmarker_check=%u\nmarker_phase=%s\nmarker_item=%p\nmarker_id=%s\nmarker_text=%s\nmarker_rank=%d\nmarker_selected=%d\nmarker_focus=%p\nmarker_tree_focus=%d\nmarker_foreground_pid=%lu\nmarker_theme=%d\nmarker_effective_dark=%d\nmarker_system_apps_light=%lu\nmarker_system_theme_query=%ld\nmarker_high_contrast=%d\nmarker_dpi=%u\nmarker_expected=0x%08lX\nmarker_first=0x%08lX\n",serial,phase.GetString(),item,static_cast<LPCSTR>(CW2A(id,CP_UTF8)),static_cast<LPCSTR>(CW2A(text.Left(2048),CP_UTF8)),rank,(::SendMessage(tree,TVM_GETITEMSTATE,reinterpret_cast<WPARAM>(item),TVIS_SELECTED)&TVIS_SELECTED)!=0,focus,focus==tree,foregroundPid,static_cast<int>(ThemeManager::GetSelectedTheme()),ThemeManager::IsDark(),light,appearanceResult,ThemeManager::IsHighContrast(),UiMetrics::DpiForWindow(tree),expected,first);
    Rect(report,"marker_rect",marker);Rect(report,"marker_label",label);Rect(report,"marker_client",client);Rect(report,"marker_expected_rect",expectedRect);
    size_t mismatches=0,invalid=0,beforeBad=0,afterBad=0;POINT firstBad={-1,-1},lastBad={-1,-1};std::map<COLORREF,size_t> colors;
    for(int y=marker.top;y<marker.bottom;++y){size_t row=0;for(int x=marker.left;x<marker.right;++x){const size_t index=static_cast<size_t>(y-marker.top)*(marker.right-marker.left)+x-marker.left;const COLORREF value=samples[index];++colors[value];if(before.At(x,y)!=expected)++beforeBad;if(after.At(x,y)!=expected)++afterBad;if(value!=expected){if(!mismatches)firstBad={x,y};lastBad={x,y};if(mismatches<30)report.AppendFormat("marker_bad_pixel=%d,%d;actual=0x%08lX;expected=0x%08lX\n",x,y,value,expected);++mismatches;++row;if(value==CLR_INVALID)++invalid;}}report.AppendFormat("marker_bad_row=%d;count=%u\n",y,static_cast<unsigned>(row));}
    report.AppendFormat("marker_bad_count=%u\nmarker_invalid_count=%u\nmarker_snapshot_before_bad=%u\nmarker_snapshot_after_bad=%u\nmarker_bad_first=%ld,%ld\nmarker_bad_last=%ld,%ld\n",static_cast<unsigned>(mismatches),static_cast<unsigned>(invalid),static_cast<unsigned>(beforeBad),static_cast<unsigned>(afterBad),firstBad.x,firstBad.y,lastBad.x,lastBad.y);
    unsigned count=0;for(const auto& color:colors){if(count++==16){report+="marker_histogram_truncated=1\n";break;}report.AppendFormat("marker_color=0x%08lX;count=%u\n",color.first,static_cast<unsigned>(color.second));}
    POINT screen=firstBad;::ClientToScreen(tree,&screen);HWND covering=::WindowFromPoint(screen);wchar_t kind[128]={};::GetClassNameW(covering,kind,_countof(kind));report.AppendFormat("marker_window_at_bad=%p;class=%s\n",covering,static_cast<LPCSTR>(CW2A(kind,CP_UTF8)));
    const auto root=Root();if(!root.empty()){
        std::wstring safePhase(static_cast<LPCWSTR>(CA2W(phase,CP_UTF8)));for(auto& c:safePhase)if(c==L'/'||c==L'\\'||c==L':')c=L'_';
        const auto prefix=root+L"\\marker-theme"+std::to_wstring(ThemeManager::GetSelectedTheme())+L"-dpi"+std::to_wstring(UiMetrics::DpiForWindow(tree))+L"-"+safePhase+L"-check"+std::to_wstring(serial);
        RECT row={0,label.top,client.right,label.bottom};RECT enlarged=marker;::InflateRect(&enlarged,2,2);
        before.Save(prefix+L"-tree-before.png");after.Save(prefix+L"-tree-after.png");before.Crop(row).Save(prefix+L"-row.png");before.Crop(enlarged,8).Save(prefix+L"-marker-x8.png");
        Frame map;map.width=marker.right-marker.left;map.height=marker.bottom-marker.top;map.pixels.resize(samples.size());for(size_t i=0;i<samples.size();++i)map.pixels[i]=samples[i]==expected?0x000000: samples[i]==CLR_INVALID?0xFF00FF:0xFF0000;map.Save(prefix+L"-mismatch-map.png");
        report.AppendFormat("marker_artifact_prefix=%s\n",static_cast<LPCSTR>(CW2A(prefix.c_str(),CP_UTF8)));
    }
    report+="marker_diagnostic=end\n";
}
}
