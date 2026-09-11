#include "stdafx.h"
#include "ContextAttributeBars.h"
#include "../UiMetrics.h"
#include "../toolbars/ToolbarFactory.h"
#include "../resource.h"

namespace
{
void AddPlaceholder(HWND bar, LPCWSTR text)
{
	TBBUTTON button = {}; button.iString = reinterpret_cast<INT_PTR>(text); button.fsStyle = TBSTYLE_BUTTON;
	::SendMessage(bar, TB_ADDBUTTONS, 1, reinterpret_cast<LPARAM>(&button));
}

CString TextOf(const CWindow& window) { CString text; window.GetWindowText(text); return text; }
}

bool ContextAttributeBars::AddCaption(CCustomStatic& caption, HWND bar, int position, UINT textId, LPCWSTR placeholder, HFONT font)
{
	wchar_t text[MAX_LOAD_STRING + 1] = {};
	FbeLoadString(_Module.GetResourceInstance(), textId, text, MAX_LOAD_STRING);
	AddPlaceholder(bar, text);
	RECT rect = {}; ::SendMessage(bar, TB_GETITEMRECT, position, reinterpret_cast<LPARAM>(&rect)); --rect.bottom;
	if(!caption.Create(bar, rect, NULL, WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE | SS_NOPREFIX, 0, IDC_ID)) return false;
	caption.SetFont(UiMetrics::DialogFont() ? UiMetrics::DialogFont() : font);
	caption.SetWindowText(text); caption.SetEnabled(true);
	AddPlaceholder(bar, placeholder);
	return true;
}

bool ContextAttributeBars::AddBox(HWND bar, int position, CComboBox& box, CCustomEdit& edit, DWORD style, UINT id, HFONT font)
{
	RECT rect = {}; ::SendMessage(bar, TB_GETITEMRECT, position, reinterpret_cast<LPARAM>(&rect)); --rect.bottom;
	if(!box.Create(bar, rect, NULL, style, WS_EX_CLIENTEDGE, id)) return false;
	box.SetFont(font);
	return edit.SubclassWindow(box.ChildWindowFromPoint(CPoint(3, 3))) != NULL;
}

bool ContextAttributeBars::Create(HWND parent)
{
	const DWORD barStyle = ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST;
	m_linksBar = ::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, barStyle, 0, 0, 100, 100, parent, NULL, _Module.GetModuleInstance(), NULL);
	m_tableBar = ::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, barStyle, 0, 0, 100, 100, parent, NULL, _Module.GetModuleInstance(), NULL);
	m_tableBar2 = ::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, barStyle, 0, 0, 100, 100, parent, NULL, _Module.GetModuleInstance(), NULL);
	if(!m_linksBar || !m_tableBar || !m_tableBar2) { Destroy(); return false; }
	ToolbarFactory::SetDialogFontForToolbarRow(m_linksBar); ToolbarFactory::SetDialogFontForToolbarRow(m_tableBar); ToolbarFactory::SetDialogFontForToolbarRow(m_tableBar2);
	::SendMessage(m_linksBar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0); ::SendMessage(m_tableBar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0); ::SendMessage(m_tableBar2, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	::SendMessage(m_linksBar, TB_SETDRAWTEXTFLAGS, DT_CALCRECT, DT_CALCRECT); ::SendMessage(m_tableBar, TB_SETDRAWTEXTFLAGS, DT_CALCRECT, DT_CALCRECT); ::SendMessage(m_tableBar2, TB_SETDRAWTEXTFLAGS, DT_CALCRECT, DT_CALCRECT);
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(m_linksBar, WM_GETFONT, 0, 0));
	if(!AddCaption(m_idCaption, m_linksBar, 0, IDS_TB_CAPT_ID, L"123456789012345678901234567890", font) || !AddCaption(m_hrefCaption, m_linksBar, 2, IDS_TB_CAPT_HREF, L"123456789012345678901234567890", font) || !AddCaption(m_sectionCaption, m_linksBar, 4, IDS_TB_CAPT_SECTION_ID, L"123456789012345678901234567890", font) || !AddCaption(m_imageTitleCaption, m_linksBar, 6, IDS_TB_CAPT_IMAGE_TITLE, L"123456789012345678901234567890", font) || !AddCaption(m_tableIdCaption, m_tableBar, 0, IDS_TB_CAPT_TABLE_ID, L"12345678901234567890", font) || !AddCaption(m_tableStyleCaption, m_tableBar, 2, IDS_TB_CAPT_TABLE_STYLE, L"123456789012345", font) || !AddCaption(m_cellIdCaption, m_tableBar, 4, IDS_TB_CAPT_ID, L"12345678901234567890", font) || !AddCaption(m_cellStyleCaption, m_tableBar, 6, IDS_TB_CAPT_STYLE, L"123456789012345", font) || !AddCaption(m_colspanCaption, m_tableBar2, 0, IDS_TB_CAPT_COLSPAN, L"12345", font) || !AddCaption(m_rowspanCaption, m_tableBar2, 2, IDS_TB_CAPT_ROWSPAN, L"12345", font) || !AddCaption(m_rowAlignCaption, m_tableBar2, 4, IDS_TB_CAPT_TR_ALIGN, L"12345678", font) || !AddCaption(m_alignCaption, m_tableBar2, 6, IDS_TB_CAPT_TD_ALIGN, L"12345678", font) || !AddCaption(m_valignCaption, m_tableBar2, 8, IDS_TB_CAPT_TD_VALIGN, L"12345678", font)) { Destroy(); return false; }
	const DWORD common = WS_CHILD | WS_VISIBLE | CBS_AUTOHSCROLL;
	if(!AddBox(m_linksBar, 1, m_idBox, m_id, common, IDC_ID, font) || !AddBox(m_linksBar, 3, m_hrefBox, m_href, common | WS_VSCROLL | CBS_DROPDOWN | CBS_SORT, IDC_HREF, font) || !AddBox(m_linksBar, 5, m_sectionBox, m_section, common, IDC_SECTION, font) || !AddBox(m_linksBar, 7, m_imageTitleBox, m_imageTitle, common, IDC_IMAGE_TITLE, font) || !AddBox(m_tableBar, 1, m_tableIdBox, m_tableId, common, IDC_IDT, font) || !AddBox(m_tableBar, 3, m_tableStyleBox, m_tableStyle, common, IDC_STYLET, font) || !AddBox(m_tableBar, 5, m_cellIdBox, m_cellId, common, IDC_ID, font) || !AddBox(m_tableBar, 7, m_cellStyleBox, m_cellStyle, common, IDC_STYLE, font) || !AddBox(m_tableBar2, 1, m_colspanBox, m_colspan, common, IDC_COLSPAN, font) || !AddBox(m_tableBar2, 3, m_rowspanBox, m_rowspan, common, IDC_ROWSPAN, font) || !AddBox(m_tableBar2, 5, m_rowAlignBox, m_rowAlign, common | WS_VSCROLL | CBS_DROPDOWNLIST, IDC_ALIGNTR, font) || !AddBox(m_tableBar2, 7, m_alignBox, m_align, common | WS_VSCROLL | CBS_DROPDOWNLIST, IDC_ALIGN, font) || !AddBox(m_tableBar2, 9, m_valignBox, m_valign, common | WS_VSCROLL | CBS_DROPDOWNLIST, IDC_VALIGN, font)) { Destroy(); return false; }
	for(int i = 0; i != 4; ++i) { static const wchar_t* align[] = { L"", L"left", L"right", L"center" }; m_rowAlignBox.InsertString(i, align[i]); m_alignBox.InsertString(i, align[i]); }
	static const wchar_t* valign[] = { L"", L"top", L"middle", L"bottom" }; for(int i = 0; i != 4; ++i) m_valignBox.InsertString(i, valign[i]);
	UpdateMetrics(); return true;
}

void ContextAttributeBars::Destroy() { if(m_linksBar) ::DestroyWindow(m_linksBar); if(m_tableBar) ::DestroyWindow(m_tableBar); if(m_tableBar2) ::DestroyWindow(m_tableBar2); m_linksBar = m_tableBar = m_tableBar2 = NULL; }
void ContextAttributeBars::UpdateMetrics() { ToolbarFactory::SetDialogFontForToolbarRow(m_linksBar, true); ToolbarFactory::SetDialogFontForToolbarRow(m_tableBar, true); ToolbarFactory::SetDialogFontForToolbarRow(m_tableBar2, true); ToolbarFactory::AutoSizeToolbar(m_linksBar); ToolbarFactory::AutoSizeToolbar(m_tableBar); ToolbarFactory::AutoSizeToolbar(m_tableBar2); }
void ContextAttributeBars::UpdateLocalization()
{
	struct Binding { CCustomStatic* caption; CWindow* editor; UINT textId; LPCWSTR placeholder; };
	const Binding links[] = { { &m_idCaption, &m_idBox, IDS_TB_CAPT_ID, L"123456789012345678901234567890" }, { &m_hrefCaption, &m_hrefBox, IDS_TB_CAPT_HREF, L"123456789012345678901234567890" }, { &m_sectionCaption, &m_sectionBox, IDS_TB_CAPT_SECTION_ID, L"123456789012345678901234567890" }, { &m_imageTitleCaption, &m_imageTitleBox, IDS_TB_CAPT_IMAGE_TITLE, L"123456789012345678901234567890" } };
	const Binding table[] = { { &m_tableIdCaption, &m_tableIdBox, IDS_TB_CAPT_TABLE_ID, L"12345678901234567890" }, { &m_tableStyleCaption, &m_tableStyleBox, IDS_TB_CAPT_TABLE_STYLE, L"123456789012345" }, { &m_cellIdCaption, &m_cellIdBox, IDS_TB_CAPT_ID, L"12345678901234567890" }, { &m_cellStyleCaption, &m_cellStyleBox, IDS_TB_CAPT_STYLE, L"123456789012345" } };
	const Binding table2[] = { { &m_colspanCaption, &m_colspanBox, IDS_TB_CAPT_COLSPAN, L"12345" }, { &m_rowspanCaption, &m_rowspanBox, IDS_TB_CAPT_ROWSPAN, L"12345" }, { &m_rowAlignCaption, &m_rowAlignBox, IDS_TB_CAPT_TR_ALIGN, L"12345678" }, { &m_alignCaption, &m_alignBox, IDS_TB_CAPT_TD_ALIGN, L"12345678" }, { &m_valignCaption, &m_valignBox, IDS_TB_CAPT_TD_VALIGN, L"12345678" } };
	const auto rebuild = [](HWND toolbar, const Binding* bindings, size_t count)
	{
		if(!::IsWindow(toolbar)) return;
		::SendMessage(toolbar, WM_SETREDRAW, FALSE, 0);
		for(int index = static_cast<int>(::SendMessage(toolbar, TB_BUTTONCOUNT, 0, 0)) - 1; index >= 0; --index) ::SendMessage(toolbar, TB_DELETEBUTTON, index, 0);
		for(size_t index = 0; index < count; ++index)
		{
			wchar_t text[MAX_LOAD_STRING + 1] = {};
			FbeLoadString(_Module.GetResourceInstance(), bindings[index].textId, text, MAX_LOAD_STRING);
			AddPlaceholder(toolbar, text); AddPlaceholder(toolbar, bindings[index].placeholder);
		}
		::SendMessage(toolbar, TB_AUTOSIZE, 0, 0);
		for(size_t index = 0; index < count; ++index)
		{
			RECT captionRect = {}, editorRect = {};
			::SendMessage(toolbar, TB_GETITEMRECT, static_cast<WPARAM>(index * 2), reinterpret_cast<LPARAM>(&captionRect));
			::SendMessage(toolbar, TB_GETITEMRECT, static_cast<WPARAM>(index * 2 + 1), reinterpret_cast<LPARAM>(&editorRect));
			--captionRect.bottom; --editorRect.bottom;
			wchar_t text[MAX_LOAD_STRING + 1] = {};
			FbeLoadString(_Module.GetResourceInstance(), bindings[index].textId, text, MAX_LOAD_STRING);
			bindings[index].caption->SetWindowText(text);
			::SetWindowPos(bindings[index].caption->m_hWnd, NULL, captionRect.left, captionRect.top, captionRect.right - captionRect.left, captionRect.bottom - captionRect.top, SWP_NOACTIVATE | SWP_NOZORDER);
			::SetWindowPos(bindings[index].editor->m_hWnd, NULL, editorRect.left, editorRect.top, editorRect.right - editorRect.left, editorRect.bottom - editorRect.top, SWP_NOACTIVATE | SWP_NOZORDER);
		}
		::SendMessage(toolbar, WM_SETREDRAW, TRUE, 0);
		::RedrawWindow(toolbar, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
	};
	rebuild(m_linksBar, links, _countof(links)); rebuild(m_tableBar, table, _countof(table)); rebuild(m_tableBar2, table2, _countof(table2));
}
void ContextAttributeBars::SetLinkState(const LinkAttributeState& s) { m_id.SetWindowText(s.id); m_href.SetWindowText(s.href); m_section.SetWindowText(s.section); m_imageTitle.SetWindowText(s.imageTitle); }
LinkAttributeState ContextAttributeBars::GetLinkState() const { LinkAttributeState s; s.id = TextOf(m_id); s.href = TextOf(m_href); s.section = TextOf(m_section); s.imageTitle = TextOf(m_imageTitle); return s; }
void ContextAttributeBars::SetTableState(const TableAttributeState& s) { m_tableId.SetWindowText(s.tableId); m_tableStyle.SetWindowText(s.tableStyle); m_cellId.SetWindowText(s.id); m_cellStyle.SetWindowText(s.style); m_colspan.SetWindowText(s.colspan); m_rowspan.SetWindowText(s.rowspan); m_rowAlignBox.SelectString(-1, s.rowAlign); m_alignBox.SelectString(-1, s.align); m_valignBox.SelectString(-1, s.valign); }
TableAttributeState ContextAttributeBars::GetTableState() const { TableAttributeState s; s.tableId=TextOf(m_tableId); s.tableStyle=TextOf(m_tableStyle); s.id=TextOf(m_cellId); s.style=TextOf(m_cellStyle); s.colspan=TextOf(m_colspan); s.rowspan=TextOf(m_rowspan); s.rowAlign=TextOf(m_rowAlignBox); s.align=TextOf(m_alignBox); s.valign=TextOf(m_valignBox); return s; }

namespace
{
void SetAvailable(CComboBox& box, CCustomStatic& caption, bool available) { box.EnableWindow(available); caption.SetEnabled(available); }
void FocusEdit(CCustomEdit& edit, bool selectAll) { edit.SetFocus(); if(selectAll) { CString text = TextOf(edit); edit.SetSel(0, text.GetLength(), FALSE); } }
}

void ContextAttributeBars::SetLinkAvailability(const LinkAttributeAvailability& a) { SetAvailable(m_idBox,m_idCaption,a.id); SetAvailable(m_hrefBox,m_hrefCaption,a.href); SetAvailable(m_sectionBox,m_sectionCaption,a.section); SetAvailable(m_imageTitleBox,m_imageTitleCaption,a.imageTitle); }
void ContextAttributeBars::SetTableAvailability(const TableAttributeAvailability& a) { SetAvailable(m_tableIdBox,m_tableIdCaption,a.tableId); SetAvailable(m_tableStyleBox,m_tableStyleCaption,a.tableStyle); SetAvailable(m_cellIdBox,m_cellIdCaption,a.cellId); SetAvailable(m_cellStyleBox,m_cellStyleCaption,a.cellStyle); SetAvailable(m_colspanBox,m_colspanCaption,a.colspan); SetAvailable(m_rowspanBox,m_rowspanCaption,a.rowspan); SetAvailable(m_rowAlignBox,m_rowAlignCaption,a.rowAlign); SetAvailable(m_alignBox,m_alignCaption,a.align); SetAvailable(m_valignBox,m_valignCaption,a.valign); }
void ContextAttributeBars::ApplySelectionState(const LinkAttributeState& linkState, const LinkAttributeAvailability& linkAvailability, const TableAttributeState& tableState, const TableAttributeAvailability& tableAvailability)
{
	LinkAttributeState displayedLink = linkState;
	if(::GetFocus() == m_href) displayedLink.href = TextOf(m_href);
	SetLinkState(displayedLink); SetTableState(tableState);
	SetLinkAvailability(linkAvailability); SetTableAvailability(tableAvailability);
}
void ContextAttributeBars::ClearLinkState() { SetLinkState(LinkAttributeState()); }
void ContextAttributeBars::ClearTableState() { SetTableState(TableAttributeState()); }
void ContextAttributeBars::BeginHrefCatalogUpdate() { const CString text = TextOf(m_href); m_hrefBox.ResetContent(); m_href.SetWindowText(text); m_href.SetSel(0, text.GetLength() + 1); }
void ContextAttributeBars::FocusLinkField(LinkAttributeField field, bool selectAll) { switch(field) { case LinkAttributeField::Id: FocusEdit(m_id,selectAll); break; case LinkAttributeField::Href: FocusEdit(m_href,selectAll); break; case LinkAttributeField::Section: FocusEdit(m_section,selectAll); break; case LinkAttributeField::ImageTitle: FocusEdit(m_imageTitle,selectAll); break; } }
void ContextAttributeBars::FocusTableField(TableAttributeField field) { switch(field) { case TableAttributeField::TableId: FocusEdit(m_tableId,false); break; case TableAttributeField::TableStyle: FocusEdit(m_tableStyle,false); break; case TableAttributeField::CellId: FocusEdit(m_cellId,false); break; case TableAttributeField::CellStyle: FocusEdit(m_cellStyle,false); break; case TableAttributeField::Colspan: FocusEdit(m_colspan,false); break; case TableAttributeField::Rowspan: FocusEdit(m_rowspan,false); break; case TableAttributeField::RowAlign: FocusEdit(m_rowAlign,false); break; case TableAttributeField::Align: FocusEdit(m_align,false); break; case TableAttributeField::VAlign: FocusEdit(m_valign,false); break; } }
bool ContextAttributeBars::ContainsFocus(HWND window) const { return window == m_id.m_hWnd || window == m_href.m_hWnd || window == m_section.m_hWnd || window == m_imageTitle.m_hWnd || window == m_tableId.m_hWnd || window == m_tableStyle.m_hWnd || window == m_cellId.m_hWnd || window == m_cellStyle.m_hWnd || window == m_colspan.m_hWnd || window == m_rowspan.m_hWnd || window == m_rowAlign.m_hWnd || window == m_align.m_hWnd || window == m_valign.m_hWnd || ::IsChild(m_linksBar, window) || ::IsChild(m_tableBar, window) || ::IsChild(m_tableBar2, window); }

ContextAttributeBarsDiagnostics ContextAttributeBars::RunDiagnostics()
{
	ContextAttributeBarsDiagnostics result = {};
	LinkAttributeState link; link.id=L"link-id"; link.href=L"#target"; link.section=L"section-id"; link.imageTitle=L"cover";
	TableAttributeState table; table.tableId=L"table-id"; table.tableStyle=L"table-style"; table.id=L"cell-id"; table.style=L"cell-style"; table.colspan=L"2"; table.rowspan=L"3"; table.rowAlign=L"left"; table.align=L"center"; table.valign=L"middle";
	SetLinkState(link); SetTableState(table);
	const LinkAttributeState readLink=GetLinkState(); const TableAttributeState readTable=GetTableState();
	SetLinkAvailability(LinkAttributeAvailability{ true, false, true, false });
	SetTableAvailability(TableAttributeAvailability{ true, false, true, false, true, false, true, false, true });
	result.availability=::IsWindowEnabled(m_idBox)!=FALSE && ::IsWindowEnabled(m_hrefBox)==FALSE && ::IsWindowEnabled(m_tableIdBox)!=FALSE && ::IsWindowEnabled(m_tableStyleBox)==FALSE && ::IsWindowEnabled(m_valignBox)!=FALSE;
	UpdateMetrics();
	UpdateLocalization();
	result.layout=::IsWindow(m_linksBar)!=FALSE && ::IsWindow(m_tableBar)!=FALSE && ::IsWindow(m_tableBar2)!=FALSE && ::IsWindow(m_idBox)!=FALSE && ::IsWindow(m_valignBox)!=FALSE;
	result.controls=::IsWindow(m_idBox)!=FALSE && ::IsWindow(m_hrefBox)!=FALSE && ::IsWindow(m_tableIdBox)!=FALSE && ::IsWindow(m_valignBox)!=FALSE;
	result.ids=::GetDlgCtrlID(m_hrefBox)==IDC_HREF && ::GetDlgCtrlID(m_tableIdBox)==IDC_IDT && ::GetDlgCtrlID(m_valignBox)==IDC_VALIGN;
	result.catalogs=m_alignBox.GetCount()==4 && m_rowAlignBox.GetCount()==4 && m_valignBox.GetCount()==4;
	result.state=readLink.id==link.id && readLink.href==link.href && readLink.section==link.section && readLink.imageTitle==link.imageTitle && readTable.tableId==table.tableId && readTable.tableStyle==table.tableStyle && readTable.id==table.id && readTable.style==table.style && readTable.colspan==table.colspan && readTable.rowspan==table.rowspan && readTable.rowAlign==table.rowAlign && readTable.align==table.align && readTable.valign==table.valign;
	return result;
}
