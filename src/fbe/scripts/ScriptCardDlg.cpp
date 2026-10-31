#include "stdafx.h"
#include "ScriptCardDlg.h"
#include "../RuntimeLocalization.h"
#include "../ThemeManager.h"
#include "../UiMetrics.h"
#include "../utils/utils.h"
#include <richedit.h>
#include <commctrl.h>
#include <shellapi.h>
#include <thread>

namespace {
constexpr UINT_PTR kCardTimer = 0x5343;

CString Label(LPCWSTR key, LPCWSTR fallback)
{
	return FbeLoadRuntimeStringByKey(key, fallback);
}

CString ListText(const std::vector<CString>& values)
{
	CString text;
	for(const CString& value : values)
	{
		if(!text.IsEmpty()) text += L"\r\n";
		text += L"• " + value;
	}
	return text;
}

CString NumberedText(const std::vector<CString>& values)
{
	CString text;
	for(size_t index = 0; index < values.size(); ++index)
	{
		if(!text.IsEmpty()) text += L"\r\n";
		CString number;
		number.Format(L"%u. ", static_cast<unsigned>(index + 1));
		text += number + values[index];
	}
	return text;
}

CString TriState(int value)
{
	return value < 0 ? Label(L"fbe.script_card.unknown", L"Unknown") :
		value ? Label(L"fbe.script_card.yes", L"Yes") : Label(L"fbe.script_card.no", L"No");
}

CString ScriptCategory(const CString& relativePath)
{
	const int lastSlash = (std::max)(relativePath.ReverseFind(L'\\'), relativePath.ReverseFind(L'/'));
	if(lastSlash < 0) return CString();
	CString result;
	int start = 0;
	while(start < lastSlash)
	{
		int end = relativePath.Find(L'/', start);
		const int backslash = relativePath.Find(L'\\', start);
		if(end < 0 || (backslash >= 0 && backslash < end)) end = backslash;
		if(end < 0 || end > lastSlash) end = lastSlash;
		CString part = relativePath.Mid(start, end - start);
		int prefix = 0;
		while(prefix < part.GetLength() && part[prefix] >= L'0' && part[prefix] <= L'9') ++prefix;
		if(prefix > 0 && prefix < part.GetLength() && part[prefix] == L'_')
			part = part.Mid(prefix + 1);
		if(!part.IsEmpty())
		{
			::CharUpperBuffW(part.GetBuffer(), 1);
			part.ReleaseBuffer();
			if(!result.IsEmpty()) result += L"/";
			result += part;
		}
		start = end + 1;
	}
	return result;
}

CString AuthorRoleLabel(const CString& role)
{
	if(role == L"author") return Label(L"fbe.script_card.role.author", L"Author");
	if(role == L"original_author") return Label(L"fbe.script_card.role.original_author", L"Original author");
	if(role == L"coauthor") return Label(L"fbe.script_card.role.coauthor", L"Co-author");
	if(role == L"modifier") return Label(L"fbe.script_card.role.modifier", L"Modified by");
	if(role == L"contributor") return Label(L"fbe.script_card.role.contributor", L"Contributor");
	if(role == L"ai_assistance") return Label(L"fbe.script_card.role.ai_assistance", L"AI assistance");
	return Label(L"fbe.script_card.role.unknown", L"Author not specified");
}
}

CScriptCardDlg::~CScriptCardDlg()
{
	if(IsWindow()) DestroyWindow();
	if(m_headingFont != NULL) ::DeleteObject(m_headingFont);
	if(m_richEdit != NULL) ::FreeLibrary(m_richEdit);
}

bool CScriptCardDlg::ShowScript(HWND owner, const ScriptDescriptor& script, const CString& resourceRoot,
	const std::function<void(const CString&)>& runScript)
{
	if(script.isFolder || script.uid.IsEmpty()) return false;
	m_script = script;
	m_lastCard = ScriptCards::Card();
	m_sections.clear();
	for(VisualBlock& block : m_blocks) {
		if(block.heading) ::DestroyWindow(block.heading);
		if(block.body) ::DestroyWindow(block.body);
	}
	m_blocks.clear();
	m_runScript = runScript;
	if(!IsWindow())
	{
		m_richEdit = ::LoadLibraryW(L"Msftedit.dll");
		if(m_richEdit == NULL || Create(owner) == NULL) return false;
	}
	ApplyScriptIcon();
	ApplyWindowIcon();
	RefreshLocalization();
	m_headerText = script.name;
	SetDlgItemText(IDC_SCRIPT_CARD_HEADER, m_headerText);
	m_bodyText = Label(L"fbe.script_card.loading", L"Checking script description and linked files...");
	SetDlgItemText(IDC_SCRIPT_CARD_BODY, m_bodyText);
	m_headerRightText.Empty();
	if(m_headerRight) ::SetWindowTextW(m_headerRight, L"");
	m_statusText.Empty();
	m_warningText.Empty();
	if(m_warning) ::ShowWindow(m_warning, SW_HIDE);
	m_originText.Empty();
	m_scrollY = 0;
	Layout();
	ApplyRichFormatting();
	auto work = std::make_shared<Work>();
	work->path = script.path;
	work->resourceRoot = resourceRoot;
	m_work = work;
	std::thread([work]() {
		work->card = ScriptCards::LoadFile(work->path);
		ScriptCards::VerifyDependencies(work->card, work->resourceRoot);
		work->done.store(true, std::memory_order_release);
	}).detach();
	SetTimer(kCardTimer, 100);
	ShowWindow(SW_SHOWNORMAL);
	SetForegroundWindow(m_hWnd);
	return true;
}

void CScriptCardDlg::RefreshLocalization()
{
	if(!IsWindow()) return;
	CString caption = Label(L"fbe.script_card.caption", L"About script");
	if(!m_script.name.IsEmpty()) caption += L" — " + m_script.name;
	SetWindowText(caption);
	SetDlgItemText(IDC_SCRIPT_CARD_RUN, Label(L"fbe.script_card.run", L"Run"));
	SetDlgItemText(IDC_SCRIPT_CARD_OPEN, Label(L"fbe.script_card.open", L"Open file"));
	SetDlgItemText(IDC_SCRIPT_CARD_COPY, Label(L"fbe.script_card.copy", L"Copy details"));
	SetDlgItemText(IDCANCEL, Label(L"fbe.script_card.close", L"Close"));
	if(m_work == nullptr && !m_script.uid.IsEmpty()) ShowCard(m_lastCard);
	else if(m_work != nullptr)
	{
		m_bodyText = Label(L"fbe.script_card.loading", L"Checking script description and linked files...");
		SetDlgItemText(IDC_SCRIPT_CARD_BODY, m_bodyText);
		m_statusText.Empty();
		m_warningText.Empty();
		if(m_warning) ::ShowWindow(m_warning, SW_HIDE);
	}
	Layout();
}

LRESULT CScriptCardDlg::OnInit(UINT, WPARAM, LPARAM, BOOL&)
{
	::SetPropW(m_hWnd, L"FBE_SKIP_SYSTEM_DIALOG_LOCALIZATION", reinterpret_cast<HANDLE>(1));
	ModifyStyle(0, WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_dpi = UiMetrics::DpiForWindow(m_hWnd);
	m_headerRight = ::CreateWindowExW(0, L"RICHEDIT50W", L"", WS_CHILD | WS_VISIBLE |
		ES_MULTILINE | ES_READONLY, 0, 0, 1, 1, m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
	m_warning = ::CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
		0, 0, 1, 1, m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
	m_warningTip = ::CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL, WS_POPUP | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
	if(m_warningTip && m_warning)
	{
		TOOLINFOW tip = { sizeof(tip) };
		tip.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		tip.hwnd = m_hWnd;
		tip.uId = reinterpret_cast<UINT_PTR>(m_warning);
		tip.lpszText = const_cast<LPWSTR>(L"");
		::SendMessageW(m_warningTip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tip));
	}
	m_viewport = ::CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
		WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 1, 1, m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
	if(m_viewport) ::SetWindowSubclass(m_viewport, ViewportSubclass, 1, reinterpret_cast<DWORD_PTR>(this));
	::ShowWindow(GetDlgItem(IDC_SCRIPT_CARD_BODY), SW_HIDE);
	ApplyFont();
	ApplyWindowIcon();
	ThemeManager::ApplyToWindow(m_hWnd);
	ApplyBodyPalette();
	RestorePlacement();
	RefreshLocalization();
	Layout();
	return TRUE;
}

void CScriptCardDlg::ApplyFont()
{
	HFONT font = UiMetrics::CreateDialogFontForDpi(m_dpi);
	if(font == NULL) return;
	::SendMessageW(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
	const int ids[] = { IDC_SCRIPT_CARD_HEADER, IDC_SCRIPT_CARD_BODY, IDC_SCRIPT_CARD_RUN,
		IDC_SCRIPT_CARD_OPEN, IDC_SCRIPT_CARD_COPY, IDCANCEL };
	for(int id : ids) ::SendMessageW(GetDlgItem(id), WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
	for(HWND extra : { m_headerRight, m_warning })
		if(extra) ::SendMessageW(extra, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
	for(const VisualBlock& block : m_blocks)
		if(block.body) ::SendMessageW(block.body, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
	LOGFONTW details = {};
	::GetObjectW(font, sizeof(details), &details);
	details.lfWeight = FW_SEMIBOLD;
	HFONT heading = ::CreateFontIndirectW(&details);
	if(heading) {
		for(const VisualBlock& block : m_blocks)
			if(block.heading) ::SendMessageW(block.heading, WM_SETFONT, reinterpret_cast<WPARAM>(heading), TRUE);
		if(m_headingFont) ::DeleteObject(m_headingFont);
		m_headingFont = heading;
	}
	if(m_font != NULL) ::DeleteObject(m_font);
	m_font = font;
	ApplyRichFormatting();
}

void CScriptCardDlg::ApplyBodyPalette()
{
	HWND body = GetDlgItem(IDC_SCRIPT_CARD_BODY);
	if(!::IsWindow(body)) return;
	const bool highContrast = ThemeManager::IsHighContrast();
	const COLORREF background = highContrast ? ::GetSysColor(COLOR_WINDOW) : ThemeManager::WindowColor();
	const COLORREF foreground = highContrast ? ::GetSysColor(COLOR_WINDOWTEXT) : ThemeManager::TextColor();
	for(HWND rich : { static_cast<HWND>(GetDlgItem(IDC_SCRIPT_CARD_HEADER)),
		static_cast<HWND>(GetDlgItem(IDC_SCRIPT_CARD_BODY)), m_headerRight })
	{
		if(!::IsWindow(rich)) continue;
		::SendMessageW(rich, EM_SETBKGNDCOLOR, 0, background);
		CHARFORMAT2W format = {};
		format.cbSize = sizeof(format);
		format.dwMask = CFM_COLOR | CFM_BACKCOLOR;
		format.crTextColor = foreground;
		format.crBackColor = background;
		::SendMessageW(rich, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
		::SendMessageW(rich, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
		::InvalidateRect(rich, NULL, TRUE);
	}
	for(const VisualBlock& block : m_blocks)
	{
		for(HWND rich : { block.heading, block.body })
		{
			if(!::IsWindow(rich)) continue;
			const COLORREF surface = highContrast ? background :
				rich == block.heading ? ThemeManager::HoverColor() : background;
			::SendMessageW(rich, EM_SETBKGNDCOLOR, 0, surface);
			CHARFORMAT2W format = {};
			format.cbSize = sizeof(format);
			format.dwMask = CFM_COLOR;
			format.crTextColor = block.warning && rich == block.heading ?
				(highContrast ? foreground : ThemeManager::WarningColor()) : foreground;
			::SendMessageW(rich, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
			::InvalidateRect(rich, NULL, TRUE);
		}
	}
	if(m_warning) ::InvalidateRect(m_warning, NULL, TRUE);
	ApplyRichFormatting();
	ApplySettingFormatting();
}

void CScriptCardDlg::ApplyScriptIcon()
{
	if(!IsWindow()) return;
	const int side = UiMetrics::ScaleForDpi(44, m_dpi);
	CString iconPath(m_script.path);
	const int dot = iconPath.ReverseFind(L'.');
	if(dot >= 0) iconPath = iconPath.Left(dot) + L".ico";
	HICON icon = static_cast<HICON>(::LoadImageW(NULL, iconPath, IMAGE_ICON, side, side, LR_LOADFROMFILE));
	if(icon == NULL)
	{
		SHFILEINFOW info = {};
		if(::SHGetFileInfoW(m_script.path, FILE_ATTRIBUTE_NORMAL, &info, sizeof(info),
			SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_LARGEICON) != 0) icon = info.hIcon;
	}
	::SendMessageW(GetDlgItem(IDC_SCRIPT_CARD_ICON), STM_SETICON, reinterpret_cast<WPARAM>(icon), 0);
	if(m_icon) ::DestroyIcon(m_icon);
	m_icon = icon;
}

void CScriptCardDlg::ApplyWindowIcon()
{
	if(!IsWindow()) return;
	const int smallSide = UiMetrics::ScaleForDpi(16, m_dpi);
	const int largeSide = UiMetrics::ScaleForDpi(32, m_dpi);
	HICON newSmall = static_cast<HICON>(::LoadImageW(_Module.GetResourceInstance(),
		MAKEINTRESOURCEW(IDI_SCRIPT_CARD), IMAGE_ICON, smallSide, smallSide, LR_DEFAULTCOLOR));
	HICON newBig = static_cast<HICON>(::LoadImageW(_Module.GetResourceInstance(),
		MAKEINTRESOURCEW(IDI_SCRIPT_CARD), IMAGE_ICON, largeSide, largeSide, LR_DEFAULTCOLOR));
	if(newSmall) ::SendMessageW(m_hWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(newSmall));
	if(newBig) ::SendMessageW(m_hWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(newBig));
	if(newSmall) { if(m_windowIconSmall) ::DestroyIcon(m_windowIconSmall); m_windowIconSmall = newSmall; }
	if(newBig) { if(m_windowIconBig) ::DestroyIcon(m_windowIconBig); m_windowIconBig = newBig; }
}

void CScriptCardDlg::AppendSection(const CString& label, const CString& value, bool warning)
{
	if(value.IsEmpty()) return;
	if(!m_bodyText.IsEmpty()) m_bodyText += L"\r\n\r\n";
	// RichEdit stores CRLF as one paragraph character. Its selection offsets
	// therefore differ from the source CString after every preceding newline.
	LONG begin = m_bodyText.GetLength();
	for(int index = 0; index < m_bodyText.GetLength(); ++index)
		if(m_bodyText[index] == L'\n' && index > 0 && m_bodyText[index - 1] == L'\r') --begin;
	m_bodyText += label;
	m_sections.push_back({ begin, begin + label.GetLength(), warning });
	m_bodyText += L"\r\n" + value;
}

void CScriptCardDlg::AddVisualBlock(const CString& label, const CString& value, int layout, bool warning)
{
	if(value.IsEmpty()) return;
	AppendSection(label, value, warning);
	VisualBlock block;
	block.title = label;
	block.content = value;
	block.layout = layout;
	block.warning = warning;
	m_blocks.push_back(block);
}

void CScriptCardDlg::AddSettingsBlock(const std::vector<ScriptCards::Setting>& settings)
{
	if(settings.empty()) return;
	VisualBlock block;
	block.title = Label(L"fbe.script_card.settings", L"Settings");
	block.settings = true;
	LONG offset = 0;
	bool hasSharedSetting = false;
	for(const ScriptCards::Setting& setting : settings)
	{
		hasSharedSetting = hasSharedSetting || setting.sharedSetting;
		if(!block.content.IsEmpty()) { block.content += L"\r\n\r\n"; offset += 2; }
		if(!setting.title.IsEmpty())
		{
			block.titles.push_back({ offset, offset + setting.title.GetLength() });
			block.content += setting.title;
			offset += setting.title.GetLength();
			if(!setting.name.IsEmpty())
			{
				block.content += L"\r\n"; ++offset;
				block.names.push_back({ offset, offset + setting.name.GetLength() });
				block.content += setting.name;
				offset += setting.name.GetLength();
			}
		}
		else
		{
			block.names.push_back({ offset, offset + setting.name.GetLength() });
			block.content += setting.name;
			offset += setting.name.GetLength();
		}
		if(setting.valueKind != ScriptCards::Setting::ValueKind::Missing || !setting.expression.IsEmpty())
		{
			CString value = setting.value;
			if(setting.valueKind == ScriptCards::Setting::ValueKind::Boolean)
				value = value == L"true" ? Label(L"fbe.script_card.yes", L"Yes") :
					Label(L"fbe.script_card.no", L"No");
			else if(setting.valueKind == ScriptCards::Setting::ValueKind::String && value.IsEmpty())
				value = Label(L"fbe.script_card.empty_string", L"Empty string");
			else if(setting.valueKind == ScriptCards::Setting::ValueKind::Missing)
				value = Label(L"fbe.script_card.setting_dynamic", L"Value determined when the script runs");
			const CString valueLabel = setting.valueKind == ScriptCards::Setting::ValueKind::Missing ?
				Label(L"fbe.script_card.setting_value", L"Value") : setting.defaultValue ?
				Label(L"fbe.script_card.setting_default", L"Default value") :
				Label(L"fbe.script_card.setting_source", L"Value in source code");
			block.content += L"\r\n" + valueLabel + L": ";
			offset += 1 + valueLabel.GetLength() + 2;
			if(setting.valueKind != ScriptCards::Setting::ValueKind::Missing)
				block.values.push_back({ offset, offset + value.GetLength() });
			block.content += value;
			offset += value.GetLength();
		}
		if(!setting.description.IsEmpty())
		{
			block.content += L"\r\n" + setting.description;
			offset += 1 + setting.description.GetLength();
		}
	}
	if(hasSharedSetting)
	{
		block.content += L"\r\n\r\n";
		block.content += Label(L"fbe.script_card.setting_shared_note",
			L"This parameter is set in shared script settings. Its current value may differ from the default shown here.");
	}
	AppendSection(block.title, block.content);
	m_blocks.push_back(block);
}

LRESULT CALLBACK CScriptCardDlg::BlockSubclass(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR, DWORD_PTR context)
{
	CScriptCardDlg* card = reinterpret_cast<CScriptCardDlg*>(context);
	if(message == WM_MOUSEWHEEL)
		return ::SendMessageW(card->m_viewport, message, wParam, lParam);
	if(message == WM_KEYDOWN && (wParam == VK_HOME || wParam == VK_END ||
		wParam == VK_PRIOR || wParam == VK_NEXT) &&
		(::GetKeyState(VK_SHIFT) & 0x8000) == 0 && (::GetKeyState(VK_CONTROL) & 0x8000) == 0)
	{
		CHARRANGE selection = {};
		::SendMessageW(window, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
		if(selection.cpMin == selection.cpMax)
		{
			const int code = wParam == VK_HOME ? SB_TOP : wParam == VK_END ? SB_BOTTOM :
				wParam == VK_PRIOR ? SB_PAGEUP : SB_PAGEDOWN;
			::SendMessageW(card->m_viewport, WM_VSCROLL, code, 0);
			return 0;
		}
	}
	if(message == WM_NCDESTROY) ::RemoveWindowSubclass(window, BlockSubclass, 1);
	return ::DefSubclassProc(window, message, wParam, lParam);
}

LRESULT CALLBACK CScriptCardDlg::ViewportSubclass(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR, DWORD_PTR context)
{
	CScriptCardDlg* card = reinterpret_cast<CScriptCardDlg*>(context);
	if(message == WM_VSCROLL) { card->HandleViewportScroll(wParam); return 0; }
	if(message == WM_MOUSEWHEEL)
		return ::SendMessageW(card->m_hWnd, message, wParam, lParam);
	if(message == WM_NCDESTROY) ::RemoveWindowSubclass(window, ViewportSubclass, 1);
	return ::DefSubclassProc(window, message, wParam, lParam);
}

void CScriptCardDlg::RebuildVisualBlocks()
{
	for(VisualBlock& block : m_blocks)
	{
		if(block.heading) ::DestroyWindow(block.heading);
		if(block.body) ::DestroyWindow(block.body);
		block.heading = block.body = NULL;
	}
	if(!m_viewport) return;
	for(VisualBlock& block : m_blocks)
	{
		block.heading = ::CreateWindowExW(0, L"RICHEDIT50W", block.title,
			WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE, 0, 0, 1, 1,
			m_viewport, NULL, _Module.GetModuleInstance(), NULL);
		block.body = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"RICHEDIT50W", block.content,
			WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE, 0, 0, 1, 1,
			m_viewport, NULL, _Module.GetModuleInstance(), NULL);
		for(HWND rich : { block.heading, block.body })
			if(rich) ::SetWindowSubclass(rich, BlockSubclass, 1, reinterpret_cast<DWORD_PTR>(this));
		if(block.heading && m_headingFont)
			::SendMessageW(block.heading, WM_SETFONT, reinterpret_cast<WPARAM>(m_headingFont), FALSE);
		if(block.heading) {
			CHARFORMAT2W heading = {};
			heading.cbSize = sizeof(heading);
			heading.dwMask = CFM_BOLD;
			heading.dwEffects = CFE_BOLD;
			::SendMessageW(block.heading, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&heading));
		}
		if(block.body && m_font)
			::SendMessageW(block.body, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), FALSE);
	}
	ApplyBodyPalette();
}

void CScriptCardDlg::ApplySettingFormatting()
{
	for(const VisualBlock& block : m_blocks)
	{
		if(!block.settings || !::IsWindow(block.body)) continue;
		::SendMessageW(block.body, WM_SETREDRAW, FALSE, 0);
		for(const VisualBlock::Range& range : block.titles)
		{
			CHARRANGE selection = { range.begin, range.end };
			::SendMessageW(block.body, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
			CHARFORMAT2W format = {};
			format.cbSize = sizeof(format);
			format.dwMask = CFM_BOLD;
			format.dwEffects = CFE_BOLD;
			::SendMessageW(block.body, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
		}
		for(const VisualBlock::Range& range : block.names)
		{
			CHARRANGE selection = { range.begin, range.end };
			::SendMessageW(block.body, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
			CHARFORMAT2W format = {};
			format.cbSize = sizeof(format);
			format.dwMask = CFM_FACE;
			wcscpy_s(format.szFaceName, L"Consolas");
			::SendMessageW(block.body, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
		}
		for(const VisualBlock::Range& range : block.values)
		{
			CHARRANGE selection = { range.begin, range.end };
			::SendMessageW(block.body, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
			CHARFORMAT2W format = {};
			format.cbSize = sizeof(format);
			format.dwMask = CFM_FACE;
			wcscpy_s(format.szFaceName, L"Consolas");
			::SendMessageW(block.body, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
		}
		CHARRANGE start = { 0, 0 };
		::SendMessageW(block.body, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&start));
		::SendMessageW(block.body, WM_SETREDRAW, TRUE, 0);
		::InvalidateRect(block.body, NULL, FALSE);
	}
}

void CScriptCardDlg::LayoutVisualBlocks(int width, int height)
{
	if(!m_viewport) return;
	const int scaleGap = UiMetrics::ScaleForDpi(8, m_dpi);
	const int inset = UiMetrics::ScaleForDpi(4, m_dpi);
	const int available = (std::max)(1, width - 2 * inset - ::GetSystemMetrics(SM_CXVSCROLL));
	const int headingHeight = UiMetrics::ScaleForDpi(26, m_dpi);
	HDC dc = ::GetDC(m_viewport);
	HFONT previous = dc && m_font ? reinterpret_cast<HFONT>(::SelectObject(dc, m_font)) : NULL;
	TEXTMETRICW metrics = {};
	if(dc) ::GetTextMetricsW(dc, &metrics);
	auto contentHeight = [&](const VisualBlock& block, int columnWidth) {
		if(!::IsWindow(block.body)) return UiMetrics::ScaleForDpi(30, m_dpi);
		// Ask the actual RichEdit control to wrap at the final column width.
		::SetWindowPos(block.body, NULL, 0, 0, columnWidth, (std::max)(1, height),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
		if(block.settings)
		{
			const int padding = UiMetrics::ScaleForDpi(10, m_dpi);
			RECT interior = { padding, padding, (std::max)(padding + 1, columnWidth - padding),
				(std::max)(padding + 1, height - padding) };
			::SendMessageW(block.body, EM_SETRECT, 0, reinterpret_cast<LPARAM>(&interior));
		}
		const int lines = (std::max)(1, static_cast<int>(::SendMessageW(block.body, EM_GETLINECOUNT, 0, 0)));
		RECT formatting = {}, client = {};
		::SendMessageW(block.body, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&formatting));
		::GetClientRect(block.body, &client);
		const int verticalInsets = (std::max)(0, static_cast<int>(formatting.top)) +
			(std::max)(0, static_cast<int>(client.bottom - formatting.bottom)) +
			UiMetrics::ScaleForDpi(4, m_dpi);
		const int lineHeight = (std::max)(1, static_cast<int>(metrics.tmHeight + metrics.tmExternalLeading));
		return (std::max)(UiMetrics::ScaleForDpi(30, m_dpi), lines * lineHeight + verticalInsets +
			UiMetrics::ScaleForDpi(4, m_dpi));
	};
	auto headingWidth = [&](const VisualBlock& block) {
		if(!dc) return UiMetrics::ScaleForDpi(180, m_dpi);
		HFONT old = m_headingFont ? reinterpret_cast<HFONT>(::SelectObject(dc, m_headingFont)) : NULL;
		SIZE size = {};
		::GetTextExtentPoint32W(dc, block.title, block.title.GetLength(), &size);
		if(old) ::SelectObject(dc, old);
		return static_cast<int>(size.cx) + UiMetrics::ScaleForDpi(12, m_dpi);
	};
	auto measuredHeadingHeight = [&](const VisualBlock& block, int columnWidth) {
		if(!::IsWindow(block.heading)) return headingHeight;
		::SetWindowPos(block.heading, NULL, 0, 0, columnWidth, (std::max)(1, height),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
		const int lines = (std::max)(1, static_cast<int>(::SendMessageW(block.heading, EM_GETLINECOUNT, 0, 0)));
		return (std::max)(headingHeight, lines * headingHeight);
	};
	struct Placement { HWND window; int x, y, width, height; };
	std::vector<Placement> placements;
	placements.reserve(m_blocks.size() * 2);
	int y = inset;
	for(size_t index = 0; index < m_blocks.size();)
	{
		const int type = m_blocks[index].layout;
		int count = type == 1 ? (available >= UiMetrics::ScaleForDpi(590, m_dpi) ? 3 :
			available >= UiMetrics::ScaleForDpi(385, m_dpi) ? 2 : 1) :
			type == 2 && available >= UiMetrics::ScaleForDpi(460, m_dpi) ? 2 : 1;
		std::vector<int> widths;
		if(type == 3)
		{
			const bool first = index == 0 || m_blocks[index - 1].layout != 3;
			const int usable = (std::max)(1, available - 2 * scaleGap);
			const int thirdWidth = usable / 3;
			const int halfWidth = (available - scaleGap) / 2;
			if(first && index + 2 < m_blocks.size() && m_blocks[index + 2].layout == 3 &&
				thirdWidth >= headingWidth(m_blocks[index]) &&
				thirdWidth >= headingWidth(m_blocks[index + 1]) &&
				usable - 2 * thirdWidth >= headingWidth(m_blocks[index + 2]))
			{
				count = 3;
				widths = { thirdWidth, thirdWidth, usable - 2 * thirdWidth };
			}
			else if(first && index + 1 < m_blocks.size() && m_blocks[index + 1].layout == 3 &&
				halfWidth >= headingWidth(m_blocks[index]) &&
				available - scaleGap - halfWidth >= headingWidth(m_blocks[index + 1]))
				count = 2;
		}
		size_t end = index + 1;
		while(end < m_blocks.size() && m_blocks[end].layout == type && end - index < static_cast<size_t>(count)) ++end;
		const int columns = static_cast<int>(end - index);
		const int columnWidth = (available - scaleGap * (columns - 1)) / columns;
		if(widths.size() != static_cast<size_t>(columns))
			widths.assign(static_cast<size_t>(columns), columnWidth);
		int rowBody = 0;
		int rowHeading = headingHeight;
		for(size_t cell = index; cell < end; ++cell)
		{
			const int cellWidth = widths[cell - index];
			rowBody = (std::max)(rowBody, contentHeight(m_blocks[cell], cellWidth));
			if(type == 3)
				rowHeading = (std::max)(rowHeading, measuredHeadingHeight(m_blocks[cell], cellWidth));
		}
		int x = inset;
		for(size_t cell = index; cell < end; ++cell)
		{
			VisualBlock& block = m_blocks[cell];
			const int cellWidth = widths[cell - index];
			placements.push_back({ block.heading, x, y - m_scrollY, cellWidth, rowHeading });
			placements.push_back({ block.body, x, y + rowHeading - m_scrollY, cellWidth,
				type == 3 ? contentHeight(block, cellWidth) : rowBody });
			x += cellWidth + scaleGap;
		}
		y += rowHeading + rowBody + scaleGap;
		index = end;
	}
	if(dc) { if(previous) ::SelectObject(dc, previous); ::ReleaseDC(m_viewport, dc); }
	m_contentHeight = y + inset;
	const int maximum = (std::max)(0, m_contentHeight - height);
	if(m_scrollY > maximum) {
		const int difference = m_scrollY - maximum;
		m_scrollY = maximum;
		for(Placement& placement : placements) placement.y += difference;
	}
	SCROLLINFO scroll = { sizeof(scroll), SIF_PAGE | SIF_RANGE | SIF_POS };
	scroll.nMin = 0;
	scroll.nMax = (std::max)(0, m_contentHeight - 1);
	scroll.nPage = static_cast<UINT>((std::max)(0, height));
	scroll.nPos = m_scrollY;
	::SetScrollInfo(m_viewport, SB_VERT, &scroll, TRUE);
	HDWP deferred = ::BeginDeferWindowPos(static_cast<int>(placements.size()));
	for(const Placement& placement : placements)
	{
		if(!deferred) break;
		deferred = ::DeferWindowPos(deferred, placement.window, NULL, placement.x, placement.y,
			placement.width, placement.height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
	}
	if(!deferred || !::EndDeferWindowPos(deferred))
		for(const Placement& placement : placements)
			::SetWindowPos(placement.window, NULL, placement.x, placement.y, placement.width, placement.height,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
	for(const VisualBlock& block : m_blocks)
		if(block.settings && ::IsWindow(block.body))
		{
			RECT client = {};
			::GetClientRect(block.body, &client);
			const int padding = UiMetrics::ScaleForDpi(10, m_dpi);
			RECT interior = { padding, padding, (std::max)(padding + 1, static_cast<int>(client.right) - padding),
				(std::max)(padding + 1, static_cast<int>(client.bottom) - padding) };
			::SendMessageW(block.body, EM_SETRECT, 0, reinterpret_cast<LPARAM>(&interior));
		}
	::RedrawWindow(m_viewport, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

int CScriptCardDlg::CharacteristicLayoutRows() const
{
	RECT rects[3] = {};
	int count = 0;
	for(const VisualBlock& block : m_blocks)
		if(block.layout == 3)
		{
			if(count >= 3 || !::IsWindow(block.heading) || !::GetWindowRect(block.heading, &rects[count])) return -1;
			++count;
		}
	if(count != 3) return -1;
	const int widths[] = { rects[0].right - rects[0].left,
		rects[1].right - rects[1].left, rects[2].right - rects[2].left };
	if(rects[0].top == rects[1].top && rects[1].top == rects[2].top)
		return abs(widths[0] - widths[1]) <= 1 && abs(widths[1] - widths[2]) <= 1 ? 1 : -1;
	if(rects[0].top == rects[1].top && rects[1].top < rects[2].top)
		return abs(widths[0] - widths[1]) <= 1 && widths[2] > widths[0] ? 2 : -1;
	return rects[0].top < rects[1].top && rects[1].top < rects[2].top ? 3 : -1;
}

void CScriptCardDlg::ScrollTo(int offset)
{
	RECT bounds = {};
	::GetClientRect(m_viewport, &bounds);
	const int viewportHeight = static_cast<int>(bounds.bottom - bounds.top);
	const int clamped = (std::max)(0, (std::min)(offset, m_contentHeight - viewportHeight));
	if(clamped == m_scrollY) return;
	m_scrollY = clamped;
	LayoutVisualBlocks(bounds.right - bounds.left, bounds.bottom - bounds.top);
}

bool CScriptCardDlg::SettingsLastLineFits() const
{
	bool found = false;
	RECT viewport = {};
	if (!m_viewport || !::GetWindowRect(m_viewport, &viewport)) return false;
	for (const VisualBlock& block : m_blocks)
	{
		if (!block.settings || !::IsWindow(block.body)) continue;
		found = true;
		const int length = ::GetWindowTextLengthW(block.body);
		if (length <= 0) return false;
		POINTL point = { -1, -1 };
		::SendMessageW(block.body, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&point), length - 1);
		RECT format = {}; ::SendMessageW(block.body, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&format));
		HDC dc = ::GetDC(block.body);
		if (!dc) return false;
		HFONT font = reinterpret_cast<HFONT>(::SendMessageW(block.body, WM_GETFONT, 0, 0));
		HGDIOBJ previous = font ? ::SelectObject(dc, font) : NULL;
		TEXTMETRICW metrics = {}; const bool measured = ::GetTextMetricsW(dc, &metrics) != FALSE;
		if (previous) ::SelectObject(dc, previous);
		::ReleaseDC(block.body, dc);
		RECT bodyBounds = {};
		if (!measured || !::GetWindowRect(block.body, &bodyBounds) || point.y < 0 ||
			point.y + metrics.tmHeight + metrics.tmExternalLeading > format.bottom ||
			bodyBounds.top + point.y < viewport.top ||
			bodyBounds.top + point.y + metrics.tmHeight + metrics.tmExternalLeading > viewport.bottom) return false;
	}
	return found;
}

void CScriptCardDlg::HandleViewportScroll(WPARAM command)
{
	SCROLLINFO info = { sizeof(info), SIF_TRACKPOS };
	::GetScrollInfo(m_viewport, SB_VERT, &info);
	const int step = UiMetrics::ScaleForDpi(40, m_dpi);
	int target = m_scrollY;
	RECT bounds = {};
	::GetClientRect(m_viewport, &bounds);
	const int page = (std::max)(1, static_cast<int>(bounds.bottom - bounds.top));
	switch(LOWORD(command))
	{
	case SB_LINEUP: target -= step; break;
	case SB_LINEDOWN: target += step; break;
	case SB_PAGEUP: target -= page; break;
	case SB_PAGEDOWN: target += page; break;
	case SB_TOP: target = 0; break;
	case SB_BOTTOM: target = m_contentHeight; break;
	case SB_THUMBTRACK: target = info.nTrackPos; break;
	case SB_THUMBPOSITION:
		// nTrackPos retains the full 32-bit value while tracking; for a posted
		// position without an active tracker, Windows supplies the 16-bit value
		// in the message itself.
		target = (static_cast<UINT>(info.nTrackPos) & 0xffffU) == HIWORD(command)
			? info.nTrackPos : static_cast<int>(HIWORD(command));
		break;
	default: break;
	}
	ScrollTo(target);
}

LRESULT CScriptCardDlg::OnViewportWheel(UINT, WPARAM wParam, LPARAM, BOOL&)
{
	ScrollTo(m_scrollY - GET_WHEEL_DELTA_WPARAM(wParam) * UiMetrics::ScaleForDpi(80, m_dpi) / WHEEL_DELTA);
	return 0;
}

void CScriptCardDlg::ApplyRichFormatting()
{
	if(!IsWindow() || !m_font) return;
	LOGFONTW face = {};
	if(!::GetObjectW(m_font, sizeof(face), &face)) return;
	const LONG baseTwips = (std::max)(160L, static_cast<LONG>(::MulDiv(abs(face.lfHeight), 1440, m_dpi)));
	const COLORREF warningColor = ThemeManager::IsHighContrast() ? ::GetSysColor(COLOR_WINDOWTEXT) :
		ThemeManager::WarningColor();
	auto apply = [&](HWND rich, const std::vector<SectionRange>& spans, bool header) {
		if(!::IsWindow(rich)) return;
		POINT scroll = {}; ::SendMessageW(rich, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
		CHARRANGE selection = {}; ::SendMessageW(rich, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
		::SendMessageW(rich, WM_SETREDRAW, FALSE, 0);
		CHARFORMAT2W normal = {};
		normal.cbSize = sizeof(normal); normal.dwMask = CFM_BOLD | CFM_SIZE;
		normal.yHeight = baseTwips;
		::SendMessageW(rich, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&normal));
		for(const SectionRange& span : spans)
		{
			CHARRANGE range = { span.begin, span.end };
			::SendMessageW(rich, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
			CHARFORMAT2W heading = {};
			heading.cbSize = sizeof(heading);
			heading.dwMask = CFM_BOLD | CFM_SIZE | CFM_COLOR;
			heading.dwEffects = CFE_BOLD;
			heading.yHeight = baseTwips + (header ? 100 : 20);
			heading.crTextColor = span.warning ? warningColor :
				ThemeManager::IsHighContrast() ? ::GetSysColor(COLOR_WINDOWTEXT) : ThemeManager::TextColor();
			::SendMessageW(rich, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&heading));
		}
		::SendMessageW(rich, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
		::SendMessageW(rich, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
		::SendMessageW(rich, WM_SETREDRAW, TRUE, 0);
		::InvalidateRect(rich, NULL, FALSE);
	};
	std::vector<SectionRange> title;
	const int titleEnd = m_headerText.Find(L"\r\n");
	if(!m_headerText.IsEmpty()) title.push_back({ 0, titleEnd >= 0 ? titleEnd : m_headerText.GetLength(), false });
	apply(GetDlgItem(IDC_SCRIPT_CARD_HEADER), title, true);
	apply(GetDlgItem(IDC_SCRIPT_CARD_BODY), m_sections, false);
}

void CScriptCardDlg::Layout()
{
	if(!m_viewport || !GetDlgItem(IDC_SCRIPT_CARD_BODY)) return;
	::SetWindowPos(GetDlgItem(IDC_SCRIPT_CARD_BODY), NULL, 0, 0, 0, 0,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOREDRAW);
	RECT client = {}; GetClientRect(&client);
	const int clientWidth = static_cast<int>(client.right);
	const int clientHeight = static_cast<int>(client.bottom);
	const int margin = UiMetrics::ScaleForDpi(12, m_dpi);
	const int gap = UiMetrics::ScaleForDpi(8, m_dpi);
	const int iconSide = UiMetrics::ScaleForDpi(44, m_dpi);
	const int buttonHeight = UiMetrics::ScaleForDpi(30, m_dpi);
	int widths[4] = {};
	const int ids[] = { IDC_SCRIPT_CARD_COPY, IDC_SCRIPT_CARD_OPEN, IDC_SCRIPT_CARD_RUN, IDCANCEL };
	HDC dc = ::GetDC(m_hWnd);
	HFONT oldFont = dc && m_font ? reinterpret_cast<HFONT>(::SelectObject(dc, m_font)) : NULL;
	for(int index = 0; index < 4; ++index)
	{
		wchar_t label[256] = {};
		::GetWindowTextW(GetDlgItem(ids[index]), label, _countof(label));
		SIZE size = {};
		if(dc) ::GetTextExtentPoint32W(dc, label, static_cast<int>(wcslen(label)), &size);
		widths[index] = max(UiMetrics::ScaleForDpi(76, m_dpi), size.cx + UiMetrics::ScaleForDpi(28, m_dpi));
	}
	const int usable = (std::max)(0, clientWidth - 2 * margin);
	const bool splitHeader = usable >= UiMetrics::ScaleForDpi(570, m_dpi);
	const int leftWidth = splitHeader ? (usable * 62 / 100) - iconSide - gap : usable - iconSide - gap;
	const int rightWidth = splitHeader ? usable - iconSide - gap - leftWidth - gap : usable - iconSide - gap;
	RECT titleMeasure = { 0, 0, (std::max)(1, leftWidth), 0 };
	RECT detailsMeasure = { 0, 0, (std::max)(1, leftWidth), 0 };
	RECT rightMeasure = { 0, 0, (std::max)(1, rightWidth), 0 };
	if(dc) {
		LOGFONTW titleFont = {};
		if(m_font) ::GetObjectW(m_font, sizeof(titleFont), &titleFont);
		titleFont.lfHeight = titleFont.lfHeight * 14 / 9;
		titleFont.lfWeight = FW_SEMIBOLD;
		HFONT large = ::CreateFontIndirectW(&titleFont);
		HFONT beforeTitle = large ? reinterpret_cast<HFONT>(::SelectObject(dc, large)) : NULL;
		::DrawTextW(dc, m_script.name, -1, &titleMeasure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
		if(beforeTitle) ::SelectObject(dc, beforeTitle);
		if(large) ::DeleteObject(large);
		CString details = m_headerText.Mid(m_script.name.GetLength());
		if(details.Left(2) == L"\r\n") details = details.Mid(2);
		if(!details.IsEmpty()) ::DrawTextW(dc, details, -1, &detailsMeasure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
		::DrawTextW(dc, m_headerRightText, -1, &rightMeasure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
	}
	if(dc) { if(oldFont) ::SelectObject(dc, oldFont); ::ReleaseDC(m_hWnd, dc); }
	const int textPadding = UiMetrics::ScaleForDpi(10, m_dpi);
	const int leftHeight = (std::max)(iconSide, static_cast<int>(titleMeasure.bottom + detailsMeasure.bottom) + textPadding);
	const int rightHeight = m_headerRightText.IsEmpty() ? 0 : static_cast<int>(rightMeasure.bottom) + textPadding * 2;
	const int headerHeight = splitHeader ? (std::max)(leftHeight, rightHeight) : leftHeight +
		(rightHeight ? gap + rightHeight : 0);
	const int buttonTop = clientHeight - margin - buttonHeight;
	const int viewportTop = margin + headerHeight + gap;
	const int viewportHeight = (std::max)(0, buttonTop - gap - viewportTop);
	const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW;
	struct Move { HWND window; int x, y, width, height; };
	std::vector<Move> moves;
	moves.reserve(10);
	moves.push_back({ GetDlgItem(IDC_SCRIPT_CARD_ICON), margin, margin, iconSide, iconSide });
	moves.push_back({ GetDlgItem(IDC_SCRIPT_CARD_HEADER), margin + iconSide + gap, margin, leftWidth, leftHeight });
	moves.push_back({ m_headerRight, splitHeader ? margin + iconSide + gap + leftWidth + gap :
		margin + iconSide + gap, splitHeader ? margin : margin + leftHeight + gap, rightWidth, rightHeight });
	moves.push_back({ m_viewport, margin, viewportTop, usable, viewportHeight });
	int x = clientWidth - margin;
	for(int index = 3; index >= 0; --index)
	{
		x -= widths[index];
		moves.push_back({ GetDlgItem(ids[index]), x, buttonTop, widths[index], buttonHeight });
		x -= gap;
	}
	if(m_warning)
	{
		const int warningWidth = (std::max)(0, x - margin);
		SIZE warningSize = {};
		if(!m_warningText.IsEmpty())
		{
			HDC measure = ::GetDC(m_hWnd);
			HFONT previous = measure && m_font ? reinterpret_cast<HFONT>(::SelectObject(measure, m_font)) : NULL;
			if(measure) ::GetTextExtentPoint32W(measure, m_warningText, m_warningText.GetLength(), &warningSize);
			if(measure) { if(previous) ::SelectObject(measure, previous); ::ReleaseDC(m_hWnd, measure); }
		}
		::SetWindowTextW(m_warning, warningSize.cx + UiMetrics::ScaleForDpi(4, m_dpi) <= warningWidth ?
			m_warningText.GetString() : L"⚠");
		moves.push_back({ m_warning, margin, buttonTop, warningWidth, buttonHeight });
		::ShowWindow(m_warning, m_warningText.IsEmpty() ? SW_HIDE : SW_SHOWNA);
	}
	HDWP positions = ::BeginDeferWindowPos(static_cast<int>(moves.size()));
	for(const Move& move : moves)
	{
		if(!positions) break;
		positions = ::DeferWindowPos(positions, move.window, NULL, move.x, move.y,
			move.width, move.height, flags);
	}
	if(!positions || !::EndDeferWindowPos(positions))
		for(const Move& move : moves)
			::SetWindowPos(move.window, NULL, move.x, move.y, move.width, move.height, flags);
	RECT viewport = {};
	::GetClientRect(m_viewport, &viewport);
	LayoutVisualBlocks(viewport.right - viewport.left, viewport.bottom - viewport.top);
	::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

LRESULT CScriptCardDlg::OnSize(UINT, WPARAM, LPARAM, BOOL&) { Layout(); return 0; }

LRESULT CScriptCardDlg::OnMinMax(UINT, WPARAM, LPARAM parameter, BOOL&)
{
	MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(parameter);
	MONITORINFO monitor = { sizeof(monitor) };
	if(::GetMonitorInfoW(::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &monitor))
	{
		info->ptMinTrackSize.x = min(UiMetrics::ScaleForDpi(620, m_dpi), monitor.rcWork.right - monitor.rcWork.left);
		info->ptMinTrackSize.y = min(UiMetrics::ScaleForDpi(440, m_dpi), monitor.rcWork.bottom - monitor.rcWork.top);
	}
	return 0;
}

LRESULT CScriptCardDlg::OnDpiChanged(UINT, WPARAM wParam, LPARAM parameter, BOOL&)
{
	m_dpi = LOWORD(wParam);
	const RECT* suggested = reinterpret_cast<const RECT*>(parameter);
	if(suggested != NULL)
		SetWindowPos(NULL, suggested->left, suggested->top, suggested->right - suggested->left,
			suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
	ApplyFont();
	ApplyScriptIcon();
	ApplyWindowIcon();
	Layout();
	return 0;
}

LRESULT CScriptCardDlg::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&)
{
	ApplyBodyPalette();
	::InvalidateRect(m_hWnd, NULL, TRUE);
	return 0;
}

LRESULT CScriptCardDlg::OnControlColor(UINT, WPARAM dc, LPARAM window, BOOL& handled)
{
	if(reinterpret_cast<HWND>(window) != m_warning) { handled = FALSE; return 0; }
	::SetBkMode(reinterpret_cast<HDC>(dc), TRANSPARENT);
	::SetTextColor(reinterpret_cast<HDC>(dc), ThemeManager::IsHighContrast() ?
		::GetSysColor(COLOR_WINDOWTEXT) : ThemeManager::WarningColor());
	return reinterpret_cast<LRESULT>(ThemeManager::WindowBrush());
}

LRESULT CScriptCardDlg::OnTimer(UINT, WPARAM timer, LPARAM, BOOL&)
{
	if(timer != kCardTimer || !m_work || !m_work->done.load(std::memory_order_acquire)) return 0;
	KillTimer(kCardTimer);
	ShowCard(m_work->card);
	m_work.reset();
	return 0;
}

void CScriptCardDlg::ShowCard(const ScriptCards::Card& card)
{
	m_lastCard = card;
	CString header = m_script.name;
	const CString category = ScriptCategory(m_script.relativePath);
	if(!category.IsEmpty()) header += L"\r\n" + Label(L"fbe.script_card.category", L"Category") + L": " + category;
	if(!card.version.IsEmpty() && card.version != L"Не указана") header += L"\r\n" + Label(L"fbe.script_card.version", L"Version") + L": " + card.version;
	m_headerRightText.Empty();
	bool hasAuthor = false;
	bool unknownAuthor = false;
	for(size_t index = 0; index < card.authors.size(); ++index)
	{
		const ScriptCards::Author& author = card.authors[index];
		unknownAuthor = unknownAuthor || author.role == L"unknown";
		if(author.role == L"author" || author.role == L"original_author" || author.role == L"coauthor")
			hasAuthor = hasAuthor || !author.name.IsEmpty();
		if(author.name.IsEmpty() || author.role == L"unknown") continue;
		bool alreadyShown = false;
		for(size_t previous = 0; previous < index; ++previous)
			if(card.authors[previous].name.CompareNoCase(author.name) == 0) alreadyShown = true;
		if(alreadyShown) continue;
		CString roles;
		std::vector<CString> seenRoles;
		for(const ScriptCards::Author& participant : card.authors)
		{
			if(participant.name.CompareNoCase(author.name) != 0) continue;
			if(std::find(seenRoles.begin(), seenRoles.end(), participant.role) != seenRoles.end()) continue;
			seenRoles.push_back(participant.role);
			if(!roles.IsEmpty()) roles += L", ";
			roles += AuthorRoleLabel(participant.role);
		}
		if(!m_headerRightText.IsEmpty()) m_headerRightText += L"\r\n";
		m_headerRightText += author.name + L" — " + roles;
	}
	if(!hasAuthor || unknownAuthor || card.authorshipStatus == L"not_stated")
	{
		if(!m_headerRightText.IsEmpty()) m_headerRightText += L"\r\n";
		m_headerRightText += AuthorRoleLabel(L"unknown");
	}
	CString state;
	switch(card.state)
	{
	case ScriptCards::State::Missing: state = Label(L"fbe.script_card.missing", L"Detailed information about this script is not available yet."); break;
	case ScriptCards::State::Invalid: state = Label(L"fbe.script_card.invalid", L"The embedded description is invalid."); break;
	case ScriptCards::State::Unchecked: break;
	case ScriptCards::State::Current: break;
	case ScriptCards::State::Changed: state = Label(L"fbe.script_card.changed", L"⚠ Code changed. Description may be outdated"); break;
	}
	m_headerText = header;
	SetDlgItemText(IDC_SCRIPT_CARD_HEADER, m_headerText);
	m_statusText = card.state == ScriptCards::State::Changed ? state : CString();
	m_warningText = m_statusText;
	if(m_warningTip && m_warning)
	{
		TOOLINFOW tip = { sizeof(tip) };
		tip.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		tip.hwnd = m_hWnd;
		tip.uId = reinterpret_cast<UINT_PTR>(m_warning);
		tip.lpszText = const_cast<LPWSTR>(m_warningText.GetString());
		::SendMessageW(m_warningTip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&tip));
	}
	m_originText.Empty();
	if(m_headerRight) ::SetWindowTextW(m_headerRight, m_headerRightText);
	m_bodyText.Empty();
	m_sections.clear();
	for(VisualBlock& block : m_blocks) {
		if(block.heading) ::DestroyWindow(block.heading);
		if(block.body) ::DestroyWindow(block.body);
	}
	m_blocks.clear();
	if(card.state == ScriptCards::State::Missing || card.state == ScriptCards::State::Invalid)
	{
		m_bodyText = state;
		if(!card.error.IsEmpty()) m_bodyText += L"\r\n" + card.error;
		AddVisualBlock(Label(L"fbe.script_card.description", L"Description"), m_bodyText);
	}
	else
	{
		AddVisualBlock(Label(L"fbe.script_card.summary", L"Purpose"), card.summary);
		AddVisualBlock(Label(L"fbe.script_card.description", L"Description"), card.description);
		AddVisualBlock(Label(L"fbe.script_card.usage", L"How to use"), NumberedText(card.usage));
		AddVisualBlock(Label(L"fbe.script_card.modifies_document", L"Changes document"), TriState(card.modifiesDocument), 3);
		AddVisualBlock(Label(L"fbe.script_card.requires_selection", L"Selection required"), TriState(card.requiresSelection), 3);
		CString count;
		if(card.dependencyState == ScriptCards::Card::DependenciesState::Changed ||
			card.dependencyState == ScriptCards::Card::DependenciesState::Invalid)
			count = Label(L"fbe.script_card.dependencies_changed", L"Linked files changed or are missing");
		else if(!card.dependenciesComplete) count = Label(L"fbe.script_card.dependencies_unverified", L"Not verified");
		else count.Format(L"%u", static_cast<unsigned>(card.dependencies.size()));
		AddVisualBlock(Label(L"fbe.script_card.dependencies", L"Linked files"), count, 3);
		AddVisualBlock(Label(L"fbe.script_card.scope", L"Scope"), card.scope);
		if(card.modifiesDocument != 0)
		{
			CString undo = card.undo;
			if(card.undoStatus == L"verified_single_action")
				undo = Label(L"fbe.script_card.undo_single_action",
					L"Changes can be undone with one action (Ctrl+Z)");
			AddVisualBlock(Label(L"fbe.script_card.undo", L"Undo"), undo);
		}
		AddVisualBlock(Label(L"fbe.script_card.warnings", L"Warnings"), ListText(card.warnings), 2, true);
		AddVisualBlock(Label(L"fbe.script_card.examples", L"Examples"), ListText(card.examples), 2);
		AddSettingsBlock(card.settingRows);
		CString dependencies;
		for(const ScriptCards::Dependency& dependency : card.dependencies)
		{
			if(!dependencies.IsEmpty()) dependencies += L"\r\n";
			dependencies += dependency.state == ScriptCards::Dependency::State::Current ? L"✓ " :
				dependency.state == ScriptCards::Dependency::State::Changed ||
				dependency.state == ScriptCards::Dependency::State::Missing ||
				dependency.state == ScriptCards::Dependency::State::Invalid ? L"⚠ " : L"? ";
			dependencies += dependency.path;
		}
		AddVisualBlock(Label(L"fbe.script_card.dependencies", L"Linked files"), dependencies);
		AddVisualBlock(Label(L"fbe.script_card.changelog", L"Changes"), ListText(card.changelog));
		AddVisualBlock(Label(L"fbe.script_card.links", L"Links"), ListText(card.links));
		AddVisualBlock(Label(L"fbe.script_card.min_version", L"Minimum FBE version"), card.minFbeVersion);
	}
	SetDlgItemText(IDC_SCRIPT_CARD_BODY, m_bodyText);
	RebuildVisualBlocks();
	Layout();
}

CString CScriptCardDlg::PlacementPath() const
{
	return U::GetSettingsDir() + L"ScriptCard.ini";
}

void CScriptCardDlg::RestorePlacement()
{
	const CString path = PlacementPath();
	RECT current = {}; GetWindowRect(&current);
	const int width = ::GetPrivateProfileIntW(L"Window", L"Width", 0, path);
	const int height = ::GetPrivateProfileIntW(L"Window", L"Height", 0, path);
	if(width <= 0 || height <= 0) return;
	const UINT savedDpi = (std::max)(static_cast<UINT>(96), ::GetPrivateProfileIntW(L"Window", L"Dpi", 96, path));
	const int left = ::GetPrivateProfileIntW(L"Window", L"Left", current.left, path);
	const int top = ::GetPrivateProfileIntW(L"Window", L"Top", current.top, path);
	const int scaledWidth = ::MulDiv(width, m_dpi, savedDpi);
	const int scaledHeight = ::MulDiv(height, m_dpi, savedDpi);
	RECT desired = { left, top, left + scaledWidth, top + scaledHeight };
	MONITORINFO monitor = { sizeof(monitor) };
	if(!::GetMonitorInfoW(::MonitorFromRect(&desired, MONITOR_DEFAULTTONEAREST), &monitor)) return;
	const int boundedWidth = min(max(scaledWidth, UiMetrics::ScaleForDpi(570, m_dpi)), monitor.rcWork.right - monitor.rcWork.left);
	const int boundedHeight = min(max(scaledHeight, UiMetrics::ScaleForDpi(390, m_dpi)), monitor.rcWork.bottom - monitor.rcWork.top);
	const int x = min(max(left, monitor.rcWork.left), monitor.rcWork.right - boundedWidth);
	const int y = min(max(top, monitor.rcWork.top), monitor.rcWork.bottom - boundedHeight);
	SetWindowPos(NULL, x, y, boundedWidth, boundedHeight, SWP_NOZORDER | SWP_NOACTIVATE);
}

void CScriptCardDlg::SavePlacement()
{
	RECT bounds = {}; GetWindowRect(&bounds);
	const CString path = PlacementPath();
	const struct { LPCWSTR key; int value; } values[] = {
		{ L"Left", bounds.left }, { L"Top", bounds.top },
		{ L"Width", bounds.right - bounds.left }, { L"Height", bounds.bottom - bounds.top },
		{ L"Dpi", static_cast<int>(m_dpi) }
	};
	for(const auto& value : values)
	{
		CString text; text.Format(L"%d", value.value);
		::WritePrivateProfileStringW(L"Window", value.key, text, path);
	}
}

LRESULT CScriptCardDlg::OnClose(UINT, WPARAM, LPARAM, BOOL&)
{
	SavePlacement();
	ShowWindow(SW_HIDE);
	return 0;
}

LRESULT CScriptCardDlg::OnCloseCommand(WORD, WORD, HWND, BOOL&)
{
	SendMessage(WM_CLOSE);
	return 0;
}

LRESULT CScriptCardDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
	KillTimer(kCardTimer);
	m_work.reset();
	if(m_viewport && ::IsWindow(m_viewport)) ::RemoveWindowSubclass(m_viewport, ViewportSubclass, 1);
	::RemovePropW(m_hWnd, L"FBE_SKIP_SYSTEM_DIALOG_LOCALIZATION");
	::SendMessageW(GetDlgItem(IDC_SCRIPT_CARD_ICON), STM_SETICON, 0, 0);
	if(m_icon != NULL) { ::DestroyIcon(m_icon); m_icon = NULL; }
	::SendMessageW(m_hWnd, WM_SETICON, ICON_SMALL, 0);
	::SendMessageW(m_hWnd, WM_SETICON, ICON_BIG, 0);
	if(m_windowIconSmall) { ::DestroyIcon(m_windowIconSmall); m_windowIconSmall = NULL; }
	if(m_windowIconBig) { ::DestroyIcon(m_windowIconBig); m_windowIconBig = NULL; }
	if(m_font != NULL) { ::DeleteObject(m_font); m_font = NULL; }
	return 0;
}

LRESULT CScriptCardDlg::OnRun(WORD, WORD, HWND, BOOL&)
{
	if(m_runScript && !m_script.uid.IsEmpty()) m_runScript(m_script.uid);
	return 0;
}

LRESULT CScriptCardDlg::OnOpen(WORD, WORD, HWND, BOOL&)
{
	if(m_script.path.IsEmpty()) return 0;
	const CString parameters = L"\"" + m_script.path + L"\"";
	::ShellExecuteW(m_hWnd, L"open", L"notepad.exe", parameters, NULL, SW_SHOWNORMAL);
	return 0;
}

LRESULT CScriptCardDlg::OnCopy(WORD, WORD, HWND, BOOL&)
{
	CString text(m_headerText);
	if(!m_headerRightText.IsEmpty()) text += L"\r\n" + m_headerRightText;
	if(!m_statusText.IsEmpty()) text += L"\r\n" + m_statusText;
	if(!m_bodyText.IsEmpty()) text += L"\r\n\r\n" + m_bodyText;
	text += L"\r\n\r\n" + Label(L"fbe.script_card.technical_path", L"Script path") + L": " + m_script.path;
	CopyText(text);
	return 0;
}

void CScriptCardDlg::CopyText(const CString& text)
{
	const SIZE_T bytes = static_cast<SIZE_T>(text.GetLength() + 1) * sizeof(wchar_t);
	HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
	if(memory == NULL) return;
	void* target = ::GlobalLock(memory);
	if(target == NULL) { ::GlobalFree(memory); return; }
	memcpy(target, text.GetString(), bytes);
	::GlobalUnlock(memory);
	if(::OpenClipboard(m_hWnd))
	{
		::EmptyClipboard();
		if(::SetClipboardData(CF_UNICODETEXT, memory)) memory = NULL;
		::CloseClipboard();
	}
	if(memory != NULL) ::GlobalFree(memory);
}
