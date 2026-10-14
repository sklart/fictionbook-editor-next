// SettingsSourcePage.h : Source settings page declaration.

#pragma once

#include <atlhost.h>
#include "..\\..\\ThemeManager.h"
#include "..\\..\\extras\\ColorButton.h"
#include <vector>
#include "..\\..\\resource.h"
#include "..\\..\\Settings.h"
#include "SettingsPageLifecycle.h"
#include "SettingsTooltips.h"
#include "..\\..\\source\\ui\\SourceEditorControl.h"

#ifndef WM_DPICHANGED_AFTERPARENT
#define WM_DPICHANGED_AFTERPARENT 0x02E3
#endif

class CSourcePreviewHost : public CWindowImpl<CSourcePreviewHost, CStatic>
{
	SourceEditorControl m_editor;
	SourceEditorConfig m_config;
	void UpdateTagHighlight();
public:
	DECLARE_WND_SUPERCLASS(NULL, WC_STATIC)
	BEGIN_MSG_MAP(CSourcePreviewHost)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_NOTIFY, OnNotify)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
	END_MSG_MAP()
	bool Initialize(HWND placeholder);
	void ApplyConfiguration(const SourceEditorConfig& config);
	SourceEditorControl& Editor() { return m_editor; }
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNotify(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
};

class CSettingsSourcePage : public CAxDialogImpl<CSettingsSourcePage>, public ISettingsPage
{
    CComboBox m_source_palette;
	CComboBox m_tag_highlight_mode;
    CComboBox m_special_chars_style;
    CComboBox m_srcfonts;
    CButton m_src_wrap, m_src_hl, m_src_taghl, m_src_taghl_attributes, m_src_taghl_errors, m_src_eol, m_src_whitespace, m_src_line_numbers;
    CSettingsTooltips m_source_tooltips;
    std::vector<CString> m_source_theme_ids;
    std::vector<CString> m_source_theme_display_names;
    std::vector<CString> m_source_theme_names;
    std::vector<bool> m_source_theme_is_user;
    CColorButton m_source_colors[XML_SRC_COLOR_GROUP_COUNT];
    bool m_source_color_custom[XML_SRC_COLOR_GROUP_COUNT];
	CSourcePreviewHost m_preview;
	int m_previewFontSize = 10;
	CComboBox m_caretMode, m_caretWidth, m_caretColor, m_caretBlink;
	CColorButton m_caretCustomColor;
	bool m_updatingSourceColors = false;

public:
	enum { IDD = IDD_SETTINGS_SOURCE };
	void SetPreviewFontSize(int size);

BEGIN_MSG_MAP(CSettingsSourcePage)
	MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
	MESSAGE_HANDLER(WM_FBE_THEMECHANGED, OnThemeChanged)
	MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSystemAppearanceChanged)
	MESSAGE_HANDLER(WM_SYSCOLORCHANGE, OnSystemAppearanceChanged)
	MESSAGE_HANDLER(WM_THEMECHANGED, OnSystemAppearanceChanged)
	MESSAGE_HANDLER(WM_DPICHANGED_AFTERPARENT, OnSystemAppearanceChanged)
	COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
	COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
	COMMAND_HANDLER(IDC_OPTIONS_SOURCE_PALETTE, CBN_SELCHANGE, OnSourcePaletteChanged)
	COMMAND_HANDLER(IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS, BN_CLICKED, OnSpecialCharactersChanged)
	COMMAND_HANDLER(IDC_SRCFONT, CBN_SELCHANGE, OnPreviewSettingsChanged)
	COMMAND_HANDLER(IDC_WRAP, BN_CLICKED, OnPreviewSettingsChanged)
	COMMAND_HANDLER(IDC_SYNTAXHL, BN_CLICKED, OnPreviewSettingsChanged)
	COMMAND_HANDLER(IDC_TAGHL, BN_CLICKED, OnTagHighlightChanged)
	COMMAND_HANDLER(IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_MODE, CBN_SELCHANGE, OnPreviewSettingsChanged)
	COMMAND_HANDLER(IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_ATTRIBUTES, BN_CLICKED, OnPreviewSettingsChanged)
	COMMAND_HANDLER(IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_ERRORS, BN_CLICKED, OnPreviewSettingsChanged)
	COMMAND_HANDLER(IDC_SHOWEOL, BN_CLICKED, OnPreviewSettingsChanged)
	COMMAND_HANDLER(IDC_SHOWWHITESPACE, BN_CLICKED, OnPreviewSettingsChanged)
	COMMAND_HANDLER(IDC_SHOWLINENUMBERS, BN_CLICKED, OnPreviewSettingsChanged)
	COMMAND_HANDLER(IDC_OPTIONS_SOURCE_SPECIAL_CHARS_STYLE, CBN_SELCHANGE, OnPreviewSettingsChanged)
	COMMAND_HANDLER(IDC_EDITOR_CARET_MODE, CBN_SELCHANGE, OnCaretSettingsChanged)
	COMMAND_HANDLER(IDC_EDITOR_CARET_WIDTH, CBN_SELCHANGE, OnCaretSettingsChanged)
	COMMAND_HANDLER(IDC_EDITOR_CARET_COLOR, CBN_SELCHANGE, OnCaretSettingsChanged)
	COMMAND_HANDLER(IDC_EDITOR_CARET_BLINK, CBN_SELCHANGE, OnCaretSettingsChanged)
	COMMAND_HANDLER(IDC_OPTIONS_SOURCE_COLORS_RESET, BN_CLICKED, OnResetSourceColors)
	COMMAND_HANDLER(IDC_OPTIONS_SOURCE_THEME_ACTIONS, BN_CLICKED, OnThemeActions)
	NOTIFY_HANDLER(IDC_OPTIONS_SOURCE_COLOR_TEXT, CPN_SELENDOK, OnSourceColorChanged)
	NOTIFY_HANDLER(IDC_OPTIONS_SOURCE_COLOR_TAG, CPN_SELENDOK, OnSourceColorChanged)
	NOTIFY_HANDLER(IDC_OPTIONS_SOURCE_COLOR_ATTRIBUTE, CPN_SELENDOK, OnSourceColorChanged)
	NOTIFY_HANDLER(IDC_OPTIONS_SOURCE_COLOR_STRING, CPN_SELENDOK, OnSourceColorChanged)
	NOTIFY_HANDLER(IDC_OPTIONS_SOURCE_COLOR_BACKGROUND, CPN_SELENDOK, OnSourceColorChanged)
	NOTIFY_HANDLER(IDC_EDITOR_CARET_CUSTOM_COLOR, CPN_SELENDOK, OnCaretColorChanged)
	REFLECT_NOTIFICATIONS()
	CHAIN_MSG_MAP(CAxDialogImpl<CSettingsSourcePage>)
END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnThemeChanged(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnSystemAppearanceChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnTagHighlightChanged(WORD, WORD, HWND, BOOL&);
	void UpdateTagHighlightDependencies();
	LRESULT OnSourcePaletteChanged(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnSpecialCharactersChanged(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnResetSourceColors(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnThemeActions(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnSourceColorChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
	LRESULT OnPreviewSettingsChanged(WORD, WORD, HWND, BOOL&);
	LRESULT OnCaretSettingsChanged(WORD, WORD, HWND, BOOL&);
	LRESULT OnCaretColorChanged(int, LPNMHDR, BOOL&);
	void ReloadSourceThemes(const CString& selectedThemeId);
	void LoadSourceThemeControlsFromSettings();
	void UpdateSourceColorTooltips();
	void UpdateSourceThemeDisplay();
	void InvalidateSourcePreview();
	SourceEditorConfig SelectedPreviewConfiguration() const;
	CaretPreferences SelectedCaretPreferences() const;
	void UpdateCaretDependencies();
	void UpdateSpecialCharactersDependencies();
	bool Validate(); void Commit(); bool CancelChanges();
};
