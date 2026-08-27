#include "stdafx.h"
#include "SettingsEditorPage.h"
#include "Settings.h"
#include "RuntimeLocalization.h"
#include "utils.h"

extern CSettings _Settings;

namespace
{
int __stdcall EnumFontProc(const ENUMLOGFONTEX* logFont, const NEWTEXTMETRICEX*, DWORD, LPARAM data)
{
	static_cast<CSimpleArray<CString>*>(reinterpret_cast<void*>(data))->Add(logFont->elfLogFont.lfFaceName);
	return TRUE;
}

void SetText(HWND window, int controlId, LPCWSTR key, LPCWSTR fallback)
{
	::SetDlgItemText(window, controlId, FbeLoadRuntimeStringByKey(key, fallback));
}
}

LRESULT CSettingsEditorPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_SETTINGS_EDITOR);
	m_foreground.SubclassWindow(GetDlgItem(IDC_FG));
	m_background.SubclassWindow(GetDlgItem(IDC_BG));
	m_fonts = GetDlgItem(IDC_FONT);
	m_fontSize = GetDlgItem(IDC_FONT_SIZE);
	m_nbspCharacter = GetDlgItem(IDC_NBSP_CHAR);
	m_background.SetDefaultColor(::GetSysColor(COLOR_WINDOW));
	m_foreground.SetDefaultColor(::GetSysColor(COLOR_WINDOWTEXT));
	m_background.SetColor(_Settings.GetColorBG());
	m_foreground.SetColor(_Settings.GetColorFG());

	SetText(m_hWnd, IDC_OPTIONS_BODY_GROUP, L"fbe.dialog.idd_options.font_group", L"Editor");
	SetText(m_hWnd, IDC_OPTIONS_BODY_FONT, L"fbe.dialog.idd_options.font", L"Editor font:");
	SetText(m_hWnd, IDC_OPTIONS_FONT_SIZE, L"fbe.dialog.idd_options.font_size", L"Font size:");
	SetText(m_hWnd, IDC_OPTIONS_FOREGROUND_COLOR, L"fbe.dialog.idd_options.foreground_color", L"Text color:");
	SetText(m_hWnd, IDC_OPTIONS_BACKGROUND_COLOR, L"fbe.dialog.idd_options.background_color", L"Background:");
	SetText(m_hWnd, IDC_SETTINGS_OTHER_NBSP, L"fbe.dialog.idd_setting_other.nbsp", L"Non-breaking space");
	SetText(m_hWnd, IDC_SETTINGS_OTHER_NBSP_LABEL, L"fbe.dialog.idd_setting_other.nbsp_char", L"Display character:");

	CSimpleArray<CString> installedFonts;
	HDC display = ::CreateDC(_T("DISPLAY"), NULL, NULL, NULL);
	LOGFONT logFont = {};
	logFont.lfCharSet = DEFAULT_CHARSET;
	::EnumFontFamiliesEx(display, &logFont, reinterpret_cast<FONTENUMPROC>(EnumFontProc), reinterpret_cast<LPARAM>(&installedFonts), 0);
	::DeleteDC(display);
	for(int i = 0; i < installedFonts.GetSize(); ++i) m_fonts.AddString(installedFonts[i]);
	int fontIndex = m_fonts.FindStringExact(0, _Settings.GetFont());
	if(fontIndex < 0) fontIndex = m_fonts.AddString(_Settings.GetFont());
	m_fonts.SetCurSel(fontIndex);
	const int fontSizes[] = { 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72 };
	CString value;
	value.Format(_T("%d"), _Settings.GetFontSize());
	m_fontSize.SetWindowText(value);
	for(int i = 0; i < _countof(fontSizes); ++i) { value.Format(_T("%d"), fontSizes[i]); m_fontSize.AddString(value); }
	m_nbspCharacter.AddString(L"\u25A1");
	m_nbspCharacter.AddString(L"\u25AB");
	m_nbspCharacter.AddString(L"\u25E6");
	m_nbspCharacter.AddString(L"\u00A0");
	m_nbspCharacter.SelectString(0, _Settings.GetNBSPChar());
	return 1;
}

LRESULT CSettingsEditorPage::OnClickedOK(WORD, WORD, HWND, BOOL&)
{
	if(!Validate()) return 0;
	Commit();
	return 0;
}

bool CSettingsEditorPage::Validate()
{
	CString sizeText(U::GetWindowText(m_fontSize));
	int size = 0;
	if(_stscanf(sizeText, _T("%d"), &size) != 1 || size < 6 || size > 72)
	{
		MessageBeep(MB_ICONERROR);
		m_fontSize.SetFocus();
		return false;
	}
	return true;
}

void CSettingsEditorPage::Commit()
{
	CString sizeText(U::GetWindowText(m_fontSize));
	int size = 0;
	_stscanf(sizeText, _T("%d"), &size);
	_Settings.SetColorBG(m_background.GetColor());
	_Settings.SetColorFG(m_foreground.GetColor());
	_Settings.SetFont(U::GetWindowText(m_fonts));
	_Settings.SetFontSize(size);
	CString character;
	m_nbspCharacter.GetWindowText(character);
	_Settings.SetNBSPChar(character);
}

LRESULT CSettingsEditorPage::OnClickedCancel(WORD, WORD, HWND, BOOL&) { return 0; }
bool CSettingsEditorPage::CancelChanges() { return true; }
