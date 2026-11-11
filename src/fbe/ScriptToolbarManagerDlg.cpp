#include "stdafx.h"
#include "ScriptToolbarManagerDlg.h"
#include "RuntimeLocalization.h"
#include "ThemeManager.h"
#include "UiMetrics.h"
#include "settings/EditorBackgrounds.h"
#include "utils.h"

LRESULT CScriptToolbarManagerDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) { FbeApplyRuntimeDialogLocalization(m_hWnd, IDD); m_list = GetDlgItem(IDC_SCRIPT_PANELS_LIST); m_lastCommitted=m_manager.Collection().Items(); Refresh(); return TRUE; }
LRESULT CScriptToolbarManagerDlg::OnMeasureItem(UINT, WPARAM, LPARAM value, BOOL& handled)
{
	MEASUREITEMSTRUCT* item = reinterpret_cast<MEASUREITEMSTRUCT*>(value);
	if(item == NULL || item->CtlID != IDC_SCRIPT_PANELS_LIST) { handled = FALSE; return 0; }
	item->itemHeight = UiMetrics::ScaleForDpi(20, UiMetrics::DpiForWindow(m_hWnd));
	return TRUE;
}
LRESULT CScriptToolbarManagerDlg::OnDpiChanged(UINT, WPARAM dpiValue, LPARAM, BOOL& handled)
{
	const UINT dpi = HIWORD(dpiValue) ? HIWORD(dpiValue) : UiMetrics::DpiForWindow(m_hWnd);
	if(m_list.IsWindow()) ::SendMessage(m_list, LB_SETITEMHEIGHT, 0, UiMetrics::ScaleForDpi(20, dpi));
	handled = FALSE; return 0;
}
LRESULT CScriptToolbarManagerDlg::OnDrawItem(UINT, WPARAM, LPARAM value, BOOL& handled)
{
	const DRAWITEMSTRUCT* item = reinterpret_cast<DRAWITEMSTRUCT*>(value);
	if(item == NULL || item->CtlID != IDC_SCRIPT_PANELS_LIST) { handled = FALSE; return 0; }
	const auto& panels = m_manager.Collection().Items();
	if(item->itemID == static_cast<UINT>(-1) || item->itemID >= panels.size()) return TRUE;
	const bool selected = (item->itemState & ODS_SELECTED) != 0;
	const bool themed = ThemeManager::IsDark() && !ThemeManager::IsHighContrast();
	const COLORREF background = themed ? (selected ? ThemeManager::SelectionBackgroundColor() : ThemeManager::ControlColor()) : ::GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW);
	COLORREF foreground = themed ? (selected ? ThemeManager::SelectionTextColor() : (panels[item->itemID].visible ? ThemeManager::TextColor() : ThemeManager::SecondaryTextColor())) :
		::GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : (panels[item->itemID].visible ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
	if(!selected && !panels[item->itemID].visible && EditorBackgrounds::HasLowTextContrast(foreground, background))
		foreground = themed ? ThemeManager::TextColor() : ::GetSysColor(COLOR_WINDOWTEXT);
	const int saved = ::SaveDC(item->hDC); CDCHandle dc(item->hDC); CRect rect(item->rcItem);
	dc.FillSolidRect(rect, background); dc.SetTextColor(foreground); dc.SetBkMode(TRANSPARENT);
	const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
	auto scale = [dpi](int px) { return UiMetrics::ScaleForDpi(px, dpi); };
	CRect text(rect); text.left += scale(4); text.right -= scale(20);
	dc.DrawText(panels[item->itemID].name, -1, text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
	if(!panels[item->itemID].visible) {
		const int side = scale(8), x = rect.right - scale(14), y = (rect.top + rect.bottom - side) / 2;
		HPEN pen = ::CreatePen(PS_SOLID, scale(1), foreground);
		if(pen != NULL) {
			::SelectObject(item->hDC, pen); ::SelectObject(item->hDC, ::GetStockObject(HOLLOW_BRUSH));
			::Ellipse(item->hDC, x, y, x + side, y + side);
			::MoveToEx(item->hDC, x + scale(1), y + side - scale(1), NULL); ::LineTo(item->hDC, x + side - scale(1), y + scale(1));
			::SelectObject(item->hDC, ::GetStockObject(BLACK_PEN)); ::DeleteObject(pen);
		}
	}
	if(item->itemState & ODS_FOCUS) ::DrawFocusRect(item->hDC, &item->rcItem);
	::RestoreDC(item->hDC, saved); return TRUE;
}
CString CScriptToolbarManagerDlg::SelectedId() const { const int index=m_list.GetCurSel(); const std::vector<ScriptToolbarDefinition>& items=m_manager.Collection().Items(); return index>=0 && index<static_cast<int>(items.size()) ? items[index].id : CString(); }
void CScriptToolbarManagerDlg::Refresh(const CString& preferredId) { CString selected=preferredId.IsEmpty()?SelectedId():preferredId; m_list.ResetContent(); const std::vector<ScriptToolbarDefinition>& items=m_manager.Collection().Items(); for(size_t i=0;i<items.size();++i) { int row=m_list.AddString(items[i].name); if(items[i].id==selected) m_list.SetCurSel(row); } if(m_list.GetCurSel()<0 && m_list.GetCount()) m_list.SetCurSel(0); BOOL handled=FALSE; OnSelection(0,0,NULL,handled); }
LRESULT CScriptToolbarManagerDlg::OnSelection(WORD, WORD, HWND, BOOL&) { if(m_inlineEdit.IsWindow()) FinishRename(true); const ScriptToolbarDefinition* item=m_manager.Collection().Find(SelectedId()); const bool editable=item!=NULL && item->id!=L"scripts-main"; if(item) CheckDlgButton(IDC_SCRIPT_PANEL_VISIBLE,item->visible?BST_CHECKED:BST_UNCHECKED); GetDlgItem(IDC_SCRIPT_PANEL_DELETE).EnableWindow(editable); GetDlgItem(IDC_SCRIPT_PANEL_RENAME).EnableWindow(editable); return 0; }
CString CScriptToolbarManagerDlg::NextDefaultPanelName() const { return m_manager.Collection().NextDefaultName(FbeLoadRuntimeStringByKey(L"fbe.script_toolbar_manager.default_name")); }
bool CScriptToolbarManagerDlg::Commit(const CString& preferredId) { if(!m_changed(m_lastCommitted, m_manager.Collection().Items())) { m_manager.Collection().Items()=m_lastCommitted; U::MessageBox(m_hWnd,FbeLoadRuntimeStringByKey(L"fbe.script_toolbar_manager.save_failed",L"Failed to save toolbar settings."),FbeLoadRuntimeStringByKey(L"fbe.script_toolbar_manager.caption",L"FictionBook Editor"),MB_OK|MB_ICONERROR); Refresh(); return false; } m_lastCommitted=m_manager.Collection().Items(); Refresh(preferredId); return true; }
bool CScriptToolbarManagerDlg::BeginRename() { if(m_inlineEdit.IsWindow()) return true; const CString id=SelectedId(); const ScriptToolbarDefinition* item=m_manager.Collection().Find(id); if(item==NULL || id==L"scripts-main") return false; RECT rect={}; if(m_list.GetItemRect(m_list.GetCurSel(),&rect)==LB_ERR) return false; m_editingId=id; m_inlineEdit.Create(m_list,rect,NULL,WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,0,IDC_SCRIPT_PANEL_INLINE_EDIT); m_inlineEdit.SetFont(reinterpret_cast<HFONT>(::SendMessage(m_list,WM_GETFONT,0,0))); m_inlineEdit.SetWindowText(item->name); m_inlineEdit.SetFocus(); m_inlineEdit.SetSel(0,-1); return true; }
bool CScriptToolbarManagerDlg::FinishRename(bool apply) { if(!m_inlineEdit.IsWindow()) return false; CString name; m_inlineEdit.GetWindowText(name); m_inlineEdit.DestroyWindow(); CString id=m_editingId; m_editingId.Empty(); if(!apply) return true; name.Trim(); if(name.IsEmpty()) return false; const std::vector<ScriptToolbarDefinition>& items=m_manager.Collection().Items(); for(size_t i=0;i<items.size();++i) if(items[i].id!=id && items[i].name.CompareNoCase(name)==0) return false; if(!m_manager.Rename(id,name)) return false; return Commit(id); }
void CScriptToolbarManagerDlg::EndRenameForAction() { if(m_inlineEdit.IsWindow()) FinishRename(true); }
LRESULT CScriptToolbarManagerDlg::OnCreatePanel(WORD, WORD, HWND, BOOL&) { EndRenameForAction(); const ScriptToolbarDefinition& created=m_manager.Create(NextDefaultPanelName()); const CString id=created.id; if(Commit(id)) BeginRename(); return 0; }
LRESULT CScriptToolbarManagerDlg::OnRename(WORD, WORD, HWND, BOOL&) { if(m_inlineEdit.IsWindow()) FinishRename(true); else BeginRename(); return 0; }
LRESULT CScriptToolbarManagerDlg::OnListDoubleClick(WORD, WORD, HWND, BOOL&) { BeginRename(); return 0; }
LRESULT CScriptToolbarManagerDlg::OnDelete(WORD, WORD, HWND, BOOL&) { EndRenameForAction(); const int current=m_list.GetCurSel(); const CString previous=current>0 ? CString(m_manager.Collection().Items()[current-1].id) : CString(); if(m_manager.Delete(SelectedId())) Commit(previous); return 0; }
LRESULT CScriptToolbarManagerDlg::OnUp(WORD, WORD, HWND, BOOL&) { EndRenameForAction(); CString id=SelectedId(); int i=m_list.GetCurSel(); if(i>0 && m_manager.Move(id,static_cast<size_t>(i-1))) Commit(id); return 0; }
LRESULT CScriptToolbarManagerDlg::OnDown(WORD, WORD, HWND, BOOL&) { EndRenameForAction(); CString id=SelectedId(); int i=m_list.GetCurSel(); if(i>=0 && i+1<m_list.GetCount() && m_manager.Move(id,static_cast<size_t>(i+1))) Commit(id); return 0; }
LRESULT CScriptToolbarManagerDlg::OnVisible(WORD, WORD, HWND, BOOL&) { EndRenameForAction(); CString id=SelectedId(); m_manager.SetVisible(id,IsDlgButtonChecked(IDC_SCRIPT_PANEL_VISIBLE)==BST_CHECKED); Commit(id); return 0; }
LRESULT CScriptToolbarManagerDlg::OnInlineEditKillFocus(WORD, WORD, HWND, BOOL&) { FinishRename(true); return 0; }
LRESULT CScriptToolbarManagerDlg::OnClose(WORD, WORD, HWND, BOOL&) { FinishRename(false); EndDialog(IDCANCEL); return 0; }
BOOL CScriptToolbarManagerDlg::PreTranslateMessage(MSG* message) { if(message->message!=WM_KEYDOWN) return FALSE; if(message->hwnd==m_list && message->wParam==VK_F2) return BeginRename(); if(message->hwnd==m_inlineEdit) { if(message->wParam==VK_RETURN) { FinishRename(true); return TRUE; } if(message->wParam==VK_ESCAPE) { FinishRename(false); return TRUE; } } return FALSE; }
