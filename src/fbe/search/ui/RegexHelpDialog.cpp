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
class RegexHelpDialog : public CDialogImpl<RegexHelpDialog>
{
public:
    enum { IDD = IDD_REGEX_HELP };
    explicit RegexHelpDialog(FbeSearchPresets::SearchUiContext context) : m_context(context) {}

    BEGIN_MSG_MAP(RegexHelpDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
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
        FbeRegexHelp::LoadMarkdown(m_context, m_blocks, m_sourcePath);
        ThemeManager::ApplyToWindow(m_hWnd);
        ApplyThemeAndRender();
        CaptureLayoutMetrics();
        RestoreSize();
        LayoutControls();
        ::SetFocus(GetDlgItem(IDC_REGEX_HELP_CLOSE));
        return FALSE;
    }

    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&) { if (m_layoutReady) LayoutControls(); return 0; }
    LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL&)
    {
        MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
        if (info != NULL) { info->ptMinTrackSize.x = m_minimumSize.cx; info->ptMinTrackSize.y = m_minimumSize.cy; }
        return 0;
    }
    LRESULT OnWindowClose(UINT, WPARAM, LPARAM, BOOL&) { SaveSize(); EndDialog(IDC_REGEX_HELP_CLOSE); return 0; }
    LRESULT OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) { ThemeManager::ApplyToWindow(m_hWnd); ApplyThemeAndRender(); return 0; }
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
        if (!_Settings.GetRegexHelpPlacement(placement)) return;
        const int savedWidth = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
        const int savedHeight = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;
        if (savedWidth < m_minimumSize.cx || savedHeight < m_minimumSize.cy) return;
        HMONITOR monitor = ::MonitorFromRect(&placement.rcNormalPosition, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info = {}; info.cbSize = sizeof(info);
        if (monitor == NULL || !::GetMonitorInfo(monitor, &info)) return;
        const int width = (std::min)(savedWidth, static_cast<int>(info.rcWork.right - info.rcWork.left));
        const int height = (std::min)(savedHeight, static_cast<int>(info.rcWork.bottom - info.rcWork.top));
        const int left = (std::max)(static_cast<int>(info.rcWork.left), (std::min)(static_cast<int>(placement.rcNormalPosition.left), static_cast<int>(info.rcWork.right) - width));
        const int top = (std::max)(static_cast<int>(info.rcWork.top), (std::min)(static_cast<int>(placement.rcNormalPosition.top), static_cast<int>(info.rcWork.bottom) - height));
        SetWindowPos(NULL, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
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
};
}

void ShowRegexHelpDialog(HWND owner, FbeSearchPresets::SearchUiContext context)
{
    HMODULE richEdit = ::LoadLibraryW(L"Msftedit.dll");
    RegexHelpDialog dialog(context);
    dialog.DoModal(owner);
    if (richEdit != NULL) ::FreeLibrary(richEdit);
}
