// SettingsNextDlg.cpp : реализация вкладки параметров FBE Next.

#include "stdafx.h"
#include "SettingsSourcePage.h"
#include "Settings.h"
#include "XmlSourceThemes.h"
#include "RuntimeLocalization.h"
#include "utils\CFileDialogEx.h"
#include "apputils.h"

extern CSettings _Settings;

static CString ThemeString(LPCWSTR key, LPCWSTR fallback)
{
	return FbeLoadRuntimeStringByKey(key, fallback);
}

static int __stdcall EnumSourceFontProc(const ENUMLOGFONTEX* font, const NEWTEXTMETRICEX*, DWORD, LPARAM data)
{
	static_cast<CSimpleArray<CString>*>(reinterpret_cast<void*>(data))->Add(font->elfLogFont.lfFaceName);
	return TRUE;
}

static CString GetThemeDisplayName(const XmlSourceThemeInfo& theme)
{
	if(theme.id.CompareNoCase(XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_SYSTEM)) == 0)
		return FbeLoadRuntimeStringByKey(
			L"fbe.dialog.idd_setting_next.source_palette.system", L"Automatic — follow Windows theme");
	if(theme.id.CompareNoCase(XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_FBE_LIGHT)) == 0)
		return FbeLoadRuntimeStringByKey(
			L"fbe.dialog.idd_setting_next.source_palette.contrast", L"Contrast");
	if(theme.id.CompareNoCase(XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_FBE_DARK)) == 0)
		return FbeLoadRuntimeStringByKey(
			L"fbe.dialog.idd_setting_next.source_palette.dark", L"Dark");
	if(theme.id.CompareNoCase(XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_LIGHT)) == 0)
		return FbeLoadRuntimeStringByKey(
			L"fbe.dialog.idd_setting_next.source_palette.high_contrast_light", L"High contrast light");
	if(theme.id.CompareNoCase(XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_DARK)) == 0)
		return FbeLoadRuntimeStringByKey(
			L"fbe.dialog.idd_setting_next.source_palette.high_contrast_dark", L"High contrast dark");

	if(theme.id.CompareNoCase(XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_HISTORICAL)) == 0)
		return FbeLoadRuntimeStringByKey(
			L"fbe.dialog.idd_setting_next.source_palette.classic", L"Classic");
	return theme.name;
}
static const int kSourceColorControls[] = {
	IDC_OPTIONS_SOURCE_COLOR_TEXT,
	IDC_OPTIONS_SOURCE_COLOR_TAG,
	IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE,
	IDC_OPTIONS_SOURCE_COLOR_STRING,
	0, // XML comments remain a theme token, but have no editable UI control.

	IDC_OPTIONS_SOURCE_COLOR_BACKGROUND,
};

static CString GetSelectedThemeId(CComboBox& combo, const std::vector<CString>& themeIds)
{
	const int index = combo.GetCurSel();
	return index >= 0 && index < static_cast<int>(themeIds.size())
		? themeIds[index] : XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_SYSTEM);
}

static CString MakeSafeThemeFileStem(const CString& name)
{
	CString stem(name);
	stem.Trim();
	for(int i = 0; i < stem.GetLength(); ++i)
	{
		if(stem[i] < 0x20 || stem[i] == 0x7f)
		{
			stem.SetAt(i, L'_');
			continue;
		}
		switch(stem[i])
		{
		case L'\\': case L'/': case L':': case L'*': case L'?': case L'"': case L'<': case L'>': case L'|':
			stem.SetAt(i, L'_');
			break;
		}
	}
	while(!stem.IsEmpty() && (stem[stem.GetLength() - 1] == L'.' || stem[stem.GetLength() - 1] == L' '))
		stem.Delete(stem.GetLength() - 1);
	if(stem.IsEmpty()) return CString(L"theme");
	// Windows reserves DOS device names even when an extension is supplied
	// (for example, CON.fbetheme). Check the base component after trimming.
	CString deviceName(stem);
	const int extension = deviceName.Find(L'.');
	if(extension >= 0) deviceName = deviceName.Left(extension);
	deviceName.TrimRight(L". ");
	CString upper(deviceName); upper.MakeUpper();
	static const wchar_t* const reserved[] = { L"CON", L"PRN", L"AUX", L"NUL",
		L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
		L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9" };
	for(int i = 0; i < _countof(reserved); ++i)
		if(upper == reserved[i]) return stem + L"_theme";
	return stem;
}

static DWORD GetThemeDefaultColor(const CString& themeId, XmlSrcColorGroup group)
{
	static const XmlSrcStyleToken tokens[XML_SRC_COLOR_GROUP_COUNT] = {
		XML_SRC_STYLE_XML_TEXT,
		XML_SRC_STYLE_XML_TAG_NAME,
		XML_SRC_STYLE_XML_ATTRIBUTE_NAME,
		XML_SRC_STYLE_XML_ATTRIBUTE_VALUE,
		XML_SRC_STYLE_XML_COMMENT,
		XML_SRC_STYLE_EDITOR_BACKGROUND,
	};
	if(group >= XML_SRC_COLOR_GROUP_COUNT)
		group = XML_SRC_COLOR_TEXT;
	DWORD color = 0;
	if(XmlSourceThemes::GetThemeColor(themeId, tokens[group], color))
		return color;
	return CSettings::GetXmlSrcDefaultColor(
		XmlSourceThemes::GetPaletteForThemeId(themeId), group);
}

static DWORD ResolveSourceColor(const CColorButton& button, const CString& themeId, XmlSrcColorGroup group)
{
	const DWORD color = button.GetColor();
	return color == CLR_DEFAULT ? GetThemeDefaultColor(themeId, group) : color;
}

static DWORD GetThemeTokenColor(const CString& themeId, XmlSrcStyleToken token)
{
	DWORD color = 0;
	if(XmlSourceThemes::GetThemeColor(themeId, token, color))
		return color;
	return CSettings::GetXmlSrcThemeColor(XmlSourceThemes::GetPaletteForThemeId(themeId), token);
}

static DWORD ResolveSourceTokenColor(const CString& themeId, XmlSrcStyleToken token,
	const CColorButton* buttons)
{
	const XmlSrcColorGroup group = CSettings::GetXmlSrcColorGroup(token);
	if(buttons != NULL && group < XML_SRC_COLOR_GROUP_COUNT &&
		buttons[group].GetColor() != CLR_DEFAULT)
		return buttons[group].GetColor();
	return GetThemeTokenColor(themeId, token);
}

static bool IsDarkThemeBackground(DWORD color)
{
	const int luminance = (54 * GetRValue(color) + 183 * GetGValue(color) +
		19 * GetBValue(color)) / 256;
	return luminance < 128;
}

static void FinalizeThemeMetadata(const CString& sourceId, const DWORD* colors,
	bool backgroundWasChanged, bool hasExistingMetadata, XmlSourceThemeMetadata& metadata)
{
	const bool isSystemTheme = sourceId.CompareNoCase(
		XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_SYSTEM)) == 0;
	metadata.recalculateIsDark = backgroundWasChanged || !hasExistingMetadata || isSystemTheme;
	if(metadata.recalculateIsDark)
		metadata.isDark = IsDarkThemeBackground(colors[XML_SRC_STYLE_EDITOR_BACKGROUND]);
	if(!metadata.baseThemeId.IsEmpty()) return;
	metadata.baseThemeId = isSystemTheme ? (metadata.isDark ? L"fbe-dark" : L"fbe-light") : sourceId;
}

static std::vector<wchar_t> MakeThemeFileFilter()
{
	const CString label = ThemeString(L"fbe.theme.file_filter", L"FBE themes (*.fbetheme)");
	const wchar_t* pattern = L"*.fbetheme";
	std::vector<wchar_t> filter;
	filter.insert(filter.end(), label.GetString(), label.GetString() + label.GetLength());
	filter.push_back(L'\0');
	filter.insert(filter.end(), pattern, pattern + wcslen(pattern));
	filter.push_back(L'\0');
	filter.push_back(L'\0');
	return filter;
}

enum XmlSourcePreviewVariant
{
	XML_SOURCE_PREVIEW_FULL,
	XML_SOURCE_PREVIEW_COMPACT,
	XML_SOURCE_PREVIEW_MINIMAL,
	XML_SOURCE_PREVIEW_FALLBACK,
	XML_SOURCE_PREVIEW_NONE,
};

static XmlSourcePreviewVariant SelectSourcePreviewVariant(bool fullFits, bool compactFits,
	bool minimalFits, bool fallbackFits)
{
	if(fullFits) return XML_SOURCE_PREVIEW_FULL;
	if(compactFits) return XML_SOURCE_PREVIEW_COMPACT;
	if(minimalFits) return XML_SOURCE_PREVIEW_MINIMAL;
	return fallbackFits ? XML_SOURCE_PREVIEW_FALLBACK : XML_SOURCE_PREVIEW_NONE;
}
static bool IsHighContrastEnabled()
{
	HIGHCONTRAST highContrast = {};
	highContrast.cbSize = sizeof(highContrast);
	return ::SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(highContrast),
		&highContrast, 0) && (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

LRESULT CSettingsSourcePage::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CSettingsSourcePage>::OnInitDialog(uMsg, wParam, lParam, bHandled);
	m_source_palette = GetDlgItem(IDC_OPTIONS_SOURCE_PALETTE);
	m_srcfonts = GetDlgItem(IDC_SRCFONT);
	CSimpleArray<CString> fonts;
	HDC display = ::CreateDC(_T("DISPLAY"), NULL, NULL, NULL);
	LOGFONT logFont = {}; logFont.lfCharSet = ANSI_CHARSET;
	::EnumFontFamiliesEx(display, &logFont, reinterpret_cast<FONTENUMPROC>(EnumSourceFontProc), reinterpret_cast<LPARAM>(&fonts), 0);
	::DeleteDC(display);
	for(int i = 0; i < fonts.GetSize(); ++i) m_srcfonts.AddString(fonts[i]);
	int sourceFont = m_srcfonts.FindStringExact(0, _Settings.GetSrcFont());
	if(sourceFont < 0) sourceFont = 0;
	m_srcfonts.SetCurSel(sourceFont);
	m_src_wrap = GetDlgItem(IDC_WRAP); m_src_hl = GetDlgItem(IDC_SYNTAXHL); m_src_taghl = GetDlgItem(IDC_TAGHL);
	m_src_eol = GetDlgItem(IDC_SHOWEOL); m_src_whitespace = GetDlgItem(IDC_SHOWWHITESPACE); m_src_line_numbers = GetDlgItem(IDC_SHOWLINENUMBERS);
	m_src_wrap.SetCheck(_Settings.XmlSrcWrap()); m_src_hl.SetCheck(_Settings.XmlSrcSyntaxHL()); m_src_taghl.SetCheck(_Settings.XmlSrcTagHL());
	m_src_eol.SetCheck(_Settings.XmlSrcShowEOL()); m_src_whitespace.SetCheck(_Settings.XmlSrcShowSpace()); m_src_line_numbers.SetCheck(_Settings.XMLSrcShowLineNumbers());
	m_source_palette.SetDroppedWidth(250);
	m_source_tooltips.Create(m_hWnd);
	m_source_tooltips.SetMaxTipWidth(320);
	m_source_tooltips.AddTool(m_source_palette, ThemeString(L"fbe.theme.tooltip.palette", L"Choose a built-in or user source highlighting theme.").GetString());
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_THEME_ACTIONS), ThemeString(L"fbe.theme.tooltip.actions", L"Import, export, save a copy, or delete a user theme.").GetString());
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_TEXT), ThemeString(L"fbe.theme.tooltip.text", L"XML plain text color.").GetString());
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_TAG), ThemeString(L"fbe.theme.tooltip.tag", L"XML tag names and delimiters color.").GetString());
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE), ThemeString(L"fbe.theme.tooltip.attribute", L"XML attribute names color.").GetString());
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_STRING), ThemeString(L"fbe.theme.tooltip.value", L"XML attribute values color.").GetString());
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_BACKGROUND), ThemeString(L"fbe.theme.tooltip.background", L"Source editor and preview background color.").GetString());
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLORS_RESET), ThemeString(L"fbe.theme.tooltip.reset", L"Remove manual overrides and restore the selected theme colors.").GetString());


	const CString paletteText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.source_palette", L"Highlighting theme:");
	const CString sourceCodeText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.source_code", L"Source code");
	const CString backgroundText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.source_color_background", L"Background:");
	if (!sourceCodeText.IsEmpty())
		::SetDlgItemText(m_hWnd, IDC_FBE_NEXT_SOURCE_CODE_GROUP, sourceCodeText);
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_PALETTE_LABEL, paletteText);
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_THEME_ACTIONS,
		ThemeString(L"fbe.theme.actions", L"Themes"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLOR_BACKGROUND_LABEL, backgroundText);
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLOR_TEXT_LABEL,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_color_text",
			L"Plain text:"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLOR_TAG_LABEL,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_color_tag", L"Tags:"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE_LABEL,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_color_attribute",
			L"Attribute names:"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLOR_STRING_LABEL,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_color_string",
			L"Attribute values:"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLORS_RESET,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_colors_reset",
			L"Restore theme colors"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.show_special_characters",
			L"Show invisible characters"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_SPECIAL_CHARS_STYLE_LABEL,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.special_characters_style", L"Style:"));
	m_special_chars_style = GetDlgItem(IDC_OPTIONS_SOURCE_SPECIAL_CHARS_STYLE);
	m_special_chars_style.AddString(FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.special_characters_style.word_like", L"Word-like symbols"));
	m_special_chars_style.AddString(FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.special_characters_style.text_labels", L"Text labels"));
	m_special_chars_style.SetCurSel(static_cast<int>(_Settings.XmlSrcSpecialCharsStyle()));
	::SendMessage(GetDlgItem(IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS), BM_SETCHECK,
		_Settings.XmlSrcShowSpecialChars() ? BST_CHECKED : BST_UNCHECKED, 0);

	const CString currentThemeId = _Settings.GetXmlSrcThemeId();

	ReloadSourceThemes(currentThemeId);

	const CString automaticColorText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.color_automatic", L"Automatic");
	const CString moreColorsText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.color_more", L"More colors...");
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(i == XML_SRC_COLOR_COMMENT) continue;
		m_source_colors[i].SubclassWindow(GetDlgItem(kSourceColorControls[i]));
		m_source_colors[i].SetDefaultText(automaticColorText);
		m_source_colors[i].SetCustomText(moreColorsText);
		m_source_colors[i].SetDefaultColor(GetThemeDefaultColor(currentThemeId,
			static_cast<XmlSrcColorGroup>(i)));
		m_source_colors[i].SetColor(_Settings.HasXmlSrcCustomColor(static_cast<XmlSrcColorGroup>(i)) ? _Settings.GetXmlSrcColor(static_cast<XmlSrcColorGroup>(i)) : CLR_DEFAULT);
		m_source_color_custom[i] = _Settings.HasXmlSrcCustomColor(static_cast<XmlSrcColorGroup>(i));
	}
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	return 1;
}

void CSettingsSourcePage::LoadSourceThemeControlsFromSettings()
{
	const CString activeThemeId = _Settings.GetXmlSrcThemeId();
	ReloadSourceThemes(activeThemeId);
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(i == XML_SRC_COLOR_COMMENT) continue;
		const XmlSrcColorGroup group = static_cast<XmlSrcColorGroup>(i);
		m_source_color_custom[i] = _Settings.HasXmlSrcCustomColor(group);
		m_source_colors[i].SetDefaultColor(GetThemeDefaultColor(activeThemeId, group));
		m_source_colors[i].SetColor(m_source_color_custom[i] ? _Settings.GetXmlSrcColor(group) : CLR_DEFAULT);
	}
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	InvalidateSourcePreview();
}
void CSettingsSourcePage::UpdateSourceColorTooltips()
{
	static const int labels[XML_SRC_COLOR_GROUP_COUNT] = {
		IDC_OPTIONS_SOURCE_COLOR_TEXT_LABEL, IDC_OPTIONS_SOURCE_COLOR_TAG_LABEL,
		IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE_LABEL, IDC_OPTIONS_SOURCE_COLOR_STRING_LABEL,
		0, IDC_OPTIONS_SOURCE_COLOR_BACKGROUND_LABEL,
	};
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(i == XML_SRC_COLOR_COMMENT) continue;
		const DWORD color = ResolveSourceColor(m_source_colors[i],
			GetSelectedThemeId(m_source_palette, m_source_theme_ids),
			static_cast<XmlSrcColorGroup>(i));
		CString label;
		::GetWindowText(GetDlgItem(labels[i]), label.GetBufferSetLength(256), 256);
		label.ReleaseBuffer();
		label.TrimRight(L": ");
		CString colorText;
		if (m_source_colors[i].GetColor() == CLR_DEFAULT)
			colorText = ThemeString(L"fbe.theme.using_theme_colors", L"Uses the selected theme colors.");
		else
			colorText.Format(ThemeString(L"fbe.theme.current_color", L"Current color: #%02X%02X%02X"),
				GetRValue(color), GetGValue(color), GetBValue(color));
		const CString text = label + L". " + colorText;
		m_source_tooltips.UpdateTipText(text.GetString(), GetDlgItem(kSourceColorControls[i]));
	}
}

void CSettingsSourcePage::ReloadSourceThemes(const CString& selectedThemeId)
{
	m_source_palette.ResetContent();
	m_source_theme_ids.clear();
	m_source_theme_display_names.clear();
	m_source_theme_names.clear();
	m_source_theme_is_user.clear();
	int selectedIndex = 0;
	const std::vector<XmlSourceThemeInfo>& themes = XmlSourceThemes::GetAvailableThemes();
	for(size_t i = 0; i < themes.size(); ++i)
	{
		CString displayName(GetThemeDisplayName(themes[i]));
		if(themes[i].isUser)
			displayName += ThemeString(L"fbe.theme.user_suffix", L" (user)");
		CString uniqueName(displayName);
		for(int suffix = 2;; ++suffix)
		{
			bool duplicate = false;
			for(size_t index = 0; index < m_source_theme_display_names.size(); ++index)
				if(m_source_theme_display_names[index].CompareNoCase(uniqueName) == 0) { duplicate = true; break; }
			if(!duplicate) break;
			uniqueName.Format(L"%s (%d)", displayName, suffix);
		}
		const int item = m_source_palette.AddString(uniqueName);
		if(item >= 0)
		{
			m_source_theme_ids.push_back(themes[i].id);
			m_source_theme_display_names.push_back(uniqueName);
			m_source_theme_names.push_back(themes[i].name);
			m_source_theme_is_user.push_back(themes[i].isUser);
			if(themes[i].id.CompareNoCase(selectedThemeId) == 0)
				selectedIndex = item;
		}
	}
	m_source_palette.SetCurSel(selectedIndex);
}

void CSettingsSourcePage::UpdateSourceThemeDisplay()
{
	const int selectedIndex = m_source_palette.GetCurSel();
	if(selectedIndex < 0 || selectedIndex >= static_cast<int>(m_source_theme_display_names.size()))
		return;

	bool hasCustomColor = false;
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(i != XML_SRC_COLOR_COMMENT && m_source_color_custom[i])
		{
			hasCustomColor = true;
			break;
		}
	}

	CString displayName(m_source_theme_display_names[selectedIndex]);
	if(hasCustomColor)
		displayName += ThemeString(L"fbe.theme.modified_suffix", L" — modified");

	CString currentName;
	m_source_palette.GetLBText(selectedIndex, currentName);
	if(currentName == displayName)
		return;

	m_source_palette.DeleteString(selectedIndex);
	m_source_palette.InsertString(selectedIndex, displayName);
	m_source_palette.SetCurSel(selectedIndex);
}
LRESULT CSettingsSourcePage::OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	_Settings.SetXmlSrcShowSpecialChars(IsDlgButtonChecked(IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS) == BST_CHECKED);
	_Settings.SetSrcFont(U::GetWindowText(m_srcfonts));
	_Settings.SetXmlSrcWrap(m_src_wrap.GetCheck() != 0); _Settings.SetXmlSrcSyntaxHL(m_src_hl.GetCheck() != 0); _Settings.SetXmlSrcTagHL(m_src_taghl.GetCheck() != 0); _Settings.SetXmlSrcShowEOL(m_src_eol.GetCheck() != 0); _Settings.SetXmlSrcShowSpace(m_src_whitespace.GetCheck() != 0); _Settings.SetXMLSrcShowLineNumbers(m_src_line_numbers.GetCheck() != 0);
	const int specialCharsStyle = m_special_chars_style.GetCurSel();
	_Settings.SetXmlSrcSpecialCharsStyle(specialCharsStyle == XML_SRC_SPECIAL_CHARS_TEXT_LABELS ? XML_SRC_SPECIAL_CHARS_TEXT_LABELS : XML_SRC_SPECIAL_CHARS_WORD_LIKE);
	_Settings.SetXmlSrcThemeId(GetSelectedThemeId(m_source_palette, m_source_theme_ids));
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(i == XML_SRC_COLOR_COMMENT) continue;
		_Settings.SetXmlSrcColor(static_cast<XmlSrcColorGroup>(i),
			m_source_color_custom[i] ? m_source_colors[i].GetColor() : XML_SRC_COLOR_DEFAULT);
	}
	EndDialog(wID);
	return 0;
}

LRESULT CSettingsSourcePage::OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	EndDialog(wID);
	return 0;
}

LRESULT CSettingsSourcePage::OnSourcePaletteChanged(WORD, WORD, HWND, BOOL&)
{
	const CString themeId = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(i == XML_SRC_COLOR_COMMENT) continue;
		const DWORD color = GetThemeDefaultColor(themeId,
			static_cast<XmlSrcColorGroup>(i));
		m_source_color_custom[i] = false;
		m_source_colors[i].SetDefaultColor(color);
		m_source_colors[i].SetColor(CLR_DEFAULT);
	}
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	InvalidateSourcePreview();
	return 0;
}


LRESULT CSettingsSourcePage::OnThemeActions(WORD, WORD, HWND, BOOL&)
{
	const CString sourceId = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	bool selectedIsUser = false;
	const std::vector<XmlSourceThemeInfo>& availableThemes = XmlSourceThemes::GetAvailableThemes();
	for(size_t themeIndex = 0; themeIndex < availableThemes.size(); ++themeIndex)
		if(availableThemes[themeIndex].id.CompareNoCase(sourceId) == 0) { selectedIsUser = availableThemes[themeIndex].isUser; break; }
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, 1, ThemeString(L"fbe.theme.menu.import", L"Import theme..."));
	menu.AppendMenu(MF_STRING, 2, ThemeString(L"fbe.theme.menu.export", L"Export theme..."));
	menu.AppendMenu(MF_STRING, 3, ThemeString(L"fbe.theme.menu.save_as", L"Save theme as..."));
	if(selectedIsUser) {
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, 4, ThemeString(L"fbe.theme.menu.delete", L"Delete selected user theme..."));
	}
	RECT buttonRect = {};
	::GetWindowRect(GetDlgItem(IDC_OPTIONS_SOURCE_THEME_ACTIONS), &buttonRect);
	const UINT command = ::TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
		buttonRect.left, buttonRect.bottom, 0, m_hWnd, NULL);
	if(command == 1)
	{
		const std::vector<wchar_t> filter = MakeThemeFileFilter();
		WTL::CFileDialogEx dialog(TRUE, L"fbetheme", NULL,
			OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_HIDEREADONLY,
			filter.data(), m_hWnd);
		if(dialog.DoModal() != IDOK) return 0;
		int imported = 0;
		int failed = 0;
		int cancelled = 0;
		CString lastImportedId;
		CString failures;
		int shownFailures = 0;
		const CSimpleArray<CString>& paths = dialog.GetFileNames();
		for(int i = 0; i < paths.GetSize(); ++i)
		{
			CString importedId, error;
			XmlSourceThemeImport parsedTheme = {};
			if(!XmlSourceThemes::LoadImportTheme(paths[i], parsedTheme, error))
			{
				++failed;
				::OutputDebugStringW((paths[i] + L": " + error + L"\r\n").GetString());
				if(shownFailures < 10) { failures += paths[i] + L": " + error + L"\r\n"; ++shownFailures; }
				continue;
			}
			XmlSourceThemes::ImportThemeConflictMode conflictMode = XmlSourceThemes::IMPORT_THEME_COPY;
			if(XmlSourceThemes::IsUserTheme(parsedTheme.info.id))
			{
				CString conflict;
				conflict.Format(ThemeString(L"fbe.theme.conflict.replace",
					L"Theme \"%s\" (ID: %s) already exists.\n\nReplace the existing user theme?\nYes: replace\nNo: import a copy\nCancel: skip this file."),
					parsedTheme.info.name, parsedTheme.info.id);
				const int decision = ::MessageBox(m_hWnd, conflict, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_YESNOCANCEL | MB_ICONQUESTION);
				if(decision == IDCANCEL) { ++cancelled; continue; }
				if(decision == IDYES) conflictMode = XmlSourceThemes::IMPORT_THEME_REPLACE_USER;
			}			if(XmlSourceThemes::ImportThemeFile(parsedTheme, importedId, error, conflictMode))
			{
				++imported;
				lastImportedId = importedId;
			}
			else
			{
				++failed;
				::OutputDebugStringW((paths[i] + L": " + error + L"\r\n").GetString());
				if(shownFailures < 10) { failures += paths[i] + L": " + error + L"\r\n"; ++shownFailures; }
			}
		}
		if(imported > 0)
		{
			ReloadSourceThemes(lastImportedId);
			BOOL handled = FALSE;
			OnSourcePaletteChanged(0, 0, NULL, handled);
		}
		CString result;
		result.Format(ThemeString(L"fbe.theme.import.summary", L"Imported: %d. Errors: %d. Cancelled: %d."), imported, failed, cancelled);
		if(!failures.IsEmpty()) result += L"\r\n\r\n" + failures;
		if(failed > shownFailures) { CString more; more.Format(ThemeString(L"fbe.theme.import.more_errors", L"Additional errors: %d."), failed - shownFailures); result += L"\r\n" + more; }
		::MessageBox(m_hWnd, result, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_OK | (failed ? MB_ICONWARNING : MB_ICONINFORMATION));
		return 0;
	}

	if(command == 4)
	{
		CString sourceName;
		const int selectedIndex = m_source_palette.GetCurSel();
		if(selectedIndex >= 0 && selectedIndex < static_cast<int>(m_source_theme_names.size()))
			sourceName = m_source_theme_names[selectedIndex];
		const bool deletedThemeWasActive = _Settings.GetStoredXmlSrcThemeId().CompareNoCase(sourceId) == 0;
		CString confirmation;
		confirmation.Format(ThemeString(deletedThemeWasActive ? L"fbe.theme.delete.confirm_active" : L"fbe.theme.delete.confirm_inactive",
			deletedThemeWasActive ?
			L"Delete active user theme \"%s\"?\n\nThe file is deleted immediately and cannot be restored by Cancel. The editor will switch to FBE Light and manual colors will be reset." :
			L"Delete user theme \"%s\"?\n\nThe file is deleted immediately and cannot be restored by Cancel."), sourceName);
		if(::MessageBox(m_hWnd, confirmation, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_YESNO | MB_ICONQUESTION) != IDYES)
			return 0;
		const CString fallbackId = XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_FBE_LIGHT);
		CString error;
		if(!XmlSourceThemes::DeleteUserTheme(sourceId, error))
			::MessageBox(m_hWnd, error, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_OK | MB_ICONERROR);
		else if(deletedThemeWasActive)
		{
			_Settings.SetXmlSrcThemeId(fallbackId, false);
			for(int group = 0; group < XML_SRC_COLOR_GROUP_COUNT; ++group)
				_Settings.SetXmlSrcColor(static_cast<XmlSrcColorGroup>(group), XML_SRC_COLOR_DEFAULT, false);
			_Settings.Save();
			LoadSourceThemeControlsFromSettings();
			const HWND mainWindow = _Settings.GetMainWindow();
			if(::IsWindow(mainWindow))
				::PostMessage(mainWindow, WM_FBE_APPLY_XML_SOURCE_THEME, 0, 0);
		}
		else
		{
			// An inactive file is removed immediately, without changing active settings.
			LoadSourceThemeControlsFromSettings();
		}		return 0;
	}
	if(command != 2 && command != 3) return 0;

	DWORD colors[XML_SRC_STYLE_TOKEN_COUNT] = {};
	for(int token = 0; token < XML_SRC_STYLE_TOKEN_COUNT; ++token)
		colors[token] = ResolveSourceTokenColor(sourceId,
			static_cast<XmlSrcStyleToken>(token), m_source_colors);


	CString sourceName;
	const int sourceIndex = m_source_palette.GetCurSel();
	if(sourceIndex >= 0 && sourceIndex < static_cast<int>(m_source_theme_names.size()))
		sourceName = m_source_theme_names[sourceIndex];
	CString error;
	if(command == 3)
	{
		CString name = ThemeString(L"fbe.theme.save.default_name", L"New theme");
		if(AU::InputBox(name, ThemeString(L"fbe.theme.save.title", L"Save theme as"), ThemeString(L"fbe.theme.save.prompt", L"Theme name:")) != IDYES)
			return 0;
		CString savedId;
		XmlSourceThemeMetadata metadata = {};
		const bool hasExistingMetadata = XmlSourceThemes::GetThemeMetadata(sourceId, metadata);
		FinalizeThemeMetadata(sourceId, colors,
			m_source_colors[XML_SRC_COLOR_BACKGROUND].GetColor() != CLR_DEFAULT,
			hasExistingMetadata, metadata);
		if(!XmlSourceThemes::SaveThemeAsUser(name, colors, savedId, error, &metadata))
			::MessageBox(m_hWnd, error, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_OK | MB_ICONERROR);
		else
		{
			ReloadSourceThemes(savedId);
			BOOL handled = FALSE;
			OnSourcePaletteChanged(0, 0, NULL, handled);
		}
		return 0;
	}

	CString exportId;
	const std::vector<wchar_t> filter = MakeThemeFileFilter();
	CFileDialog dialog(FALSE, L"fbetheme", MakeSafeThemeFileStem(sourceName) + L".fbetheme", OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
		filter.data(), m_hWnd);
	if(dialog.DoModal() != IDOK) return 0;
	XmlSourceThemeMetadata metadata = {};
	const bool hasExistingMetadata = XmlSourceThemes::GetThemeMetadata(sourceId, metadata);
	FinalizeThemeMetadata(sourceId, colors,
		m_source_colors[XML_SRC_COLOR_BACKGROUND].GetColor() != CLR_DEFAULT,
		hasExistingMetadata, metadata);
	exportId = XmlSourceThemes::IsUserTheme(sourceId) ? sourceId :
		XmlSourceThemes::MakeAvailableThemeId(L"custom-" + metadata.baseThemeId);
	// Existing user themes retain optional metadata without a self-referential base id.
	if(metadata.baseThemeId.CompareNoCase(exportId) == 0) metadata.baseThemeId.Empty();
	if(!XmlSourceThemes::ExportThemeFile(exportId, sourceName, colors, dialog.m_szFileName, error, &metadata))
		::MessageBox(m_hWnd, error, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_OK | MB_ICONERROR);
	return 0;
}

LRESULT CSettingsSourcePage::OnResetSourceColors(WORD, WORD, HWND, BOOL&)
{
	const CString themeId = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(i == XML_SRC_COLOR_COMMENT) continue;
		m_source_color_custom[i] = false;
		m_source_colors[i].SetDefaultColor(GetThemeDefaultColor(themeId,
			static_cast<XmlSrcColorGroup>(i)));
		m_source_colors[i].SetColor(CLR_DEFAULT);
	}
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	InvalidateSourcePreview();
	return 0;
}
LRESULT CSettingsSourcePage::OnSourceColorChanged(int idCtrl, LPNMHDR, BOOL&)
{
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(kSourceColorControls[i] == idCtrl)
		{
			m_source_color_custom[i] = m_source_colors[i].GetColor() != CLR_DEFAULT;
			break;
		}
	}
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	InvalidateSourcePreview();
	return 0;
}

void CSettingsSourcePage::InvalidateSourcePreview()
{
	HWND preview = GetDlgItem(IDC_OPTIONS_SOURCE_PREVIEW);
	if(preview != NULL)
		::InvalidateRect(preview, NULL, TRUE);
}

LRESULT CSettingsSourcePage::OnMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	MSG message = {};
	message.hwnd = m_hWnd;
	message.message = uMsg;
	message.wParam = wParam;
	message.lParam = lParam;
	m_source_tooltips.RelayEvent(&message);
	bHandled = FALSE;
	return 0;
}
LRESULT CSettingsSourcePage::OnDrawItem(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
{
	DRAWITEMSTRUCT* drawItem = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
	if(drawItem == NULL || drawItem->CtlID != IDC_OPTIONS_SOURCE_PREVIEW) { bHandled = FALSE; return 0; }
	const bool highContrast = IsHighContrastEnabled();
	const CString id = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	const auto colorFor = [&](XmlSrcStyleToken token) {
		return highContrast ? static_cast<DWORD>(::GetSysColor(COLOR_WINDOWTEXT)) :
			ResolveSourceTokenColor(id, token, m_source_colors);
	};
	const DWORD background = highContrast ? static_cast<DWORD>(::GetSysColor(COLOR_WINDOW)) :
		ResolveSourceTokenColor(id, XML_SRC_STYLE_EDITOR_BACKGROUND, m_source_colors);
	HBRUSH brush = ::CreateSolidBrush(background);
	::FillRect(drawItem->hDC, &drawItem->rcItem, brush); ::DeleteObject(brush);
	::FrameRect(drawItem->hDC, &drawItem->rcItem, reinterpret_cast<HBRUSH>(::GetStockObject(highContrast ? BLACK_BRUSH : GRAY_BRUSH)));
	LOGFONTW font = {};
	font.lfHeight = -::MulDiv(static_cast<int>(_Settings.GetFontSize()), ::GetDeviceCaps(drawItem->hDC, LOGPIXELSY), 72);
	wcsncpy_s(font.lfFaceName, _Settings.GetSrcFont(), _TRUNCATE);
	HFONT createdFont = ::CreateFontIndirectW(&font);
	HFONT oldFont = static_cast<HFONT>(::SelectObject(drawItem->hDC, createdFont ? createdFont : ::GetStockObject(DEFAULT_GUI_FONT)));
	const int oldMode = ::SetBkMode(drawItem->hDC, TRANSPARENT);
	TEXTMETRICW metrics = {}; ::GetTextMetricsW(drawItem->hDC, &metrics);
	RECT bounds = drawItem->rcItem; ::InflateRect(&bounds, -4, -2);
	const int lineHeight = metrics.tmHeight + 1;
	const int availableWidth = bounds.right - bounds.left;

	// Each variant is measured as complete XML lines before anything is painted.
	// This guarantees that a closing delimiter is never clipped at a DPI/font size.
	static const wchar_t* fullLines[] = {
		L"<?xml version=\"1.0\"?>", L"<section id=\"main\">",
		L"  <p class=\"body\">Text &amp;</p>", L"</section>" };
	static const wchar_t* compactLines[] = {
		L"<section id=\"m\">", L"  <p>Text &amp;</p>", L"</section>" };
	static const wchar_t* minimalLines[] = { L"<p id=\"x\">T&amp;</p>" };
	static const wchar_t* fallbackLines[] = { L"<p/>" };
	const auto fits = [&](const wchar_t* const* lines, int count) {
		if(bounds.bottom - bounds.top < count * lineHeight) return false;
		for(int i = 0; i < count; ++i) {
			SIZE size = {}; ::GetTextExtentPoint32W(drawItem->hDC, lines[i], static_cast<int>(wcslen(lines[i])), &size);
			if(size.cx > availableWidth) return false;
		}
		return true;
	};
	const XmlSourcePreviewVariant variant = SelectSourcePreviewVariant(
		fits(fullLines, _countof(fullLines)), fits(compactLines, _countof(compactLines)),
		fits(minimalLines, _countof(minimalLines)), fits(fallbackLines, _countof(fallbackLines)));
	if(variant == XML_SOURCE_PREVIEW_NONE) {
		::SetBkMode(drawItem->hDC, oldMode); ::SelectObject(drawItem->hDC, oldFont); if(createdFont) ::DeleteObject(createdFont);
		bHandled = TRUE; return 0;
	}

	const DWORD text = colorFor(XML_SRC_STYLE_XML_TEXT);
	const DWORD tag = colorFor(XML_SRC_STYLE_XML_TAG_NAME);
	const DWORD delimiter = colorFor(XML_SRC_STYLE_XML_TAG_DELIMITER);
	const DWORD attribute = colorFor(XML_SRC_STYLE_XML_ATTRIBUTE_NAME);
	const DWORD value = colorFor(XML_SRC_STYLE_XML_ATTRIBUTE_VALUE);
	const DWORD entity = colorFor(XML_SRC_STYLE_XML_ENTITY);
	const DWORD pi = colorFor(XML_SRC_STYLE_XML_PROCESSING_INSTRUCTION);
	int x = bounds.left, y = bounds.top;
	const auto nextLine = [&]() { x = bounds.left; y += lineHeight; };
	const auto token = [&](const wchar_t* valueText, DWORD color) {
		SIZE size = {}; ::GetTextExtentPoint32W(drawItem->hDC, valueText, static_cast<int>(wcslen(valueText)), &size);
		ATLASSERT(x + size.cx <= bounds.right);
		::SetTextColor(drawItem->hDC, color);
		::ExtTextOutW(drawItem->hDC, x, y, 0, NULL, valueText, static_cast<UINT>(wcslen(valueText)), NULL);
		x += size.cx;
	};
	if(variant == XML_SOURCE_PREVIEW_FULL) {
		token(L"<?xml version", pi); token(L"=", delimiter); token(L"\"1.0\"", value); token(L"?>", pi); nextLine();
		token(L"<", delimiter); token(L"section", tag); token(L" id", attribute); token(L"=", delimiter); token(L"\"main\"", value); token(L">", delimiter); nextLine();
		token(L"  <", delimiter); token(L"p", tag); token(L" class", attribute); token(L"=", delimiter); token(L"\"body\"", value); token(L">", delimiter); token(L"Text ", text); token(L"&amp;", entity); token(L"</", delimiter); token(L"p", tag); token(L">", delimiter); nextLine();
		token(L"</", delimiter); token(L"section", tag); token(L">", delimiter);
	} else if(variant == XML_SOURCE_PREVIEW_COMPACT) {
		token(L"<", delimiter); token(L"section", tag); token(L" id", attribute); token(L"=", delimiter); token(L"\"m\"", value); token(L">", delimiter); nextLine();
		token(L"  <", delimiter); token(L"p", tag); token(L">", delimiter); token(L"Text ", text); token(L"&amp;", entity); token(L"</", delimiter); token(L"p", tag); token(L">", delimiter); nextLine();
		token(L"</", delimiter); token(L"section", tag); token(L">", delimiter);
	} else if(variant == XML_SOURCE_PREVIEW_MINIMAL) {
		token(L"<", delimiter); token(L"p", tag); token(L" id", attribute); token(L"=", delimiter); token(L"\"x\"", value); token(L">", delimiter); token(L"T", text); token(L"&amp;", entity); token(L"</", delimiter); token(L"p", tag); token(L">", delimiter);
	} else {
		token(L"<", delimiter); token(L"p", tag); token(L"/>", delimiter);
	}
	::SetBkMode(drawItem->hDC, oldMode); ::SelectObject(drawItem->hDC, oldFont); if(createdFont) ::DeleteObject(createdFont);
	bHandled = TRUE; return 0;
}
