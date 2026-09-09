#include "stdafx.h"
#include "ArchiveEntryPicker.h"
#include "RuntimeLocalization.h"

LRESULT CArchiveEntryPicker::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	SetWindowText(FbeLoadRuntimeStringByKey(L"fbe.archive.picker.caption", L"Select a FictionBook document"));
	::SetWindowText(GetDlgItem(IDOK), FbeLoadRuntimeStringByKey(L"fbe.archive.picker.open", L"Open"));
	::SetWindowText(GetDlgItem(IDCANCEL), FbeLoadRuntimeStringByKey(L"fbe.archive.picker.cancel", L"Cancel"));
	m_list.Attach(GetDlgItem(IDC_ARCHIVE_ENTRY_LIST));
	m_list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list.InsertColumn(0, FbeLoadRuntimeStringByKey(L"fbe.archive.picker.filename", L"Filename"), LVCFMT_LEFT, 125);
	m_list.InsertColumn(1, FbeLoadRuntimeStringByKey(L"fbe.archive.picker.path", L"Internal path"), LVCFMT_LEFT, 220);
	m_list.InsertColumn(2, FbeLoadRuntimeStringByKey(L"fbe.archive.picker.size", L"Size"), LVCFMT_RIGHT, 65);
	for (size_t index = 0; index < m_entries.size(); ++index)
	{
		const FbeArchive::Entry& entry = m_entries[index];
		const int slash = max(entry.path.ReverseFind(L'\\'), entry.path.ReverseFind(L'/'));
		const CString filename = slash >= 0 ? entry.path.Mid(slash + 1) : entry.path;
		const int item = m_list.InsertItem(static_cast<int>(index), filename);
		m_list.SetItemText(item, 1, entry.path);
		CString size; size.Format(L"%I64u", entry.uncompressedSize);
		m_list.SetItemText(item, 2, size);
	}
	if (!m_entries.empty()) m_list.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	CenterWindow(GetParent());
	return TRUE;
}

LRESULT CArchiveEntryPicker::OnOk(WORD, WORD, HWND, BOOL&)
{
	m_selected = m_list.GetNextItem(-1, LVNI_SELECTED);
	if (m_selected >= 0) EndDialog(IDOK);
	return 0;
}
LRESULT CArchiveEntryPicker::OnCancel(WORD, WORD, HWND, BOOL&) { EndDialog(IDCANCEL); return 0; }
LRESULT CArchiveEntryPicker::OnDoubleClick(int, LPNMHDR, BOOL& handled) { handled = TRUE; BOOL ignored = FALSE; return OnOk(0, IDOK, NULL, ignored); }
