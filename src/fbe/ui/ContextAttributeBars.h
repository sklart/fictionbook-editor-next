#pragma once

#include "ContextAttributeControls.h"

struct LinkAttributeState { CString id; CString href; CString section; CString imageTitle; };
struct TableAttributeState { CString tableId; CString tableStyle; CString id; CString style; CString colspan; CString rowspan; CString rowAlign; CString align; CString valign; };
enum class ContextBarMode { None, Link, Image, Section, Table };

// Owns Win32/WTL presentation state only. CMainFrame stays the notification
// parent and the owner of document commands.
class ContextAttributeBars
{
public:
	bool Create(HWND parent);
	void Destroy();
	HWND LinksBar() const { return m_linksBar; } HWND TableBar() const { return m_tableBar; } HWND TableBar2() const { return m_tableBar2; }
	void UpdateMetrics(); void UpdateLocalization();
	void SetMode(ContextBarMode mode);
	ContextBarMode Mode() const { return m_mode; }
	void SetLinkState(const LinkAttributeState& state); LinkAttributeState GetLinkState() const;
	void SetTableState(const TableAttributeState& state); TableAttributeState GetTableState() const;
	CComboBox& IdBox() { return m_idBox; } CComboBox& HrefBox() { return m_hrefBox; } CComboBox& ImageTitleBox() { return m_imageTitleBox; } CComboBox& SectionBox() { return m_sectionBox; }
	CCustomEdit& IdEdit() { return m_id; } CCustomEdit& HrefEdit() { return m_href; } CCustomEdit& ImageTitleEdit() { return m_imageTitle; } CCustomEdit& SectionEdit() { return m_section; }
	CComboBox& TableIdBox() { return m_tableIdBox; } CComboBox& TableStyleBox() { return m_tableStyleBox; } CComboBox& CellIdBox() { return m_cellIdBox; } CComboBox& CellStyleBox() { return m_cellStyleBox; }
	CComboBox& ColspanBox() { return m_colspanBox; } CComboBox& RowspanBox() { return m_rowspanBox; } CComboBox& RowAlignBox() { return m_rowAlignBox; } CComboBox& AlignBox() { return m_alignBox; } CComboBox& VAlignBox() { return m_valignBox; }
	CCustomEdit& TableIdEdit() { return m_tableId; } CCustomEdit& TableStyleEdit() { return m_tableStyle; } CCustomEdit& CellIdEdit() { return m_cellId; } CCustomEdit& CellStyleEdit() { return m_cellStyle; }
	CCustomEdit& ColspanEdit() { return m_colspan; } CCustomEdit& RowspanEdit() { return m_rowspan; } CCustomEdit& RowAlignEdit() { return m_rowAlign; } CCustomEdit& AlignEdit() { return m_align; } CCustomEdit& VAlignEdit() { return m_valign; }
	CCustomStatic& IdCaption() { return m_idCaption; } CCustomStatic& HrefCaption() { return m_hrefCaption; } CCustomStatic& SectionCaption() { return m_sectionCaption; } CCustomStatic& ImageTitleCaption() { return m_imageTitleCaption; }
	CCustomStatic& TableIdCaption() { return m_tableIdCaption; } CCustomStatic& TableStyleCaption() { return m_tableStyleCaption; } CCustomStatic& CellIdCaption() { return m_cellIdCaption; } CCustomStatic& CellStyleCaption() { return m_cellStyleCaption; }
	CCustomStatic& ColspanCaption() { return m_colspanCaption; } CCustomStatic& RowspanCaption() { return m_rowspanCaption; } CCustomStatic& RowAlignCaption() { return m_rowAlignCaption; } CCustomStatic& AlignCaption() { return m_alignCaption; } CCustomStatic& VAlignCaption() { return m_valignCaption; }
private:
	HWND m_linksBar = NULL, m_tableBar = NULL, m_tableBar2 = NULL;
	ContextBarMode m_mode = ContextBarMode::None;
	CComboBox m_idBox, m_hrefBox, m_imageTitleBox, m_sectionBox, m_tableIdBox, m_tableStyleBox, m_cellIdBox, m_cellStyleBox, m_colspanBox, m_rowspanBox, m_rowAlignBox, m_alignBox, m_valignBox;
	CCustomEdit m_id, m_href, m_imageTitle, m_section, m_tableId, m_tableStyle, m_cellId, m_cellStyle, m_colspan, m_rowspan, m_rowAlign, m_align, m_valign;
	CCustomStatic m_idCaption, m_hrefCaption, m_sectionCaption, m_imageTitleCaption, m_tableIdCaption, m_tableStyleCaption, m_cellIdCaption, m_cellStyleCaption, m_colspanCaption, m_rowspanCaption, m_rowAlignCaption, m_alignCaption, m_valignCaption;
	void AddCaption(CCustomStatic& caption, HWND bar, int position, UINT textId, LPCWSTR placeholder, HFONT font);
	void AddBox(HWND bar, int position, CComboBox& box, CCustomEdit& edit, DWORD style, UINT id, HFONT font);
	void SetCaptionText(CCustomStatic& caption, UINT textId);
};
