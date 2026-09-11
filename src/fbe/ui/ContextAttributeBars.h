#pragma once

#include "ContextAttributeControls.h"

struct LinkAttributeState { CString id; CString href; CString section; CString imageTitle; };
struct TableAttributeState { CString tableId; CString tableStyle; CString id; CString style; CString colspan; CString rowspan; CString rowAlign; CString align; CString valign; };
struct LinkAttributeAvailability { bool id; bool href; bool section; bool imageTitle; };
struct TableAttributeAvailability { bool tableId; bool tableStyle; bool cellId; bool cellStyle; bool colspan; bool rowspan; bool rowAlign; bool align; bool valign; };
enum class ContextBarMode { None, Link, Image, Section, Table };
enum class LinkAttributeField { Id, Href, Section, ImageTitle };
enum class TableAttributeField { TableId, TableStyle, CellId, CellStyle, Colspan, Rowspan, RowAlign, Align, VAlign };
struct ContextAttributeBarsDiagnostics { bool controls; bool ids; bool catalogs; bool state; bool linkMode; bool tableMode; };

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
	void SetLinkAvailability(const LinkAttributeAvailability& availability);
	void SetTableAvailability(const TableAttributeAvailability& availability);
	void ApplySelectionState(const LinkAttributeState& linkState, const LinkAttributeAvailability& linkAvailability, const TableAttributeState& tableState, const TableAttributeAvailability& tableAvailability);
	void ClearLinkState(); void ClearTableState();
	void FocusLinkField(LinkAttributeField field, bool selectAll = false);
	void FocusTableField(TableAttributeField field);
	bool ContainsFocus(HWND window) const;
	ContextAttributeBarsDiagnostics RunDiagnostics();
	// The existing document catalog builder still accepts CComboBox.  Keep this
	// narrow bridge here rather than exposing every presentation control.
	void BeginHrefCatalogUpdate();
	CComboBox& HrefCatalogForPopulation() { return m_hrefBox; }
private:
	HWND m_linksBar = NULL, m_tableBar = NULL, m_tableBar2 = NULL;
	ContextBarMode m_mode = ContextBarMode::None;
	CComboBox m_idBox, m_hrefBox, m_imageTitleBox, m_sectionBox, m_tableIdBox, m_tableStyleBox, m_cellIdBox, m_cellStyleBox, m_colspanBox, m_rowspanBox, m_rowAlignBox, m_alignBox, m_valignBox;
	CCustomEdit m_id, m_href, m_imageTitle, m_section, m_tableId, m_tableStyle, m_cellId, m_cellStyle, m_colspan, m_rowspan, m_rowAlign, m_align, m_valign;
	CCustomStatic m_idCaption, m_hrefCaption, m_sectionCaption, m_imageTitleCaption, m_tableIdCaption, m_tableStyleCaption, m_cellIdCaption, m_cellStyleCaption, m_colspanCaption, m_rowspanCaption, m_rowAlignCaption, m_alignCaption, m_valignCaption;
	void AddCaption(CCustomStatic& caption, HWND bar, int position, UINT textId, LPCWSTR placeholder, HFONT font);
	void AddBox(HWND bar, int position, CComboBox& box, CCustomEdit& edit, DWORD style, UINT id, HFONT font);
};
