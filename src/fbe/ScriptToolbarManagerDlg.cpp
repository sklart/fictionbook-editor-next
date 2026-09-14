#include "stdafx.h"
#include "ScriptToolbarManagerDlg.h"

LRESULT CScriptToolbarManagerDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) { m_list = GetDlgItem(IDC_SCRIPT_PANELS_LIST); Refresh(); return TRUE; }
CString CScriptToolbarManagerDlg::SelectedId() const { const int index=m_list.GetCurSel(); const std::vector<ScriptToolbarDefinition>& items=m_manager.Collection().Items(); return index>=0 && index<static_cast<int>(items.size()) ? items[index].id : CString(); }
void CScriptToolbarManagerDlg::Refresh() { CString selected=SelectedId(); m_list.ResetContent(); const std::vector<ScriptToolbarDefinition>& items=m_manager.Collection().Items(); for(size_t i=0;i<items.size();++i) { int row=m_list.AddString(items[i].name); if(items[i].id==selected) m_list.SetCurSel(row); } if(m_list.GetCurSel()<0 && m_list.GetCount()) m_list.SetCurSel(0); BOOL handled=FALSE; OnSelection(0,0,NULL,handled); }
LRESULT CScriptToolbarManagerDlg::OnSelection(WORD, WORD, HWND, BOOL&) { const ScriptToolbarDefinition* item=m_manager.Collection().Find(SelectedId()); if(item) { SetDlgItemText(IDC_SCRIPT_PANEL_NAME,item->name); CheckDlgButton(IDC_SCRIPT_PANEL_VISIBLE,item->visible?BST_CHECKED:BST_UNCHECKED); GetDlgItem(IDC_SCRIPT_PANEL_DELETE).EnableWindow(item->id!=L"scripts-main"); } return 0; }
bool CScriptToolbarManagerDlg::Commit() { if(!m_changed()) { ::MessageBox(m_hWnd,L"Unable to save script toolbar settings.",L"FictionBook Editor",MB_OK|MB_ICONERROR); return false; } Refresh(); return true; }
LRESULT CScriptToolbarManagerDlg::OnCreatePanel(WORD, WORD, HWND, BOOL&) { CString name; GetDlgItemText(IDC_SCRIPT_PANEL_NAME,name); name.Trim(); if(name.IsEmpty()) return 0; m_manager.Create(name); Commit(); return 0; }
LRESULT CScriptToolbarManagerDlg::OnRename(WORD, WORD, HWND, BOOL&) { CString name; GetDlgItemText(IDC_SCRIPT_PANEL_NAME,name); name.Trim(); if(!name.IsEmpty()) { m_manager.Rename(SelectedId(),name); Commit(); } return 0; }
LRESULT CScriptToolbarManagerDlg::OnDelete(WORD, WORD, HWND, BOOL&) { if(m_manager.Delete(SelectedId())) Commit(); return 0; }
LRESULT CScriptToolbarManagerDlg::OnUp(WORD, WORD, HWND, BOOL&) { int i=m_list.GetCurSel(); if(i>0 && m_manager.Move(SelectedId(),static_cast<size_t>(i-1))) Commit(); return 0; }
LRESULT CScriptToolbarManagerDlg::OnDown(WORD, WORD, HWND, BOOL&) { int i=m_list.GetCurSel(); if(i>=0 && i+1<m_list.GetCount() && m_manager.Move(SelectedId(),static_cast<size_t>(i+1))) Commit(); return 0; }
LRESULT CScriptToolbarManagerDlg::OnVisible(WORD, WORD, HWND, BOOL&) { m_manager.SetVisible(SelectedId(),IsDlgButtonChecked(IDC_SCRIPT_PANEL_VISIBLE)==BST_CHECKED); Commit(); return 0; }
LRESULT CScriptToolbarManagerDlg::OnClose(WORD, WORD, HWND, BOOL&) { EndDialog(IDCANCEL); return 0; }
