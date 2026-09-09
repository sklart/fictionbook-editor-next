#pragma once

#include <cstdint>
#include <atlctrls.h>
#include "resource.h"
#include "SettingsTooltips.h"

class CFBEView;

// A child of CMainFrame, never an owned popup. It owns presentation state only;
// offsets, snapshots and revisions remain in CFBEView's coordinator.
class CFindResultsPane : public CWindowImpl<CFindResultsPane>
{
public:
	DECLARE_WND_CLASS_EX(L"FBEFindResultsPane", CS_DBLCLKS, COLOR_WINDOW)
	BEGIN_MSG_MAP(CFindResultsPane)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		COMMAND_ID_HANDLER(IDCANCEL, OnHide)
		NOTIFY_HANDLER(IDC_FIND_RESULTS_LIST, LVN_ITEMACTIVATE, OnItemActivate)
		NOTIFY_HANDLER(IDC_FIND_RESULTS_LIST, NM_CUSTOMDRAW, OnListCustomDraw)
	END_MSG_MAP()

	void Attach(CFBEView* view);
	void Detach(CFBEView* view = NULL);
	void Refresh();
	void ApplyDpi();
	CFBEView* AttachedView() const { return m_view; }

private:
	void UpdateHeader();
	void LayoutChildren();
	int Scale(int logicalPixels) const;
	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnHide(WORD, WORD, HWND, BOOL&);
	LRESULT OnItemActivate(int, LPNMHDR, BOOL&);
	LRESULT OnListCustomDraw(int, LPNMHDR, BOOL&);
	CFBEView* m_view = NULL;
	CStatic m_header;
	CButton m_close;
	CListViewCtrl m_list;
	CStatic m_status;
	CSettingsTooltips m_tooltips;
	std::uint64_t m_revision = 0;
};
