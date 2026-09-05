#include "stdafx.h"
#include "SettingsEditorPage.h"
#include "Settings.h"
#include "RuntimeLocalization.h"
#include "..\\common\\ModernFileDialog.h"
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
	m_backgroundImage = GetDlgItem(IDC_EDITOR_BACKGROUND_IMAGE);
	m_backgroundLayout = GetDlgItem(IDC_EDITOR_BACKGROUND_LAYOUT);
	m_backgroundPreview.SubclassWindow(GetDlgItem(IDC_EDITOR_BACKGROUND_PREVIEW));
	m_customBackgroundPath = _Settings.GetEditorBackgroundCustomPath();
	m_tooltips.Initialize(m_hWnd);
	m_tooltips.Add(m_fonts, L"fbe.settings.tooltip.editor.font", L"Font used in the visual editor.");
	m_tooltips.Add(m_fontSize, L"fbe.settings.tooltip.editor.font_size", L"Font size used in the visual editor.");
	m_tooltips.Add(m_foreground, L"fbe.settings.tooltip.editor.foreground", L"Text color in the visual editor.");
	m_tooltips.Add(m_background, L"fbe.settings.tooltip.editor.background", L"Background color in the visual editor.");
	m_tooltips.Add(m_nbspCharacter, L"fbe.settings.tooltip.editor.nbsp", L"Changes only the visual representation of non-breaking spaces; document content is not changed.");
	m_tooltips.Add(m_backgroundImage, L"fbe.settings.tooltip.editor.background_image", L"An optional local image used behind the editor text.");
	m_tooltips.Add(m_backgroundLayout, L"fbe.settings.tooltip.editor.background_layout", L"How the selected background image is placed.");
	m_tooltips.Add(m_backgroundPreview, L"fbe.settings.tooltip.editor.background_preview", L"Preview of the selected editor background.");
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
	SetText(m_hWnd, IDC_SETTINGS_OTHER_NBSP_LABEL, L"fbe.dialog.idd_setting_other.nbsp_char", L"Display as:");
	SetText(m_hWnd, IDC_EDITOR_BACKGROUND_GROUP, L"fbe.settings.editor_background.group", L"Editor background");
	SetText(m_hWnd, IDC_EDITOR_BACKGROUND_IMAGE_LABEL, L"fbe.settings.editor_background.image", L"Background image:");
	SetText(m_hWnd, IDC_EDITOR_BACKGROUND_BROWSE, L"fbe.settings.editor_background.browse", L"Browse...");
	SetText(m_hWnd, IDC_EDITOR_BACKGROUND_LAYOUT_LABEL, L"fbe.settings.editor_background.layout", L"Layout:");

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
	int nbspIndex = m_nbspCharacter.SelectString(0, _Settings.GetNBSPChar());
	if(nbspIndex == CB_ERR) nbspIndex = m_nbspCharacter.AddString(_Settings.GetNBSPChar());
	m_nbspCharacter.SetCurSel(nbspIndex);
	m_backgroundImage.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.editor_background.none", L"None (color only)"));
	EditorBackgrounds::Load(m_builtInBackgrounds);
	for(size_t i = 0; i < m_builtInBackgrounds.size(); ++i)
		m_backgroundImage.AddString(FbeLoadRuntimeStringByKey(m_builtInBackgrounds[i].localizationKey, m_builtInBackgrounds[i].name));
	m_backgroundImage.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.editor_background.custom", L"Custom image"));
	int backgroundIndex = 0;
	if(_Settings.GetEditorBackgroundKind() == L"builtin") for(size_t i = 0; i < m_builtInBackgrounds.size(); ++i) if(m_builtInBackgrounds[i].id == _Settings.GetEditorBackgroundId()) { backgroundIndex = static_cast<int>(i + 1); break; }
	else if(_Settings.GetEditorBackgroundKind() == L"custom") backgroundIndex = static_cast<int>(m_builtInBackgrounds.size() + 1);
	m_backgroundImage.SetCurSel(backgroundIndex);
	m_backgroundLayout.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.editor_background.tile", L"Tile"));
	m_backgroundLayout.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.editor_background.center", L"Center"));
	m_backgroundLayout.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.editor_background.contain", L"Contain"));
	m_backgroundLayout.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.editor_background.cover", L"Cover"));
	const CString layout = _Settings.GetEditorBackgroundLayout();
	m_backgroundLayout.SetCurSel(layout == L"center" ? 1 : layout == L"contain" ? 2 : layout == L"cover" ? 3 : 0);
	UpdateBackgroundPreview();
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
	if(U::GetWindowText(m_nbspCharacter).IsEmpty()) { MessageBeep(MB_ICONERROR); m_nbspCharacter.SetFocus(); return false; }
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
	const int backgroundIndex = m_backgroundImage.GetCurSel();
	if(backgroundIndex <= 0) _Settings.SetEditorBackgroundKind(L"none");
	else if(backgroundIndex <= static_cast<int>(m_builtInBackgrounds.size())) { _Settings.SetEditorBackgroundKind(L"builtin"); _Settings.SetEditorBackgroundId(m_builtInBackgrounds[backgroundIndex - 1].id); }
	else _Settings.SetEditorBackgroundKind(L"custom");
	_Settings.SetEditorBackgroundCustomPath(m_customBackgroundPath);
	const int layoutIndex = m_backgroundLayout.GetCurSel();
	_Settings.SetEditorBackgroundLayout(layoutIndex == 1 ? L"center" : layoutIndex == 2 ? L"contain" : layoutIndex == 3 ? L"cover" : L"tile");
}

LRESULT CSettingsEditorPage::OnBrowseBackground(WORD, WORD, HWND, BOOL&)
{
	const COMDLG_FILTERSPEC filters[] = { { L"Images (*.png;*.jpg;*.jpeg)", L"*.png;*.jpg;*.jpeg" } };
	ModernFileDialog::Request request; request.title = FbeLoadRuntimeStringByKey(L"fbe.settings.editor_background.choose", L"Choose editor background").GetString();
	request.defaultExtension = L"png"; request.fileMustExist = true; request.pathMustExist = true; request.filters = filters; request.filterCount = _countof(filters);
	const ModernFileDialog::Result result = ModernFileDialog::Show(m_hWnd, request);
	if(result.outcome == ModernFileDialog::Outcome::Accepted && !result.paths.empty()) { m_customBackgroundPath = result.paths.front().c_str(); m_backgroundImage.SetCurSel(static_cast<int>(m_builtInBackgrounds.size() + 1)); UpdateBackgroundPreview(); }
	return 0;
}

LRESULT CSettingsEditorPage::OnBackgroundSelectionChanged(WORD, WORD, HWND, BOOL&) { UpdateBackgroundPreview(); return 0; }
LRESULT CSettingsEditorPage::OnPreviewSettingsChanged(WORD, WORD, HWND, BOOL&) { UpdateBackgroundPreview(); return 0; }

void CSettingsEditorPage::UpdateBackgroundPreview()
{
	CString path; const int index = m_backgroundImage.GetCurSel();
	if(index > 0 && index <= static_cast<int>(m_builtInBackgrounds.size())) EditorBackgrounds::ResolveBuiltIn(m_builtInBackgrounds[index - 1].id, path);
	else if(index == static_cast<int>(m_builtInBackgrounds.size() + 1) && EditorBackgrounds::IsSupportedLocalImage(m_customBackgroundPath)) path = m_customBackgroundPath;
	HBITMAP bitmap = NULL;
	if(!path.IsEmpty()) { CImage image; if(SUCCEEDED(image.Load(path))) bitmap = image.Detach(); }
	CString text = FbeLoadRuntimeStringByKey(L"fbe.settings.editor_background.preview_text", L"Sample editor text\r\nThe quick brown fox.");
	CString sizeText(U::GetWindowText(m_fontSize)); int size = 12; _stscanf(sizeText, L"%d", &size);
	m_backgroundPreview.SetPreview(bitmap, U::GetWindowText(m_fonts), size, m_foreground.GetColor(), m_background.GetColor(), text);
}

void CEditorBackgroundPreview::SetPreview(HBITMAP bitmap, const CString& face, int size, COLORREF foreground, COLORREF background, const CString& text)
{
	if(m_bitmap) ::DeleteObject(m_bitmap); m_bitmap = bitmap;
	if(m_font) ::DeleteObject(m_font); m_font = ::CreateFont(-::MulDiv(size, 96, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
	m_foreground = foreground; m_background = background; m_text = text; Invalidate();
}

LRESULT CEditorBackgroundPreview::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
	CPaintDC dc(m_hWnd); RECT rc; GetClientRect(&rc);
	HBRUSH brush = m_bitmap ? ::CreatePatternBrush(m_bitmap) : ::CreateSolidBrush(m_background);
	::FillRect(dc, &rc, brush); ::DeleteObject(brush);
	::SetBkMode(dc, TRANSPARENT); ::SetTextColor(dc, m_foreground);
	HFONT old = m_font ? static_cast<HFONT>(::SelectObject(dc, m_font)) : NULL;
	RECT text = rc; ::InflateRect(&text, -6, -3); ::DrawText(dc, m_text, -1, &text, DT_LEFT | DT_VCENTER | DT_WORDBREAK);
	if(old) ::SelectObject(dc, old); ::FrameRect(dc, &rc, static_cast<HBRUSH>(::GetStockObject(GRAY_BRUSH))); return 0;
}

LRESULT CEditorBackgroundPreview::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
	if(m_bitmap) { ::DeleteObject(m_bitmap); m_bitmap = NULL; }
	if(m_font) { ::DeleteObject(m_font); m_font = NULL; }
	return 0;
}

LRESULT CSettingsEditorPage::OnClickedCancel(WORD, WORD, HWND, BOOL&) { return 0; }
bool CSettingsEditorPage::CancelChanges() { return true; }
