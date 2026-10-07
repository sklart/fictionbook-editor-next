#include "stdafx.h"
#include "..\\..\\resource.h"
#include "RegexHelpDialog.h"
#include "RegexHelpMarkdown.h"
#include "..\\..\\RuntimeLocalization.h"
#include "..\\..\\Settings.h"
#include "..\\..\\ThemeManager.h"
#include <richedit.h>

extern CSettings _Settings;

namespace
{
const UINT WM_REGEX_HELP_RENDER = WM_APP + 211;

struct RegexHelpRenderMetrics
{
    ULONGLONG markdownReadMs = 0;
    ULONGLONG parseMs = 0;
    ULONGLONG renderMs = 0;
    ULONGLONG dialogFirstVisibleMs = 0;
    ULONGLONG totalReadyMs = 0;
    size_t blockCount = 0;
    bool deferred = false;
};

RECT DefaultHelpBounds(const RECT& work, const CSize& minimumSize)
{
    const int workWidth = static_cast<int>(work.right - work.left);
    const int workHeight = static_cast<int>(work.bottom - work.top);
    const int width = (std::min)(workWidth, (std::max)(static_cast<int>(minimumSize.cx), ::MulDiv(workWidth, 80, 100)));
    const int height = (std::min)(workHeight, (std::max)(static_cast<int>(minimumSize.cy), ::MulDiv(workHeight, 80, 100)));
    const int left = work.left + (workWidth - width) / 2;
    const int top = work.top + (workHeight - height) / 2;
    return { left, top, left + width, top + height };
}

RECT ClampHelpBounds(const RECT& work, const RECT& saved, const CSize& minimumSize)
{
    const int savedWidth = static_cast<int>(saved.right - saved.left);
    const int savedHeight = static_cast<int>(saved.bottom - saved.top);
    if (savedWidth < minimumSize.cx || savedHeight < minimumSize.cy) return {};
    const int width = (std::min)(savedWidth, static_cast<int>(work.right - work.left));
    const int height = (std::min)(savedHeight, static_cast<int>(work.bottom - work.top));
    const int left = (std::max)(static_cast<int>(work.left), (std::min)(static_cast<int>(saved.left), static_cast<int>(work.right) - width));
    const int top = (std::max)(static_cast<int>(work.top), (std::min)(static_cast<int>(saved.top), static_cast<int>(work.bottom) - height));
    return { left, top, left + width, top + height };
}

bool IsWithinWorkArea(const RECT& bounds, const RECT& work)
{
    return bounds.left >= work.left && bounds.top >= work.top && bounds.right <= work.right && bounds.bottom <= work.bottom;
}

class RegexHelpDialog : public CDialogImpl<RegexHelpDialog>
{
public:
    enum { IDD = IDD_REGEX_HELP };
    explicit RegexHelpDialog(FbeSearchPresets::SearchUiContext context, bool forceDefaultPlacement = false) : m_context(context), m_forceDefaultPlacement(forceDefaultPlacement) {}
    const RegexHelpRenderMetrics& Metrics() const { return m_metrics; }

    BEGIN_MSG_MAP(RegexHelpDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_REGEX_HELP_RENDER, OnDeferredRender)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_CLOSE, OnWindowClose)
        MESSAGE_HANDLER(WM_THEMECHANGED, OnThemeChanged)
        MESSAGE_HANDLER(WM_SETTINGCHANGE, OnThemeChanged)
        MESSAGE_HANDLER(WM_FBE_THEMECHANGED, OnThemeChanged)
        COMMAND_ID_HANDLER(IDC_REGEX_HELP_CLOSE, OnClose)
        COMMAND_ID_HANDLER(IDCANCEL, OnClose)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
    {
        FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_REGEX_HELP);
        SetWindowText(FbeLoadRuntimeStringByKey(
            m_context == FbeSearchPresets::SearchUiContext::Design ? L"fbe.regex_help.design.caption" : L"fbe.regex_help.source.caption",
            m_context == FbeSearchPresets::SearchUiContext::Design ? L"Regular expression help — Design" : L"Regular expression help — Source"));
        ThemeManager::ApplyToWindow(m_hWnd);
        CaptureLayoutMetrics();
        RestoreSize();
        LayoutControls();
        const HWND text = GetDlgItem(IDC_REGEX_HELP_TEXT);
        if (text)
        {
            ::SendMessage(text, EM_SETBKGNDCOLOR, 0, ThemeManager::WindowColor());
            ::SetWindowTextW(text, L"Loading\x2026");
        }
        m_metrics.dialogFirstVisibleMs = ::GetTickCount64() - m_createdAt;
        ::PostMessage(m_hWnd, WM_REGEX_HELP_RENDER, 0, 0);
        ::SetFocus(GetDlgItem(IDC_REGEX_HELP_CLOSE));
        return FALSE;
    }

    LRESULT OnDeferredRender(UINT, WPARAM, LPARAM, BOOL&)
    {
        if (m_rendered) return 0;
        FbeRegexHelp::MarkdownLoadMetrics loadMetrics;
        FbeRegexHelp::LoadMarkdown(m_context, m_blocks, m_sourcePath, &loadMetrics);
        m_metrics.markdownReadMs = loadMetrics.markdownReadMs;
        m_metrics.parseMs = loadMetrics.parseMs;
        m_metrics.blockCount = loadMetrics.blockCount;
        const ULONGLONG renderStarted = ::GetTickCount64();
        ApplyThemeAndRender();
        m_metrics.renderMs = ::GetTickCount64() - renderStarted;
        m_metrics.totalReadyMs = ::GetTickCount64() - m_createdAt;
        m_metrics.deferred = true;
        m_rendered = true;
        return 0;
    }

    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&) { if (m_layoutReady) LayoutControls(); return 0; }
    LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL&)
    {
        MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
        if (info != NULL) { info->ptMinTrackSize.x = m_minimumSize.cx; info->ptMinTrackSize.y = m_minimumSize.cy; }
        return 0;
    }
    LRESULT OnWindowClose(UINT, WPARAM, LPARAM, BOOL&) { SaveSize(); EndDialog(IDC_REGEX_HELP_CLOSE); return 0; }
    LRESULT OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&)
    {
        ThemeManager::ApplyToWindow(m_hWnd);
        const HWND text = GetDlgItem(IDC_REGEX_HELP_TEXT);
        if (text) ::SendMessage(text, EM_SETBKGNDCOLOR, 0, ThemeManager::WindowColor());
        if (m_rendered) ApplyThemeAndRender();
        return 0;
    }
    LRESULT OnClose(WORD, WORD, HWND, BOOL&) { SaveSize(); EndDialog(IDC_REGEX_HELP_CLOSE); return 0; }

private:
    void ApplyThemeAndRender()
    {
        const HWND text = GetDlgItem(IDC_REGEX_HELP_TEXT);
        if (!text) return;
        ::SendMessage(text, EM_SETBKGNDCOLOR, 0, ThemeManager::WindowColor());
        FbeRegexHelp::RenderMarkdown(text, m_blocks);
    }
    void CaptureLayoutMetrics()
    {
        RECT window = {}; GetWindowRect(&window);
        m_minimumSize = CSize(window.right - window.left, window.bottom - window.top);
        RECT client = {}; GetClientRect(&client);
        RECT text = {}; ::GetWindowRect(GetDlgItem(IDC_REGEX_HELP_TEXT), &text);
        ::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&text), 2);
        RECT close = {}; ::GetWindowRect(GetDlgItem(IDC_REGEX_HELP_CLOSE), &close);
        ::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&close), 2);
        m_margin = text.left;
        m_bottomMargin = client.bottom - close.bottom;
        m_gap = close.top - text.bottom;
        m_buttonSize = CSize(close.right - close.left, close.bottom - close.top);
        m_layoutReady = true;
    }
    void RestoreSize()
    {
        WINDOWPLACEMENT placement = {}; placement.length = sizeof(placement);
        if (m_forceDefaultPlacement || !_Settings.GetRegexHelpPlacement(placement))
        {
            const HWND owner = ::GetWindow(m_hWnd, GW_OWNER);
            const HMONITOR monitor = ::MonitorFromWindow(owner ? owner : m_hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO info = {}; info.cbSize = sizeof(info);
            if (monitor == NULL || !::GetMonitorInfo(monitor, &info)) return;
            const RECT bounds = DefaultHelpBounds(info.rcWork, m_minimumSize);
            SetWindowPos(NULL, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
            return;
        }

        HMONITOR monitor = ::MonitorFromRect(&placement.rcNormalPosition, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info = {}; info.cbSize = sizeof(info);
        if (monitor == NULL || !::GetMonitorInfo(monitor, &info)) return;
        const RECT bounds = ClampHelpBounds(info.rcWork, placement.rcNormalPosition, m_minimumSize);
        if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) return;
        SetWindowPos(NULL, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    void SaveSize()
    {
        WINDOWPLACEMENT placement = {}; placement.length = sizeof(placement);
        if (::GetWindowPlacement(m_hWnd, &placement)) _Settings.SetRegexHelpPlacement(placement, true);
    }
    void LayoutControls()
    {
        RECT client = {}; GetClientRect(&client);
        const int width = (std::max)(0, static_cast<int>(client.right) - static_cast<int>(client.left));
        const int height = (std::max)(0, static_cast<int>(client.bottom) - static_cast<int>(client.top));
        const int closeLeft = (std::max)(m_margin, width - m_margin - static_cast<int>(m_buttonSize.cx));
        const int closeTop = (std::max)(m_margin, height - m_bottomMargin - static_cast<int>(m_buttonSize.cy));
        const int textBottom = (std::max)(m_margin, closeTop - m_gap);
        ::SetWindowPos(GetDlgItem(IDC_REGEX_HELP_TEXT), NULL, m_margin, m_margin, (std::max)(0, width - 2 * m_margin), (std::max)(0, textBottom - m_margin), SWP_NOZORDER | SWP_NOACTIVATE);
        ::SetWindowPos(GetDlgItem(IDC_REGEX_HELP_CLOSE), NULL, closeLeft, closeTop, m_buttonSize.cx, m_buttonSize.cy, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    FbeSearchPresets::SearchUiContext m_context;
    std::vector<FbeRegexHelp::MarkdownBlock> m_blocks;
    CString m_sourcePath;
    CSize m_minimumSize = CSize(0, 0);
    CSize m_buttonSize = CSize(0, 0);
    int m_margin = 0;
    int m_bottomMargin = 0;
    int m_gap = 0;
    bool m_layoutReady = false;
    bool m_rendered = false;
    bool m_forceDefaultPlacement = false;
    ULONGLONG m_createdAt = ::GetTickCount64();
    RegexHelpRenderMetrics m_metrics;
};
}

void ShowRegexHelpDialog(HWND owner, FbeSearchPresets::SearchUiContext context)
{
    HMODULE richEdit = ::LoadLibraryW(L"Msftedit.dll");
    RegexHelpDialog dialog(context);
    dialog.DoModal(owner);
    if (richEdit != NULL) ::FreeLibrary(richEdit);
}
bool RunRegexHelpPlacementRuntimeSmoke(HWND owner, CStringA& report)
{
    const HMONITOR monitor = ::MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {}; info.cbSize = sizeof(info);
    if (monitor == NULL || !::GetMonitorInfo(monitor, &info))
    {
        report = "monitor=0\r\nresult=fail\r\n";
        return false;
    }
    const CSize minimumSize(300, 200);
    const RECT defaultBounds = DefaultHelpBounds(info.rcWork, minimumSize);
    const int workWidth = info.rcWork.right - info.rcWork.left;
    const int workHeight = info.rcWork.bottom - info.rcWork.top;
    const bool defaultLarge = IsWithinWorkArea(defaultBounds, info.rcWork) &&
        defaultBounds.right - defaultBounds.left == (std::min)(workWidth, (std::max)(static_cast<int>(minimumSize.cx), ::MulDiv(workWidth, 80, 100))) &&
        defaultBounds.bottom - defaultBounds.top == (std::min)(workHeight, (std::max)(static_cast<int>(minimumSize.cy), ::MulDiv(workHeight, 80, 100)));
    const RECT saved = { info.rcWork.left + 20, info.rcWork.top + 30, info.rcWork.left + 620, info.rcWork.top + 530 };
    const RECT restored = ClampHelpBounds(info.rcWork, saved, minimumSize);
    const bool restore = ::EqualRect(&saved, &restored) != FALSE;
    const RECT outside = { info.rcWork.right + 500, info.rcWork.bottom + 300, info.rcWork.right + 1100, info.rcWork.bottom + 800 };
    const RECT clamped = ClampHelpBounds(info.rcWork, outside, minimumSize);
    const bool clamp = IsWithinWorkArea(clamped, info.rcWork) && clamped.right - clamped.left == 600 && clamped.bottom - clamped.top == 500;
    report.Format("default_large=%d\r\nsaved_restore=%d\r\noutside_clamp=%d\r\nresult=%s\r\n", defaultLarge ? 1 : 0, restore ? 1 : 0, clamp ? 1 : 0, defaultLarge && restore && clamp ? "pass" : "fail");
    return defaultLarge && restore && clamp;
}

bool RunRegexHelpVisualCapture(HWND owner, LPCWSTR artifactDirectory, CStringA& report)
{
    if (artifactDirectory == NULL || *artifactDirectory == L'\0') { report = "artifacts=0\r\nresult=fail\r\n"; return false; }
    auto capture = [&](HWND window, LPCWSTR name) -> bool
    {
        RECT rect = {}; if (!window || !::GetWindowRect(window, &rect)) return false;
        const int width = rect.right - rect.left, height = rect.bottom - rect.top;
        HDC source = width > 0 && height > 0 ? ::GetWindowDC(window) : NULL, memory = source ? ::CreateCompatibleDC(source) : NULL;
        BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(info.bmiHeader); info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = height; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        void* pixels = NULL; HBITMAP bitmap = source ? ::CreateDIBSection(source, &info, DIB_RGB_COLORS, &pixels, NULL, 0) : NULL; HGDIOBJ previous = memory && bitmap ? ::SelectObject(memory, bitmap) : NULL;
        const bool printed = previous && ::PrintWindow(window, memory, 0) != FALSE; bool saved = false;
        if (printed)
        {
            BITMAPFILEHEADER header = {}; header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader); header.bfSize = header.bfOffBits + width * height * 4;
            CString path = CString(artifactDirectory) + L"\\" + name; HANDLE file = ::CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); DWORD written = 0;
            if (file != INVALID_HANDLE_VALUE && ::WriteFile(file, &header, sizeof(header), &written, NULL) && written == sizeof(header) && ::WriteFile(file, &info.bmiHeader, sizeof(info.bmiHeader), &written, NULL) && written == sizeof(info.bmiHeader) && ::WriteFile(file, pixels, width * height * 4, &written, NULL) && written == static_cast<DWORD>(width * height * 4)) saved = true;
            if (file != INVALID_HANDLE_VALUE) ::CloseHandle(file);
        }
        if (previous) ::SelectObject(memory, previous); if (bitmap) ::DeleteObject(bitmap); if (memory) ::DeleteDC(memory); if (source) ::ReleaseDC(window, source);
        return saved;
    };
    auto waitForDeferredRender = [&](HWND window) -> bool
    {
        const HWND text = window ? ::GetDlgItem(window, IDC_REGEX_HELP_TEXT) : NULL;
        for (int attempt = 0; text && attempt < 80; ++attempt)
        {
            MSG message = {};
            while (::PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessage(&message); }
            CString value; const int length = ::GetWindowTextLengthW(text);
            wchar_t* buffer = value.GetBuffer(length + 1);
            ::GetWindowTextW(text, buffer, length + 1);
            value.ReleaseBuffer();
            if (value != L"Loading\x2026") return !value.IsEmpty();
            ::Sleep(1);
        }
        return false;
    };
    auto scrollToLine = [&](HWND window, int line) -> bool
    {
        const HWND text = window ? ::GetDlgItem(window, IDC_REGEX_HELP_TEXT) : NULL;
        if (!text) return false;
        const int lineCount = static_cast<int>(::SendMessage(text, EM_GETLINECOUNT, 0, 0));
        if (lineCount <= line) return false;
        ::SendMessage(text, EM_LINESCROLL, 0, line);
        return true;
    };
    HMODULE richEdit = ::LoadLibraryW(L"Msftedit.dll");
    RegexHelpDialog initial(FbeSearchPresets::SearchUiContext::Design, true);
    HWND initialWindow = initial.Create(owner); if (initialWindow) { ::ShowWindow(initialWindow, SW_SHOWNOACTIVATE); ::UpdateWindow(initialWindow); }
    const bool initialRendered = waitForDeferredRender(initialWindow);
    RECT initialBounds = {}; const bool defaultSize = initialRendered && ::GetWindowRect(initialWindow, &initialBounds) && capture(initialWindow, L"full-help-design-start.bmp");
    const bool designTable = initialRendered && scrollToLine(initialWindow, 55) && capture(initialWindow, L"full-help-design-table.bmp");
    const bool designCode = initialRendered && scrollToLine(initialWindow, 160) && capture(initialWindow, L"full-help-design-code.bmp");
    const bool narrow = initialRendered && ::SetWindowPos(initialWindow, NULL, initialBounds.left, initialBounds.top, 430, 520, SWP_NOZORDER | SWP_NOACTIVATE) != FALSE && capture(initialWindow, L"full-help-design-narrow.bmp");
    if (initialWindow) ::SetWindowPos(initialWindow, NULL, initialBounds.left + 12, initialBounds.top + 12, 720, 520, SWP_NOZORDER | SWP_NOACTIVATE);
    WINDOWPLACEMENT placement = {}; placement.length = sizeof(placement); const bool savedPlacement = initialWindow && ::GetWindowPlacement(initialWindow, &placement) != FALSE;
    if (initialWindow) initial.DestroyWindow();
    if (savedPlacement) _Settings.SetRegexHelpPlacement(placement, false);
    RegexHelpDialog restored(FbeSearchPresets::SearchUiContext::Source);
    HWND restoredWindow = savedPlacement ? restored.Create(owner) : NULL; if (restoredWindow) { ::ShowWindow(restoredWindow, SW_SHOWNOACTIVATE); ::UpdateWindow(restoredWindow); }
    const bool restoredRendered = waitForDeferredRender(restoredWindow);
    RECT restoredBounds = {}; const bool restoredSize = restoredRendered && ::GetWindowRect(restoredWindow, &restoredBounds) && restoredBounds.right - restoredBounds.left == placement.rcNormalPosition.right - placement.rcNormalPosition.left && restoredBounds.bottom - restoredBounds.top == placement.rcNormalPosition.bottom - placement.rcNormalPosition.top && capture(restoredWindow, L"full-help-code-start.bmp");
    const bool codeTable = restoredRendered && scrollToLine(restoredWindow, 28) && capture(restoredWindow, L"full-help-code-table.bmp");
    const RegexHelpRenderMetrics& designMetrics = initial.Metrics();
    const RegexHelpRenderMetrics& codeMetrics = restored.Metrics();
    const bool metrics = designMetrics.deferred && codeMetrics.deferred && designMetrics.blockCount > 0 && codeMetrics.blockCount > 0 &&
        designMetrics.dialogFirstVisibleMs <= designMetrics.totalReadyMs && codeMetrics.dialogFirstVisibleMs <= codeMetrics.totalReadyMs;
    if (restoredWindow) restored.DestroyWindow(); if (richEdit != NULL) ::FreeLibrary(richEdit);
    const bool passed = defaultSize && designTable && designCode && narrow && restoredSize && codeTable && metrics;
    report.Format("initial=%d\r\ndesign_table=%d\r\ndesign_code=%d\r\nnarrow=%d\r\nrestored=%d\r\ncode_table=%d\r\nmetrics=%d\r\ndesign_markdown_read_ms=%llu\r\ndesign_parse_ms=%llu\r\ndesign_render_ms=%llu\r\ndesign_dialog_first_visible_ms=%llu\r\ndesign_total_ready_ms=%llu\r\ndesign_block_count=%llu\r\ncode_markdown_read_ms=%llu\r\ncode_parse_ms=%llu\r\ncode_render_ms=%llu\r\ncode_dialog_first_visible_ms=%llu\r\ncode_total_ready_ms=%llu\r\ncode_block_count=%llu\r\nresult=%s\r\n",
        defaultSize ? 1 : 0, designTable ? 1 : 0, designCode ? 1 : 0, narrow ? 1 : 0, restoredSize ? 1 : 0, codeTable ? 1 : 0, metrics ? 1 : 0,
        static_cast<unsigned long long>(designMetrics.markdownReadMs), static_cast<unsigned long long>(designMetrics.parseMs), static_cast<unsigned long long>(designMetrics.renderMs), static_cast<unsigned long long>(designMetrics.dialogFirstVisibleMs), static_cast<unsigned long long>(designMetrics.totalReadyMs), static_cast<unsigned long long>(designMetrics.blockCount),
        static_cast<unsigned long long>(codeMetrics.markdownReadMs), static_cast<unsigned long long>(codeMetrics.parseMs), static_cast<unsigned long long>(codeMetrics.renderMs), static_cast<unsigned long long>(codeMetrics.dialogFirstVisibleMs), static_cast<unsigned long long>(codeMetrics.totalReadyMs), static_cast<unsigned long long>(codeMetrics.blockCount), passed ? "pass" : "fail");
    return passed;
}
