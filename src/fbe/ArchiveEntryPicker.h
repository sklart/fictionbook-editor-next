#pragma once

#include "resource.h"
#include "archive\\ArchiveReader.h"

class CArchiveEntryPicker : public CDialogImpl<CArchiveEntryPicker>
{
public:
	enum { IDD = IDD_ARCHIVE_ENTRY };
	explicit CArchiveEntryPicker(const std::vector<FbeArchive::Entry>& entries) : m_entries(entries), m_selected(-1) {}
	int SelectedIndex() const { return m_selected; }
	BEGIN_MSG_MAP(CArchiveEntryPicker)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
		COMMAND_ID_HANDLER(IDOK, OnOk)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		NOTIFY_HANDLER(IDC_ARCHIVE_ENTRY_LIST, NM_DBLCLK, OnDoubleClick)
	END_MSG_MAP()
private:
	const std::vector<FbeArchive::Entry>& m_entries;
	CListViewCtrl m_list;
	int m_selected;
	bool m_hasFolders;
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnOk(WORD, WORD, HWND, BOOL&);
	LRESULT OnCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnDoubleClick(int, LPNMHDR, BOOL&);
	void LayoutControls(int width, int height);
};
