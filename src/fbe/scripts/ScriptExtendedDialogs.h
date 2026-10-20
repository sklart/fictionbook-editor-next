#pragma once
#include "../ThemeManager.h"
#include "../UiMetrics.h"
#include "../RuntimeLocalization.h"
#include "ScriptJson.h"
#include "../../common/RuntimeLocalizationCommon.h"
#include <set>

namespace FbeScripts { namespace ExtendedDialogs {
inline bool ValidText(const CString& text, int limit)
{
    if (text.GetLength() > limit) return false;
    for (int i = 0; i < text.GetLength(); ++i) {
        const unsigned c = text[i];
        if (!c) return false;
        if (c >= 0xd800 && c <= 0xdbff) { if (++i == text.GetLength() || text[i] < 0xdc00 || text[i] > 0xdfff) return false; }
        else if (c >= 0xdc00 && c <= 0xdfff) return false;
    }
    return true;
}
struct Item { CString id, label; };
struct Options { CString title, prompt; std::vector<Item> items; std::set<std::wstring> selected; };
inline bool Text(const std::wstring& json, size_t& cursor, CString& value, int limit = 4096)
{
    std::wstring parsed;
    if (!FbeRuntimeLocalization::JsonParseString(json, cursor, parsed)) return false;
    value.SetString(parsed.data(), static_cast<int>(parsed.size())); return ValidText(value, limit);
}
inline bool Take(const std::wstring& json, size_t& cursor, wchar_t c)
{
    FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
    if (cursor == json.size() || json[cursor] != c) return false;
    ++cursor; return true;
}
inline bool Parse(BSTR source, bool multiple, Options& options)
{
    using namespace FbeRuntimeLocalization;
    if (::SysStringLen(source) > 1024 * 1024) return false;
    const std::wstring json(source ? source : L"", ::SysStringLen(source)); size_t cursor = 0;
    std::set<std::wstring> keys, ids;
    if (!Take(json, cursor, L'{')) return false;
    for (;;) {
        CString key;
        if (!Text(json, cursor, key, 32) || !keys.insert(std::wstring(key)).second || !Take(json, cursor, L':')) return false;
        if (key == L"title" || key == L"prompt" || (!multiple && key == L"selectedId")) {
            CString value; if (!Text(json, cursor, value, key == L"selectedId" ? 256 : 4096)) return false;
            if (key == L"title") options.title = value; else if (key == L"prompt") options.prompt = value;
            else { if (value.IsEmpty()) return false; options.selected.insert(std::wstring(value)); }
        } else if (key == L"items") {
            if (!Take(json, cursor, L'[')) return false;
            if (!Take(json, cursor, L']')) for (;;) {
                if (options.items.size() >= 2048 || !Take(json, cursor, L'{')) return false;
                Item item; std::set<std::wstring> fields;
                for (;;) {
                    CString field, value;
                    if (!Text(json, cursor, field, 16) || !fields.insert(std::wstring(field)).second || !Take(json, cursor, L':') ||
                        !Text(json, cursor, value, field == L"id" ? 256 : 4096)) return false;
                    if (field == L"id") item.id = value; else if (field == L"label") item.label = value; else return false;
                    if (Take(json, cursor, L'}')) break;
                    if (!Take(json, cursor, L',')) return false;
                }
                if (fields.size() != 2 || item.id.IsEmpty() || !ids.insert(std::wstring(item.id)).second) return false;
                options.items.push_back(std::move(item));
                if (Take(json, cursor, L']')) break;
                if (!Take(json, cursor, L',')) return false;
            }
        } else if (multiple && key == L"selectedIds") {
            if (!Take(json, cursor, L'[')) return false;
            if (!Take(json, cursor, L']')) for (;;) {
                CString id;
                if (!Text(json, cursor, id, 256) || id.IsEmpty() || !options.selected.insert(std::wstring(id)).second) return false;
                if (Take(json, cursor, L']')) break;
                if (!Take(json, cursor, L',')) return false;
            }
        } else return false;
        if (Take(json, cursor, L'}')) break;
        if (!Take(json, cursor, L',')) return false;
    }
    JsonSkipWhitespace(json, cursor);
    if (cursor != json.size() || !keys.count(L"items") || (!multiple && options.items.empty())) return false;
    for (const auto& id : options.selected) if (!ids.count(id)) return false;
    return true;
}
inline thread_local bool modalActive = false;
struct ModalScope {
    HWND focus = ::GetFocus();
    ModalScope() { if (modalActive) CheckError(E_UNEXPECTED); modalActive = true; }
    ~ModalScope() { modalActive = false; if (::IsWindow(focus) && ::IsWindowEnabled(focus)) ::SetFocus(focus); }
};
class ListDialog : public CDialogImpl<ListDialog> {
    const Options& options; bool multiple; CListViewCtrl list; CFont font;
    int Scale(int n) const { return UiMetrics::ScaleForDpi(n, UiMetrics::DpiForWindow(m_hWnd)); }
    void Fonts() {
        CFont replacement; replacement.Attach(UiMetrics::CreateDialogFontForDpi(UiMetrics::DpiForWindow(m_hWnd)));
        if (!replacement) return;
        for (HWND child = ::GetWindow(m_hWnd, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
            ::SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(replacement.m_hFont), TRUE);
        font.Attach(replacement.Detach());
    }
    void Layout() {
        if (!list.IsWindow()) return;
        RECT r = {}; GetClientRect(&r); const int gap = Scale(8), button = Scale(86), height = Scale(26), top = Scale(62);
        ::SetWindowPos(GetDlgItem(IDC_ARCHIVE_ENTRY_MESSAGE), nullptr, gap, gap, max(1, r.right-2*gap), top-2*gap, SWP_NOZORDER);
        list.SetWindowPos(nullptr, gap, top, max(1,r.right-2*gap), max(1,r.bottom-top-height-3*gap), SWP_NOZORDER);
        ::SetWindowPos(GetDlgItem(IDOK), nullptr, r.right-2*button-2*gap, r.bottom-height-gap, button, height, SWP_NOZORDER);
        ::SetWindowPos(GetDlgItem(IDCANCEL), nullptr, r.right-button-gap, r.bottom-height-gap, button, height, SWP_NOZORDER);
        int width = max(1,r.right-3*gap);
        for (const auto& item : options.items) width = max(width,list.GetStringWidth(item.label)+Scale(28));
        list.SetColumnWidth(0,width);
    }
    void Clamp() {
        MONITORINFO monitor = {sizeof(monitor)};
        if (!::GetMonitorInfoW(::MonitorFromWindow(GetParent(),MONITOR_DEFAULTTONEAREST),&monitor)) return;
        RECT r = {}; GetWindowRect(&r);
        const int width = min(r.right-r.left,monitor.rcWork.right-monitor.rcWork.left);
        const int height = min(r.bottom-r.top,monitor.rcWork.bottom-monitor.rcWork.top);
        SetWindowPos(nullptr,max(monitor.rcWork.left,min(r.left,monitor.rcWork.right-width)),
            max(monitor.rcWork.top,min(r.top,monitor.rcWork.bottom-height)),width,height,SWP_NOZORDER|SWP_NOACTIVATE);
    }
public:
    enum { IDD = IDD_ARCHIVE_ENTRY };
    std::vector<CString> selected;
    ListDialog(const Options& value, bool multi) : options(value), multiple(multi) {}
    BEGIN_MSG_MAP(ListDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, Init)
        MESSAGE_HANDLER(WM_SIZE, Size)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, Limits)
        MESSAGE_HANDLER(WM_DPICHANGED, Dpi)
        MESSAGE_HANDLER(WM_FBE_THEMECHANGED, Theme)
        COMMAND_ID_HANDLER(IDOK, Accept)
        COMMAND_ID_HANDLER(IDCANCEL, Cancel)
    END_MSG_MAP()
    LRESULT Init(UINT,WPARAM,LPARAM,BOOL&) {
        SetWindowText(options.title); SetDlgItemText(IDC_ARCHIVE_ENTRY_MESSAGE,options.prompt);
        SetDlgItemText(IDOK,FbeLoadRuntimeStringByKey(L"fbe.common.ok",L"OK"));
        SetDlgItemText(IDCANCEL,FbeLoadRuntimeStringByKey(L"fbe.common.cancel",L"Cancel"));
        list.Attach(GetDlgItem(IDC_ARCHIVE_ENTRY_LIST));
        list.ModifyStyle(LVS_SINGLESEL|LVS_TYPEMASK,(multiple?0:LVS_SINGLESEL)|LVS_REPORT|LVS_NOCOLUMNHEADER);
        list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT|LVS_EX_LABELTIP|LVS_EX_DOUBLEBUFFER);
        list.InsertColumn(0,L""); int first = -1;
        for (size_t i=0;i<options.items.size();++i) {
            list.InsertItem(static_cast<int>(i),options.items[i].label);
            if (options.selected.count(std::wstring(options.items[i].id))) {
                list.SetItemState(static_cast<int>(i),LVIS_SELECTED,LVIS_SELECTED); if(first<0) first=static_cast<int>(i);
            }
        }
        if(first<0&&!multiple&&!options.items.empty()) { first=0; list.SetItemState(0,LVIS_SELECTED,LVIS_SELECTED); }
        Fonts(); CenterWindow(GetParent()); Clamp(); Layout(); ThemeManager::ApplyToWindow(m_hWnd);
        if(first>=0) { list.SetItemState(first,LVIS_FOCUSED,LVIS_FOCUSED); list.EnsureVisible(first,FALSE); }
        list.SetFocus(); return FALSE;
    }
    LRESULT Size(UINT,WPARAM,LPARAM,BOOL&) { Layout(); return 0; }
    LRESULT Dpi(UINT,WPARAM,LPARAM value,BOOL&) {
        const RECT& r=*reinterpret_cast<RECT*>(value); SetWindowPos(nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER);
        font.DeleteObject(); Fonts(); Clamp(); Layout(); return 0;
    }
    LRESULT Theme(UINT,WPARAM,LPARAM,BOOL&) { ThemeManager::ApplyToWindow(m_hWnd); return 0; }
    LRESULT Limits(UINT,WPARAM,LPARAM value,BOOL&) {
        auto* info=reinterpret_cast<MINMAXINFO*>(value); MONITORINFO monitor={sizeof(monitor)};
        if(::GetMonitorInfoW(::MonitorFromWindow(m_hWnd,MONITOR_DEFAULTTONEAREST),&monitor)) {
            info->ptMinTrackSize.x=min(Scale(320),monitor.rcWork.right-monitor.rcWork.left);
            info->ptMinTrackSize.y=min(Scale(220),monitor.rcWork.bottom-monitor.rcWork.top);
            info->ptMaxTrackSize.x=monitor.rcWork.right-monitor.rcWork.left;
            info->ptMaxTrackSize.y=monitor.rcWork.bottom-monitor.rcWork.top;
        } return 0;
    }
    LRESULT Accept(WORD,WORD,HWND,BOOL&) {
        selected.clear();
        for(int index=-1;(index=list.GetNextItem(index,LVNI_SELECTED))>=0;) selected.push_back(options.items[static_cast<size_t>(index)].id);
        if(!multiple&&selected.size()!=1) return 0;
        EndDialog(IDOK); return 0;
    }
    LRESULT Cancel(WORD,WORD,HWND,BOOL&) { EndDialog(IDCANCEL); return 0; }
};
inline CString Show(HWND owner, DISPID method, const std::vector<CComVariant>& args)
{
    auto error=[](LPCWSTR code,HRESULT hr){ return Dialogs::Error(code,hr,false); };
    for(const auto& value:args) if(!ValidText(CString(value.bstrVal,::SysStringLen(value.bstrVal)),method==154||method==155?1024*1024:32767))
        return error(L"invalid-options",E_INVALIDARG);
    if(!::IsWindow(owner)) return error(L"owner-unavailable",E_UNEXPECTED);
    if(method==154||method==155) {
        Options options; if(!Parse(args[0].bstrVal,method==155,options)) return error(L"invalid-options",E_INVALIDARG);
        ModalScope scope; ListDialog dialog(options,method==155); const INT_PTR result=dialog.DoModal(owner);
        if(result==-1) return error(L"dialog-failed",E_FAIL);
        if(result!=IDOK) return L"{\"status\":\"cancelled\"}";
        if(method==154) return L"{\"status\":\"ok\",\"selectedId\":\""+JsonEscape(dialog.selected.front())+L"\"}";
        CString json=L"{\"status\":\"ok\",\"selectedIds\":[";
        for(size_t i=0;i<dialog.selected.size();++i) { if(i)json+=L","; json+=L"\""+JsonEscape(dialog.selected[i])+L"\""; }
        return json+L"]}";
    }
    if(method==156) {
        CString color(args[0].bstrVal); if(color.GetLength()!=7||color[0]!=L'#') return error(L"invalid-options",E_INVALIDARG);
        unsigned rgb=0; for(int i=1;i<7;++i) { wchar_t c=color[i]; unsigned n=c>=L'0'&&c<=L'9'?c-L'0':c>=L'a'&&c<=L'f'?c-L'a'+10:c>=L'A'&&c<=L'F'?c-L'A'+10:16U; if(n>15) return error(L"invalid-options",E_INVALIDARG); rgb=rgb*16+n; }
        COLORREF custom[16]={}; CHOOSECOLORW choice={sizeof(choice)}; choice.hwndOwner=owner;
        choice.rgbResult=RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255); choice.lpCustColors=custom; choice.Flags=CC_RGBINIT|CC_FULLOPEN;
        ModalScope scope;
        if(!::ChooseColorW(&choice)) { const DWORD failure=::CommDlgExtendedError(); return failure?error(L"dialog-failed",HRESULT_FROM_WIN32(failure)):CString(L"{\"status\":\"cancelled\"}"); }
        CString result; result.Format(L"{\"status\":\"ok\",\"color\":\"#%02X%02X%02X\"}",GetRValue(choice.rgbResult),GetGValue(choice.rgbResult),GetBValue(choice.rgbResult)); return result;
    }
    const CString buttons(args[2].bstrVal),icon(args[3].bstrVal); UINT flags=0;
    if(buttons==L"ok") flags=MB_OK; else if(buttons==L"okcancel") flags=MB_OKCANCEL;
    else if(buttons==L"yesno")flags=MB_YESNO;else if(buttons==L"yesnocancel")flags=MB_YESNOCANCEL;else return error(L"invalid-options",E_INVALIDARG);
    if(icon==L"info")flags|=MB_ICONINFORMATION;else if(icon==L"warning")flags|=MB_ICONWARNING;
    else if(icon==L"error")flags|=MB_ICONERROR;else if(icon==L"question")flags|=MB_ICONQUESTION;else if(icon!=L"none")return error(L"invalid-options",E_INVALIDARG);
    ModalScope scope; const int button=ThemeManager::MessageBox(owner,args[0].bstrVal,args[1].bstrVal,flags);
    if(!button) return error(L"dialog-failed",E_FAIL);
    if(button==IDCANCEL)return L"{\"status\":\"cancelled\",\"button\":\"cancel\"}";
    return CString(L"{\"status\":\"ok\",\"button\":\"")+(button==IDYES?L"yes":button==IDNO?L"no":L"ok")+L"\"}";
}
} }
