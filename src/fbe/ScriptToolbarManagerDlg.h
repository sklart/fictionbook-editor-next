#pragma once
#include "resource.h"
#include "toolbars\\ScriptToolbarManager.h"
#include <functional>

class CScriptToolbarManagerDlg : public CDialogImpl<CScriptToolbarManagerDlg>
{
public:
	enum { IDD = IDD_SCRIPT_TOOLBAR_MANAGER };
	CScriptToolbarManagerDlg(ScriptToolbarManager& manager, const std::function<bool()>& changed) : m_manager(manager), m_changed(changed) {}
	BEGIN_MSG_MAP(CScriptToolbarManagerDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_SCRIPT_PANELS_LIST, LBN_SELCHANGE, OnSelection)
		COMMAND_HANDLER(IDC_SCRIPT_PANEL_CREATE, BN_CLICKED, OnCreatePanel)
		COMMAND_HANDLER(IDC_SCRIPT_PANEL_RENAME, BN_CLICKED, OnRename)
		COMMAND_HANDLER(IDC_SCRIPT_PANEL_DELETE, BN_CLICKED, OnDelete)
		COMMAND_HANDLER(IDC_SCRIPT_PANEL_UP, BN_CLICKED, OnUp)
		COMMAND_HANDLER(IDC_SCRIPT_PANEL_DOWN, BN_CLICKED, OnDown)
		COMMAND_HANDLER(IDC_SCRIPT_PANEL_VISIBLE, BN_CLICKED, OnVisible)
		COMMAND_ID_HANDLER(IDCANCEL, OnClose)
	END_MSG_MAP()
private:
	ScriptToolbarManager& m_manager; std::function<bool()> m_changed; CListBox m_list;
	std::vector<ScriptToolbarDefinition> m_lastCommitted;
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&); LRESULT OnSelection(WORD, WORD, HWND, BOOL&);
	LRESULT OnCreatePanel(WORD, WORD, HWND, BOOL&); LRESULT OnRename(WORD, WORD, HWND, BOOL&); LRESULT OnDelete(WORD, WORD, HWND, BOOL&);
	LRESULT OnUp(WORD, WORD, HWND, BOOL&); LRESULT OnDown(WORD, WORD, HWND, BOOL&); LRESULT OnVisible(WORD, WORD, HWND, BOOL&); LRESULT OnClose(WORD, WORD, HWND, BOOL&);
	void Refresh(); CString SelectedId() const; bool Commit();
};
