// SettingsNextDlg.cpp : реализация вкладки параметров FBE Next.

#include "stdafx.h"
#include "SettingsNextDlg.h"
#include "Settings.h"
#include "XmlSourceThemes.h"
#include "RuntimeLocalization.h"
#include "utils\CFileDialogEx.h"
#include "apputils.h"

extern CSettings _Settings;

static CString GetThemeDisplayName(const XmlSourceThemeInfo& theme)
{
	if(theme.id.CompareNoCase(XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_SYSTEM)) == 0)
		return FbeLoadRuntimeStringByKey(
			L"fbe.dialog.idd_setting_next.source_palette.system", L"Автоматически — по теме Windows");
	if(theme.id.CompareNoCase(XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_FBE_LIGHT)) == 0)
		return FbeLoadRuntimeStringByKey(
			L"fbe.dialog.idd_setting_next.source_palette.fbe_light", L"FBE Light");
	if(theme.id.CompareNoCase(XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_FBE_DARK)) == 0)
		return FbeLoadRuntimeStringByKey(
			L"fbe.dialog.idd_setting_next.source_palette.fbe_dark", L"FBE Dark");

	if(theme.id.CompareNoCase(XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_HISTORICAL)) == 0)
		return FbeLoadRuntimeStringByKey(
			L"fbe.dialog.idd_setting_next.source_palette.historical", L"Историческая FBE");
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
		switch(stem[i])
		{
		case L'\\': case L'/': case L':': case L'*': case L'?': case L'"': case L'<': case L'>': case L'|':
			stem.SetAt(i, L'_');
			break;
		}
	}
	while(!stem.IsEmpty() && (stem[stem.GetLength() - 1] == L'.' || stem[stem.GetLength() - 1] == L' '))
		stem.Delete(stem.GetLength() - 1);
	return stem.IsEmpty() ? CString(L"theme") : stem;
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

LRESULT CSettingsNextDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CSettingsNextDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);
	m_source_palette = GetDlgItem(IDC_OPTIONS_SOURCE_PALETTE);
	m_source_palette.SetDroppedWidth(250);
	m_source_tooltips.Create(m_hWnd);
	m_source_tooltips.SetMaxTipWidth(320);
	m_source_tooltips.AddTool(m_source_palette, L"\x0412\x044B\x0431\x043E\x0440 \x0432\x0441\x0442\x0440\x043E\x0435\x043D\x043D\x043E\x0439 \x0438\x043B\x0438 \x043F\x043E\x043B\x044C\x0437\x043E\x0432\x0430\x0442\x0435\x043B\x044C\x0441\x043A\x043E\x0439 \x0442\x0435\x043C\x044B \x043F\x043E\x0434\x0441\x0432\x0435\x0442\x043A\x0438.");
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_THEME_ACTIONS), L"\x0418\x043C\x043F\x043E\x0440\x0442, \x044D\x043A\x0441\x043F\x043E\x0440\x0442, \x0441\x043E\x0445\x0440\x0430\x043D\x0435\x043D\x0438\x0435 \x043A\x043E\x043F\x0438\x0438 \x0438 \x0443\x0434\x0430\x043B\x0435\x043D\x0438\x0435 \x043F\x043E\x043B\x044C\x0437\x043E\x0432\x0430\x0442\x0435\x043B\x044C\x0441\x043A\x043E\x0439 \x0442\x0435\x043C\x044B.");
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_TEXT), L"\x0426\x0432\x0435\x0442 \x043E\x0431\x044B\x0447\x043D\x043E\x0433\x043E \x0442\x0435\x043A\x0441\x0442\x0430 XML.");
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_TAG), L"\x0426\x0432\x0435\x0442 \x0438\x043C\x0451\x043D \x0438 \x0442\x0435\x0433\x043E\x0432 \x0438 \x0438\x0445 \x0440\x0430\x0437\x0434\x0435\x043B\x0438\x0442\x0435\x043B\x0435\x0439.");
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE), L"\x0426\x0432\x0435\x0442 \x0438\x043C\x0451\x043D \x0438 \x0430\x0442\x0440\x0438\x0431\x0443\x0442\x043E\x0432 XML.");
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_STRING), L"\x0426\x0432\x0435\x0442 \x0437\x043D\x0430\x0447\x0435\x043D\x0438\x0439 \x0430\x0442\x0440\x0438\x0431\x0443\x0442\x043E\x0432 XML.");
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_BACKGROUND), L"\x0426\x0432\x0435\x0442 \x0444\x043E\x043D\x0430 \x0440\x0435\x0434\x0430\x043A\x0442\x043E\x0440\x0430 \x0438 \x043F\x0440\x0435\x0434\x043F\x0440\x043E\x0441\x043C\x043E\x0442\x0440\x0430.");
	m_source_tooltips.AddTool(GetDlgItem(IDC_OPTIONS_SOURCE_COLORS_RESET), L"\x0423\x0434\x0430\x043B\x0438\x0442\x044C \x0440\x0443\x0447\x043D\x044B\x0435 \x043F\x0435\x0440\x0435\x043E\x043F\x0440\x0435\x0434\x0435\x043B\x0435\x043D\x0438\x044F \x0438 \x0432\x0435\x0440\x043D\x0443\x0442\x044C \x0446\x0432\x0435\x0442\x0430 \x0432\x044B\x0431\x0440\x0430\x043D\x043D\x043E\x0439 \x0442\x0435\x043C\x044B.");

	// XML comments are recognized by Lexilla, but FBE does not preserve comment
	// nodes through the visual editor. Do not expose a colour setting that cannot
	// reliably affect a saved document.
	::ShowWindow(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_COMMENT_LABEL), SW_HIDE);
	::ShowWindow(GetDlgItem(IDC_OPTIONS_SOURCE_COLOR_COMMENT), SW_HIDE);

	const CString backupText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.create_backup_file",
		L"Create a backup copy (.bak) when saving an existing file");
	const CString savingText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.saving",
		L"");
	const CString windowTitleText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.window_title",
		L"");
	const CString showFullPathText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.show_full_path_in_window_title",
		L"Show full file path in the window title");
	const CString paletteText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.source_palette", L"Тема подсветки:");
	const CString sourceCodeText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.source_code", L"Source code");
	const CString backgroundText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.source_color_background", L"Background:");
	if (!savingText.IsEmpty())
		::SetDlgItemText(m_hWnd, IDC_FBE_NEXT_SAVING_GROUP, savingText);
	if (!windowTitleText.IsEmpty())
		::SetDlgItemText(m_hWnd, IDC_FBE_NEXT_WINDOW_TITLE_GROUP, windowTitleText);
	if (!sourceCodeText.IsEmpty())
		::SetDlgItemText(m_hWnd, IDC_FBE_NEXT_SOURCE_CODE_GROUP, sourceCodeText);
	::SetDlgItemText(m_hWnd, IDC_CREATE_BACKUP_FILE, backupText);
	::SetDlgItemText(m_hWnd, IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE, showFullPathText);
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_PALETTE_LABEL, paletteText);
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLOR_BACKGROUND_LABEL, backgroundText);
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLOR_TEXT_LABEL,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_color_text",
			L"\x041E\x0431\x044B\x0447\x043D\x044B\x0439 \x0442\x0435\x043A\x0441\x0442:"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLOR_TAG_LABEL,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_color_tag", L"\x0422\x0435\x0433\x0438:"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE_LABEL,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_color_attribute",
			L"\x0418\x043C\x0435\x043D\x0430 \x0430\x0442\x0440\x0438\x0431\x0443\x0442\x043E\x0432:"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLOR_STRING_LABEL,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_color_string",
			L"\x0417\x043D\x0430\x0447\x0435\x043D\x0438\x044F \x0430\x0442\x0440\x0438\x0431\x0443\x0442\x043E\x0432:"));
	::SetDlgItemText(m_hWnd, IDC_OPTIONS_SOURCE_COLORS_RESET,
		FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_colors_reset",
			L"\x0412\x043E\x0441\x0441\x0442\x0430\x043D\x043E\x0432\x0438\x0442\x044C \x0446\x0432\x0435\x0442\x0430 \x0441\x0445\x0435\x043C\x044B"));
	::SendMessage(GetDlgItem(IDC_CREATE_BACKUP_FILE), BM_SETCHECK,
		_Settings.GetCreateBackupFile() ? BST_CHECKED : BST_UNCHECKED, 0);
	::SendMessage(GetDlgItem(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE), BM_SETCHECK,
		_Settings.GetShowFullPathInWindowTitle() ? BST_CHECKED : BST_UNCHECKED, 0);

	const CString currentThemeId = _Settings.GetXmlSrcThemeId();

	ReloadSourceThemes(currentThemeId);

	const CString automaticColorText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.color_automatic", L"\x0410\x0432\x0442\x043E\x043C\x0430\x0442\x0438\x0447\x0435\x0441\x043A\x0438");
	const CString moreColorsText = FbeLoadRuntimeStringByKey(
		L"fbe.dialog.idd_setting_next.color_more", L"\x0414\x0440\x0443\x0433\x0438\x0435 \x0446\x0432\x0435\x0442\x0430...");
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(i == XML_SRC_COLOR_COMMENT) continue;
		m_source_colors[i].SubclassWindow(GetDlgItem(kSourceColorControls[i]));
		m_source_colors[i].SetDefaultText(automaticColorText);
		m_source_colors[i].SetCustomText(moreColorsText);
		m_source_colors[i].SetDefaultColor(GetThemeDefaultColor(currentThemeId,
			static_cast<XmlSrcColorGroup>(i)));
		m_source_colors[i].SetColor(_Settings.GetXmlSrcColor(static_cast<XmlSrcColorGroup>(i)));
		m_source_color_custom[i] = _Settings.HasXmlSrcCustomColor(static_cast<XmlSrcColorGroup>(i));
	}
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	return 1;
}

void CSettingsNextDlg::UpdateSourceColorTooltips()
{
	static const int labels[XML_SRC_COLOR_GROUP_COUNT] = {
		IDC_OPTIONS_SOURCE_COLOR_TEXT_LABEL, IDC_OPTIONS_SOURCE_COLOR_TAG_LABEL,
		IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE_LABEL, IDC_OPTIONS_SOURCE_COLOR_STRING_LABEL,
		IDC_OPTIONS_SOURCE_COLOR_COMMENT_LABEL, IDC_OPTIONS_SOURCE_COLOR_BACKGROUND_LABEL,
	};
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(i == XML_SRC_COLOR_COMMENT) continue;
		const DWORD color = m_source_colors[i].GetColor();
		CString label;
		::GetWindowText(GetDlgItem(labels[i]), label.GetBufferSetLength(256), 256);
		label.ReleaseBuffer();
		label.TrimRight(L": ");
		CString text;
		text.Format(L"%s. \x0422\x0435\x043A\x0443\x0449\x0438\x0439 \x0446\x0432\x0435\x0442: #%02X%02X%02X", label,
			GetRValue(color), GetGValue(color), GetBValue(color));
		m_source_tooltips.UpdateTipText(text.GetString(), GetDlgItem(kSourceColorControls[i]));
	}
}

void CSettingsNextDlg::ReloadSourceThemes(const CString& selectedThemeId)
{
	m_source_palette.ResetContent();
	m_source_theme_ids.clear();
	m_source_theme_display_names.clear();
	int selectedIndex = 0;
	const std::vector<XmlSourceThemeInfo>& themes = XmlSourceThemes::GetAvailableThemes();
	for(size_t i = 0; i < themes.size(); ++i)
	{
		const int item = m_source_palette.AddString(GetThemeDisplayName(themes[i]));
		if(item >= 0)
		{
			m_source_theme_ids.push_back(themes[i].id);
			m_source_theme_display_names.push_back(GetThemeDisplayName(themes[i]));
			if(themes[i].id.CompareNoCase(selectedThemeId) == 0)
				selectedIndex = item;
		}
	}
	m_source_palette.SetCurSel(selectedIndex);
}

void CSettingsNextDlg::UpdateSourceThemeDisplay()
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
		displayName += L" \x2014 \x0438\x0437\x043C\x0435\x043D\x0435\x043D\x0430";

	CString currentName;
	m_source_palette.GetLBText(selectedIndex, currentName);
	if(currentName == displayName)
		return;

	m_source_palette.DeleteString(selectedIndex);
	m_source_palette.InsertString(selectedIndex, displayName);
	m_source_palette.SetCurSel(selectedIndex);
}
LRESULT CSettingsNextDlg::OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	_Settings.SetCreateBackupFile(IsDlgButtonChecked(IDC_CREATE_BACKUP_FILE) == BST_CHECKED);
	_Settings.SetShowFullPathInWindowTitle(IsDlgButtonChecked(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE) == BST_CHECKED);
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

LRESULT CSettingsNextDlg::OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	EndDialog(wID);
	return 0;
}

LRESULT CSettingsNextDlg::OnSourcePaletteChanged(WORD, WORD, HWND, BOOL&)
{
	const CString themeId = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(i == XML_SRC_COLOR_COMMENT) continue;
		const DWORD color = GetThemeDefaultColor(themeId,
			static_cast<XmlSrcColorGroup>(i));
		m_source_color_custom[i] = false;
		m_source_colors[i].SetColor(color);
		m_source_colors[i].SetDefaultColor(color);
	}
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	InvalidateSourcePreview();
	return 0;
}


LRESULT CSettingsNextDlg::OnThemeActions(WORD, WORD, HWND, BOOL&)
{
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, 1, L"\x0418\x043C\x043F\x043E\x0440\x0442\x0438\x0440\x043E\x0432\x0430\x0442\x044C \x0442\x0435\x043C\x0443...");
	menu.AppendMenu(MF_STRING, 2, L"\x042D\x043A\x0441\x043F\x043E\x0440\x0442\x0438\x0440\x043E\x0432\x0430\x0442\x044C \x0432 \x0444\x0430\x0439\x043B...");
	menu.AppendMenu(MF_STRING, 3, L"\x0421\x043E\x0445\x0440\x0430\x043D\x0438\x0442\x044C \x043A\x0430\x043A \x043F\x043E\x043B\x044C\x0437\x043E\x0432\x0430\x0442\x0435\x043B\x044C\x0441\x043A\x0443\x044E \x0442\x0435\x043C\x0443...");
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, 4, L"\x0423\x0434\x0430\x043B\x0438\x0442\x044C \x0432\x044B\x0431\x0440\x0430\x043D\x043D\x0443\x044E \x043F\x043E\x043B\x044C\x0437\x043E\x0432\x0430\x0442\x0435\x043B\x044C\x0441\x043A\x0443\x044E \x0442\x0435\x043C\x0443...");
	RECT buttonRect = {};
	::GetWindowRect(GetDlgItem(IDC_OPTIONS_SOURCE_THEME_ACTIONS), &buttonRect);
	const UINT command = ::TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
		buttonRect.left, buttonRect.bottom, 0, m_hWnd, NULL);
	if(command == 1)
	{
		WTL::CFileDialogEx dialog(TRUE, L"fbetheme", NULL,
			OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER,
			L"FBE themes (*.fbetheme)\0*.fbetheme\0\0", m_hWnd);
		if(dialog.DoModal() != IDOK) return 0;
		int imported = 0;
		int skipped = 0;
		CString lastImportedId;
		CString failures;
		const CSimpleArray<CString>& paths = dialog.GetFileNames();
		for(int i = 0; i < paths.GetSize(); ++i)
		{
			CString importedId, error;
			if(XmlSourceThemes::ImportThemeFile(paths[i], importedId, error))
			{
				++imported;
				lastImportedId = importedId;
			}
			else
			{
				++skipped;
				if(failures.GetLength() < 1800)
					failures += paths[i] + L": " + error + L"\r\n";
			}
		}
		if(imported > 0)
		{
			ReloadSourceThemes(lastImportedId);
			BOOL handled = FALSE;
			OnSourcePaletteChanged(0, 0, NULL, handled);
		}
		CString result;
		result.Format(L"\x0418\x043C\x043F\x043E\x0440\x0442\x0438\x0440\x043E\x0432\x0430\x043D\x043E: %d. \x041F\x0440\x043E\x043F\x0443\x0449\x0435\x043D\x043E: %d.", imported, skipped);
		if(!failures.IsEmpty()) result += L"\r\n\r\n" + failures;
		::MessageBox(m_hWnd, result, L"FictionBook Editor", MB_OK | (skipped ? MB_ICONWARNING : MB_ICONINFORMATION));
		return 0;
	}

	const CString sourceId = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	if(command == 4)
	{
		if(::MessageBox(m_hWnd, L"\x0423\x0434\x0430\x043B\x0438\x0442\x044C \x0432\x044B\x0431\x0440\x0430\x043D\x043D\x0443\x044E \x043F\x043E\x043B\x044C\x0437\x043E\x0432\x0430\x0442\x0435\x043B\x044C\x0441\x043A\x0443\x044E \x0442\x0435\x043C\x0443?", L"FictionBook Editor", MB_YESNO | MB_ICONQUESTION) != IDYES)
			return 0;
		CString error;
		if(!XmlSourceThemes::DeleteUserTheme(sourceId, error))
			::MessageBox(m_hWnd, error, L"FictionBook Editor", MB_OK | MB_ICONERROR);
		else
		{
			ReloadSourceThemes(XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_FBE_LIGHT));
			BOOL handled = FALSE;
			OnSourcePaletteChanged(0, 0, NULL, handled);
		}
		return 0;
	}
	if(command != 2 && command != 3) return 0;

	DWORD colors[XML_SRC_STYLE_TOKEN_COUNT] = {};
	const bool exportSystemTheme = sourceId.CompareNoCase(
		XmlSourceThemes::GetThemeIdForPalette(XML_SRC_COLOR_PALETTE_SYSTEM)) == 0;
	for(int token = 0; token < XML_SRC_STYLE_TOKEN_COUNT; ++token)
	{
		if(exportSystemTheme)
			colors[token] = CSettings::GetXmlSrcThemeColor(XML_SRC_COLOR_PALETTE_SYSTEM,
				static_cast<XmlSrcStyleToken>(token));
		else if(!XmlSourceThemes::GetThemeColor(sourceId, static_cast<XmlSrcStyleToken>(token), colors[token]))
			colors[token] = CSettings::GetXmlSrcThemeColor(XML_SRC_COLOR_PALETTE_FBE_LIGHT,
				static_cast<XmlSrcStyleToken>(token));
	}
	const XmlSrcStyleToken groups[XML_SRC_COLOR_GROUP_COUNT] = {
		XML_SRC_STYLE_XML_TEXT, XML_SRC_STYLE_XML_TAG_NAME, XML_SRC_STYLE_XML_ATTRIBUTE_NAME,
		XML_SRC_STYLE_XML_ATTRIBUTE_VALUE, XML_SRC_STYLE_XML_COMMENT, XML_SRC_STYLE_EDITOR_BACKGROUND,
	};
	for(int group = 0; group < XML_SRC_COLOR_GROUP_COUNT; ++group)
		if(group != XML_SRC_COLOR_COMMENT)
			colors[groups[group]] = m_source_colors[group].GetColor();
	colors[XML_SRC_STYLE_EDITOR_FOREGROUND] = colors[XML_SRC_STYLE_XML_TEXT];
	colors[XML_SRC_STYLE_XML_TAG_DELIMITER] = colors[XML_SRC_STYLE_XML_TAG_NAME];
	colors[XML_SRC_STYLE_XML_NAMESPACE] = colors[XML_SRC_STYLE_XML_ATTRIBUTE_NAME];

	CString sourceName;
	const int sourceIndex = m_source_palette.GetCurSel();
	if(sourceIndex >= 0 && sourceIndex < static_cast<int>(m_source_theme_display_names.size()))
		sourceName = m_source_theme_display_names[sourceIndex];
	CString error;
	if(command == 3)
	{
		CString name = L"\x041D\x043E\x0432\x0430\x044F \x0442\x0435\x043C\x0430";
		if(AU::InputBox(name, L"\x0421\x043E\x0445\x0440\x0430\x043D\x0438\x0442\x044C \x0442\x0435\x043C\x0443 \x043A\x0430\x043A", L"\x041D\x0430\x0437\x0432\x0430\x043D\x0438\x0435 \x0442\x0435\x043C\x044B:") != IDYES)
			return 0;
		CString savedId;
		if(!XmlSourceThemes::SaveThemeAsUser(name, colors, savedId, error))
			::MessageBox(m_hWnd, error, L"FictionBook Editor", MB_OK | MB_ICONERROR);
		else
		{
			ReloadSourceThemes(savedId);
			BOOL handled = FALSE;
			OnSourcePaletteChanged(0, 0, NULL, handled);
		}
		return 0;
	}

	CString exportId = L"custom-" + sourceId;
	CFileDialog dialog(FALSE, L"fbetheme", MakeSafeThemeFileStem(sourceName) + L".fbetheme", OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
		L"FBE themes (*.fbetheme)\0*.fbetheme\0\0", m_hWnd);
	if(dialog.DoModal() != IDOK) return 0;
	if(!XmlSourceThemes::ExportThemeFile(exportId, sourceName, colors, dialog.m_szFileName, error))
		::MessageBox(m_hWnd, error, L"FictionBook Editor", MB_OK | MB_ICONERROR);
	return 0;
}

LRESULT CSettingsNextDlg::OnResetSourceColors(WORD, WORD, HWND, BOOL&)
{
	const CString themeId = GetSelectedThemeId(m_source_palette, m_source_theme_ids);
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(i == XML_SRC_COLOR_COMMENT) continue;
		m_source_color_custom[i] = false;
		m_source_colors[i].SetColor(GetThemeDefaultColor(themeId,
			static_cast<XmlSrcColorGroup>(i)));
	}
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	InvalidateSourcePreview();
	return 0;
}
LRESULT CSettingsNextDlg::OnSourceColorChanged(int idCtrl, LPNMHDR, BOOL&)
{
	for(int i = 0; i < XML_SRC_COLOR_GROUP_COUNT; ++i)
	{
		if(kSourceColorControls[i] == idCtrl)
		{
			m_source_color_custom[i] = true;
			break;
		}
	}
	UpdateSourceColorTooltips();
	UpdateSourceThemeDisplay();
	InvalidateSourcePreview();
	return 0;
}

void CSettingsNextDlg::InvalidateSourcePreview()
{
	HWND preview = GetDlgItem(IDC_OPTIONS_SOURCE_PREVIEW);
	if(preview != NULL)
		::InvalidateRect(preview, NULL, TRUE);
}

LRESULT CSettingsNextDlg::OnMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
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
LRESULT CSettingsNextDlg::OnDrawItem(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
{
	DRAWITEMSTRUCT* drawItem = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
	if(drawItem == NULL || drawItem->CtlID != IDC_OPTIONS_SOURCE_PREVIEW)
	{
		bHandled = FALSE;
		return 0;
	}

	const DWORD background = m_source_colors[XML_SRC_COLOR_BACKGROUND].GetColor();
	const DWORD text = m_source_colors[XML_SRC_COLOR_TEXT].GetColor();
	const DWORD tag = m_source_colors[XML_SRC_COLOR_TAG].GetColor();
	const DWORD attribute = m_source_colors[XML_SRC_COLOR_ATTRIBUTE].GetColor();
	const DWORD stringValue = m_source_colors[XML_SRC_COLOR_STRING].GetColor();

	HBRUSH brush = ::CreateSolidBrush(background);
	::FillRect(drawItem->hDC, &drawItem->rcItem, brush);
	::DeleteObject(brush);
	::FrameRect(drawItem->hDC, &drawItem->rcItem,
		reinterpret_cast<HBRUSH>(::GetStockObject(GRAY_BRUSH)));

	HFONT oldFont = static_cast<HFONT>(::SelectObject(drawItem->hDC,
		::GetStockObject(DEFAULT_GUI_FONT)));
	const int oldBackgroundMode = ::SetBkMode(drawItem->hDC, TRANSPARENT);
	int x = drawItem->rcItem.left + 4;
	TEXTMETRICW metrics = {};
	::GetTextMetricsW(drawItem->hDC, &metrics);
	const int lineHeight = metrics.tmHeight + 2;
	int y = drawItem->rcItem.top + 2;
	const auto drawToken = [&](const wchar_t* token, DWORD color)
	{
		::SetTextColor(drawItem->hDC, color);
		::TextOutW(drawItem->hDC, x, y, token, static_cast<int>(wcslen(token)));
		SIZE size = {};
		::GetTextExtentPoint32W(drawItem->hDC, token, static_cast<int>(wcslen(token)), &size);
		x += size.cx;
	};

	drawToken(L"<section", tag);
	drawToken(L" id", attribute);
	drawToken(L"=", tag);
	drawToken(L"\"main\"", stringValue);
	drawToken(L">", tag);
	x = drawItem->rcItem.left + 4;
	y += lineHeight;
	::SetTextColor(drawItem->hDC, text);
	::TextOutW(drawItem->hDC, x, y, L"  ", 2);
	x += 12;
	drawToken(L"<p", tag);
	drawToken(L" class", attribute);
	drawToken(L"=", tag);
	drawToken(L"\"body\"", stringValue);
	drawToken(L">", tag);
	drawToken(L"\x041F\x0440\x0438\x043C\x0435\x0440 \x0442\x0435\x043A\x0441\x0442\x0430 &amp; \x0441\x0438\x043C\x0432\x043E\x043B\x043E\x0432", text);
	drawToken(L"</p>", tag);
	x = drawItem->rcItem.left + 4;
	y += lineHeight;
	::SetTextColor(drawItem->hDC, tag);
	::TextOutW(drawItem->hDC, x, y, L"</section>", 10);
	::SetBkMode(drawItem->hDC, oldBackgroundMode);
	::SelectObject(drawItem->hDC, oldFont);
	bHandled = TRUE;
	return 0;
}
