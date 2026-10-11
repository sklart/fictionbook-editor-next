#include "stdafx.h"
#include "ArchiveEntryPicker.h"
#include "RuntimeLocalization.h"
#include "UiMetrics.h"

namespace
{
	CString EntryFilename(const CString& path)
	{
		const int slash = max(path.ReverseFind(L'\\'), path.ReverseFind(L'/'));
		return slash >= 0 ? path.Mid(slash + 1) : path;
	}

	int ScaleForWindow(HWND window, int logicalPixels)
	{
		return UiMetrics::ScaleForDpi(logicalPixels, UiMetrics::DpiForWindow(window));
	}

	int ControlTextWidth(HWND control)
	{
		CString text;
		const int length = ::GetWindowTextLengthW(control);
		::GetWindowTextW(control, text.GetBuffer(length + 1), length + 1);
		text.ReleaseBuffer();
		HDC dc = ::GetDC(control);
		if (!dc) return 0;
		HFONT font = reinterpret_cast<HFONT>(::SendMessage(control, WM_GETFONT, 0, 0));
		HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
		SIZE size = {};
		::GetTextExtentPoint32W(dc, text, text.GetLength(), &size);
		if (oldFont) ::SelectObject(dc, oldFont);
		::ReleaseDC(control, dc);
		return size.cx;
	}

	int MessageHeight(HWND control, int width)
	{
		CString text;
		const int length = ::GetWindowTextLengthW(control);
		::GetWindowTextW(control, text.GetBuffer(length + 1), length + 1);
		text.ReleaseBuffer();
		HDC dc = ::GetDC(control);
		if (!dc) return 0;
		HFONT font = reinterpret_cast<HFONT>(::SendMessage(control, WM_GETFONT, 0, 0));
		HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
		RECT bounds = { 0, 0, max(1, width), 0 };
		::DrawTextW(dc, text, text.GetLength(), &bounds, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
		if (oldFont) ::SelectObject(dc, oldFont);
		::ReleaseDC(control, dc);
		return bounds.bottom - bounds.top;
	}

	int VerticalScrollWidth(HWND window)
	{
		typedef int (WINAPI* GetSystemMetricsForDpiProc)(int, UINT);
		HMODULE user32 = ::GetModuleHandle(L"user32.dll");
		GetSystemMetricsForDpiProc getMetric = user32
			? reinterpret_cast<GetSystemMetricsForDpiProc>(::GetProcAddress(user32, "GetSystemMetricsForDpi")) : NULL;
		return getMetric ? getMetric(SM_CXVSCROLL, UiMetrics::DpiForWindow(window)) : ::GetSystemMetrics(SM_CXVSCROLL);
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
	ApplyRuntimeTexts();
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
	if (!m_entries.empty()) {
		m_list.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_list.SetFocus();
	}
	RECT item = {};
	const int rowHeight = (m_list.GetItemCount() != 0 && m_list.GetItemRect(0, &item, LVIR_BOUNDS)) ? item.bottom - item.top : 18;
	const int visibleRows = min(max(static_cast<int>(m_entries.size()), 5), 10);
	RECT client = {}, window = {}; GetClientRect(&client); GetWindowRect(&window);
	const int listHeight = ScaleForWindow(m_hWnd, 24) + visibleRows * rowHeight + ScaleForWindow(m_hWnd, 4);
	const int sizeColumn = m_hasFolders ? 2 : 1;
	const int contentWidth = PreferredColumnWidth(0, ScaleForWindow(m_hWnd, 390)) +
		(m_hasFolders ? PreferredColumnWidth(1, ScaleForWindow(m_hWnd, 180)) : 0) +
		PreferredColumnWidth(sizeColumn, ScaleForWindow(m_hWnd, 72)) + ScaleForWindow(m_hWnd, 16);
	MONITORINFO monitor = { sizeof(monitor) };
	HWND owner = GetParent(); if (!owner) owner = ::GetActiveWindow();
	const HMONITOR currentMonitor = ::MonitorFromWindow(owner ? owner : m_hWnd, MONITOR_DEFAULTTONEAREST);
	const int nonClientWidth = window.right - window.left - client.right;
	const int nonClientHeight = window.bottom - window.top - client.bottom;
	const bool hasMonitor = ::GetMonitorInfo(currentMonitor, &monitor) != FALSE;
	const int workWidth = hasMonitor ? monitor.rcWork.right - monitor.rcWork.left : window.right - window.left;
	const int workHeight = hasMonitor ? monitor.rcWork.bottom - monitor.rcWork.top : window.bottom - window.top;
	const int maximumWindowWidth = workWidth * 4 / 5;
	const int maximumClientWidth = max(0, maximumWindowWidth - nonClientWidth);
	const int buttonWidth = max(ScaleForWindow(m_hWnd, 76),
		max(ControlTextWidth(GetDlgItem(IDOK)), ControlTextWidth(GetDlgItem(IDCANCEL))) + ScaleForWindow(m_hWnd, 24));
	const int buttonMinimumWidth = buttonWidth * 2 + ScaleForWindow(m_hWnd, 24);
	const int desiredClientWidth = min(maximumClientWidth, max(max(client.right, contentWidth), buttonMinimumWidth));
	const int margin = ScaleForWindow(m_hWnd, 8);
	const int messageHeight = max(ScaleForWindow(m_hWnd, 32),
		MessageHeight(GetDlgItem(IDC_ARCHIVE_ENTRY_MESSAGE), desiredClientWidth - margin * 2));
	const int desiredClientHeight = max(ScaleForWindow(m_hWnd, 240),
		margin + messageHeight + ScaleForWindow(m_hWnd, 4) + listHeight +
		ScaleForWindow(m_hWnd, 8) + ScaleForWindow(m_hWnd, 24) + margin);
	const int desiredWindowHeight = min(desiredClientHeight + nonClientHeight, workHeight);
	SetWindowPos(NULL, 0, 0, desiredClientWidth + nonClientWidth, desiredWindowHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	GetClientRect(&client); LayoutControls(client.right, client.bottom);
	CenterWindow(owner);
	if (hasMonitor)
	{
		GetWindowRect(&window);
		const int x = max(monitor.rcWork.left, min(window.left, monitor.rcWork.right - (window.right - window.left)));
		const int y = max(monitor.rcWork.top, min(window.top, monitor.rcWork.bottom - (window.bottom - window.top)));
		SetWindowPos(NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	return FALSE;
}

LRESULT CArchiveEntryPicker::OnSize(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	LayoutControls(LOWORD(lParam), HIWORD(lParam));
	return 0;
}

LRESULT CArchiveEntryPicker::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	MINMAXINFO* const info = reinterpret_cast<MINMAXINFO*>(lParam);
	MONITORINFO monitor = { sizeof(monitor) };
	const bool hasMonitor = ::GetMonitorInfo(::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &monitor) != FALSE;
	const int workWidth = hasMonitor ? monitor.rcWork.right - monitor.rcWork.left : ScaleForWindow(m_hWnd, 600);
	const int workHeight = hasMonitor ? monitor.rcWork.bottom - monitor.rcWork.top : ScaleForWindow(m_hWnd, 260);
	info->ptMinTrackSize.x = min(ScaleForWindow(m_hWnd, 600), workWidth);
	info->ptMinTrackSize.y = min(ScaleForWindow(m_hWnd, 260), workHeight);
	return 0;
}

LRESULT CArchiveEntryPicker::OnShowWindow(UINT, WPARAM, LPARAM, BOOL&)
{
	// Apply once more after dialog activation: this guarantees that a generic
	// IDOK caption from a framework-localizer cannot replace the picker action.
	ApplyRuntimeTexts();
	return 0;
}

void CArchiveEntryPicker::LayoutControls(int width, int height)
{
	if (!m_list.IsWindow()) return;
	const int margin = ScaleForWindow(m_hWnd, 8), buttonHeight = ScaleForWindow(m_hWnd, 24), gap = ScaleForWindow(m_hWnd, 8);
	const int buttonWidth = max(ScaleForWindow(m_hWnd, 76),
		max(ControlTextWidth(GetDlgItem(IDOK)), ControlTextWidth(GetDlgItem(IDCANCEL))) + ScaleForWindow(m_hWnd, 24));
	const int messageHeight = max(ScaleForWindow(m_hWnd, 32),
		MessageHeight(GetDlgItem(IDC_ARCHIVE_ENTRY_MESSAGE), width - margin * 2));
	::SetWindowPos(GetDlgItem(IDC_ARCHIVE_ENTRY_MESSAGE), NULL, margin, margin, max(0, width - margin * 2), messageHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	const int buttonsY = max(margin, height - margin - buttonHeight);
	const int listTop = margin + messageHeight + ScaleForWindow(m_hWnd, 4);
	::SetWindowPos(m_list, NULL, margin, listTop, max(0, width - margin * 2), max(0, buttonsY - listTop - gap), SWP_NOZORDER | SWP_NOACTIVATE);
	::SetWindowPos(GetDlgItem(IDCANCEL), NULL, width - margin - buttonWidth, buttonsY, buttonWidth, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	::SetWindowPos(GetDlgItem(IDOK), NULL, width - margin * 2 - buttonWidth * 2, buttonsY, buttonWidth, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	LayoutColumns();
}

void CArchiveEntryPicker::LayoutColumns()
{
	if (!m_list.IsWindow()) return;
	RECT client = {}; m_list.GetClientRect(&client);
	int available = max(0, client.right - client.left);
	RECT item = {};
	const int itemHeight = (m_list.GetItemCount() != 0 && m_list.GetItemRect(0, &item, LVIR_BOUNDS)) ? item.bottom - item.top : 0;
	RECT listClient = {}; m_list.GetClientRect(&listClient);
	if (itemHeight > 0 && m_list.GetItemCount() * itemHeight > listClient.bottom - listClient.top - ScaleForWindow(m_hWnd, 24))
		available = max(0, available - VerticalScrollWidth(m_hWnd));
	const int sizeColumn = m_hasFolders ? 2 : 1;
	const int sizeWidth = min(ScaleForWindow(m_hWnd, 96), PreferredColumnWidth(sizeColumn, ScaleForWindow(m_hWnd, 72)));
	const int remainder = max(0, available - sizeWidth);
	m_list.SetColumnWidth(sizeColumn, sizeWidth);
	if (m_hasFolders)
	{
		const int bookWidth = remainder * 3 / 5;
		m_list.SetColumnWidth(0, bookWidth);
		m_list.SetColumnWidth(1, remainder - bookWidth);
	}
	else m_list.SetColumnWidth(0, remainder);
}

int CArchiveEntryPicker::PreferredColumnWidth(int column, int minimum)
{
	int width = minimum;
	for (int item = 0; item < m_list.GetItemCount(); ++item)
	{
		wchar_t text[1024] = {};
		m_list.GetItemText(item, column, text, _countof(text));
		width = max(width, m_list.GetStringWidth(text) + ScaleForWindow(m_hWnd, 16));
	}
	return width;
}

void CArchiveEntryPicker::ApplyRuntimeTexts()
{
	SetWindowText(FbeLoadRuntimeStringByKey(L"fbe.archive.picker.caption", L"Select a book from the archive"));
	::SetWindowText(GetDlgItem(IDC_ARCHIVE_ENTRY_MESSAGE), FbeLoadRuntimeStringByKey(L"fbe.archive.picker.message", L"Several FictionBook documents were found in the archive. Select the book to open."));
	::SetWindowText(GetDlgItem(IDOK), FbeLoadRuntimeStringByKey(L"fbe.archive.picker.open", L"Open"));
	::SetWindowText(GetDlgItem(IDCANCEL), FbeLoadRuntimeStringByKey(L"fbe.archive.picker.cancel", L"Cancel"));
}

LRESULT CArchiveEntryPicker::OnOk(WORD, WORD, HWND, BOOL&)
{
	m_selected = m_list.GetNextItem(-1, LVNI_SELECTED);
	if (m_selected >= 0) EndDialog(IDOK);
	return 0;
}
LRESULT CArchiveEntryPicker::OnCancel(WORD, WORD, HWND, BOOL&) { EndDialog(IDCANCEL); return 0; }
LRESULT CArchiveEntryPicker::OnDoubleClick(int, LPNMHDR, BOOL& handled) { handled = TRUE; BOOL ignored = FALSE; return OnOk(0, IDOK, NULL, ignored); }
