#include "stdafx.h"
#include "ThemeManager.h"
#include "RuntimeLocalization.h"
#include "resource.h"
#include <map>
#include <vector>

namespace
{
InterfaceTheme g_selected = INTERFACE_THEME_AUTOMATIC;
bool g_systemDark = false;
bool g_highContrast = false;
HHOOK g_themeCbtHook = NULL;
HBRUSH g_windowBrush = NULL;
HBRUSH g_controlBrush = NULL;
HBRUSH g_brushes[THEME_COLOR_COUNT] = {};
std::map<UINT, HBITMAP> g_nativeMenuBitmaps;

void ApplyNativeMenuBitmaps(HMENU menu)
{
	if(menu == NULL) return;
	for(int index = 0; index < ::GetMenuItemCount(menu); ++index)
	{
		const UINT command = ::GetMenuItemID(menu, index);
		const std::map<UINT, HBITMAP>::const_iterator bitmap = g_nativeMenuBitmaps.find(command);
		if(bitmap != g_nativeMenuBitmaps.end())
		{
			MENUITEMINFO info = {}; info.cbSize = sizeof(info); info.fMask = MIIM_BITMAP; info.hbmpItem = bitmap->second;
			::SetMenuItemInfo(menu, index, TRUE, &info);
		}
		ApplyNativeMenuBitmaps(::GetSubMenu(menu, index));
	}
}

bool IsHighContrastEnabled()
{
	HIGHCONTRAST highContrast = {}; highContrast.cbSize = sizeof(highContrast);
	return ::SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0) &&
		(highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

bool ReadAppsUseLightTheme()
{
	DWORD value = 1, valueSize = sizeof(value);
	const LONG result = ::RegGetValueW(HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &value, &valueSize);
	return result == ERROR_SUCCESS && value == 0;
}

void RebuildBrushes()
{
	if(g_windowBrush) ::DeleteObject(g_windowBrush);
	if(g_controlBrush) ::DeleteObject(g_controlBrush);
	for(int i = 0; i < THEME_COLOR_COUNT; ++i) if(g_brushes[i]) { ::DeleteObject(g_brushes[i]); g_brushes[i] = NULL; }
	g_windowBrush = ::CreateSolidBrush(ThemeManager::WindowColor());
	g_controlBrush = ::CreateSolidBrush(ThemeManager::ControlColor());
	for(int i = 0; i < THEME_COLOR_COUNT; ++i)
		g_brushes[i] = ::CreateSolidBrush(ThemeManager::Color(static_cast<ThemeColorRole>(i)));
}

typedef HRESULT (WINAPI* DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
typedef HRESULT (WINAPI* SetPreferredAppModeFn)(int);
typedef void (WINAPI* FlushMenuThemesFn)();

const UINT_PTR kThemeControlSubclassId = 0x46424554; // "FBET"
const wchar_t kHeaderThemeStateProperty[] = L"FBE.HeaderThemeState";

struct HeaderThemeState
{
	int hotItem = -1;
	int pressedItem = -1;
	bool trackingMouse = false;
};

bool IsClass(HWND window, LPCWSTR className)
{
	wchar_t actual[64] = {};
	return ::GetClassNameW(window, actual, _countof(actual)) && ::lstrcmpiW(actual, className) == 0;
}

bool IsGroupBox(HWND window)
{
	return IsClass(window, L"Button") &&
		((::GetWindowLongPtrW(window, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX);
}

bool IsRadioButton(HWND window)
{
	if(!IsClass(window, L"Button")) return false;
	const LONG_PTR type = ::GetWindowLongPtrW(window, GWL_STYLE) & BS_TYPEMASK;
	return type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
}

bool HasClientEdge(HWND window)
{
	return (::GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_CLIENTEDGE) != 0;
}

void PaintDarkClientEdge(HWND window)
{
	HDC dc = ::GetWindowDC(window);
	if(!dc) return;
	RECT rect = {}; ::GetWindowRect(window, &rect);
	::OffsetRect(&rect, -rect.left, -rect.top);
	const int edge = (std::max)(1, ::GetSystemMetrics(SM_CXEDGE));
	for(int index = 0; index < edge; ++index)
	{
		::FrameRect(dc, &rect, ThemeManager::Brush(THEME_COLOR_BORDER));
		::InflateRect(&rect, -1, -1);
	}
	::ReleaseDC(window, dc);
}

bool UsesClassicSurfacePalette(HWND window)
{
	// Explorer visual styles ignore the colours set through the common-control
	// messages for rebar/toolbar/status surfaces. Disable them only for these
	// surfaces in dark mode; ordinary controls keep native modern rendering.
	return IsClass(window, TOOLBARCLASSNAMEW) || IsClass(window, REBARCLASSNAMEW) ||
		IsClass(window, STATUSCLASSNAMEW);
}

bool IsComboBox(HWND window)
{
	return IsClass(window, WC_COMBOBOXW) || IsClass(window, WC_COMBOBOXEXW);
}

bool IsComboDropList(HWND window)
{
	// The list opened by a ComboBox is a top-level popup with this private
	// common-control class. It is not included in EnumChildWindows, so it must
	// be themed when CBN_DROPDOWN tells us that it has been created.
	return IsClass(window, L"ComboLBox");
}

bool IsHeader(HWND window)
{
	return IsClass(window, WC_HEADERW);
}

void ApplyComboDropListTheme(HWND combo)
{
	COMBOBOXINFO info = {}; info.cbSize = sizeof(info);
	if(::GetComboBoxInfo(combo, &info) && ::IsWindow(info.hwndList))
		ThemeManager::ApplyToWindow(info.hwndList);
}

HeaderThemeState* HeaderState(HWND window)
{
	HeaderThemeState* state = reinterpret_cast<HeaderThemeState*>(::GetPropW(window, kHeaderThemeStateProperty));
	if(!state)
	{
		state = new HeaderThemeState;
		::SetPropW(window, kHeaderThemeStateProperty, state);
	}
	return state;
}

int HeaderItemAt(HWND window, LPARAM lParam)
{
	HDHITTESTINFO hit = {};
	hit.pt.x = GET_X_LPARAM(lParam); hit.pt.y = GET_Y_LPARAM(lParam);
	return static_cast<int>(::SendMessage(window, HDM_HITTEST, 0, reinterpret_cast<LPARAM>(&hit)));
}

void InvalidateHeaderItem(HWND window, int item)
{
	if(item < 0) return;
	RECT rect = {};
	if(::SendMessage(window, HDM_GETITEMRECT, item, reinterpret_cast<LPARAM>(&rect)))
		::InvalidateRect(window, &rect, FALSE);
}

void PaintDarkHeader(HWND window, HeaderThemeState& state)
{
	PAINTSTRUCT paint = {};
	HDC dc = ::BeginPaint(window, &paint);
	RECT client = {}; ::GetClientRect(window, &client);
	::FillRect(dc, &client, ThemeManager::ControlBrush());
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(window, WM_GETFONT, 0, 0));
	HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
	const int itemCount = static_cast<int>(::SendMessage(window, HDM_GETITEMCOUNT, 0, 0));
	for(int item = 0; item < itemCount; ++item)
	{
		RECT rect = {};
		if(!::SendMessage(window, HDM_GETITEMRECT, item, reinterpret_cast<LPARAM>(&rect))) continue;
		const ThemeColorRole surface = item == state.pressedItem ? THEME_COLOR_PRESSED :
			item == state.hotItem ? THEME_COLOR_HOVER : THEME_COLOR_CONTROL;
		::FillRect(dc, &rect, ThemeManager::Brush(surface));

		wchar_t text[512] = {};
		HDITEM headerItem = {}; headerItem.mask = HDI_TEXT | HDI_FORMAT;
		headerItem.pszText = text; headerItem.cchTextMax = _countof(text);
		::SendMessage(window, HDM_GETITEM, item, reinterpret_cast<LPARAM>(&headerItem));
		RECT textRect = rect; textRect.left += 8; textRect.right -= 8;
		const bool sorted = (headerItem.fmt & (HDF_SORTUP | HDF_SORTDOWN)) != 0;
		if(sorted) textRect.right -= 12;
		UINT flags = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
		if((headerItem.fmt & HDF_JUSTIFYMASK) == HDF_RIGHT) flags |= DT_RIGHT;
		else if((headerItem.fmt & HDF_JUSTIFYMASK) == HDF_CENTER) flags |= DT_CENTER;
		else flags |= DT_LEFT;
		::SetBkMode(dc, TRANSPARENT);
		::SetTextColor(dc, ::IsWindowEnabled(window) ? ThemeManager::TextColor() : ThemeManager::DisabledTextColor());
		::DrawTextW(dc, text, -1, &textRect, flags);

		if(sorted)
		{
			const LONG middle = rect.top + (rect.bottom - rect.top) / 2;
			const LONG right = rect.right - 7;
			POINT triangle[3] = {};
			if(headerItem.fmt & HDF_SORTUP) { triangle[0] = { right - 5, middle + 3 }; triangle[1] = { right + 1, middle + 3 }; triangle[2] = { right - 2, middle - 3 }; }
			else { triangle[0] = { right - 5, middle - 3 }; triangle[1] = { right + 1, middle - 3 }; triangle[2] = { right - 2, middle + 3 }; }
			HBRUSH arrow = ::CreateSolidBrush(ThemeManager::SecondaryTextColor());
			HGDIOBJ oldBrush = ::SelectObject(dc, arrow); HGDIOBJ oldPen = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
			::Polygon(dc, triangle, _countof(triangle));
			::SelectObject(dc, oldPen); ::SelectObject(dc, oldBrush); ::DeleteObject(arrow);
		}

		RECT separator = rect; separator.left = separator.right - 1;
		::FillRect(dc, &separator, ThemeManager::Brush(THEME_COLOR_SEPARATOR));
	}
	if(oldFont) ::SelectObject(dc, oldFont);
	::EndPaint(window, &paint);
}

LRESULT HandleDarkHeaderMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	HeaderThemeState* state = HeaderState(window);
	switch(message)
	{
	case WM_MOUSEMOVE:
		{
			const int item = HeaderItemAt(window, lParam);
			if(item != state->hotItem) { InvalidateHeaderItem(window, state->hotItem); state->hotItem = item; InvalidateHeaderItem(window, state->hotItem); }
			if(!state->trackingMouse) { TRACKMOUSEEVENT tracking = { sizeof(tracking), TME_LEAVE, window, 0 }; ::TrackMouseEvent(&tracking); state->trackingMouse = true; }
		}
		break;
	case WM_MOUSELEAVE:
		state->trackingMouse = false; InvalidateHeaderItem(window, state->hotItem); state->hotItem = -1;
		break;
	case WM_LBUTTONDOWN:
		state->pressedItem = HeaderItemAt(window, lParam); InvalidateHeaderItem(window, state->pressedItem);
		break;
	case WM_LBUTTONUP:
	case WM_CANCELMODE:
		InvalidateHeaderItem(window, state->pressedItem); state->pressedItem = -1;
		break;
	case WM_PAINT:
		PaintDarkHeader(window, *state); return 0;
	case WM_THEMECHANGED:
		::InvalidateRect(window, NULL, TRUE);
		break;
	}
	return ::DefSubclassProc(window, message, wParam, lParam);
}

void PaintDarkGroupBox(HWND window)
{
	PAINTSTRUCT paint = {};
	HDC dc = ::BeginPaint(window, &paint);
	RECT client = {}; ::GetClientRect(window, &client);
	::FillRect(dc, &client, ThemeManager::WindowBrush());

	wchar_t caption[256] = {};
	::GetWindowTextW(window, caption, _countof(caption));
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(window, WM_GETFONT, 0, 0));
	HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
	SIZE textSize = {};
	::GetTextExtentPoint32W(dc, caption, ::lstrlenW(caption), &textSize);

	RECT border = client;
	border.top += (textSize.cy + 1) / 2;
	::FrameRect(dc, &border, ThemeManager::Brush(THEME_COLOR_BORDER));
	RECT captionRect = { 8, 0, (std::min)(client.right - 2, 12 + textSize.cx), textSize.cy };
	::FillRect(dc, &captionRect, ThemeManager::WindowBrush());
	::SetBkMode(dc, TRANSPARENT);
	::SetTextColor(dc, ::IsWindowEnabled(window) ? ThemeManager::TextColor() : ThemeManager::DisabledTextColor());
	::DrawTextW(dc, caption, -1, &captionRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
	if(oldFont) ::SelectObject(dc, oldFont);
	::EndPaint(window, &paint);
}

LRESULT CALLBACK ThemeControlSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
	if(message == WM_NCDESTROY)
	{
		if(IsHeader(window)) delete reinterpret_cast<HeaderThemeState*>(::RemovePropW(window, kHeaderThemeStateProperty));
		::RemoveWindowSubclass(window, ThemeControlSubclassProc, kThemeControlSubclassId);
		return ::DefSubclassProc(window, message, wParam, lParam);
	}
	if(IsHighContrastEnabled()) return ::DefSubclassProc(window, message, wParam, lParam);
	if(ThemeManager::IsDark() && HasClientEdge(window) && message == WM_NCPAINT)
	{
		const LRESULT result = ::DefSubclassProc(window, message, wParam, lParam);
		PaintDarkClientEdge(window);
		return result;
	}
	if(ThemeManager::IsDark() && IsHeader(window)) return HandleDarkHeaderMessage(window, message, wParam, lParam);
	if(message == WM_COMMAND && HIWORD(wParam) == CBN_DROPDOWN)
	{
		HWND combo = reinterpret_cast<HWND>(lParam);
		if(IsComboBox(combo)) ApplyComboDropListTheme(combo);
	}
	if(ThemeManager::IsDark() && IsGroupBox(window))
	{
		if(message == WM_ERASEBKGND) return 1;
		if(message == WM_PAINT) { PaintDarkGroupBox(window); return 0; }
	}
	if(message == WM_CTLCOLORDLG)
	{
		HDC dc = reinterpret_cast<HDC>(wParam);
		::SetBkColor(dc, ThemeManager::WindowColor());
		::SetTextColor(dc, ThemeManager::TextColor());
		return reinterpret_cast<LRESULT>(ThemeManager::WindowBrush());
	}
	if(message == WM_CTLCOLORSTATIC)
	{
		// Labels are normally painted directly on a dialog/panel.  Using the
		// control brush here leaves visible rectangles behind captions in dark
		// dialogs, so keep them on the owning window background.
		HDC dc = reinterpret_cast<HDC>(wParam);
		HWND control = reinterpret_cast<HWND>(lParam);
		const bool enabled = !control || ::IsWindowEnabled(control) != FALSE;
		::SetTextColor(dc, enabled ? ThemeManager::TextColor() : ThemeManager::DisabledTextColor());
		::SetBkColor(dc, ThemeManager::WindowColor());
		return reinterpret_cast<LRESULT>(ThemeManager::WindowBrush());
	}
	if(message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX || message == WM_CTLCOLORBTN)
	{
		HDC dc = reinterpret_cast<HDC>(wParam);
		HWND control = reinterpret_cast<HWND>(lParam);
		const bool enabled = !control || ::IsWindowEnabled(control) != FALSE;
		::SetTextColor(dc, enabled ? ThemeManager::TextColor() : ThemeManager::DisabledTextColor());
		::SetBkColor(dc, ThemeManager::ControlColor());
		return reinterpret_cast<LRESULT>(ThemeManager::ControlBrush());
	}
	return ::DefSubclassProc(window, message, wParam, lParam);
}

void ApplyNativeControlPalette(HWND window)
{
	if(IsHighContrastEnabled()) return;
	if(IsClass(window, WC_TREEVIEWW))
	{
		::SendMessage(window, TVM_SETBKCOLOR, 0, ThemeManager::WindowColor());
		::SendMessage(window, TVM_SETTEXTCOLOR, 0, ThemeManager::TextColor());
		::SendMessage(window, TVM_SETLINECOLOR, 0, ThemeManager::SeparatorColor());
	}
	else if(IsClass(window, WC_LISTVIEWW))
	{
		::SendMessage(window, LVM_SETBKCOLOR, 0, ThemeManager::WindowColor());
		::SendMessage(window, LVM_SETTEXTBKCOLOR, 0, ThemeManager::WindowColor());
		::SendMessage(window, LVM_SETTEXTCOLOR, 0, ThemeManager::TextColor());
		HWND header = reinterpret_cast<HWND>(::SendMessage(window, LVM_GETHEADER, 0, 0));
		if(::IsWindow(header)) ThemeManager::ApplyToWindow(header);
	}
	else if(IsClass(window, WC_TABCONTROLW))
	{
		::SendMessage(window, CCM_SETBKCOLOR, 0, ThemeManager::ControlColor());
	}
	else if(IsClass(window, TOOLBARCLASSNAMEW))
	{
		COLORSCHEME colours = {};
		colours.dwSize = sizeof(colours);
		colours.clrBtnHighlight = ThemeManager::HoverColor();
		colours.clrBtnShadow = ThemeManager::BorderColor();
		::SendMessage(window, TB_SETCOLORSCHEME, 0, reinterpret_cast<LPARAM>(&colours));
	}
	else if(IsClass(window, L"Edit"))
		::SendMessage(window, EM_SETBKGNDCOLOR, 0, ThemeManager::ControlColor());
	else if(IsClass(window, STATUSCLASSNAMEW))
		::SendMessage(window, SB_SETBKCOLOR, 0, ThemeManager::ControlColor());
	else if(IsClass(window, REBARCLASSNAMEW))
		::SendMessage(window, RB_SETBKCOLOR, 0, ThemeManager::ControlColor());
}

void ApplyPreferredAppMode(bool dark)
{
	// Ordinal 135 exists only on modern Windows 10 builds.  Resolving it at
	// runtime leaves Windows 7 on the normal, supported light-menu path.
	HMODULE uxtheme = ::LoadLibraryW(L"uxtheme.dll");
	if(!uxtheme) return;
	SetPreferredAppModeFn setMode = reinterpret_cast<SetPreferredAppModeFn>(::GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
	// AllowDark leaves popup menus light when FBE is explicitly Dark but Windows
	// itself is light. ForceDark makes FBE's selected theme govern its menus.
	if(setMode) setMode(dark ? 2 /* ForceDark */ : 0 /* Default */);
	// Rebuild popup-menu rendering after changing the preferred app mode.  This
	// export is available only on supported Windows 10/11 builds, so resolving
	// it dynamically keeps the Windows 7 path untouched.
	FlushMenuThemesFn flushMenus = reinterpret_cast<FlushMenuThemesFn>(::GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));
	if(flushMenus) flushMenus();
	::FreeLibrary(uxtheme);
}

void ApplyModernTitleBar(HWND window, bool dark)
{
	// DwmSetWindowAttribute is absent on older Windows and dwmapi.dll need not
	// have been loaded yet.  Resolve it at the point a real top-level HWND is
	// available; this keeps the Windows 7 path harmless and avoids a light
	// non-client frame briefly appearing on modern Windows.
	HMODULE dwmapi = ::LoadLibraryW(L"dwmapi.dll");
	DwmSetWindowAttributeFn setAttribute = dwmapi ? reinterpret_cast<DwmSetWindowAttributeFn>(::GetProcAddress(dwmapi, "DwmSetWindowAttribute")) : NULL;
	if(setAttribute)
	{
		const BOOL enabled = dark ? TRUE : FALSE;
		// 20 is the documented Windows 10 20H1 attribute; 19 is used by 1809.
		if(FAILED(setAttribute(window, 20, &enabled, sizeof(enabled))))
			setAttribute(window, 19, &enabled, sizeof(enabled));

		// These attributes are supported on Windows 11.  Numeric constants keep
		// the v143/Windows 7 SDK baseline intact; unsupported attributes simply
		// fail and leave the system title bar unchanged.
		const DWORD kDwmBorderColor = 34;
		const DWORD kDwmCaptionColor = 35;
		const DWORD kDwmTextColor = 36;
		const COLORREF kDwmDefaultColor = 0xFFFFFFFF;
		const COLORREF caption = dark ? ThemeManager::WindowColor() : kDwmDefaultColor;
		const COLORREF text = dark ? ThemeManager::TextColor() : kDwmDefaultColor;
		const COLORREF border = dark ? ThemeManager::BorderColor() : kDwmDefaultColor;
		setAttribute(window, kDwmCaptionColor, &caption, sizeof(caption));
		setAttribute(window, kDwmTextColor, &text, sizeof(text));
		setAttribute(window, kDwmBorderColor, &border, sizeof(border));
	}
	if(dwmapi) ::FreeLibrary(dwmapi);
	HMODULE uxtheme = ::LoadLibraryW(L"uxtheme.dll");
	if(uxtheme)
	{
		typedef BOOL (WINAPI* AllowDarkModeForWindowFn)(HWND, BOOL);
		AllowDarkModeForWindowFn allow = reinterpret_cast<AllowDarkModeForWindowFn>(::GetProcAddress(uxtheme, MAKEINTRESOURCEA(133)));
		if(allow) allow(window, dark ? TRUE : FALSE);
		::FreeLibrary(uxtheme);
	}
}

BOOL CALLBACK ApplyChild(HWND window, LPARAM)
{
	ThemeManager::ApplyToWindow(window);
	return TRUE;
}

BOOL CALLBACK ApplyThreadWindow(HWND window, LPARAM)
{
	ThemeManager::ApplyToWindow(window);
	return TRUE;
}

LRESULT CALLBACK ThemeCbtHookProc(int code, WPARAM wParam, LPARAM lParam)
{
	if(code == HCBT_ACTIVATE && wParam != 0)
		ThemeManager::ApplyToWindow(reinterpret_cast<HWND>(wParam));
	return ::CallNextHookEx(g_themeCbtHook, code, wParam, lParam);
}

void EnsureThemeCbtHook()
{
	if(g_themeCbtHook == NULL)
		g_themeCbtHook = ::SetWindowsHookExW(WH_CBT, ThemeCbtHookProc, NULL, ::GetCurrentThreadId());
}
}

namespace ThemeManager
{
namespace
{
struct TaskDialogCallbackState
{
	PFTASKDIALOGCALLBACK callback;
	LONG_PTR callbackData;
};

HRESULT CALLBACK ThemedTaskDialogCallback(HWND window, UINT notification, WPARAM wParam, LPARAM lParam, LONG_PTR reference)
{
	TaskDialogCallbackState* state = reinterpret_cast<TaskDialogCallbackState*>(reference);
	if(notification == TDN_CREATED)
		ApplyToWindow(window);
	return state->callback != NULL ? state->callback(window, notification, wParam, lParam, state->callbackData) : S_OK;
}

struct ThemedMessageButton
{
	UINT id;
	CString text;
};

class ThemedMessageDialog
{
	HWND m_window = NULL;
	HWND m_owner = NULL;
	CString m_message;
	CString m_caption;
	UINT m_type = 0;
	int m_result = 0;
	int m_buttonTop = 0;
	UINT m_defaultId = IDOK;
	std::vector<ThemedMessageButton> m_buttons;
	std::vector<HWND> m_buttonWindows;

	static ATOM RegisterWindowClass()
	{
		static ATOM atom = 0;
		if(atom != 0) return atom;
		WNDCLASSEXW klass = {}; klass.cbSize = sizeof(klass); klass.style = CS_HREDRAW | CS_VREDRAW;
		klass.lpfnWndProc = WindowProc; klass.hInstance = _Module.GetModuleInstance();
		klass.hCursor = ::LoadCursor(NULL, IDC_ARROW); klass.hbrBackground = NULL;
		klass.lpszClassName = L"FBEThemedMessageDialog";
		atom = ::RegisterClassExW(&klass);
		return atom;
	}

	static LPCWSTR IconFor(UINT type)
	{
		switch(type & MB_ICONMASK)
		{
		case MB_ICONHAND: return IDI_ERROR;
		case MB_ICONQUESTION: return IDI_QUESTION;
		case MB_ICONEXCLAMATION: return IDI_WARNING;
		case MB_ICONASTERISK: return IDI_INFORMATION;
		default: return NULL;
		}
	}

	static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		ThemedMessageDialog* dialog = reinterpret_cast<ThemedMessageDialog*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
		if(message == WM_NCCREATE)
		{
			dialog = static_cast<ThemedMessageDialog*>(reinterpret_cast<LPCREATESTRUCTW>(lParam)->lpCreateParams);
			::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
			dialog->m_window = window;
		}
		if(dialog == NULL) return ::DefWindowProcW(window, message, wParam, lParam);
		switch(message)
		{
		case WM_CREATE: dialog->CreateControls(); return 0;
		case WM_ERASEBKGND: return 1;
		case WM_PAINT: dialog->Paint(); return 0;
		case WM_COMMAND:
			if(HIWORD(wParam) == BN_CLICKED) { dialog->Close(static_cast<UINT>(LOWORD(wParam))); return 0; }
			break;
		case WM_CLOSE: dialog->Close(dialog->CancelResult()); return 0;
		case WM_DESTROY: return 0;
		}
		return ::DefWindowProcW(window, message, wParam, lParam);
	}

	CString ButtonText(UINT id) const
	{
		switch(id)
		{
		case IDOK: return FbeLoadRuntimeString(IDS_MB_OK);
		case IDCANCEL: return FbeLoadRuntimeString(IDS_MB_CANCEL);
		case IDYES: return FbeLoadRuntimeString(IDS_MB_YES);
		case IDNO: return FbeLoadRuntimeString(IDS_MB_NO);
		case IDABORT: return FbeLoadRuntimeString(IDS_MB_ABORT);
		case IDRETRY: return FbeLoadRuntimeString(IDS_MB_RETRY);
		case IDIGNORE: return FbeLoadRuntimeString(IDS_MB_IGNORE);
		case IDCLOSE: return FbeLoadRuntimeString(IDS_MB_CLOSE);
		default: return CString();
		}
	}

	void AddButton(UINT id) { ThemedMessageButton button = { id, ButtonText(id) }; m_buttons.push_back(button); }

	void BuildButtons()
	{
		switch(m_type & MB_TYPEMASK)
		{
		case MB_OKCANCEL: AddButton(IDOK); AddButton(IDCANCEL); break;
		case MB_YESNO: AddButton(IDYES); AddButton(IDNO); break;
		case MB_YESNOCANCEL: AddButton(IDYES); AddButton(IDNO); AddButton(IDCANCEL); break;
		case MB_RETRYCANCEL: AddButton(IDRETRY); AddButton(IDCANCEL); break;
		case MB_ABORTRETRYIGNORE: AddButton(IDABORT); AddButton(IDRETRY); AddButton(IDIGNORE); break;
		default: AddButton(IDOK); break;
		}
		UINT defaultIndex = 0;
		switch(m_type & MB_DEFMASK) { case MB_DEFBUTTON2: defaultIndex = 1; break; case MB_DEFBUTTON3: defaultIndex = 2; break; case MB_DEFBUTTON4: defaultIndex = 3; break; }
		if(defaultIndex >= m_buttons.size()) defaultIndex = 0;
		m_defaultId = m_buttons[defaultIndex].id;
	}

	int CancelResult() const
	{
		for(size_t index = 0; index < m_buttons.size(); ++index) if(m_buttons[index].id == IDCANCEL) return IDCANCEL;
		for(size_t index = 0; index < m_buttons.size(); ++index) if(m_buttons[index].id == IDNO) return IDNO;
		return static_cast<int>(m_defaultId);
	}

	void CreateControls()
	{
		const int iconX = 22, textX = 68, textY = 24;
		if(LPCWSTR icon = IconFor(m_type))
		{
			HWND control = ::CreateWindowExW(0, WC_STATICW, NULL, WS_CHILD | WS_VISIBLE | SS_ICON, iconX, textY, 32, 32, m_window, NULL, _Module.GetModuleInstance(), NULL);
			::SendMessageW(control, STM_SETICON, reinterpret_cast<WPARAM>(::LoadIconW(NULL, icon)), 0);
		}
		RECT client = {}; ::GetClientRect(m_window, &client);
		::CreateWindowExW(0, WC_STATICW, m_message, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
			textX, textY, client.right - textX - 20, m_buttonTop - textY - 14, m_window, NULL, _Module.GetModuleInstance(), NULL);
		const int width = 92, gap = 9, margin = 18;
		int x = client.right - margin - static_cast<int>(m_buttons.size()) * width - static_cast<int>(m_buttons.size() - 1) * gap;
		for(size_t index = 0; index < m_buttons.size(); ++index)
		{
			const DWORD style = WS_CHILD | WS_VISIBLE | (m_buttons[index].id == m_defaultId ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON);
			HWND button = ::CreateWindowExW(0, WC_BUTTONW, m_buttons[index].text, style, x, m_buttonTop + 12, width, 27,
				m_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(m_buttons[index].id)), _Module.GetModuleInstance(), NULL);
			m_buttonWindows.push_back(button); x += width + gap;
		}
		ThemeManager::ApplyToWindow(m_window);
		for(size_t index = 0; index < m_buttons.size(); ++index)
			if(m_buttons[index].id == m_defaultId) { ::SetFocus(m_buttonWindows[index]); break; }
	}

	void Paint()
	{
		PAINTSTRUCT paint = {}; HDC dc = ::BeginPaint(m_window, &paint);
		RECT client = {}; ::GetClientRect(m_window, &client);
		RECT content = client; content.bottom = m_buttonTop;
		::FillRect(dc, &content, ThemeManager::WindowBrush());
		RECT buttons = client; buttons.top = m_buttonTop;
		::FillRect(dc, &buttons, ThemeManager::ControlBrush());
		RECT separator = { client.left, m_buttonTop, client.right, m_buttonTop + 1 };
		::FillRect(dc, &separator, ThemeManager::Brush(THEME_COLOR_SEPARATOR));
		::FrameRect(dc, &client, ThemeManager::Brush(THEME_COLOR_BORDER));
		::EndPaint(m_window, &paint);
	}

	void Close(UINT result) { m_result = static_cast<int>(result); if(::IsWindow(m_window)) ::DestroyWindow(m_window); }

	bool HandleMessage(MSG& message)
	{
		if(message.message != WM_KEYDOWN || (message.hwnd != m_window && !::IsChild(m_window, message.hwnd))) return false;
		if(message.wParam == VK_ESCAPE) { Close(CancelResult()); return true; }
		if(message.wParam == VK_RETURN)
		{
			for(size_t index = 0; index < m_buttons.size(); ++index)
				if(m_buttons[index].id == m_defaultId) { ::SendMessage(m_buttonWindows[index], BM_CLICK, 0, 0); return true; }
		}
		if(message.wParam == VK_TAB)
		{
			HWND next = ::GetNextDlgTabItem(m_window, ::GetFocus(), (::GetKeyState(VK_SHIFT) & 0x8000) != 0);
			if(next) ::SetFocus(next);
			return true;
		}
		return false;
	}

public:
	ThemedMessageDialog(HWND owner, LPCWSTR message, LPCWSTR caption, UINT type) :
		m_owner(owner), m_message(message ? message : L""), m_caption(caption ? caption : L""), m_type(type) {}

	int Show()
	{
		if(RegisterWindowClass() == 0) return 0;
		BuildButtons();
		HDC dc = ::GetDC(NULL); HFONT font = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
		HGDIOBJ old = ::SelectObject(dc, font); RECT text = { 0, 0, 420, 0 };
		::DrawTextW(dc, m_message, -1, &text, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
		::SelectObject(dc, old); ::ReleaseDC(NULL, dc);
		const int width = 500; m_buttonTop = (std::max)(104, static_cast<int>(text.bottom - text.top) + 48);
		const int height = m_buttonTop + 57;
		RECT ownerRect = {}; if(!::GetWindowRect(m_owner, &ownerRect)) ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &ownerRect, 0);
		const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
		const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;
		m_window = ::CreateWindowExW(WS_EX_DLGMODALFRAME, L"FBEThemedMessageDialog", m_caption,
			WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, width, height, m_owner, NULL, _Module.GetModuleInstance(), this);
		if(!m_window) return 0;
		const bool enableOwner = ::IsWindow(m_owner) && ::IsWindowEnabled(m_owner);
		if(enableOwner) ::EnableWindow(m_owner, FALSE);
		::ShowWindow(m_window, SW_SHOW); ::UpdateWindow(m_window); ::SetForegroundWindow(m_window);
		MSG message = {};
		while(::IsWindow(m_window) && ::GetMessageW(&message, NULL, 0, 0) > 0)
		{
			if(!HandleMessage(message) && !::IsDialogMessageW(m_window, &message)) { ::TranslateMessage(&message); ::DispatchMessageW(&message); }
		}
		if(enableOwner) { ::EnableWindow(m_owner, TRUE); ::SetForegroundWindow(m_owner); }
		return m_result ? m_result : CancelResult();
	}
};
}

UINT TrackPopupMenu(HMENU menu, UINT flags, int x, int y, HWND owner)
{
	if(menu == NULL || !::IsWindow(owner)) return 0;
	ApplyNativeMenuBitmaps(menu);
	return ::TrackPopupMenuEx(menu, flags | TPM_RETURNCMD, x, y, owner, NULL);
}

void RegisterNativeMenuBitmap(UINT command, HBITMAP bitmap)
{
	if(command != 0 && bitmap != NULL) g_nativeMenuBitmaps[command] = bitmap;
}

void UnregisterNativeMenuBitmap(UINT command)
{
	g_nativeMenuBitmaps.erase(command);
}

int MessageBox(HWND owner, LPCWSTR message, LPCWSTR caption, UINT type)
{
	// FBE's custom surface intentionally handles only ordinary in-process
	// messages.  Service/system-modal requests retain their Windows semantics.
	if(!IsDark() || IsHighContrastEnabled() || (type & (MB_SYSTEMMODAL | MB_SERVICE_NOTIFICATION)) != 0)
		return ::MessageBoxW(owner, message, caption, type);
	ThemedMessageDialog dialog(owner ? owner : ::GetActiveWindow(), message, caption, type);
	const int result = dialog.Show();
	return result != 0 ? result : ::MessageBoxW(owner, message, caption, type);
}

HRESULT TaskDialogIndirect(const TASKDIALOGCONFIG& config, int* button, int* radioButton, BOOL* verification)
{
	TaskDialogCallbackState state = { config.pfCallback, config.lpCallbackData };
	TASKDIALOGCONFIG themed = config;
	themed.pfCallback = ThemedTaskDialogCallback;
	themed.lpCallbackData = reinterpret_cast<LONG_PTR>(&state);
	return ::TaskDialogIndirect(&themed, button, radioButton, verification);
}

void SetSelectedTheme(InterfaceTheme theme)
{
	if(theme < INTERFACE_THEME_AUTOMATIC || theme > INTERFACE_THEME_DARK)
		theme = INTERFACE_THEME_AUTOMATIC;
	const bool wasDark = IsDark();
	const bool wasHighContrast = g_highContrast;
	g_selected = theme;
	g_systemDark = ReadAppsUseLightTheme();
	g_highContrast = IsHighContrastEnabled();
	EnsureThemeCbtHook();
	ApplyPreferredAppMode(IsDark() && !g_highContrast);
	if(wasDark != IsDark() || wasHighContrast != g_highContrast || !g_windowBrush) RebuildBrushes();
}

InterfaceTheme GetSelectedTheme() { return g_selected; }
bool IsDark() { return g_selected == INTERFACE_THEME_DARK || (g_selected == INTERFACE_THEME_AUTOMATIC && g_systemDark); }
bool IsHighContrast() { return g_highContrast; }
COLORREF Color(ThemeColorRole role)
{
	if(IsHighContrastEnabled())
	{
		switch(role)
		{
		case THEME_COLOR_WINDOW: return ::GetSysColor(COLOR_WINDOW);
		case THEME_COLOR_CONTROL: return ::GetSysColor(COLOR_BTNFACE);
		case THEME_COLOR_TEXT: case THEME_COLOR_SECONDARY_TEXT: return ::GetSysColor(COLOR_WINDOWTEXT);
		case THEME_COLOR_DISABLED_TEXT: return ::GetSysColor(COLOR_GRAYTEXT);
		case THEME_COLOR_SELECTION_BACKGROUND: return ::GetSysColor(COLOR_HIGHLIGHT);
		case THEME_COLOR_SELECTION_TEXT: return ::GetSysColor(COLOR_HIGHLIGHTTEXT);
		default: return ::GetSysColor(COLOR_WINDOWTEXT);
		}
	}
	if(!IsDark())
	{
		switch(role)
		{
		case THEME_COLOR_WINDOW: return ::GetSysColor(COLOR_WINDOW);
		case THEME_COLOR_CONTROL: return ::GetSysColor(COLOR_BTNFACE);
		case THEME_COLOR_TEXT: return ::GetSysColor(COLOR_WINDOWTEXT);
		case THEME_COLOR_BORDER: return ::GetSysColor(COLOR_3DSHADOW);
		case THEME_COLOR_SEPARATOR: return ::GetSysColor(COLOR_3DLIGHT);
		case THEME_COLOR_SECONDARY_TEXT: return ::GetSysColor(COLOR_GRAYTEXT);
		case THEME_COLOR_DISABLED_TEXT: return ::GetSysColor(COLOR_GRAYTEXT);
		case THEME_COLOR_SELECTION_BACKGROUND: return ::GetSysColor(COLOR_HIGHLIGHT);
		case THEME_COLOR_SELECTION_TEXT: return ::GetSysColor(COLOR_HIGHLIGHTTEXT);
		case THEME_COLOR_HOVER: return RGB(229, 241, 251);
		case THEME_COLOR_PRESSED: return RGB(204, 228, 247);
		case THEME_COLOR_FOCUS: case THEME_COLOR_ACCENT: return RGB(0, 120, 215);
		case THEME_COLOR_ERROR: return RGB(196, 43, 28);
		case THEME_COLOR_WARNING: return RGB(156, 99, 0);
		case THEME_COLOR_SUCCESS: return RGB(16, 124, 16);
		default: return ::GetSysColor(COLOR_WINDOW);
		}
	}
	switch(role)
	{
	case THEME_COLOR_WINDOW: return RGB(32, 32, 32);
	case THEME_COLOR_CONTROL: return RGB(45, 45, 45);
	case THEME_COLOR_TEXT: return RGB(230, 230, 230);
	case THEME_COLOR_BORDER: return RGB(92, 92, 92);
	case THEME_COLOR_SEPARATOR: return RGB(62, 62, 62);
	case THEME_COLOR_SECONDARY_TEXT: return RGB(184, 184, 184);
	case THEME_COLOR_DISABLED_TEXT: return RGB(136, 136, 136);
	case THEME_COLOR_SELECTION_BACKGROUND: return RGB(38, 79, 120);
	case THEME_COLOR_SELECTION_TEXT: return RGB(255, 255, 255);
	case THEME_COLOR_HOVER: return RGB(58, 58, 58);
	case THEME_COLOR_PRESSED: return RGB(72, 72, 72);
	case THEME_COLOR_FOCUS: case THEME_COLOR_ACCENT: return RGB(76, 194, 255);
	case THEME_COLOR_ERROR: return RGB(255, 99, 71);
	case THEME_COLOR_WARNING: return RGB(255, 184, 77);
	case THEME_COLOR_SUCCESS: return RGB(98, 202, 125);
	default: return RGB(32, 32, 32);
	}
}
COLORREF WindowColor() { return Color(THEME_COLOR_WINDOW); }
COLORREF TextColor() { return Color(THEME_COLOR_TEXT); }
COLORREF ControlColor() { return Color(THEME_COLOR_CONTROL); }
COLORREF BorderColor() { return Color(THEME_COLOR_BORDER); }
COLORREF SeparatorColor() { return Color(THEME_COLOR_SEPARATOR); }
COLORREF SecondaryTextColor() { return Color(THEME_COLOR_SECONDARY_TEXT); }
COLORREF DisabledTextColor() { return Color(THEME_COLOR_DISABLED_TEXT); }
COLORREF SelectionBackgroundColor() { return Color(THEME_COLOR_SELECTION_BACKGROUND); }
COLORREF SelectionTextColor() { return Color(THEME_COLOR_SELECTION_TEXT); }
COLORREF HoverColor() { return Color(THEME_COLOR_HOVER); }
COLORREF PressedColor() { return Color(THEME_COLOR_PRESSED); }
COLORREF FocusColor() { return Color(THEME_COLOR_FOCUS); }
COLORREF AccentColor() { return Color(THEME_COLOR_ACCENT); }
COLORREF ErrorColor() { return Color(THEME_COLOR_ERROR); }
COLORREF WarningColor() { return Color(THEME_COLOR_WARNING); }
COLORREF SuccessColor() { return Color(THEME_COLOR_SUCCESS); }
HBRUSH Brush(ThemeColorRole role) { if(!g_brushes[role]) RebuildBrushes(); return g_brushes[role]; }
HBRUSH WindowBrush() { if(!g_windowBrush) RebuildBrushes(); return g_windowBrush; }
HBRUSH ControlBrush() { if(!g_controlBrush) RebuildBrushes(); return g_controlBrush; }

void ApplyToWindow(HWND window)
{
	if(!::IsWindow(window)) return;
	const bool dark = IsDark() && !IsHighContrastEnabled();
	::SetWindowSubclass(window, ThemeControlSubclassProc, kThemeControlSubclassId, 0);
	// A single-space app/sub-app pair is the documented opt-out marker for
	// visual styles. An empty string merely selects the default theme again.
	if(dark && (UsesClassicSurfacePalette(window) || IsRadioButton(window)))
		// UxTheme draws radio labels using its system disabled colour.  The
		// classic path honours the parent's WM_CTLCOLORBTN palette, including
		// DisabledTextColor(), while leaving ordinary buttons untouched.
		::SetWindowTheme(window, L" ", L" ");
	else if(dark && IsComboBox(window))
		// DarkMode_CFD is the Windows 10/11 ComboBox visual-style contract. It
		// themes the edit/list field, glyph, focused border and disabled state.
		// On Windows 7 it is simply unavailable and falls back to light UxTheme.
		::SetWindowTheme(window, L"DarkMode_CFD", NULL);
	else if(dark && IsComboDropList(window))
		::SetWindowTheme(window, L"DarkMode_Explorer", NULL);
	else
		::SetWindowTheme(window, dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
	// Common controls reset custom colours while processing WM_THEMECHANGED.
	// Set their palette only after that notification has completed.
	::SendMessage(window, WM_THEMECHANGED, 0, 0);
	// WM_THEMECHANGED can reset the DWM non-client state.  Apply the title bar
	// last so a main window created in Dark remains dark after all child themes.
	ApplyModernTitleBar(window, dark);
	ApplyNativeControlPalette(window);
	::SendMessage(window, WM_FBE_THEMECHANGED, 0, 0);
	::RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
	::EnumChildWindows(window, ApplyChild, 0);
}

void ApplyToAllThreadWindows(DWORD threadId)
{
	RebuildBrushes();
	::EnumThreadWindows(threadId, ApplyThreadWindow, 0);
}

bool RefreshSystemTheme()
{
	const bool oldDark = IsDark();
	const bool oldHighContrast = g_highContrast;
	g_systemDark = ReadAppsUseLightTheme();
	g_highContrast = IsHighContrastEnabled();
	const bool highContrastChanged = oldHighContrast != g_highContrast;
	if((g_selected != INTERFACE_THEME_AUTOMATIC || oldDark == IsDark()) && !highContrastChanged) return false;
	ApplyPreferredAppMode(IsDark() && !g_highContrast);
	ApplyToAllThreadWindows(::GetCurrentThreadId());
	return true;
}
}
