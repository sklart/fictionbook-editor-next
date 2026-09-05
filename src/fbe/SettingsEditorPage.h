#pragma once

#include <atlhost.h>
#include <ColorButton.h>
#include "resource.h"
#include "SettingsPageLifecycle.h"
#include "SettingsTooltips.h"
#include "EditorBackgrounds.h"

class CEditorBackgroundPreview : public CWindowImpl<CEditorBackgroundPreview, CStatic>
{
	HBITMAP m_bitmap = NULL;
	HFONT m_font = NULL;
	COLORREF m_foreground = RGB(0, 0, 0);
	COLORREF m_background = RGB(255, 255, 255);
	CString m_text;
public:
	DECLARE_WND_SUPERCLASS(NULL, WC_STATIC)
	BEGIN_MSG_MAP(CEditorBackgroundPreview)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
	END_MSG_MAP()
	void SetPreview(HBITMAP bitmap, const CString& face, int size, COLORREF foreground, COLORREF background, const CString& text);
	LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnEraseBackground(UINT, WPARAM, LPARAM, BOOL&) { return 1; }
	LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
};

class CSettingsEditorPage : public CAxDialogImpl<CSettingsEditorPage>, public ISettingsPage
{
	CColorButton m_foreground;
	CColorButton m_background;
	CComboBox m_fonts;
	CComboBox m_fontSize;
	CComboBox m_nbspCharacter;
	CComboBox m_backgroundImage;
	CComboBox m_backgroundLayout;
	CEditorBackgroundPreview m_backgroundPreview;
	std::vector<EditorBackgroundDescriptor> m_builtInBackgrounds;
	CString m_customBackgroundPath;
	CSettingsTooltips m_tooltips;

public:
	enum { IDD = IDD_SETTINGS_EDITOR };
	BEGIN_MSG_MAP(CSettingsEditorPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
		COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
		COMMAND_HANDLER(IDC_EDITOR_BACKGROUND_BROWSE, BN_CLICKED, OnBrowseBackground)
		COMMAND_HANDLER(IDC_EDITOR_BACKGROUND_IMAGE, CBN_SELCHANGE, OnBackgroundSelectionChanged)
		COMMAND_HANDLER(IDC_EDITOR_BACKGROUND_LAYOUT, CBN_SELCHANGE, OnBackgroundSelectionChanged)
		COMMAND_HANDLER(IDC_FONT, CBN_SELCHANGE, OnPreviewSettingsChanged)
		COMMAND_HANDLER(IDC_FONT_SIZE, CBN_SELCHANGE, OnPreviewSettingsChanged)
		COMMAND_HANDLER(IDC_FG, BN_CLICKED, OnPreviewSettingsChanged)
		COMMAND_HANDLER(IDC_BG, BN_CLICKED, OnPreviewSettingsChanged)
		REFLECT_NOTIFICATIONS()
		CHAIN_MSG_MAP(CAxDialogImpl<CSettingsEditorPage>)
	END_MSG_MAP()
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClickedOK(WORD, WORD, HWND, BOOL&);
	LRESULT OnClickedCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnBrowseBackground(WORD, WORD, HWND, BOOL&);
	LRESULT OnBackgroundSelectionChanged(WORD, WORD, HWND, BOOL&);
	LRESULT OnPreviewSettingsChanged(WORD, WORD, HWND, BOOL&);
	void UpdateBackgroundPreview();
	bool Validate(); void Commit(); bool CancelChanges();
};
