#include "stdafx.h"
#include "HtmlExportOptionsDialog.h"
#include "RuntimeLocalization.h"
#include "TemplateResolver.h"
#include "resource.h"

namespace
{
UINT OptionsWindowDpi(HWND window)
{
    using GetDpiForWindowProc = UINT (WINAPI*)(HWND);
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const auto getDpi = user32 ? reinterpret_cast<GetDpiForWindowProc>(
        GetProcAddress(user32, "GetDpiForWindow")) : nullptr;
    if (getDpi)
    {
        const UINT dpi = getDpi(window);
        if (dpi) return dpi;
    }
    HDC dc = GetDC(window);
    const UINT dpi = dc ? static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX)) : 96;
    if (dc) ReleaseDC(window, dc);
    return dpi ? dpi : 96;
}
}

void CHtmlExportOptionsDialog::LoadSettings()
{
    HtmlExportSettingsStore::Load(_Settings, m_settings);
    m_workingSettings=m_settings;
}
void CHtmlExportOptionsDialog::Persist()
{
    HtmlExportSettingsStore::Save(_Settings,m_settings);
}
LRESULT CHtmlExportOptionsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
    SetWindowText(LoadExportHtmlString(IDS_HTML_EXPORT_OPTIONS_TITLE));
    CTabCtrl tabs = GetDlgItem(IDC_OPTIONS_TABS);
    const UINT titles[] = { IDS_OPTIONS_TAB_GENERAL, IDS_OPTIONS_TAB_APPEARANCE, IDS_OPTIONS_TAB_IMAGES, IDS_OPTIONS_TAB_NOTES };
    for (int index = 0; index < static_cast<int>(_countof(titles)); ++index) {
        TCITEM item = {};
        item.mask = TCIF_TEXT;
        CString title = LoadExportHtmlString(titles[index]);
        item.pszText = const_cast<LPTSTR>(static_cast<LPCTSTR>(title));
        tabs.InsertItem(index, &item);
    }
    m_generalPage.Attach(&m_workingSettings);
    m_generalPage.SetSplitSupported(m_splitSupported);
    m_appearancePage.Attach(&m_workingSettings);
    m_imagesPage.Attach(&m_workingSettings);
    m_notesPage.Attach(&m_workingSettings);
    m_generalPage.Create(m_hWnd);
    m_appearancePage.Create(m_hWnd);
    m_imagesPage.Create(m_hWnd);
    m_notesPage.Create(m_hWnd);
    CapturePageLayouts();
    CRect client, window;
    GetClientRect(client);
    GetWindowRect(window);
    m_initialClient = client.Size();
    m_minimumWindow = window.Size();
    m_initialDpi = m_layoutDpi = OptionsWindowDpi(m_hWnd);
    auto initialRect = [this](int id) {
        CRect rect;
        GetDlgItem(id).GetWindowRect(rect);
        ScreenToClient(rect);
        return rect;
    };
    m_initialTabs = initialRect(IDC_OPTIONS_TABS);
    m_initialOk = initialRect(IDOK);
    m_initialCancel = initialRect(IDCANCEL);
    MONITORINFO monitor = { sizeof(monitor) };
    if (GetMonitorInfoW(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &monitor))
    {
        const int width = min(window.Width(), monitor.rcWork.right - monitor.rcWork.left);
        const int height = min(window.Height(), monitor.rcWork.bottom - monitor.rcWork.top);
        const int x = min(max(window.left, monitor.rcWork.left), monitor.rcWork.right - width);
        const int y = min(max(window.top, monitor.rcWork.top), monitor.rcWork.bottom - height);
        SetWindowPos(NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    LayoutChrome();
    LayoutPages();
    SelectPage(0);
    return TRUE;
}
LRESULT CHtmlExportOptionsDialog::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    LayoutChrome();
    LayoutPages();
    return 0;
}
LRESULT CHtmlExportOptionsDialog::OnDpiChanged(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    m_layoutDpi = LOWORD(wParam);
    const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
    if (suggested)
    {
        MONITORINFO monitor = { sizeof(monitor) };
        if (GetMonitorInfoW(MonitorFromRect(suggested, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            const int width = min(suggested->right - suggested->left, monitor.rcWork.right - monitor.rcWork.left);
            const int height = min(suggested->bottom - suggested->top, monitor.rcWork.bottom - monitor.rcWork.top);
            const int x = min(max(suggested->left, monitor.rcWork.left), monitor.rcWork.right - width);
            const int y = min(max(suggested->top, monitor.rcWork.top), monitor.rcWork.bottom - height);
            SetWindowPos(NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    LayoutChrome();
    LayoutPages();
    return 0;
}

LRESULT CHtmlExportOptionsDialog::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    if (!m_initialDpi || !lParam) return 0;
    auto* bounds = reinterpret_cast<MINMAXINFO*>(lParam);
    MONITORINFO monitor = { sizeof(monitor) };
    if (GetMonitorInfoW(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &monitor))
    {
        const UINT dpi = OptionsWindowDpi(m_hWnd);
        bounds->ptMinTrackSize.x = min(MulDiv(m_minimumWindow.cx, dpi, m_initialDpi),
            monitor.rcWork.right - monitor.rcWork.left);
        bounds->ptMinTrackSize.y = min(MulDiv(m_minimumWindow.cy, dpi, m_initialDpi),
            monitor.rcWork.bottom - monitor.rcWork.top);
    }
    return 0;
}
LRESULT CHtmlExportOptionsDialog::OnTabChanged(int, LPNMHDR, BOOL&)
{
    SelectPage(CTabCtrl(GetDlgItem(IDC_OPTIONS_TABS)).GetCurSel());
    return 0;
}
void CHtmlExportOptionsDialog::LayoutPages()
{
    CTabCtrl tabs = GetDlgItem(IDC_OPTIONS_TABS);
    if (!tabs.IsWindow()) return;
    CRect page;
    tabs.GetWindowRect(&page);
    ScreenToClient(&page);
    tabs.AdjustRect(FALSE, &page);
    HWND pages[] = { m_generalPage.m_hWnd, m_appearancePage.m_hWnd, m_imagesPage.m_hWnd, m_notesPage.m_hWnd };
    for (size_t index = 0; index < _countof(pages); ++index)
        if (pages[index]) {
            ::SetWindowPos(pages[index], NULL, page.left, page.top, page.Width(), page.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
            LayoutPageFields(static_cast<int>(index), pages[index]);
        }
}

void CHtmlExportOptionsDialog::CapturePageLayouts()
{
    HWND pages[] = { m_generalPage.m_hWnd, m_appearancePage.m_hWnd, m_imagesPage.m_hWnd, m_notesPage.m_hWnd };
    for (int index = 0; index < static_cast<int>(_countof(pages)); ++index)
    {
        RECT client = {};
        ::GetClientRect(pages[index], &client);
        m_pageLayouts[index].initialSize = CSize(client.right, client.bottom);
        for (HWND child = ::GetWindow(pages[index], GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
        {
            RECT rect = {};
            ::GetWindowRect(child, &rect);
            ::MapWindowPoints(NULL, pages[index], reinterpret_cast<POINT*>(&rect), 2);
            m_pageLayouts[index].controls.push_back({ child, CRect(rect), ::GetDlgCtrlID(child) });
        }
    }
}

void CHtmlExportOptionsDialog::LayoutPageFields(int index, HWND page)
{
    if (!m_initialDpi || !page) return;
    RECT client = {};
    ::GetClientRect(page, &client);
    const PageLayout& layout = m_pageLayouts[index];
    const auto scale = [this](int value) { return MulDiv(value, m_layoutDpi, m_initialDpi); };
    const int extraWidth = max(0, client.right - scale(layout.initialSize.cx));
    for (const PageControlLayout& control : layout.controls)
    {
        CRect rect(scale(control.initial.left), scale(control.initial.top),
            scale(control.initial.right), scale(control.initial.bottom));
        const bool stretch = (index == 0 && (control.id == IDC_TEMPLATE || control.id == IDC_DOCUMENT_STRUCTURE)) ||
            (index == 1 && control.id == IDC_CUSTOM_CSS) ||
            (index == 2 && (control.id == IDC_COVER_MODE || control.id == IDC_IMAGES_FOLDER ||
                control.id == IDC_IMAGES_FOLDER_NAME)) ||
            (index == 3 && control.id == IDC_NOTE_PLACEMENT);
        const bool move = (index == 0 && control.id == IDC_BROWSE) ||
            (index == 1 && (control.id == IDC_BROWSE_CSS || control.id == IDC_CLEAR_CSS));
        if (stretch) rect.right += extraWidth;
        else if (move) rect.OffsetRect(extraWidth, 0);
        ::SetWindowPos(control.window, NULL, rect.left, rect.top, rect.Width(), rect.Height(),
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void CHtmlExportOptionsDialog::LayoutChrome()
{
    if (!m_initialDpi || !GetDlgItem(IDC_OPTIONS_TABS).IsWindow()) return;
    CRect client;
    GetClientRect(client);
    const auto scale = [this](int value) { return MulDiv(value, m_layoutDpi, m_initialDpi); };
    const int extraWidth = max(0, client.Width() - scale(m_initialClient.cx));
    const int extraHeight = max(0, client.Height() - scale(m_initialClient.cy));
    GetDlgItem(IDC_OPTIONS_TABS).MoveWindow(scale(m_initialTabs.left), scale(m_initialTabs.top),
        scale(m_initialTabs.Width()) + extraWidth, scale(m_initialTabs.Height()) + extraHeight);
    GetDlgItem(IDOK).MoveWindow(client.right - scale(m_initialClient.cx - m_initialOk.left),
        client.bottom - scale(m_initialClient.cy - m_initialOk.top),
        scale(m_initialOk.Width()), scale(m_initialOk.Height()));
    GetDlgItem(IDCANCEL).MoveWindow(client.right - scale(m_initialClient.cx - m_initialCancel.left),
        client.bottom - scale(m_initialClient.cy - m_initialCancel.top),
        scale(m_initialCancel.Width()), scale(m_initialCancel.Height()));
}
void CHtmlExportOptionsDialog::SelectPage(int page)
{
    if (page < 0 || page > 3) return;
    m_currentPage = page;
    CTabCtrl(GetDlgItem(IDC_OPTIONS_TABS)).SetCurSel(page);
    HWND pages[] = { m_generalPage.m_hWnd, m_appearancePage.m_hWnd, m_imagesPage.m_hWnd, m_notesPage.m_hWnd };
    for (int index = 0; index < static_cast<int>(_countof(pages)); ++index)
        if (pages[index]) ::ShowWindow(pages[index], index == page ? SW_SHOW : SW_HIDE);
}
LRESULT CHtmlExportOptionsDialog::OnOk(WORD, WORD, HWND, BOOL&)
{
    HtmlExportSettings candidate = m_workingSettings;
    CString error;
    if (!m_generalPage.SaveToSettings(candidate, error)) { SelectPage(0); ShowExportHtmlTaskDialog(m_hWnd, IDS_ERROR, NULL, error, TDCBF_OK_BUTTON, TD_ERROR_ICON); return 0; }
    if (!m_appearancePage.SaveToSettings(candidate, error)) { SelectPage(1); ShowExportHtmlTaskDialog(m_hWnd, IDS_ERROR, NULL, error, TDCBF_OK_BUTTON, TD_ERROR_ICON); return 0; }
    if (!m_imagesPage.SaveToSettings(candidate, error)) { SelectPage(2); ShowExportHtmlTaskDialog(m_hWnd, IDS_ERROR, NULL, error, TDCBF_OK_BUTTON, TD_ERROR_ICON); return 0; }
    if (!m_notesPage.SaveToSettings(candidate, error)) { SelectPage(3); ShowExportHtmlTaskDialog(m_hWnd, IDS_ERROR, NULL, error, TDCBF_OK_BUTTON, TD_ERROR_ICON); return 0; }
    m_workingSettings = candidate;
    m_settings = m_workingSettings;
    EndDialog(IDOK);
    return 0;
}
LRESULT CHtmlExportOptionsDialog::OnCancel(WORD, WORD, HWND, BOOL&) { EndDialog(IDCANCEL); return 0; }
STDMETHODIMP CHtmlFileDialogEvents::OnButtonClicked(IFileDialogCustomize* customize, DWORD id)
{
    if (id==SettingsButtonId && options) {
        HWND h=owner;
        CComPtr<IOleWindow> w;
        if(customize && SUCCEEDED(customize->QueryInterface(IID_PPV_ARGS(&w)))) w->GetWindow(&h);
        options->DoModal(h);
        if (customize) {
            CComPtr<IFileDialog> fileDialog;
            if (SUCCEEDED(customize->QueryInterface(IID_PPV_ARGS(&fileDialog))) && fileDialog)
                fileDialog->SetOkButtonLabel(LoadExportHtmlString(IDS_SAVE_BUTTON));
        }
    }
    return S_OK;
}
