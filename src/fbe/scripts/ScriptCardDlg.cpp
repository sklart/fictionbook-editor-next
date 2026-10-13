#include "stdafx.h"
#include "ScriptCardDlg.h"
#include "../RuntimeLocalization.h"
#include "../ThemeManager.h"
#include "../UiMetrics.h"
#include "../utils/utils.h"
#include <richedit.h>
#include <shellapi.h>
#include <thread>

namespace {
constexpr UINT_PTR kCardTimer = 0x5343;

CString Label(LPCWSTR key, LPCWSTR fallback)
{
	return FbeLoadRuntimeStringByKey(key, fallback);
}

void AppendSection(CString& text, const CString& label, const CString& value)
{
	if(value.IsEmpty()) return;
	if(!text.IsEmpty()) text += L"\r\n\r\n";
	text += label + L"\r\n" + value;
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

CString TriState(int value)
{
	return value < 0 ? Label(L"fbe.script_card.unknown", L"Unknown") :
		value ? Label(L"fbe.script_card.yes", L"Yes") : Label(L"fbe.script_card.no", L"No");
}
}

CScriptCardDlg::~CScriptCardDlg()
{
	if(IsWindow()) DestroyWindow();
	if(m_richEdit != NULL) ::FreeLibrary(m_richEdit);
}

bool CScriptCardDlg::ShowScript(HWND owner, const ScriptDescriptor& script, const CString& resourceRoot,
	const std::function<void(const CString&)>& runScript)
{
	if(script.isFolder || script.uid.IsEmpty()) return false;
	m_script = script;
	m_lastCard = ScriptCards::Card();
	m_runScript = runScript;
	if(!IsWindow())
	{
		m_richEdit = ::LoadLibraryW(L"Msftedit.dll");
		if(m_richEdit == NULL || Create(owner) == NULL) return false;
	}
	ApplyScriptIcon();
	RefreshLocalization();
	SetDlgItemText(IDC_SCRIPT_CARD_HEADER, script.name);
	m_bodyText = Label(L"fbe.script_card.loading", L"Checking script description and linked files...");
	SetDlgItemText(IDC_SCRIPT_CARD_BODY, m_bodyText);
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
	}
	Layout();
}

LRESULT CScriptCardDlg::OnInit(UINT, WPARAM, LPARAM, BOOL&)
{
	::SetPropW(m_hWnd, L"FBE_SKIP_SYSTEM_DIALOG_LOCALIZATION", reinterpret_cast<HANDLE>(1));
	ModifyStyle(0, WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_dpi = UiMetrics::DpiForWindow(m_hWnd);
	ApplyFont();
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
	if(m_font != NULL) ::DeleteObject(m_font);
	m_font = font;
}

void CScriptCardDlg::ApplyBodyPalette()
{
	HWND body = GetDlgItem(IDC_SCRIPT_CARD_BODY);
	if(!::IsWindow(body)) return;
	const bool highContrast = ThemeManager::IsHighContrast();
	const COLORREF background = highContrast ? ::GetSysColor(COLOR_WINDOW) : ThemeManager::WindowColor();
	const COLORREF foreground = highContrast ? ::GetSysColor(COLOR_WINDOWTEXT) : ThemeManager::TextColor();
	::SendMessageW(body, EM_SETBKGNDCOLOR, 0, background);
	CHARFORMAT2W format = {};
	format.cbSize = sizeof(format);
	format.dwMask = CFM_COLOR | CFM_BACKCOLOR;
	format.crTextColor = foreground;
	format.crBackColor = background;
	::SendMessageW(body, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
	::SendMessageW(body, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
	::InvalidateRect(body, NULL, TRUE);
}

void CScriptCardDlg::ApplyScriptIcon()
{
	if(!IsWindow()) return;
	const int side = UiMetrics::ScaleForDpi(32, m_dpi);
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

void CScriptCardDlg::Layout()
{
	if(!GetDlgItem(IDC_SCRIPT_CARD_BODY)) return;
	RECT client = {}; GetClientRect(&client);
	const int margin = UiMetrics::ScaleForDpi(12, m_dpi);
	const int gap = UiMetrics::ScaleForDpi(6, m_dpi);
	const int headerHeight = UiMetrics::ScaleForDpi(110, m_dpi);
	const int iconSide = UiMetrics::ScaleForDpi(32, m_dpi);
	const int buttonHeight = UiMetrics::ScaleForDpi(30, m_dpi);
	int widths[4] = {};
	const int ids[] = { IDC_SCRIPT_CARD_RUN, IDC_SCRIPT_CARD_OPEN, IDC_SCRIPT_CARD_COPY, IDCANCEL };
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
	if(dc) { if(oldFont) ::SelectObject(dc, oldFont); ::ReleaseDC(m_hWnd, dc); }
	const int buttonTop = client.bottom - margin - buttonHeight;
	const int bodyTop = margin + headerHeight + gap;
	const int bodyHeight = max(0, buttonTop - gap - bodyTop);
	HDWP positions = ::BeginDeferWindowPos(7);
	if(positions) positions = ::DeferWindowPos(positions, GetDlgItem(IDC_SCRIPT_CARD_ICON), NULL,
		margin, margin, iconSide, iconSide, SWP_NOZORDER | SWP_NOACTIVATE);
	if(positions) positions = ::DeferWindowPos(positions, GetDlgItem(IDC_SCRIPT_CARD_HEADER), NULL,
		margin + iconSide + gap, margin, max(0, client.right - 2 * margin - iconSide - gap), headerHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	if(positions) positions = ::DeferWindowPos(positions, GetDlgItem(IDC_SCRIPT_CARD_BODY), NULL,
		margin, bodyTop, max(0, client.right - 2 * margin), bodyHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	int x = client.right - margin;
	for(int index = 3; index >= 0; --index)
	{
		x -= widths[index];
		if(positions) positions = ::DeferWindowPos(positions, GetDlgItem(ids[index]), NULL,
			x, buttonTop, widths[index], buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
		x -= gap;
	}
	if(positions) ::EndDeferWindowPos(positions);
}

LRESULT CScriptCardDlg::OnSize(UINT, WPARAM, LPARAM, BOOL&) { Layout(); return 0; }

LRESULT CScriptCardDlg::OnMinMax(UINT, WPARAM, LPARAM parameter, BOOL&)
{
	MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(parameter);
	MONITORINFO monitor = { sizeof(monitor) };
	if(::GetMonitorInfoW(::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &monitor))
	{
		info->ptMinTrackSize.x = min(UiMetrics::ScaleForDpi(570, m_dpi), monitor.rcWork.right - monitor.rcWork.left);
		info->ptMinTrackSize.y = min(UiMetrics::ScaleForDpi(390, m_dpi), monitor.rcWork.bottom - monitor.rcWork.top);
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
	Layout();
	return 0;
}

LRESULT CScriptCardDlg::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&)
{
	ApplyBodyPalette();
	::InvalidateRect(m_hWnd, NULL, TRUE);
	return 0;
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
	const int separator = m_script.relativePath.ReverseFind(L'\\');
	if(separator >= 0) header += L"\r\n" + Label(L"fbe.script_card.category", L"Category") + L": " + m_script.relativePath.Left(separator);
	if(!card.version.IsEmpty()) header += L"\r\n" + Label(L"fbe.script_card.version", L"Version") + L": " + card.version;
	CString authorNames;
	for(const ScriptCards::Author& author : card.authors)
	{
		if(!authorNames.IsEmpty()) authorNames += L", ";
		authorNames += author.name.IsEmpty() ? Label(L"fbe.script_card.unknown_author", L"Unknown author") : author.name;
	}
	if(!authorNames.IsEmpty()) header += L"\r\n" + Label(L"fbe.script_card.authors", L"Authors and contributors") + L": " + authorNames;
	CString state;
	switch(card.state)
	{
	case ScriptCards::State::Missing: state = Label(L"fbe.script_card.missing", L"Detailed information about this script is not available yet."); break;
	case ScriptCards::State::Invalid: state = Label(L"fbe.script_card.invalid", L"The embedded description is invalid."); break;
	case ScriptCards::State::Unchecked: state = Label(L"fbe.script_card.unchecked", L"Code revision not checked"); break;
	case ScriptCards::State::Current: state = Label(L"fbe.script_card.current", L"Description matches this code revision"); break;
	case ScriptCards::State::Changed: state = Label(L"fbe.script_card.changed", L"Warning: description may not match the current script"); break;
	}
	if(card.dependencyState == ScriptCards::Card::DependenciesState::Changed)
		state += L"; " + Label(L"fbe.script_card.dependencies_changed", L"linked files changed or are missing");
	else if(card.dependencyState == ScriptCards::Card::DependenciesState::Unverified && card.state != ScriptCards::State::Missing)
		state += L"; " + Label(L"fbe.script_card.dependencies_unverified", L"not all linked files were verified");
	header += L"\r\n" + state;
	SetDlgItemText(IDC_SCRIPT_CARD_HEADER, header);
	m_bodyText.Empty();
	if(card.state == ScriptCards::State::Missing || card.state == ScriptCards::State::Invalid)
	{
		m_bodyText = state;
		if(!card.error.IsEmpty()) m_bodyText += L"\r\n" + card.error;
		m_bodyText += L"\r\n" + m_script.path;
	}
	else
	{
		AppendSection(m_bodyText, Label(L"fbe.script_card.summary", L"Purpose"), card.summary);
		AppendSection(m_bodyText, Label(L"fbe.script_card.description", L"Description"), card.description);
		CString authors;
		for(const ScriptCards::Author& author : card.authors)
		{
			if(!authors.IsEmpty()) authors += L"\r\n";
			authors += author.name.IsEmpty() ? Label(L"fbe.script_card.unknown_author", L"Unknown author") : author.name;
			authors += L" (" + author.role + L")";
		}
		AppendSection(m_bodyText, Label(L"fbe.script_card.authors", L"Authors and contributors"), authors);
		AppendSection(m_bodyText, Label(L"fbe.script_card.usage", L"How to use"), ListText(card.usage));
		AppendSection(m_bodyText, Label(L"fbe.script_card.warnings", L"Warnings"), ListText(card.warnings));
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
		AppendSection(m_bodyText, Label(L"fbe.script_card.dependencies", L"Linked files"), dependencies);
		AppendSection(m_bodyText, Label(L"fbe.script_card.scope", L"Scope"), card.scope);
		AppendSection(m_bodyText, Label(L"fbe.script_card.modifies_document", L"Changes document"), TriState(card.modifiesDocument));
		AppendSection(m_bodyText, Label(L"fbe.script_card.requires_selection", L"Selection required"), TriState(card.requiresSelection));
		AppendSection(m_bodyText, Label(L"fbe.script_card.settings", L"Settings"), ListText(card.settings));
		AppendSection(m_bodyText, Label(L"fbe.script_card.examples", L"Examples"), ListText(card.examples));
		AppendSection(m_bodyText, Label(L"fbe.script_card.undo", L"Undo"), card.undo);
		AppendSection(m_bodyText, Label(L"fbe.script_card.tags", L"Tags"), ListText(card.tags));
		AppendSection(m_bodyText, Label(L"fbe.script_card.changelog", L"Changes"), ListText(card.changelog));
		AppendSection(m_bodyText, Label(L"fbe.script_card.links", L"Links"), ListText(card.links));
		AppendSection(m_bodyText, Label(L"fbe.script_card.min_version", L"Minimum FBE version"), card.minFbeVersion);
		CString revision = Label(L"fbe.script_card.expected_hash", L"Expected SHA-256") + L": " + card.expectedSha256 +
			L"\r\n" + Label(L"fbe.script_card.current_hash", L"Current SHA-256") + L": " + card.currentSha256;
		AppendSection(m_bodyText, Label(L"fbe.script_card.revision", L"Code revision"), revision);
	}
	SetDlgItemText(IDC_SCRIPT_CARD_BODY, m_bodyText);
	ApplyBodyPalette();
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
	::RemovePropW(m_hWnd, L"FBE_SKIP_SYSTEM_DIALOG_LOCALIZATION");
	::SendMessageW(GetDlgItem(IDC_SCRIPT_CARD_ICON), STM_SETICON, 0, 0);
	if(m_icon != NULL) { ::DestroyIcon(m_icon); m_icon = NULL; }
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
	CString text(m_bodyText);
	text += L"\r\n\r\n" + m_script.path;
	const SIZE_T bytes = static_cast<SIZE_T>(text.GetLength() + 1) * sizeof(wchar_t);
	HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
	if(memory == NULL) return 0;
	void* target = ::GlobalLock(memory);
	if(target == NULL) { ::GlobalFree(memory); return 0; }
	memcpy(target, text.GetString(), bytes);
	::GlobalUnlock(memory);
	if(::OpenClipboard(m_hWnd))
	{
		::EmptyClipboard();
		if(::SetClipboardData(CF_UNICODETEXT, memory)) memory = NULL;
		::CloseClipboard();
	}
	if(memory != NULL) ::GlobalFree(memory);
	return 0;
}
