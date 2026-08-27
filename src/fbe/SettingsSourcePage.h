// SettingsSourcePage.h : Source settings page declaration.

#pragma once

#include <atlhost.h>
#include <ColorButton.h>
#include <vector>
#include "resource.h"
#include "Settings.h"

class CSettingsSourcePage : public CAxDialogImpl<CSettingsSourcePage>
{
    CComboBox m_source_palette;
    CComboBox m_special_chars_style;
    CComboBox m_srcfonts;
    CButton m_src_wrap, m_src_hl, m_src_taghl, m_src_eol, m_src_whitespace, m_src_line_numbers;
    CToolTipCtrl m_source_tooltips;
    std::vector<CString> m_source_theme_ids;
    std::vector<CString> m_source_theme_display_names;
    std::vector<CString> m_source_theme_names;
    std::vector<bool> m_source_theme_is_user;
    CColorButton m_source_colors[XML_SRC_COLOR_GROUP_COUNT];
    bool m_source_color_custom[XML_SRC_COLOR_GROUP_COUNT];

public:
	enum { IDD = IDD_SETTINGS_SOURCE };

BEGIN_MSG_MAP(CSettingsSourcePage)
	MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
	MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
	MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseMessage)
	COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
	COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
	COMMAND_HANDLER(IDC_OPTIONS_SOURCE_PALETTE, CBN_SELCHANGE, OnSourcePaletteChanged)
	COMMAND_HANDLER(IDC_OPTIONS_SOURCE_COLORS_RESET, BN_CLICKED, OnResetSourceColors)
	COMMAND_HANDLER(IDC_OPTIONS_SOURCE_THEME_ACTIONS, BN_CLICKED, OnThemeActions)
	NOTIFY_HANDLER(IDC_OPTIONS_SOURCE_COLOR_TEXT, CPN_SELENDOK, OnSourceColorChanged)
	NOTIFY_HANDLER(IDC_OPTIONS_SOURCE_COLOR_TAG, CPN_SELENDOK, OnSourceColorChanged)
	NOTIFY_HANDLER(IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE, CPN_SELENDOK, OnSourceColorChanged)
	NOTIFY_HANDLER(IDC_OPTIONS_SOURCE_COLOR_STRING, CPN_SELENDOK, OnSourceColorChanged)
	NOTIFY_HANDLER(IDC_OPTIONS_SOURCE_COLOR_BACKGROUND, CPN_SELENDOK, OnSourceColorChanged)
	REFLECT_NOTIFICATIONS()
	CHAIN_MSG_MAP(CAxDialogImpl<CSettingsSourcePage>)
END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnSourcePaletteChanged(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnResetSourceColors(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnThemeActions(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnSourceColorChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
	LRESULT OnDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	void ReloadSourceThemes(const CString& selectedThemeId);
	void LoadSourceThemeControlsFromSettings();
	void UpdateSourceColorTooltips();
	void UpdateSourceThemeDisplay();
	void InvalidateSourcePreview();
};
