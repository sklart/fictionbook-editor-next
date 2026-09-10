#pragma once

#include "resource.h"

class CSettings;

struct ScriptsToolbarCommand
{
	int command;
	CString name;
	CString relativePath;
	TBBUTTON button;
};

class CScriptsToolbarCustomizeDlg : public CDialogImpl<CScriptsToolbarCustomizeDlg>
{
public:
	enum { IDD = IDD_SCRIPTS_TOOLBAR_CUSTOMIZE };
	CScriptsToolbarCustomizeDlg(HWND toolbar, const std::vector<ScriptsToolbarCommand>& available,
		const CSimpleArray<TBBUTTON>& defaults, CSettings& settings);
	~CScriptsToolbarCustomizeDlg();

	BEGIN_MSG_MAP(CScriptsToolbarCustomizeDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
		MESSAGE_HANDLER(WM_CLOSE, OnWindowClose)
		MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
		MESSAGE_HANDLER(WM_MEASUREITEM, OnMeasureItem)
		COMMAND_HANDLER(IDC_SCRIPTS_TOOLBAR_SEARCH, EN_CHANGE, OnSearchChanged)
		COMMAND_HANDLER(IDC_SCRIPTS_TOOLBAR_AVAILABLE, LBN_SELCHANGE, OnSelectionChanged)
		COMMAND_HANDLER(IDC_SCRIPTS_TOOLBAR_CURRENT, LBN_SELCHANGE, OnSelectionChanged)
		NOTIFY_CODE_HANDLER(TTN_GETDISPINFOW, OnToolTipText)
		COMMAND_HANDLER(IDC_SCRIPTS_TOOLBAR_AVAILABLE, LBN_DBLCLK, OnAdd)
		COMMAND_HANDLER(IDC_SCRIPTS_TOOLBAR_CURRENT, LBN_DBLCLK, OnRemove)
		COMMAND_ID_HANDLER(IDC_SCRIPTS_TOOLBAR_ADD, OnAdd)
		COMMAND_ID_HANDLER(IDC_SCRIPTS_TOOLBAR_REMOVE, OnRemove)
		COMMAND_ID_HANDLER(IDC_SCRIPTS_TOOLBAR_UP, OnUp)
		COMMAND_ID_HANDLER(IDC_SCRIPTS_TOOLBAR_DOWN, OnDown)
		COMMAND_ID_HANDLER(IDC_SCRIPTS_TOOLBAR_RESET, OnReset)
		COMMAND_ID_HANDLER(IDCANCEL, OnClose)
	END_MSG_MAP()

private:
	HWND m_toolbar;
	const std::vector<ScriptsToolbarCommand>& m_available;
	CSimpleArray<TBBUTTON> m_defaults;
	CSettings& m_settings;
	CListBox m_availableList;
	CListBox m_currentList;
	CToolTipCtrl m_toolTip;
	CString m_toolTipText;
	CSize m_minimumSize;
	HFONT m_dialogFont;
	UINT m_dpi;

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnWindowClose(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDpiChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDrawItem(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnMeasureItem(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSearchChanged(WORD, WORD, HWND, BOOL&);
	LRESULT OnSelectionChanged(WORD, WORD, HWND, BOOL&);
	LRESULT OnToolTipText(int, LPNMHDR, BOOL&);
	LRESULT OnAdd(WORD, WORD, HWND, BOOL&);
	LRESULT OnRemove(WORD, WORD, HWND, BOOL&);
	LRESULT OnUp(WORD, WORD, HWND, BOOL&);
	LRESULT OnDown(WORD, WORD, HWND, BOOL&);
	LRESULT OnReset(WORD, WORD, HWND, BOOL&);
	LRESULT OnClose(WORD, WORD, HWND, BOOL&);

	void PopulateAvailable();
	void PopulateCurrent(int select = -1);
	void LayoutControls(int width, int height);
	void UpdateMetrics();
	void UpdateButtonState();
	void DrawListItem(const DRAWITEMSTRUCT& item);
	void RestorePlacement();
	int Scale(int px) const;
	bool ToolbarContainsCommand(int command) const;
	int SelectedAvailableCommand() const;
	void SavePlacement();
};
