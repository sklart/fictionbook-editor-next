#include "stdafx.h"
#include "FindResultsPane.h"
#include "apputils.h"
#include "FBEView.h"
#include "RuntimeLocalization.h"
#include "UiMetrics.h"

int CFindResultsPane::Scale(int logicalPixels) const { return UiMetrics::Scale(logicalPixels); }

LRESULT CFindResultsPane::OnCreate(UINT, WPARAM, LPARAM, BOOL&)
{
	m_header.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS, 0, IDC_STATIC);
	m_close.Create(m_hWnd, rcDefault, L"\x00D7", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, IDCANCEL);
	m_list.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL, 0, IDC_FIND_RESULTS_LIST);
	m_status.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS, 0, IDC_FIND_RESULTS_STATUS);
	m_list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_tooltips.Initialize(m_hWnd);
	m_tooltips.Add(m_close, L"fbe.dialog.idd_find_results.close", L"Close Results pane");
	m_list.InsertColumn(0, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.number", L"#"), LVCFMT_RIGHT, Scale(38));
	m_list.InsertColumn(1, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.context", L"Context"), LVCFMT_LEFT, Scale(260));
	ApplyDpi();
	return 0;
}

void CFindResultsPane::ApplyDpi()
{
	if (!m_hWnd) return;
	HFONT font = UiMetrics::DialogFont();
	if (font) { m_header.SetFont(font); m_close.SetFont(font); m_list.SetFont(font); m_status.SetFont(font); }
	LayoutChildren();
}

void CFindResultsPane::LayoutChildren()
{
	if (!m_list.IsWindow()) return;
	RECT client = {}; GetClientRect(&client);
	const int margin = Scale(6), headerHeight = Scale(22), footerHeight = Scale(19), closeWidth = Scale(24), closeHeight = Scale(20);
	const int width = (std::max)(0, static_cast<int>(client.right - client.left));
	const int height = (std::max)(0, static_cast<int>(client.bottom - client.top));
	m_header.SetWindowPos(HWND_TOP, margin, margin, (std::max)(0, width - 3 * margin - closeWidth), headerHeight, SWP_NOZORDER);
	m_close.SetWindowPos(HWND_TOP, width - margin - closeWidth, margin, closeWidth, closeHeight, SWP_NOZORDER);
	const int listTop = margin + headerHeight + Scale(3), statusTop = (std::max)(listTop, height - margin - footerHeight);
	m_list.SetWindowPos(HWND_TOP, margin, listTop, (std::max)(0, width - 2 * margin), (std::max)(0, statusTop - listTop - Scale(3)), SWP_NOZORDER);
	m_status.SetWindowPos(HWND_TOP, margin, statusTop, (std::max)(0, width - 2 * margin), footerHeight, SWP_NOZORDER);
	m_list.SetColumnWidth(0, Scale(38));
	m_list.SetColumnWidth(1, (std::max)(0, width - 2 * margin - Scale(38)));
}

LRESULT CFindResultsPane::OnSize(UINT, WPARAM, LPARAM, BOOL&) { LayoutChildren(); return 0; }

void CFindResultsPane::Attach(CFBEView* view) { if (m_view != view) { m_view = view; m_revision = 0; } Refresh(); }
void CFindResultsPane::Detach(CFBEView* view)
{
	if (view != NULL && view != m_view) return;
	m_view = NULL; m_revision = 0;
	if (m_list.IsWindow()) m_list.DeleteAllItems();
	if (m_header.IsWindow()) m_header.SetWindowText(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.caption", L"Find results"));
	if (m_status.IsWindow()) m_status.SetWindowText(L"");
}

void CFindResultsPane::UpdateHeader()
{
	if (!m_header.IsWindow()) return;
	CString title = FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.caption", L"Find results");
	if (m_view != NULL && m_view->AreFindResultsCurrent())
	{
		CString query(m_view->FindResultsQuery()); query.Trim();
		if (!query.IsEmpty()) title += L" — \x00AB" + query + L"\x00BB";
		CString count; count.Format(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.count", L"%Iu results"), m_view->FindResultCount());
		title += L" — " + count;
	}
	m_header.SetWindowText(title);
}

void CFindResultsPane::Refresh()
{
	if (!m_list.IsWindow()) return;
	m_list.DeleteAllItems(); UpdateHeader();
	if (m_view == NULL) { m_status.SetWindowText(L""); return; }
	if (!m_view->AreFindResultsCurrent()) { m_status.SetWindowText(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.stale", L"Search results are stale. Run Find All again.")); return; }
	m_revision = m_view->FindResultsRevision();
	for (std::size_t index = 0; index < m_view->FindResultCount(); ++index) { CString number; number.Format(L"%Iu", index + 1); const int item = m_list.InsertItem(static_cast<int>(index), number); m_list.SetItemText(item, 1, m_view->FindResultPreview(index)); }
	CString status; status.Format(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.count", L"%Iu results"), m_view->FindResultCount()); m_status.SetWindowText(status);
}

LRESULT CFindResultsPane::OnHide(WORD, WORD, HWND, BOOL&) { ::SendMessage(GetParent(), WM_APP + 43, 0, 0); return 0; }
LRESULT CFindResultsPane::OnItemActivate(int, LPNMHDR header, BOOL&)
{
	const NMLISTVIEW* item = reinterpret_cast<const NMLISTVIEW*>(header);
	if (m_view != NULL && item != NULL && item->iItem >= 0 && (m_revision != m_view->FindResultsRevision() || !m_view->SelectFindResult(static_cast<std::size_t>(item->iItem)))) Refresh();
	return 0;
}

LRESULT CFindResultsPane::OnListCustomDraw(int, LPNMHDR header, BOOL&)
{
	NMLVCUSTOMDRAW* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(header);
	if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
	if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) return CDRF_NOTIFYSUBITEMDRAW;
	if (m_view == NULL || draw->nmcd.dwDrawStage != (CDDS_ITEMPREPAINT | CDDS_SUBITEM) || draw->iSubItem != 1) return CDRF_DODEFAULT;
	std::size_t matchStart = 0, matchLength = 0; const int item = static_cast<int>(draw->nmcd.dwItemSpec);
	if (item < 0 || !m_view->FindResultPreviewMatch(static_cast<std::size_t>(item), &matchStart, &matchLength) || matchLength == 0) return CDRF_DODEFAULT;
	const CString text = m_view->FindResultPreview(static_cast<std::size_t>(item)); if (matchStart >= static_cast<std::size_t>(text.GetLength())) return CDRF_DODEFAULT;
	matchLength = (std::min)(matchLength, static_cast<std::size_t>(text.GetLength()) - matchStart); RECT cell = {}; if (!m_list.GetSubItemRect(item, 1, LVIR_LABEL, &cell)) return CDRF_DODEFAULT;
	cell.left += Scale(3); HDC dc = draw->nmcd.hdc; HFONT oldFont = static_cast<HFONT>(::SelectObject(dc, reinterpret_cast<HGDIOBJ>(::SendMessage(m_list, WM_GETFONT, 0, 0)))); const bool selected = (m_list.GetItemState(item, LVIS_SELECTED) & LVIS_SELECTED) != 0;
	::SetBkMode(dc, TRANSPARENT); ::SetTextColor(dc, selected ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : ::GetSysColor(COLOR_WINDOWTEXT)); ::DrawText(dc, text, text.GetLength(), &cell, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
	SIZE prefix = {}; ::GetTextExtentPoint32(dc, text, static_cast<int>(matchStart), &prefix); const CString matched = text.Mid(static_cast<int>(matchStart), static_cast<int>(matchLength)); SIZE matchedSize = {}; ::GetTextExtentPoint32(dc, matched, matched.GetLength(), &matchedSize);
	RECT highlight = cell; highlight.left += prefix.cx; highlight.right = highlight.left + matchedSize.cx;
	if (highlight.left < cell.right && highlight.right > cell.left) { HBRUSH brush = ::CreateSolidBrush(selected ? RGB(46, 112, 184) : RGB(255, 235, 120)); ::FillRect(dc, &highlight, brush); ::DeleteObject(brush); ::SetTextColor(dc, selected ? RGB(255, 255, 255) : RGB(100, 45, 0)); ::DrawText(dc, matched, matched.GetLength(), &highlight, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_NOCLIP); }
	::SelectObject(dc, oldFont); return CDRF_SKIPDEFAULT;
}
