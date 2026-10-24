// SettingsSourcePage.cpp : Source settings page implementation.

#include "stdafx.h"
#include "../../../common/ModernFileDialog.h"
#include "SettingsSourcePage.h"
#include "EditorFontFamilies.h"
#include "..\\..\\Settings.h"
#include "..\\..\\XmlSourceThemes.h"
#include "..\\..\\RuntimeLocalization.h"
#include "..\\..\\utils\\CFileDialogEx.h"
#include "..\\..\\apputils.h"

extern CSettings _Settings;

bool CSourcePreviewHost::Initialize(HWND placeholder)
{
	if(!SubclassWindow(placeholder) || !m_editor.Create(m_hWnd)) return false;
	static const char sample[] =
		"<?xml version=\"1.0\"?>\r\n"
		"<section id=\"main\">\r\n"
		"\t<p class=\"body\">A  B &amp; \xc2\xa0</p>\r\n"
		"\t<em note=\"A long attribute value for wrapping and highlighting\">Text</em>\r\n"
		"\t<broken>\r\n"
		"</section>";
	m_editor.Send(SCI_SETUNDOCOLLECTION, FALSE);
	m_editor.Send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(sample));
	m_editor.Send(SCI_EMPTYUNDOBUFFER);
	m_editor.Send(SCI_SETREADONLY, TRUE);
	const char* tag = strstr(sample, "<p class");
	if(tag != NULL) m_editor.Send(SCI_GOTOPOS, static_cast<uptr_t>(tag - sample + 1));
	RECT bounds = {};
	GetClientRect(&bounds);
	::SetWindowPos(m_editor.Handle(), NULL, 0, 0, bounds.right, bounds.bottom,
		SWP_NOZORDER | SWP_NOACTIVATE);
	::ShowWindow(m_editor.Handle(), SW_SHOWNA);
	return true;
}

void CSourcePreviewHost::ApplyConfiguration(const SourceEditorConfig& config)
{
	m_config = config;
	if(m_editor.Handle() == NULL) return;
	m_editor.ApplyConfiguration(config);
	// Scintilla's logical focus also creates a thread-wide native caret. A
	// passive or hidden preview must not replace the focused settings edit's
	// caret. Normal WM_SETFOCUS/WM_KILLFOCUS still allow clicking the preview.
	m_editor.Send(SCI_SETFOCUS, ::GetFocus() == m_editor.Handle());
	UpdateTagHighlight();
}

void CSourcePreviewHost::UpdateTagHighlight()
{
	if(m_editor.Handle() == NULL) return;
	m_editor.UpdateTagHighlight({ m_config.tagHighlight,
		m_config.tagHighlightFullTag ? XmlTagHighlightMode::FullTag : XmlTagHighlightMode::NameOnly,
		m_config.tagHighlightAttributes, m_config.tagHighlightErrors });
}

LRESULT CSourcePreviewHost::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
	if(m_editor.Handle() != NULL)
	{
		RECT bounds = {};
		GetClientRect(&bounds);
		::SetWindowPos(m_editor.Handle(), NULL, 0, 0, bounds.right, bounds.bottom,
			SWP_NOZORDER | SWP_NOACTIVATE);
	}
	return 0;
}

LRESULT CSourcePreviewHost::OnNotify(UINT, WPARAM, LPARAM parameter, BOOL& handled)
{
	const NMHDR* notification = reinterpret_cast<const NMHDR*>(parameter);
	if(notification != NULL && notification->hwndFrom == m_editor.Handle() &&
		notification->code == SCN_UPDATEUI) UpdateTagHighlight();
	handled = FALSE;
	return 0;
}

LRESULT CSourcePreviewHost::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
	m_editor.Destroy();
	return 0;
}

static CString ThemeString(LPCWSTR key, LPCWSTR fallback)
{
	return FbeLoadRuntimeStringByKey(key, fallback);
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
	IDC_OPTIONS_SOURCE_COLOR_COMMENT,

	IDC_OPTIONS_SOURCE_COLOR_BACKGROUND,
};

static CString GetSelectedThemeId(const CComboBox& combo, const std::vector<CString>& themeIds)
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
	metadata.baseThemeId = isSystemTheme ? CString(metadata.isDark ? L"fbe-dark" : L"fbe-light") : sourceId;
}

LRESULT CSettingsSourcePage::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CSettingsSourcePage>::OnInitDialog(uMsg, wParam, lParam, bHandled);
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_SETTINGS_SOURCE);
	::SetDlgItemText(m_hWnd, IDC_SOURCE_MATCHED_TAGS_GROUP, ThemeString(L"fbe.settings.source.matched_tags", L"Matched tags"));
	::SetDlgItemText(m_hWnd, IDC_SOURCE_INVISIBLE_GROUP, ThemeString(L"fbe.settings.source.invisible", L"Invisible characters"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLOR_COMMENT_LABEL, ThemeString(L"fbe.settings.source.comment", L"Comments:"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_GROUP, FbeLoadRuntimeStringByKey(L"fbe.settings.source.theme_highlighting", L"Theme and highlighting"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_PREVIEW_GROUP, FbeLoadRuntimeStringByKey(L"fbe.settings.source.preview", L"XML preview"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_FONT, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.source_font", L"Editor font:"));
	::SetDlgItemText(m_hWnd, IDC_WRAP, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.wrap_lines", L"Wrap lines"));
	::SetDlgItemText(m_hWnd, IDC_SYNTAXHL, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.syntax_highlight", L"Syntax highlighting"));
	::SetDlgItemText(m_hWnd, IDC_TAGHL, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.tag_highlight", L"Highlight matched tags"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_MODE_LABEL, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.tag_highlight_mode", L"Matched tag mode:"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_ATTRIBUTES, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.tag_highlight_attributes", L"Highlight attributes"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_ERRORS, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.tag_highlight_errors", L"Show structural tag errors"));
	::SetDlgItemText(m_hWnd, IDC_SHOWEOL, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.show_eol", L"Show end of line marks"));
	::SetDlgItemText(m_hWnd, IDC_SHOWWHITESPACE, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.show_whitespace", L"Show white spaces"));
	::SetDlgItemText(m_hWnd, IDC_SHOWLINENUMBERS, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.show_line_numbers", L"Show line numbers"));
	m_source_palette = GetDlgItem(IDC_OPTIONS_SOURCE_PALETTE);
	m_tag_highlight_mode = GetDlgItem(IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_MODE);
	m_tag_highlight_mode.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.tag_highlight_name_only", L"Tag name only")); m_tag_highlight_mode.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.tag_highlight_full_tag", L"Whole tag"));
	m_tag_highlight_mode.SetCurSel(_Settings.XmlSrcTagHighlightMode() ? 1 : 0);
	m_srcfonts = GetDlgItem(IDC_SRCFONT);
	m_previewFontSize = static_cast<int>(_Settings.GetFontSize());
	EditorFontFamilies::Fill(m_srcfonts, _Settings.GetSrcFont());
	m_src_wrap = GetDlgItem(IDC_WRAP); m_src_hl = GetDlgItem(IDC_SYNTAXHL); m_src_taghl = GetDlgItem(IDC_TAGHL);
	m_src_taghl_attributes = GetDlgItem(IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_ATTRIBUTES); m_src_taghl_errors = GetDlgItem(IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_ERRORS);
	m_src_eol = GetDlgItem(IDC_SHOWEOL); m_src_whitespace = GetDlgItem(IDC_SHOWWHITESPACE); m_src_line_numbers = GetDlgItem(IDC_SHOWLINENUMBERS);
	m_src_wrap.SetCheck(_Settings.XmlSrcWrap()); m_src_hl.SetCheck(_Settings.XmlSrcSyntaxHL()); m_src_taghl.SetCheck(_Settings.XmlSrcTagHL());
	m_src_eol.SetCheck(_Settings.XmlSrcShowEOL()); m_src_whitespace.SetCheck(_Settings.XmlSrcShowSpace()); m_src_line_numbers.SetCheck(_Settings.XMLSrcShowLineNumbers());
	m_src_taghl_attributes.SetCheck(_Settings.XmlSrcTagHighlightAttributes()); m_src_taghl_errors.SetCheck(_Settings.XmlSrcTagHighlightErrors());
	m_source_palette.SetDroppedWidth(250);
	m_source_tooltips.Initialize(m_hWnd);
	m_source_tooltips.Add(m_source_palette, L"fbe.theme.tooltip.palette", L"Choose a built-in or user source highlighting theme.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_THEME_ACTIONS), L"fbe.theme.tooltip.actions", L"Import, export, save a copy, or delete a user theme.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_TEXT), L"fbe.theme.tooltip.text", L"XML plain text color.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_TAG), L"fbe.theme.tooltip.tag", L"XML tag names and delimiters color.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE), L"fbe.theme.tooltip.attribute", L"XML attribute names color.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_STRING), L"fbe.theme.tooltip.value", L"XML attribute values color.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_BACKGROUND), L"fbe.theme.tooltip.background", L"Source editor and preview background color.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_TEXT_LABEL), L"fbe.theme.tooltip.text", L"XML plain text color.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_TAG_LABEL), L"fbe.theme.tooltip.tag", L"XML tag names and delimiters color.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE_LABEL), L"fbe.theme.tooltip.attribute", L"XML attribute names color.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_STRING_LABEL), L"fbe.theme.tooltip.value", L"XML attribute values color.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_BACKGROUND_LABEL), L"fbe.theme.tooltip.background", L"Source editor and preview background color.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_COLORS_RESET), L"fbe.theme.tooltip.reset", L"Remove manual overrides and restore the selected theme colors.");
	m_source_tooltips.Add(m_srcfonts, L"fbe.settings.tooltip.source.font", L"Font used in the source-code editor.");
	m_source_tooltips.Add(m_src_wrap, L"fbe.settings.tooltip.source.wrap", L"Wraps long source-code lines instead of scrolling horizontally.");
	m_source_tooltips.Add(m_src_hl, L"fbe.settings.tooltip.source.syntax", L"Highlights XML syntax in the source-code editor.");
	m_source_tooltips.Add(m_src_taghl, L"fbe.settings.tooltip.source.tags", L"Highlights the matching XML tag at the caret.");
	m_source_tooltips.Add(m_tag_highlight_mode, L"fbe.settings.tooltip.source.tag_mode", L"Chooses whether matching highlights include the full tag.");
	m_source_tooltips.Add(m_src_taghl_attributes, L"fbe.settings.tooltip.source.tag_attributes", L"Includes attributes in matching tag highlighting.");
	m_source_tooltips.Add(m_src_taghl_errors, L"fbe.settings.tooltip.source.tag_errors", L"Shows structural XML tag diagnostics while editing.");
	m_source_tooltips.Add(m_src_eol, L"fbe.settings.tooltip.source.eol", L"Shows end-of-line marks in source code.");
	m_source_tooltips.Add(m_src_whitespace, L"fbe.settings.tooltip.source.whitespace", L"Shows spaces and other whitespace characters.");
	m_source_tooltips.Add(m_src_line_numbers, L"fbe.settings.tooltip.source.line_numbers", L"Shows line numbers in the source-code editor.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS), L"fbe.settings.tooltip.source.invisible", L"Shows invisible formatting characters in the source-code editor.");
	m_source_tooltips.Add(GetDlgItem(IDC_OPTIONS_SOURCE_SPECIAL_CHARS_STYLE), L"fbe.settings.tooltip.source.special_style", L"Chooses how invisible characters are displayed.");


	const CString paletteText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.source_palette", L"Highlighting theme:");
	const CString sourceCodeText = FbeLoadRuntimeStringByKey(
		L"fbe.settings.source.code_display", L"Code display");
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
	UpdateSpecialCharactersDependencies();
	m_caretMode = GetDlgItem(IDC_EDITOR_CARET_MODE);
	m_caretWidth = GetDlgItem(IDC_EDITOR_CARET_WIDTH);
	m_caretColor = GetDlgItem(IDC_EDITOR_CARET_COLOR);
	m_caretBlink = GetDlgItem(IDC_EDITOR_CARET_BLINK);
	m_caretCustomColor.SubclassWindow(GetDlgItem(IDC_EDITOR_CARET_CUSTOM_COLOR));
	::SetDlgItemText(m_hWnd, IDC_EDITOR_CARET_GROUP, ThemeString(L"fbe.settings.caret.group", L"Text cursor"));
	::SetDlgItemText(m_hWnd, IDC_EDITOR_CARET_MODE_LABEL, ThemeString(L"fbe.settings.caret.mode", L"Mode:"));
	::SetDlgItemText(m_hWnd, IDC_EDITOR_CARET_WIDTH_LABEL, ThemeString(L"fbe.settings.caret.width", L"Width:"));
	::SetDlgItemText(m_hWnd, IDC_EDITOR_CARET_COLOR_LABEL, ThemeString(L"fbe.settings.caret.color", L"Color:"));
	::SetDlgItemText(m_hWnd, IDC_EDITOR_CARET_BLINK_LABEL, ThemeString(L"fbe.settings.caret.blink", L"Blink:"));
	m_caretMode.AddString(ThemeString(L"fbe.settings.caret.system", L"System"));
	m_caretMode.AddString(ThemeString(L"fbe.settings.caret.enhanced", L"Enhanced visibility"));
	m_caretMode.AddString(ThemeString(L"fbe.settings.caret.custom", L"Custom"));
	m_caretWidth.AddString(ThemeString(L"fbe.settings.caret.system", L"System"));
	for(int width = 1; width <= 5; ++width) { CString label; label.Format(L"%d", width); m_caretWidth.AddString(label); }
	m_caretColor.AddString(ThemeString(L"fbe.settings.caret.auto_color", L"Automatic contrast"));
	m_caretColor.AddString(ThemeString(L"fbe.settings.caret.system", L"System"));
	m_caretColor.AddString(ThemeString(L"fbe.settings.caret.black", L"Black"));
	m_caretColor.AddString(ThemeString(L"fbe.settings.caret.white", L"White"));
	m_caretColor.AddString(ThemeString(L"fbe.settings.caret.custom", L"Custom"));
	m_caretBlink.AddString(ThemeString(L"fbe.settings.caret.system", L"System"));
	m_caretBlink.AddString(ThemeString(L"fbe.settings.caret.slow", L"Slow"));
	m_caretBlink.AddString(ThemeString(L"fbe.settings.caret.normal", L"Normal"));
	m_caretBlink.AddString(ThemeString(L"fbe.settings.caret.fast", L"Fast"));
	m_caretBlink.AddString(ThemeString(L"fbe.settings.caret.solid", L"Do not blink"));
	const CaretPreferences caret = _Settings.GetCaretPreferences();
	m_caretMode.SetCurSel(static_cast<int>(caret.mode));
	m_caretWidth.SetCurSel(static_cast<int>(caret.width));
	m_caretColor.SetCurSel(static_cast<int>(caret.color));
	m_caretBlink.SetCurSel(static_cast<int>(caret.blink));
	const CString automaticColorText = ThemeString(L"fbe.dialog.idd_setting_next.color_automatic", L"Automatic");
	const CString moreColorsText = ThemeString(L"fbe.dialog.idd_setting_next.color_more", L"More colors...");
	m_caretCustomColor.SetDefaultText(automaticColorText);
	m_caretCustomColor.SetCustomText(moreColorsText);
	m_caretCustomColor.SetColor(caret.customColor);
	m_source_tooltips.Add(m_caretWidth, L"fbe.settings.caret.custom_only", L"Choose Custom mode to change width, color and blinking.");
	m_source_tooltips.Add(m_caretColor, L"fbe.settings.caret.custom_only", L"Choose Custom mode to change width, color and blinking.");
	m_source_tooltips.Add(m_caretBlink, L"fbe.settings.caret.custom_only", L"Choose Custom mode to change width, color and blinking.");
	UpdateCaretDependencies();

	XmlSourceThemes::BeginSettingsEdit();
	const CString currentThemeId = _Settings.GetXmlSrcThemeId();
	m_draftThemeId = m_openingThemeId = currentThemeId;
	m_themeSelectionHistory.push_back(currentThemeId);

	ReloadSourceThemes(currentThemeId);

	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
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
	UpdateTagHighlightDependencies();
	m_preview.Initialize(GetDlgItem(IDC_OPTIONS_SOURCE_PREVIEW));
	InvalidateSourcePreview();
	return 1;
}

void CSettingsSourcePage::LoadSourceThemeControlsFromSettings()
{
	const CString activeThemeId = _Settings.GetXmlSrcThemeId();
	ReloadSourceThemes(activeThemeId);
	m_updatingSourceColors = true;
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		const XmlSrcColorGroup group = static_cast<XmlSrcColorGroup>(i);
		m_source_color_custom[i] = _Settings.HasXmlSrcCustomColor(group);
		m_source_colors[i].SetDefaultColor(GetThemeDefaultColor(activeThemeId, group));
		m_source_colors[i].SetColor(m_source_color_custom[i] ? _Settings.GetXmlSrcColor(group) : CLR_DEFAULT);
	}
	m_updatingSourceColors = false;
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	InvalidateSourcePreview();
}

LRESULT CSettingsSourcePage::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&)
{
	// The System palette follows the live interface theme. Keep explicit color
	// selections intact while refreshing the Automatic swatches in an open page.
	if(!m_source_colors[XML_SRC_COLOR_TEXT].IsWindow()) return 0;
	const CString themeId = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		m_source_colors[i].SetDefaultColor(GetThemeDefaultColor(themeId,
			static_cast<XmlSrcColorGroup>(i)));
		m_source_colors[i].Invalidate();
	}
	InvalidateSourcePreview();
	return 0;
}

LRESULT CSettingsSourcePage::OnSystemAppearanceChanged(UINT, WPARAM, LPARAM, BOOL&)
{
	if(m_preview.IsWindow()) InvalidateSourcePreview();
	return 0;
}

void CSettingsSourcePage::UpdateSourceColorTooltips()
{
	static const int labels[XML_SRC_COLOR_GROUP_COUNT] = {
		IDC_OPTIONS_SOURCE_COLOR_TEXT_LABEL, IDC_OPTIONS_SOURCE_COLOR_TAG_LABEL,
		IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE_LABEL, IDC_OPTIONS_SOURCE_COLOR_STRING_LABEL,
		IDC_OPTIONS_SOURCE_COLOR_COMMENT_LABEL, IDC_OPTIONS_SOURCE_COLOR_BACKGROUND_LABEL,
	};
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
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
		m_source_tooltips.UpdateText(GetDlgItem(kSourceColorControls[i]), text);
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
			uniqueName.Format(L"%s (%d)", static_cast<LPCWSTR>(displayName), suffix);
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
		if(m_source_color_custom[i])
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
LRESULT CSettingsSourcePage::OnClickedOK(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	return ForwardSettingsDialogCommand(m_hWnd, IDOK);
}

LRESULT CSettingsSourcePage::OnClickedCancel(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	return ForwardSettingsDialogCommand(m_hWnd, IDCANCEL);
}

CSettingsSourcePage::~CSettingsSourcePage() { XmlSourceThemes::EndSettingsEdit(); }
bool CSettingsSourcePage::Validate() { return true; }
bool CSettingsSourcePage::PrepareCommit(SettingsFileTransaction& files, CString& error)
{
	return XmlSourceThemes::PrepareSettingsEdit(files, error);
}

void CSettingsSourcePage::Commit()
{
	_Settings.SetXmlSrcShowSpecialChars(IsDlgButtonChecked(IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS) == BST_CHECKED);
	_Settings.SetSrcFont(U::GetWindowText(m_srcfonts));
	_Settings.SetXmlSrcWrap(m_src_wrap.GetCheck() != 0); _Settings.SetXmlSrcSyntaxHL(m_src_hl.GetCheck() != 0); _Settings.SetXmlSrcTagHL(m_src_taghl.GetCheck() != 0); _Settings.SetXmlSrcTagHighlightMode(m_tag_highlight_mode.GetCurSel() == 1 ? 1 : 0); _Settings.SetXmlSrcTagHighlightAttributes(m_src_taghl_attributes.GetCheck() != 0); _Settings.SetXmlSrcTagHighlightErrors(m_src_taghl_errors.GetCheck() != 0); _Settings.SetXmlSrcShowEOL(m_src_eol.GetCheck() != 0); _Settings.SetXmlSrcShowSpace(m_src_whitespace.GetCheck() != 0); _Settings.SetXMLSrcShowLineNumbers(m_src_line_numbers.GetCheck() != 0);
	const int specialCharsStyle = m_special_chars_style.GetCurSel();
	_Settings.SetXmlSrcSpecialCharsStyle(specialCharsStyle == XML_SRC_SPECIAL_CHARS_TEXT_LABELS ? XML_SRC_SPECIAL_CHARS_TEXT_LABELS : XML_SRC_SPECIAL_CHARS_WORD_LIKE);
	_Settings.SetXmlSrcThemeId(GetSelectedThemeId(m_source_palette, m_source_theme_ids));
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		_Settings.SetXmlSrcColor(static_cast<XmlSrcColorGroup>(i),
			m_source_color_custom[i] ? m_source_colors[i].GetColor() : XML_SRC_COLOR_DEFAULT);
	}
	_Settings.SetCaretPreferences(SelectedCaretPreferences());
}

bool CSettingsSourcePage::CancelChanges() { return true; }

void CSettingsSourcePage::UpdateSpecialCharactersDependencies()
{
	const BOOL enabled = IsDlgButtonChecked(IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS) == BST_CHECKED;
	m_special_chars_style.EnableWindow(enabled);
	GetDlgItem(IDC_OPTIONS_SOURCE_SPECIAL_CHARS_STYLE_LABEL).EnableWindow(enabled);
}

void CSettingsSourcePage::UpdateTagHighlightDependencies()
{
	const BOOL enabled = m_src_taghl.GetCheck() == BST_CHECKED;
	m_tag_highlight_mode.EnableWindow(enabled);
	GetDlgItem(IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_MODE_LABEL).EnableWindow(enabled);
	m_src_taghl_attributes.EnableWindow(enabled);
}

LRESULT CSettingsSourcePage::OnTagHighlightChanged(WORD, WORD, HWND, BOOL&)
{
	UpdateTagHighlightDependencies();
	InvalidateSourcePreview();
	return 0;
}

LRESULT CSettingsSourcePage::OnSpecialCharactersChanged(WORD, WORD, HWND, BOOL&)
{
	UpdateSpecialCharactersDependencies();
	InvalidateSourcePreview();
	return 0;
}

LRESULT CSettingsSourcePage::OnPreviewSettingsChanged(WORD, WORD, HWND, BOOL&)
{
	InvalidateSourcePreview();
	return 0;
}

CaretPreferences CSettingsSourcePage::SelectedCaretPreferences() const
{
	CaretPreferences preferences;
	preferences.mode = static_cast<TextCaretMode>((std::max)(0, m_caretMode.GetCurSel()));
	preferences.width = static_cast<DWORD>((std::max)(0, m_caretWidth.GetCurSel()));
	preferences.color = static_cast<TextCaretColor>((std::max)(0, m_caretColor.GetCurSel()));
	preferences.blink = static_cast<TextCaretBlink>((std::max)(0, m_caretBlink.GetCurSel()));
	preferences.customColor = m_caretCustomColor.GetColor();
	return preferences;
}

void CSettingsSourcePage::UpdateCaretDependencies()
{
	const bool custom = m_caretMode.GetCurSel() == static_cast<int>(TextCaretMode::Custom);
	m_caretWidth.EnableWindow(custom);
	m_caretColor.EnableWindow(custom);
	m_caretBlink.EnableWindow(custom);
	m_caretCustomColor.EnableWindow(custom && m_caretColor.GetCurSel() == static_cast<int>(TextCaretColor::Custom));
}

LRESULT CSettingsSourcePage::OnCaretSettingsChanged(WORD, WORD, HWND, BOOL&)
{
	UpdateCaretDependencies();
	InvalidateSourcePreview();
	return 0;
}

LRESULT CSettingsSourcePage::OnCaretColorChanged(int, LPNMHDR, BOOL&)
{
	InvalidateSourcePreview();
	return 0;
}

void CSettingsSourcePage::SaveThemeDraft()
{
	if(m_draftThemeId.IsEmpty()) return;
	ThemeDraft* draft = NULL;
	for(auto& item : m_themeDrafts) if(item.id.CompareNoCase(m_draftThemeId) == 0) { draft = &item; break; }
	if(!draft) { m_themeDrafts.push_back(ThemeDraft()); draft = &m_themeDrafts.back(); draft->id = m_draftThemeId; }
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i) draft->colors[i] = m_source_colors[i].GetColor();
}

LRESULT CSettingsSourcePage::OnSourcePaletteChanged(WORD, WORD, HWND, BOOL&)
{
	SaveThemeDraft();
	const CString themeId = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	m_draftThemeId = themeId;
	m_themeSelectionHistory.push_back(themeId);
	const ThemeDraft* draft = NULL;
	for(const auto& item : m_themeDrafts) if(item.id.CompareNoCase(themeId) == 0) { draft = &item; break; }
	m_updatingSourceColors = true;
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		const DWORD color = GetThemeDefaultColor(themeId,
			static_cast<XmlSrcColorGroup>(i));
		m_source_color_custom[i] = draft && draft->colors[i] != CLR_DEFAULT;
		m_source_colors[i].SetDefaultColor(color);
		m_source_colors[i].SetColor(draft ? draft->colors[i] : CLR_DEFAULT);
	}
	m_updatingSourceColors = false;
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	InvalidateSourcePreview();
	return 0;
}


bool CSettingsSourcePage::DeleteTheme(const CString& id, CString& error)
{
	SaveThemeDraft();
	CString selected = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	const DWORD background = GetThemeDefaultColor(id, XML_SRC_COLOR_BACKGROUND);
	if(!XmlSourceThemes::DeleteUserTheme(id, error)) return false;
	m_themeDrafts.erase(std::remove_if(m_themeDrafts.begin(), m_themeDrafts.end(), [&](const ThemeDraft& draft) {
		return draft.id.CompareNoCase(id) == 0;
	}), m_themeDrafts.end());
	if(selected.CompareNoCase(id) == 0)
	{
		selected.Empty();
		if(id.CompareNoCase(m_openingThemeId) != 0)
		{
			const auto& available = XmlSourceThemes::GetAvailableThemes();
			for(auto it = m_themeSelectionHistory.rbegin(); it != m_themeSelectionHistory.rend() && selected.IsEmpty(); ++it)
				for(const auto& theme : available)
					if(theme.id.CompareNoCase(*it) == 0 && theme.id.CompareNoCase(id) != 0) { selected = theme.id; break; }
		}
		if(selected.IsEmpty()) selected = XmlSourceThemes::GetThemeIdForPalette(
			GetRValue(background) + GetGValue(background) + GetBValue(background) < 384 ?
			XML_SRC_COLOR_PALETTE_FBE_DARK : XML_SRC_COLOR_PALETTE_FBE_LIGHT);
	}
	m_draftThemeId.Empty(); // Never save deleted controls as another theme's draft.
	ReloadSourceThemes(selected);
	BOOL handled = FALSE; OnSourcePaletteChanged(0, 0, NULL, handled);
	return true;
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
		const CString filterLabel = ThemeString(L"fbe.theme.file_filter", L"FBE themes (*.fbetheme)");
		const COMDLG_FILTERSPEC filters[] = { { filterLabel, L"*.fbetheme" } };
		ModernFileDialog::Request request; request.allowMultiSelect = true; request.fileMustExist = true; request.pathMustExist = true;
		request.defaultExtension = L"fbetheme"; request.filters = filters; request.filterCount = _countof(filters);
		const auto dialog = ModernFileDialog::Show(m_hWnd, request);
		if(dialog.outcome != ModernFileDialog::Outcome::Accepted) return 0;
		int imported = 0;
		int failed = 0;
		int cancelled = 0;
		CString lastImportedId;
		CString failures;
		int shownFailures = 0;
		std::vector<CString> paths;
		for(const auto& path : dialog.paths) paths.push_back(path.c_str());
		for(int i = 0; i < static_cast<int>(paths.size()); ++i)
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
				// The conflict prompt identifies the existing theme by parsedTheme.info.name, parsedTheme.info.id.

				CString conflict;

				conflict.Format(ThemeString(L"fbe.theme.conflict.replace",
					L"Theme \"%s\" (ID: %s) already exists.\n\nReplace the existing user theme?\nYes: replace\nNo: import a copy\nCancel: skip this file."),
					static_cast<LPCWSTR>(parsedTheme.info.name), static_cast<LPCWSTR>(parsedTheme.info.id));
				const int decision = U::MessageBox(m_hWnd, conflict, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_YESNOCANCEL | MB_ICONQUESTION);
				if(decision != IDYES && decision != IDNO) { ++cancelled; continue; }
				if(decision == IDYES) conflictMode = XmlSourceThemes::IMPORT_THEME_REPLACE_USER;
			}
			if(XmlSourceThemes::ImportThemeFile(parsedTheme, importedId, error, conflictMode))
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
		U::MessageBox(m_hWnd, result, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_OK | (failed ? MB_ICONWARNING : MB_ICONINFORMATION));
		return 0;
	}

	if(command == 4)
	{
		CString sourceName;
		const int selectedIndex = m_source_palette.GetCurSel();
		if(selectedIndex >= 0 && selectedIndex < static_cast<int>(m_source_theme_names.size()))
			sourceName = m_source_theme_names[selectedIndex];
		CString confirmation;
		confirmation.Format(ThemeString(L"fbe.theme.delete.staged", L"Delete user theme \"%s\" when you click OK? Cancel will keep it."), static_cast<LPCWSTR>(sourceName));
		if(U::MessageBox(m_hWnd, confirmation, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) return 0;
		CString error;
		if(!DeleteTheme(sourceId, error))
			U::MessageBox(m_hWnd, error, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_OK | MB_ICONERROR);

		return 0;
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
			U::MessageBox(m_hWnd, error, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_OK | MB_ICONERROR);
		else
		{
			ReloadSourceThemes(savedId);
			BOOL handled = FALSE;
			OnSourcePaletteChanged(0, 0, NULL, handled);
		}
		return 0;
	}

	CString exportId;
	const CString filterLabel = ThemeString(L"fbe.theme.file_filter", L"FBE themes (*.fbetheme)");
	const COMDLG_FILTERSPEC filters[] = { { filterLabel, L"*.fbetheme" } };
	ModernFileDialog::Request request; request.save = true; request.pathMustExist = true; request.overwritePrompt = true;
	request.defaultExtension = L"fbetheme"; request.initialFileName = (MakeSafeThemeFileStem(sourceName) + L".fbetheme").GetString();
	request.filters = filters; request.filterCount = _countof(filters);
	const auto dialog = ModernFileDialog::Show(m_hWnd, request);
	if(dialog.outcome != ModernFileDialog::Outcome::Accepted || dialog.paths.empty()) return 0;
	XmlSourceThemeMetadata metadata = {};
	const bool hasExistingMetadata = XmlSourceThemes::GetThemeMetadata(sourceId, metadata);
	FinalizeThemeMetadata(sourceId, colors,
		m_source_colors[XML_SRC_COLOR_BACKGROUND].GetColor() != CLR_DEFAULT,
		hasExistingMetadata, metadata);
	exportId = XmlSourceThemes::IsUserTheme(sourceId) ? sourceId :
		XmlSourceThemes::MakeAvailableThemeId(L"custom-" + metadata.baseThemeId);
	// Existing user themes retain optional metadata without a self-referential base id.
	if(metadata.baseThemeId.CompareNoCase(exportId) == 0) metadata.baseThemeId.Empty();
	if(!XmlSourceThemes::ExportThemeFile(exportId, sourceName, colors, CString(dialog.paths.front().c_str()), error, &metadata))
		U::MessageBox(m_hWnd, error, ThemeString(L"fbe.theme.dialog.caption", L"FictionBook Editor"), MB_OK | MB_ICONERROR);
	return 0;
}

LRESULT CSettingsSourcePage::OnResetSourceColors(WORD, WORD, HWND, BOOL&)
{
	const CString themeId = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	m_updatingSourceColors = true;
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		m_source_color_custom[i] = false;
		m_source_colors[i].SetDefaultColor(GetThemeDefaultColor(themeId,
			static_cast<XmlSrcColorGroup>(i)));
		m_source_colors[i].SetColor(CLR_DEFAULT);
	}
	m_updatingSourceColors = false;
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	InvalidateSourcePreview();
	return 0;
}
LRESULT CSettingsSourcePage::OnSourceColorChanged(int idCtrl, LPNMHDR, BOOL&)
{
	if(m_updatingSourceColors) return 0;
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

void CSettingsSourcePage::SetPreviewFontSize(int size)
{
	if(size == m_previewFontSize) return;
	m_previewFontSize = size;
	InvalidateSourcePreview();
}

int CSettingsSourcePage::PreviewLineHeight() const
{
	return m_preview.Editor().Handle() ?
		static_cast<int>(m_preview.Editor().Send(SCI_TEXTHEIGHT, 0)) : 0;
}

int CSettingsSourcePage::PreviewDisplayLineCount() const
{
	if(!m_preview.Editor().Handle()) return 0;
	const SourceEditorControl& editor = m_preview.Editor();
	const int lines = static_cast<int>(editor.Send(SCI_GETLINECOUNT));
	int displayLines = 0;
	for(int line = 0; line < lines; ++line)
		displayLines += static_cast<int>(editor.Send(SCI_WRAPCOUNT, line));
	return displayLines;
}

int CSettingsSourcePage::PreviewVerticalChromeHeight() const
{
	if(!m_preview.IsWindow() || !m_preview.Editor().Handle()) return 0;
	RECT host = {}, editor = {};
	::GetClientRect(m_preview.m_hWnd, &host);
	::GetClientRect(m_preview.Editor().Handle(), &editor);
	return max(0, host.bottom - editor.bottom) + 2 * ::GetSystemMetrics(SM_CYEDGE);
}

void CSettingsSourcePage::InvalidateSourcePreview()
{
	if(!m_preview.IsWindow()) return;
	m_preview.ApplyConfiguration(SelectedPreviewConfiguration());
	::PostMessageW(::GetParent(m_hWnd), WM_FBE_SOURCE_PREVIEW_LAYOUT, 0, 0);
}

SourceEditorConfig CSettingsSourcePage::SelectedPreviewConfiguration() const
{
	SourceEditorConfig config;
	config.showEol = m_src_eol.GetCheck() != 0;
	config.showWhitespace = m_src_whitespace.GetCheck() != 0;
	config.wrap = m_src_wrap.GetCheck() != 0;
	config.showLineNumbers = m_src_line_numbers.GetCheck() != 0;
	config.syntaxHighlight = m_src_hl.GetCheck() != 0;
	config.showSpecialCharacters = IsDlgButtonChecked(IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS) == BST_CHECKED;
	config.specialCharactersStyle = m_special_chars_style.GetCurSel() == XML_SRC_SPECIAL_CHARS_TEXT_LABELS ?
		XML_SRC_SPECIAL_CHARS_TEXT_LABELS : XML_SRC_SPECIAL_CHARS_WORD_LIKE;
	config.undoSelectionHistory = false;
	config.tagHighlight = m_src_taghl.GetCheck() != 0;
	config.tagHighlightFullTag = m_tag_highlight_mode.GetCurSel() == 1;
	config.tagHighlightAttributes = m_src_taghl_attributes.GetCheck() != 0;
	config.tagHighlightErrors = m_src_taghl_errors.GetCheck() != 0;
	const int fontIndex = m_srcfonts.GetCurSel();
	if(fontIndex != CB_ERR) m_srcfonts.GetLBText(fontIndex, config.fontName);
	if(config.fontName.IsEmpty()) config.fontName = _Settings.GetSrcFont();
	config.fontSize = m_previewFontSize;
	config.caret = SelectedCaretPreferences();
	const CString themeId = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	static_assert(SourceEditorColorCount == XML_SRC_STYLE_TOKEN_COUNT, "XML source colors must match the editor configuration");
	for(int token = 0; token < XML_SRC_STYLE_TOKEN_COUNT; ++token)
		config.colors[token] = ResolveSourceTokenColor(themeId,
			static_cast<XmlSrcStyleToken>(token), m_source_colors);
	return config;
}
