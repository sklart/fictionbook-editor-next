#include "stdafx.h"
#include "ArchiveEntryPicker.h"
#include "RuntimeLocalization.h"

namespace
{
	CString EntryFilename(const CString& path)
	{
		const int slash = max(path.ReverseFind(L'\\'), path.ReverseFind(L'/'));
		return slash >= 0 ? path.Mid(slash + 1) : path;
	}

	CString EntryFolder(const CString& path)
	{
		const int slash = max(path.ReverseFind(L'\\'), path.ReverseFind(L'/'));
		if (slash < 0) return CString();
		CString folder = path.Left(slash + 1);
		folder.Replace(L'/', L'\\');
		return folder;
	}

	CString FormatEntrySize(ULONGLONG bytes)
	{
		static const wchar_t* const keys[] = {
			L"fbe.archive.picker.size.bytes", L"fbe.archive.picker.size.kilobytes",
			L"fbe.archive.picker.size.megabytes", L"fbe.archive.picker.size.gigabytes" };
		static const wchar_t* const fallbacks[] = { L"B", L"KB", L"MB", L"GB" };
		double value = static_cast<double>(bytes);
		int unit = 0;
		while (value >= 1024.0 && unit < 3) { value /= 1024.0; ++unit; }
		CString number;
		if (unit == 0) number.Format(L"%.0f", value);
		else number.Format(value < 10.0 ? L"%.1f" : L"%.0f", value);
		CString result;
		result.Format(L"%s %s", number.GetString(), FbeLoadRuntimeStringByKey(keys[unit], fallbacks[unit]).GetString());
		return result;
	}
}

LRESULT CArchiveEntryPicker::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	SetWindowText(FbeLoadRuntimeStringByKey(L"fbe.archive.picker.caption", L"Select a book from the archive"));
	::SetWindowText(GetDlgItem(IDC_ARCHIVE_ENTRY_MESSAGE), FbeLoadRuntimeStringByKey(L"fbe.archive.picker.message", L"Several FictionBook documents were found in the archive. Select the book to open."));
	::SetWindowText(GetDlgItem(IDOK), FbeLoadRuntimeStringByKey(L"fbe.archive.picker.open", L"Open"));
	::SetWindowText(GetDlgItem(IDCANCEL), FbeLoadRuntimeStringByKey(L"fbe.archive.picker.cancel", L"Cancel"));
	m_list.Attach(GetDlgItem(IDC_ARCHIVE_ENTRY_LIST));
	m_list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_hasFolders = false;
	for (const FbeArchive::Entry& entry : m_entries) if (!EntryFolder(entry.path).IsEmpty()) { m_hasFolders = true; break; }
	m_list.InsertColumn(0, FbeLoadRuntimeStringByKey(L"fbe.archive.picker.filename", L"Book"), LVCFMT_LEFT, 125);
	if (m_hasFolders) m_list.InsertColumn(1, FbeLoadRuntimeStringByKey(L"fbe.archive.picker.folder", L"Folder in archive"), LVCFMT_LEFT, 140);
	m_list.InsertColumn(m_hasFolders ? 2 : 1, FbeLoadRuntimeStringByKey(L"fbe.archive.picker.size", L"Size"), LVCFMT_RIGHT, 65);
	for (size_t index = 0; index < m_entries.size(); ++index)
	{
		const FbeArchive::Entry& entry = m_entries[index];
		const int item = m_list.InsertItem(static_cast<int>(index), EntryFilename(entry.path));
		const int sizeColumn = m_hasFolders ? 2 : 1;
		if (m_hasFolders) m_list.SetItemText(item, 1, EntryFolder(entry.path));
		m_list.SetItemText(item, sizeColumn, FormatEntrySize(entry.uncompressedSize));
	}
	for (int column = 0; column < (m_hasFolders ? 3 : 2); ++column)
		m_list.SetColumnWidth(column, LVSCW_AUTOSIZE_USEHEADER);
	if (!m_entries.empty()) {
		m_list.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_list.SetFocus();
	}
	RECT client = {}; GetClientRect(&client); LayoutControls(client.right, client.bottom);
	HWND owner = GetParent(); if (!owner) owner = ::GetActiveWindow();
	CenterWindow(owner);
	return FALSE;
}

LRESULT CArchiveEntryPicker::OnSize(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	LayoutControls(LOWORD(lParam), HIWORD(lParam));
	return 0;
}

void CArchiveEntryPicker::LayoutControls(int width, int height)
{
	if (!m_list.IsWindow()) return;
	const int margin = 8, messageHeight = 32, buttonWidth = 76, buttonHeight = 24, gap = 8;
	::SetWindowPos(GetDlgItem(IDC_ARCHIVE_ENTRY_MESSAGE), NULL, margin, margin, max(0, width - margin * 2), messageHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	const int buttonsY = max(margin, height - margin - buttonHeight);
	::SetWindowPos(m_list, NULL, margin, margin + messageHeight + 4, max(0, width - margin * 2), max(0, buttonsY - (margin + messageHeight + 4) - gap), SWP_NOZORDER | SWP_NOACTIVATE);
	::SetWindowPos(GetDlgItem(IDCANCEL), NULL, width - margin - buttonWidth, buttonsY, buttonWidth, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	::SetWindowPos(GetDlgItem(IDOK), NULL, width - margin * 2 - buttonWidth * 2, buttonsY, buttonWidth, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CArchiveEntryPicker::OnOk(WORD, WORD, HWND, BOOL&)
{
	m_selected = m_list.GetNextItem(-1, LVNI_SELECTED);
	if (m_selected >= 0) EndDialog(IDOK);
	return 0;
}
LRESULT CArchiveEntryPicker::OnCancel(WORD, WORD, HWND, BOOL&) { EndDialog(IDCANCEL); return 0; }
LRESULT CArchiveEntryPicker::OnDoubleClick(int, LPNMHDR, BOOL& handled) { handled = TRUE; BOOL ignored = FALSE; return OnOk(0, IDOK, NULL, ignored); }
