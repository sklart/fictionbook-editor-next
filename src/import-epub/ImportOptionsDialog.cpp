#include "stdafx.h"
#include "../common/PluginSettingsStore.h"
#include "../common/PluginOptionsTabs.h"
#include "../common/PluginWindowPlacement.h"
#include "ImportOptionsDialog.h"
#include "resource.h"
#include "RuntimeLocalization.h"

#include <commctrl.h>
#include <vector>

namespace
{
    enum
    {
        IDC_OPT_COVER = 2001,
        IDC_OPT_IMAGES,
        IDC_OPT_NOTES,
        IDC_OPT_NAV_TITLES,
        IDC_OPT_REPAIR_ENCODING,
        IDC_OPT_SKIP_SERVICE,
        IDC_OPT_TABLES,
        IDC_OPT_LISTS,
        IDC_OPT_POEMS,
        IDC_OPT_SUBTITLES,
        IDC_OPT_SPLIT_HEADINGS,
        IDC_OPT_LINKS,
        IDC_OPT_CLEAN_TYPOGRAPHY,
        IDC_OPT_PAGE_BREAKS,
        IDC_OPT_SKIP_HIDDEN,
        IDC_OPT_VALIDATE,
        IDC_OPT_DIAGNOSTIC,
        IDC_OPT_LOG,
        IDC_OPT_SAVE_FB2,
        IDC_OPT_CSS_SEMANTICS,
        IDC_OPT_REMOVE_BACKLINKS,
        IDC_OPT_REMOVE_SERVICE_SECTIONS,
        IDC_OPT_LOG_ON_WARNINGS,
        IDC_OPT_SAVE_INTERMEDIATE_ON_ERROR,
        IDC_OPT_KEEP_TEMP,
        IDC_OPT_SEPARATOR_SUBTITLES,
        IDC_OPT_TITLE_BLOCK,
        IDC_OPT_UNIFY_IMAGES,
        IDC_OPT_COVER_NAME,
        IDC_OPT_UNIFY_NOTES,
        IDC_SVG_LABEL = 2100,
        IDC_SVG_MODE = 2101,
        IDC_DEFAULTS = 2201,
        IDC_IMPORT = IDOK,
        IDC_CANCEL = IDCANCEL
    };

    struct CheckDef
    {
        int id;
        UINT textId;
        UINT tooltipId;
        bool EpubImportOptions::*field;
    };

    const CheckDef kChecks[] =
    {
        { IDC_OPT_COVER, IDS_IMPORT_OPTIONS_OPT_COVER, IDS_IMPORT_OPTIONS_TT_COVER, &EpubImportOptions::importCover },
        { IDC_OPT_IMAGES, IDS_IMPORT_OPTIONS_OPT_IMAGES, IDS_IMPORT_OPTIONS_TT_IMAGES, &EpubImportOptions::importImages },
        { IDC_OPT_NOTES, IDS_IMPORT_OPTIONS_OPT_NOTES, IDS_IMPORT_OPTIONS_TT_NOTES, &EpubImportOptions::importNotes },
        { IDC_OPT_NAV_TITLES, IDS_IMPORT_OPTIONS_OPT_NAV_TITLES, IDS_IMPORT_OPTIONS_TT_NAV_TITLES, &EpubImportOptions::useNavigationTitles },
        { IDC_OPT_REPAIR_ENCODING, IDS_IMPORT_OPTIONS_OPT_REPAIR_ENCODING, IDS_IMPORT_OPTIONS_TT_REPAIR_ENCODING, &EpubImportOptions::repairEncoding },
        { IDC_OPT_SKIP_SERVICE, IDS_IMPORT_OPTIONS_OPT_SKIP_SERVICE, IDS_IMPORT_OPTIONS_TT_SKIP_SERVICE, &EpubImportOptions::skipServicePages },
        { IDC_OPT_TABLES, IDS_IMPORT_OPTIONS_OPT_TABLES, IDS_IMPORT_OPTIONS_TT_TABLES, &EpubImportOptions::importTables },
        { IDC_OPT_LISTS, IDS_IMPORT_OPTIONS_OPT_LISTS, IDS_IMPORT_OPTIONS_TT_LISTS, &EpubImportOptions::importLists },
        { IDC_OPT_POEMS, IDS_IMPORT_OPTIONS_OPT_POEMS, IDS_IMPORT_OPTIONS_TT_POEMS, &EpubImportOptions::importPoemsEpigraphs },
        { IDC_OPT_SUBTITLES, IDS_IMPORT_OPTIONS_OPT_SUBTITLES, IDS_IMPORT_OPTIONS_TT_SUBTITLES, &EpubImportOptions::importSubtitles },
        { IDC_OPT_SEPARATOR_SUBTITLES, IDS_IMPORT_OPTIONS_OPT_SEPARATOR_SUBTITLES, IDS_IMPORT_OPTIONS_TT_SEPARATOR_SUBTITLES, &EpubImportOptions::separatorSubtitles },
        { IDC_OPT_TITLE_BLOCK, IDS_IMPORT_OPTIONS_OPT_TITLE_BLOCK, IDS_IMPORT_OPTIONS_TT_TITLE_BLOCK, &EpubImportOptions::createTitleBlock },
        { IDC_OPT_UNIFY_IMAGES, IDS_IMPORT_OPTIONS_OPT_UNIFY_IMAGES, IDS_IMPORT_OPTIONS_TT_UNIFY_IMAGES, &EpubImportOptions::unifyImageNames },
        { IDC_OPT_COVER_NAME, IDS_IMPORT_OPTIONS_OPT_COVER_NAME, IDS_IMPORT_OPTIONS_TT_COVER_NAME, &EpubImportOptions::separateCoverName },
        { IDC_OPT_UNIFY_NOTES, IDS_IMPORT_OPTIONS_OPT_UNIFY_NOTES, IDS_IMPORT_OPTIONS_TT_UNIFY_NOTES, &EpubImportOptions::unifyNoteNumbers },
        { IDC_OPT_SPLIT_HEADINGS, IDS_IMPORT_OPTIONS_OPT_SPLIT_HEADINGS, IDS_IMPORT_OPTIONS_TT_SPLIT_HEADINGS, &EpubImportOptions::splitSectionsByHeadings },
        { IDC_OPT_LINKS, IDS_IMPORT_OPTIONS_OPT_LINKS, IDS_IMPORT_OPTIONS_TT_LINKS, &EpubImportOptions::preserveLinks },
        { IDC_OPT_CLEAN_TYPOGRAPHY, IDS_IMPORT_OPTIONS_OPT_CLEAN_TYPOGRAPHY, IDS_IMPORT_OPTIONS_TT_CLEAN_TYPOGRAPHY, &EpubImportOptions::cleanTypography },
        { IDC_OPT_PAGE_BREAKS, IDS_IMPORT_OPTIONS_OPT_PAGE_BREAKS, IDS_IMPORT_OPTIONS_TT_PAGE_BREAKS, &EpubImportOptions::importPageBreaks },
        { IDC_OPT_SKIP_HIDDEN, IDS_IMPORT_OPTIONS_OPT_SKIP_HIDDEN, IDS_IMPORT_OPTIONS_TT_SKIP_HIDDEN, &EpubImportOptions::skipHiddenElements },
        { IDC_OPT_VALIDATE, IDS_IMPORT_OPTIONS_OPT_VALIDATE, IDS_IMPORT_OPTIONS_TT_VALIDATE, &EpubImportOptions::validateResult },
        { IDC_OPT_DIAGNOSTIC, IDS_IMPORT_OPTIONS_OPT_DIAGNOSTIC, IDS_IMPORT_OPTIONS_TT_DIAGNOSTIC, &EpubImportOptions::addDiagnosticSection },
        { IDC_OPT_LOG, IDS_IMPORT_OPTIONS_OPT_LOG, IDS_IMPORT_OPTIONS_TT_LOG, &EpubImportOptions::writeImportLog },
        { IDC_OPT_SAVE_FB2, IDS_IMPORT_OPTIONS_OPT_SAVE_FB2, IDS_IMPORT_OPTIONS_TT_SAVE_FB2, &EpubImportOptions::saveFb2Copy },
        { IDC_OPT_CSS_SEMANTICS, IDS_IMPORT_OPTIONS_OPT_CSS_SEMANTICS, IDS_IMPORT_OPTIONS_TT_CSS_SEMANTICS, &EpubImportOptions::useCssSemanticClasses },
        { IDC_OPT_REMOVE_BACKLINKS, IDS_IMPORT_OPTIONS_OPT_REMOVE_BACKLINKS, IDS_IMPORT_OPTIONS_TT_REMOVE_BACKLINKS, &EpubImportOptions::removeFootnoteBacklinks },
        { IDC_OPT_REMOVE_SERVICE_SECTIONS, IDS_IMPORT_OPTIONS_OPT_REMOVE_SERVICE_SECTIONS, IDS_IMPORT_OPTIONS_TT_REMOVE_SERVICE_SECTIONS, &EpubImportOptions::removeServiceSections },
        { IDC_OPT_LOG_ON_WARNINGS, IDS_IMPORT_OPTIONS_OPT_LOG_ON_WARNINGS, IDS_IMPORT_OPTIONS_TT_LOG_ON_WARNINGS, &EpubImportOptions::writeLogOnWarnings },
        { IDC_OPT_SAVE_INTERMEDIATE_ON_ERROR, IDS_IMPORT_OPTIONS_OPT_SAVE_INTERMEDIATE_ON_ERROR, IDS_IMPORT_OPTIONS_TT_SAVE_INTERMEDIATE_ON_ERROR, &EpubImportOptions::saveIntermediateFb2OnError },
        { IDC_OPT_KEEP_TEMP, IDS_IMPORT_OPTIONS_OPT_KEEP_TEMP, IDS_IMPORT_OPTIONS_TT_KEEP_TEMP, &EpubImportOptions::keepTempOnError }
    };



    const int kGroupContent[] =
    {
        IDC_OPT_COVER,
        IDC_OPT_NAV_TITLES,
        IDC_OPT_SPLIT_HEADINGS,
        IDC_OPT_CSS_SEMANTICS,
        IDC_OPT_TABLES,
        IDC_OPT_LISTS,
        IDC_OPT_POEMS,
        IDC_OPT_SUBTITLES,
        IDC_OPT_SEPARATOR_SUBTITLES,
        IDC_OPT_TITLE_BLOCK,
        IDC_OPT_PAGE_BREAKS
    };

    const int kGroupImages[] =
    {
        IDC_OPT_IMAGES,
        IDC_OPT_UNIFY_IMAGES,
        IDC_OPT_COVER_NAME
    };

    const int kGroupLinksNotes[] =
    {
        IDC_OPT_NOTES,
        IDC_OPT_UNIFY_NOTES,
        IDC_OPT_LINKS,
        IDC_OPT_REMOVE_BACKLINKS
    };

    const int kGroupCleanup[] =
    {
        IDC_OPT_REPAIR_ENCODING,
        IDC_OPT_CLEAN_TYPOGRAPHY,
        IDC_OPT_SKIP_SERVICE,
        IDC_OPT_REMOVE_SERVICE_SECTIONS,
        IDC_OPT_SKIP_HIDDEN
    };

    const int kGroupDiagnostics[] =
    {
        IDC_OPT_VALIDATE,
        IDC_OPT_LOG,
        IDC_OPT_LOG_ON_WARNINGS,
        IDC_OPT_DIAGNOSTIC,
        IDC_OPT_SAVE_FB2,
        IDC_OPT_SAVE_INTERMEDIATE_ON_ERROR,
        IDC_OPT_KEEP_TEMP
    };

    struct GroupDef
    {
        UINT titleId;
        UINT tooltipId;
        const int* items;
        int count;
        bool hasSvgMode;
    };

    const GroupDef kGroups[] =
    {
        { IDS_IMPORT_OPTIONS_GROUP_CONTENT, IDS_IMPORT_OPTIONS_TT_GROUP_CONTENT, kGroupContent, static_cast<int>(_countof(kGroupContent)), false },
        { IDS_IMPORT_OPTIONS_GROUP_IMAGES, IDS_IMPORT_OPTIONS_TT_GROUP_IMAGES, kGroupImages, static_cast<int>(_countof(kGroupImages)), true },
        { IDS_IMPORT_OPTIONS_GROUP_LINKS_NOTES, IDS_IMPORT_OPTIONS_TT_GROUP_LINKS_NOTES, kGroupLinksNotes, static_cast<int>(_countof(kGroupLinksNotes)), false },
        { IDS_IMPORT_OPTIONS_GROUP_CLEANUP, IDS_IMPORT_OPTIONS_TT_GROUP_CLEANUP, kGroupCleanup, static_cast<int>(_countof(kGroupCleanup)), false },
        { IDS_IMPORT_OPTIONS_GROUP_DIAGNOSTICS, IDS_IMPORT_OPTIONS_TT_GROUP_DIAGNOSTICS, kGroupDiagnostics, static_cast<int>(_countof(kGroupDiagnostics)), false }
    };

    const CheckDef* FindCheckDef(int id)
    {
        for (int i = 0; i < static_cast<int>(_countof(kChecks)); ++i)
        {
            if (kChecks[i].id == id)
                return &kChecks[i];
        }
        return nullptr;
    }

    const wchar_t* kSettingsKey = L"Software\\FBETeam\\FictionBook Editor Next\\ImportEPUB";

    DWORD BoolToDword(bool value)
    {
        return value ? 1u : 0u;
    }

    bool DwordToBool(DWORD value)
    {
        return value != 0;
    }

    void LoadBoolValue(FbePluginSettings::Store& key, LPCWSTR name, bool& value)
    {
        DWORD data = 0;
        if (key.QueryDWORDValue(name, data) == ERROR_SUCCESS)
            value = DwordToBool(data);
    }

    void SaveBoolValue(FbePluginSettings::Store& key, LPCWSTR name, bool value)
    {
        key.SetDWORDValue(name, BoolToDword(value));
    }

    void ResetControlsToOptions(HWND hwnd, const EpubImportOptions& options)
    {
        for (int i = 0; i < static_cast<int>(_countof(kChecks)); ++i)
        {
            HWND chk = FbePluginUi::FindControl(hwnd, kChecks[i].id);
            if (chk)
                SendMessageW(chk, BM_SETCHECK, options.*(kChecks[i].field) ? BST_CHECKED : BST_UNCHECKED, 0);
        }

        HWND combo = FbePluginUi::FindControl(hwnd, IDC_SVG_MODE);
        if (combo)
        {
            int mode = options.svgConversionMode;
            if (mode < SVG_IMPORT_KEEP || mode > SVG_IMPORT_SKIP)
                mode = SVG_IMPORT_CONVERT_PNG;
            SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(mode), 0);
        }
    }

    UINT WindowDpi(HWND window)
    {
        using GetDpiForWindowProc = UINT (WINAPI*)(HWND);
        const HMODULE user32 = GetModuleHandleW(L"user32.dll");
        const auto getDpi = user32 ? reinterpret_cast<GetDpiForWindowProc>(
            GetProcAddress(user32, "GetDpiForWindow")) : nullptr;
        if (getDpi && window)
        {
            const UINT dpi = getDpi(window);
            if (dpi) return dpi;
        }
        HDC dc = GetDC(window);
        const UINT dpi = dc ? static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX)) : 96;
        if (dc) ReleaseDC(window, dc);
        return dpi ? dpi : 96;
    }

    int Scale(int value, UINT dpi) { return MulDiv(value, static_cast<int>(dpi), 96); }

    void AdjustDialogRect(RECT& rect, DWORD style, DWORD exStyle, UINT dpi)
    {
        using AdjustForDpiProc = BOOL (WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
        const HMODULE user32 = GetModuleHandleW(L"user32.dll");
        const auto adjustForDpi = user32 ? reinterpret_cast<AdjustForDpiProc>(
            GetProcAddress(user32, "AdjustWindowRectExForDpi")) : nullptr;
        if (!adjustForDpi || !adjustForDpi(&rect, style, FALSE, exStyle, dpi))
            AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    }

    class OptionsWindow
    {
    public:
        OptionsWindow(HWND owner, EpubImportOptions& options)
            : m_owner(owner), m_options(options), m_originalOptions(options), m_hwnd(nullptr), m_tooltip(nullptr), m_done(false), m_result(false),
              m_unexpectedDestroy(false),
              m_dpi(WindowDpi(owner)), m_font(nullptr)
        {
        }

        ImportOptionsDialogResult DoModal()
        {
            LoadImportOptions(m_options, &m_revision);
            m_originalOptions = m_options;
            INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
            InitCommonControlsEx(&icc);

            WNDCLASSW wc = {};
            wc.lpfnWndProc = StaticWndProc;
            wc.hInstance = _AtlBaseModule.GetModuleInstance();
            wc.lpszClassName = L"FBEImportEPUBOptionsWindow";
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hIcon = LoadIcon(_AtlBaseModule.GetModuleInstance(), MAKEINTRESOURCE(101));
            wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
            RegisterClassW(&wc);

            const DWORD style = WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN;
            RECT rc = { 0, 0, Scale(780, m_dpi), Scale(500, m_dpi) };
            AdjustDialogRect(rc, style, WS_EX_DLGMODALFRAME, m_dpi);
            MONITORINFO monitor = { sizeof(monitor) };
            const HWND monitorWindow = m_owner ? m_owner : GetDesktopWindow();
            GetMonitorInfoW(MonitorFromWindow(monitorWindow, MONITOR_DEFAULTTONEAREST), &monitor);
            const int workWidth = monitor.rcWork.right - monitor.rcWork.left;
            const int workHeight = monitor.rcWork.bottom - monitor.rcWork.top;
            const int width = min(rc.right - rc.left, workWidth);
            const int height = min(rc.bottom - rc.top, workHeight);
            RECT ownerRect = monitor.rcWork;
            if (m_owner) GetWindowRect(m_owner, &ownerRect);
            const int x = min(max(ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2,
                monitor.rcWork.left), monitor.rcWork.right - width);
            const int y = min(max(ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2,
                monitor.rcWork.top), monitor.rcWork.bottom - height);

            m_hwnd = CreateWindowExW(
                WS_EX_DLGMODALFRAME,
                wc.lpszClassName,
                LoadText(IDS_IMPORT_OPTIONS_TITLE),
                style,
                x, y, width, height,
                m_owner, nullptr, _AtlBaseModule.GetModuleInstance(), this);

            if (!m_hwnd)
                return ImportOptionsDialogResult::Error;

            FbePluginUi::ReportWindowPlacement(m_hwnd, L"ImportEPUB", false, LoadText(IDS_PLUGIN_WINDOW_SAVE_ERROR));

            if (m_owner)
                EnableWindow(m_owner, FALSE);

            ShowWindow(m_hwnd, SW_SHOW);
            UpdateWindow(m_hwnd);

            MSG msg = {};
            BOOL messageResult = 1;
            while (!m_done && (messageResult = GetMessageW(&msg, nullptr, 0, 0)) > 0)
            {
                if (!m_tabs.ProcessMessage(msg) && !IsDialogMessageW(m_hwnd, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }

            if (m_owner)
            {
                EnableWindow(m_owner, TRUE);
                SetActiveWindow(m_owner);
            }

            if (messageResult == -1 || (!m_done && messageResult == 0) || m_unexpectedDestroy)
            {
                if (m_hwnd) DestroyWindow(m_hwnd);
                m_options = m_originalOptions;
                if (messageResult == 0) PostQuitMessage(static_cast<int>(msg.wParam));
                return ImportOptionsDialogResult::Error;
            }
            return m_result ? ImportOptionsDialogResult::Confirmed : ImportOptionsDialogResult::Cancelled;
        }

    private:
        static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
        {
            OptionsWindow* self = nullptr;
            if (msg == WM_NCCREATE)
            {
                CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                self = reinterpret_cast<OptionsWindow*>(cs->lpCreateParams);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            else
            {
                self = reinterpret_cast<OptionsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            }

            if (self)
                return self->WndProc(hwnd, msg, wp, lp);
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
        {
            switch (msg)
            {
            case WM_CREATE:
                ::SetPropW(hwnd, L"FBE_SKIP_SYSTEM_DIALOG_LOCALIZATION", reinterpret_cast<HANDLE>(1));
                m_hwnd = hwnd;
                CreateControls(hwnd);
                if (!ControlsReady()) return -1;
                if (!CreateTabs()) return -1;
                UpdateFont();
                Layout();
                return 0;

            case WM_SIZE:
                Layout();
                return 0;

            case WM_GETMINMAXINFO:
                if (lp)
                {
                    MINMAXINFO* bounds = reinterpret_cast<MINMAXINFO*>(lp);
                    MONITORINFO monitor = { sizeof(monitor) };
                    if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
                    {
                        RECT minimum = { 0, 0, Scale(420, m_dpi), Scale(320, m_dpi) };
                        AdjustDialogRect(minimum, GetWindowLongW(hwnd, GWL_STYLE),
                            GetWindowLongW(hwnd, GWL_EXSTYLE), m_dpi);
                        bounds->ptMinTrackSize.x = min(minimum.right - minimum.left,
                            monitor.rcWork.right - monitor.rcWork.left);
                        bounds->ptMinTrackSize.y = min(minimum.bottom - minimum.top,
                            monitor.rcWork.bottom - monitor.rcWork.top);
                    }
                }
                return 0;

            case WM_DPICHANGED:
                m_dpi = LOWORD(wp);
                UpdateFont();
                if (lp)
                {
                    const RECT* suggested = reinterpret_cast<const RECT*>(lp);
                    MONITORINFO monitor = { sizeof(monitor) };
                    if (GetMonitorInfoW(MonitorFromRect(suggested, MONITOR_DEFAULTTONEAREST), &monitor))
                    {
                        const int width = min(suggested->right - suggested->left,
                            monitor.rcWork.right - monitor.rcWork.left);
                        const int height = min(suggested->bottom - suggested->top,
                            monitor.rcWork.bottom - monitor.rcWork.top);
                        const int x = min(max(suggested->left, monitor.rcWork.left), monitor.rcWork.right - width);
                        const int y = min(max(suggested->top, monitor.rcWork.top), monitor.rcWork.bottom - height);
                        SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                }
                Layout();
                return 0;

            case WM_MOUSEWHEEL:
                m_tabs.Wheel(wp); return 0;

            case WM_COMMAND:
                if (HIWORD(wp) == BN_CLICKED)
                {
                    switch (LOWORD(wp))
                    {
                    case IDC_IMPORT:
                        ReadOptions();
                        if(FbePluginSettings::ShowSaveError(m_hwnd, SaveImportOptions(m_options, &m_revision), LoadText(IDS_PLUGIN_SETTINGS_SAVE_ERROR), LoadText(IDS_PLUGIN_SETTINGS_CONFLICT))) return 0;
                        Finish(true);
                        return 0;
                    case IDC_DEFAULTS:
                        m_options = EpubImportOptions();
                        ResetControlsToOptions(m_hwnd, m_options);
                        return 0;
                    case IDC_CANCEL:
                        Finish(false);
                        return 0;
                    }
                }
                RevealFocus();
                break;

            case WM_NOTIFY:
                if(m_tabs.Notify(lp)) return 0;
                RevealFocus();
                break;

            case WM_CLOSE:
                Finish(false);
                return 0;

            case WM_NCDESTROY:
                if (!m_done) { m_unexpectedDestroy = true; m_done = true; }
                m_tabs.Detach();
                m_hwnd = nullptr;
                if (m_font) { DeleteObject(m_font); m_font = nullptr; }
                return DefWindowProcW(hwnd, msg, wp, lp);
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        void CreateControls(HWND hwnd)
        {
            m_tooltipText.reserve(80);
            HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

            CStringW titleText = LoadText(IDS_IMPORT_OPTIONS_HEADER);
            HWND title = CreateWindowW(L"STATIC", titleText,
                WS_CHILD | WS_VISIBLE, 14, 14, 650, 22, hwnd, nullptr, _AtlBaseModule.GetModuleInstance(), nullptr);
            SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            m_tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hwnd, nullptr, _AtlBaseModule.GetModuleInstance(), nullptr);
            SendMessageW(m_tooltip, TTM_SETMAXTIPWIDTH, 0, Scale(680, m_dpi));

            const int groupX = 14;
            const int groupW = 660;
            const int checkX = groupX + 14;
            const int checkW = groupW - 28;
            int y = 44;

            for (int g = 0; g < static_cast<int>(_countof(kGroups)); ++g)
            {
                const GroupDef& group = kGroups[g];
                const int groupHeight = 24 + group.count * 22 + (group.hasSvgMode ? 30 : 0) + 8;

                CStringW groupTitle = LoadText(group.titleId);
                HWND box = CreateWindowW(L"BUTTON", groupTitle,
                    WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                    groupX, y, groupW, groupHeight,
                    hwnd, nullptr, _AtlBaseModule.GetModuleInstance(), nullptr);
                SendMessageW(box, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                AddTooltipString(box, group.tooltipId);

                int itemY = y + 20;
                for (int i = 0; i < group.count; ++i)
                {
                    const CheckDef* def = FindCheckDef(group.items[i]);
                    if (!def)
                        continue;

                    CStringW checkText = LoadText(def->textId);
                    HWND chk = CreateWindowW(L"BUTTON", checkText,
                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                        checkX, itemY, checkW, 20, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(def->id)),
                        _AtlBaseModule.GetModuleInstance(), nullptr);
                    SendMessageW(chk, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                    SendMessageW(chk, BM_SETCHECK, m_options.*(def->field) ? BST_CHECKED : BST_UNCHECKED, 0);
                    AddTooltipString(chk, def->tooltipId);
                    itemY += 22;
                }

                if (group.hasSvgMode)
                {
                    CStringW svgLabelText = LoadText(IDS_IMPORT_OPTIONS_SVG_LABEL);
                    HWND svgLabel = CreateWindowW(L"STATIC", svgLabelText,
                        WS_CHILD | WS_VISIBLE, checkX, itemY + 2, 160, 20, hwnd, reinterpret_cast<HMENU>(IDC_SVG_LABEL), _AtlBaseModule.GetModuleInstance(), nullptr);
                    SendMessageW(svgLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                    AddTooltipString(svgLabel, IDS_IMPORT_OPTIONS_TOOLTIP_SVG_LABEL);

                    HWND svgCombo = CreateWindowW(L"COMBOBOX", nullptr,
                        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
                        checkX + 165, itemY, checkW - 165, 180, hwnd, reinterpret_cast<HMENU>(IDC_SVG_MODE),
                        _AtlBaseModule.GetModuleInstance(), nullptr);
                    SendMessageW(svgCombo, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                    CStringW svgKeep = LoadText(IDS_IMPORT_OPTIONS_SVG_KEEP);
                    CStringW svgPng = LoadText(IDS_IMPORT_OPTIONS_SVG_PNG);
                    CStringW svgJpeg = LoadText(IDS_IMPORT_OPTIONS_SVG_JPEG);
                    CStringW svgSkip = LoadText(IDS_IMPORT_OPTIONS_SVG_SKIP);
                    SendMessageW(svgCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(svgKeep.GetString()));
                    SendMessageW(svgCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(svgPng.GetString()));
                    SendMessageW(svgCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(svgJpeg.GetString()));
                    SendMessageW(svgCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(svgSkip.GetString()));
                    int svgMode = m_options.svgConversionMode;
                    if (svgMode < SVG_IMPORT_KEEP || svgMode > SVG_IMPORT_SKIP)
                        svgMode = SVG_IMPORT_CONVERT_PNG;
                    SendMessageW(svgCombo, CB_SETCURSEL, static_cast<WPARAM>(svgMode), 0);
                    AddTooltipString(svgCombo, IDS_IMPORT_OPTIONS_TOOLTIP_SVG_COMBO);
                }

                y += groupHeight + 6;
            }

            CStringW hintText = LoadText(IDS_IMPORT_OPTIONS_HINT);
            HWND hint = CreateWindowW(L"STATIC", hintText,
                WS_CHILD | WS_VISIBLE, 18, y + 4, 650, 22, hwnd, nullptr, _AtlBaseModule.GetModuleInstance(), nullptr);
            SendMessageW(hint, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            const int buttonY = y + 36;
            CStringW defaultsText = LoadText(IDS_IMPORT_OPTIONS_BUTTON_DEFAULTS);
            CStringW okText = LoadText(IDS_IMPORT_OPTIONS_BUTTON_OK);
            CStringW cancelText = LoadText(IDS_IMPORT_OPTIONS_BUTTON_CANCEL);
            HWND defaults = CreateWindowW(L"BUTTON", defaultsText, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                18, buttonY, 130, 28, hwnd, reinterpret_cast<HMENU>(IDC_DEFAULTS), _AtlBaseModule.GetModuleInstance(), nullptr);
            HWND ok = CreateWindowW(L"BUTTON", okText, WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                475, buttonY, 90, 28, hwnd, reinterpret_cast<HMENU>(IDC_IMPORT), _AtlBaseModule.GetModuleInstance(), nullptr);
            HWND cancel = CreateWindowW(L"BUTTON", cancelText, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                575, buttonY, 90, 28, hwnd, reinterpret_cast<HMENU>(IDC_CANCEL), _AtlBaseModule.GetModuleInstance(), nullptr);
            SendMessageW(defaults, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            AddTooltipString(defaults, IDS_IMPORT_OPTIONS_TOOLTIP_DEFAULTS);
        }


        bool ControlsReady() const
        {
            if (!FbePluginUi::FindControl(m_hwnd, IDC_SVG_MODE) || !FbePluginUi::FindControl(m_hwnd, IDC_DEFAULTS) ||
                !FbePluginUi::FindControl(m_hwnd, IDC_IMPORT) || !FbePluginUi::FindControl(m_hwnd, IDC_CANCEL)) return false;
            for (const CheckDef& check : kChecks)
                if (!FbePluginUi::FindControl(m_hwnd, check.id)) return false;
            return true;
        }

        bool CreateTabs()
        {
            // Section headings become tab captions; the explanatory hint is
            // retained as a tooltip instead of competing for page space.
            for(HWND child = GetWindow(m_hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
                if(GetDlgCtrlID(child) == 0) ShowWindow(child, SW_HIDE);
            std::vector<std::wstring> labels;
            for(const GroupDef& group : kGroups) labels.emplace_back(LoadText(group.titleId).GetString());
            if(!m_tabs.Create(m_hwnd, labels, {IDC_DEFAULTS, IDC_IMPORT, IDC_CANCEL})) return false;
            std::vector<std::wstring> tips;
            for(const GroupDef& group : kGroups) tips.emplace_back(LoadText(group.tooltipId).GetString());
            tips[0] += L"\n"; tips[0] += LoadText(IDS_IMPORT_OPTIONS_HINT).GetString();
            m_tabs.SetTooltips(m_tooltip, tips);
            for(int index = 0; index < static_cast<int>(_countof(kGroups)); ++index) {
                for(int item = 0; item < kGroups[index].count; ++item)
                    if(!m_tabs.Add(kGroups[index].items[item], index)) return false;
                if(kGroups[index].hasSvgMode && (!m_tabs.Add(IDC_SVG_LABEL, index) || !m_tabs.Add(IDC_SVG_MODE, index))) return false;
            }
            return true;
        }

        void UpdateFont()
        {
            if (m_tooltip) SendMessageW(m_tooltip, TTM_SETMAXTIPWIDTH, 0, Scale(680, m_dpi));
            LOGFONTW font = {};
            if (!GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(font), &font)) return;
            const UINT systemDpi = WindowDpi(nullptr);
            font.lfHeight = MulDiv(font.lfHeight, static_cast<int>(m_dpi), static_cast<int>(systemDpi));
            HFONT updated = CreateFontIndirectW(&font);
            if (!updated) return;
            m_tabs.SetFont(updated);
            if (m_tooltip) SendMessageW(m_tooltip, WM_SETFONT, reinterpret_cast<WPARAM>(updated), TRUE);
            if (m_font) DeleteObject(m_font);
            m_font = updated;
        }

        void Layout() { m_tabs.Layout(m_dpi); }
        void RevealFocus() { m_tabs.RevealFocus(); }

        CStringW LoadText(UINT stringId)
        {
            return LoadImportEpubString(stringId);
        }

        void AddTooltipString(HWND control, UINT stringId)
        {
            CStringW text = LoadText(stringId);
            if (text.IsEmpty())
                return;

            m_tooltipText.push_back(text);
            AddTooltip(control, m_tooltipText.back().GetString());
        }

        void AddTooltip(HWND control, LPCWSTR text)
        {
            if (!m_tooltip || !control || !text)
                return;

            TOOLINFOW ti = {};
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            ti.hwnd = m_hwnd;
            ti.uId = reinterpret_cast<UINT_PTR>(control);
            ti.lpszText = const_cast<LPWSTR>(text);
            SendMessageW(m_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
        }

        void ReadOptions()
        {
            for (int i = 0; i < static_cast<int>(_countof(kChecks)); ++i)
            {
                HWND chk = FbePluginUi::FindControl(m_hwnd, kChecks[i].id);
                m_options.*(kChecks[i].field) = (SendMessageW(chk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }

            HWND svgCombo = FbePluginUi::FindControl(m_hwnd, IDC_SVG_MODE);
            if (svgCombo)
            {
                LRESULT sel = SendMessageW(svgCombo, CB_GETCURSEL, 0, 0);
                if (sel >= SVG_IMPORT_KEEP && sel <= SVG_IMPORT_SKIP)
                    m_options.svgConversionMode = static_cast<int>(sel);
            }
        }

        void Finish(bool result)
        {
            if(m_hwnd) FbePluginUi::ReportWindowPlacement(m_hwnd, L"ImportEPUB", true, LoadText(IDS_PLUGIN_WINDOW_SAVE_ERROR));
            if (!result) m_options = m_originalOptions;
            m_result = result;
            m_done = true;
            if (m_hwnd)
            {
                DestroyWindow(m_hwnd);
                m_hwnd = nullptr;
            }
        }

        HWND m_owner;
        EpubImportOptions& m_options;
        EpubImportOptions m_originalOptions;
        FbePluginSettings::Revision m_revision;
        HWND m_hwnd;
        HWND m_tooltip;
        std::vector<CStringW> m_tooltipText;
        FbePluginUi::OptionsTabs m_tabs;
        bool m_done;
        bool m_result;
        bool m_unexpectedDestroy;
        UINT m_dpi;
        HFONT m_font;
    };
}

void LoadImportOptions(EpubImportOptions& options, FbePluginSettings::Revision* revision)
{
    // Start with code defaults so new options added in future versions get sane values.
    options = EpubImportOptions();

    FbePluginSettings::Store key(L"import-epub", kSettingsKey);
    if(FAILED(key.Error())) _com_issue_error(key.Error());

    LoadBoolValue(key, L"ImportCover", options.importCover);
    LoadBoolValue(key, L"ImportImages", options.importImages);
    LoadBoolValue(key, L"ImportNotes", options.importNotes);
    LoadBoolValue(key, L"UseNavigationTitles", options.useNavigationTitles);
    LoadBoolValue(key, L"RepairEncoding", options.repairEncoding);
    LoadBoolValue(key, L"SkipServicePages", options.skipServicePages);
    LoadBoolValue(key, L"ImportTables", options.importTables);
    LoadBoolValue(key, L"ImportLists", options.importLists);
    LoadBoolValue(key, L"ImportPoemsEpigraphs", options.importPoemsEpigraphs);
    LoadBoolValue(key, L"ImportSubtitles", options.importSubtitles);
    LoadBoolValue(key, L"SeparatorSubtitles", options.separatorSubtitles);
    LoadBoolValue(key, L"CreateTitleBlock", options.createTitleBlock);
    LoadBoolValue(key, L"UnifyImageNames", options.unifyImageNames);
    LoadBoolValue(key, L"SeparateCoverName", options.separateCoverName);
    LoadBoolValue(key, L"UnifyNoteNumbers", options.unifyNoteNumbers);
    LoadBoolValue(key, L"SplitSectionsByHeadings", options.splitSectionsByHeadings);
    LoadBoolValue(key, L"PreserveLinks", options.preserveLinks);
    LoadBoolValue(key, L"CleanTypography", options.cleanTypography);
    LoadBoolValue(key, L"ImportPageBreaks", options.importPageBreaks);
    LoadBoolValue(key, L"SkipHiddenElements", options.skipHiddenElements);
    LoadBoolValue(key, L"ValidateResult", options.validateResult);
    LoadBoolValue(key, L"AddDiagnosticSection", options.addDiagnosticSection);
    LoadBoolValue(key, L"WriteImportLog", options.writeImportLog);
    LoadBoolValue(key, L"SaveFb2Copy", options.saveFb2Copy);
    LoadBoolValue(key, L"UseCssSemanticClasses", options.useCssSemanticClasses);
    LoadBoolValue(key, L"RemoveFootnoteBacklinks", options.removeFootnoteBacklinks);
    LoadBoolValue(key, L"RemoveServiceSections", options.removeServiceSections);
    LoadBoolValue(key, L"WriteLogOnWarnings", options.writeLogOnWarnings);
    LoadBoolValue(key, L"SaveIntermediateFb2OnError", options.saveIntermediateFb2OnError);
    LoadBoolValue(key, L"KeepTempOnError", options.keepTempOnError);
    DWORD svgMode = static_cast<DWORD>(options.svgConversionMode);
    if (key.QueryDWORDValue(L"SvgConversionMode", svgMode) == ERROR_SUCCESS &&
        svgMode <= static_cast<DWORD>(SVG_IMPORT_SKIP))
    {
        options.svgConversionMode = static_cast<int>(svgMode);
    }
    if(FAILED(key.Error())) _com_issue_error(key.Error());
    if(revision) *revision = key.CaptureRevision();
}

HRESULT SaveImportOptions(const EpubImportOptions& options, const FbePluginSettings::Revision* revision)
{
    FbePluginSettings::Store key(L"import-epub", kSettingsKey);
    key.ExpectRevision(revision);

    SaveBoolValue(key, L"ImportCover", options.importCover);
    SaveBoolValue(key, L"ImportImages", options.importImages);
    SaveBoolValue(key, L"ImportNotes", options.importNotes);
    SaveBoolValue(key, L"UseNavigationTitles", options.useNavigationTitles);
    SaveBoolValue(key, L"RepairEncoding", options.repairEncoding);
    SaveBoolValue(key, L"SkipServicePages", options.skipServicePages);
    SaveBoolValue(key, L"ImportTables", options.importTables);
    SaveBoolValue(key, L"ImportLists", options.importLists);
    SaveBoolValue(key, L"ImportPoemsEpigraphs", options.importPoemsEpigraphs);
    SaveBoolValue(key, L"ImportSubtitles", options.importSubtitles);
    SaveBoolValue(key, L"SeparatorSubtitles", options.separatorSubtitles);
    SaveBoolValue(key, L"CreateTitleBlock", options.createTitleBlock);
    SaveBoolValue(key, L"UnifyImageNames", options.unifyImageNames);
    SaveBoolValue(key, L"SeparateCoverName", options.separateCoverName);
    SaveBoolValue(key, L"UnifyNoteNumbers", options.unifyNoteNumbers);
    SaveBoolValue(key, L"SplitSectionsByHeadings", options.splitSectionsByHeadings);
    SaveBoolValue(key, L"PreserveLinks", options.preserveLinks);
    SaveBoolValue(key, L"CleanTypography", options.cleanTypography);
    SaveBoolValue(key, L"ImportPageBreaks", options.importPageBreaks);
    SaveBoolValue(key, L"SkipHiddenElements", options.skipHiddenElements);
    SaveBoolValue(key, L"ValidateResult", options.validateResult);
    SaveBoolValue(key, L"AddDiagnosticSection", options.addDiagnosticSection);
    SaveBoolValue(key, L"WriteImportLog", options.writeImportLog);
    SaveBoolValue(key, L"SaveFb2Copy", options.saveFb2Copy);
    SaveBoolValue(key, L"UseCssSemanticClasses", options.useCssSemanticClasses);
    SaveBoolValue(key, L"RemoveFootnoteBacklinks", options.removeFootnoteBacklinks);
    SaveBoolValue(key, L"RemoveServiceSections", options.removeServiceSections);
    SaveBoolValue(key, L"WriteLogOnWarnings", options.writeLogOnWarnings);
    SaveBoolValue(key, L"SaveIntermediateFb2OnError", options.saveIntermediateFb2OnError);
    SaveBoolValue(key, L"KeepTempOnError", options.keepTempOnError);
    key.SetDWORDValue(L"SvgConversionMode", static_cast<DWORD>(options.svgConversionMode));
    return key.Commit();
}

ImportOptionsDialogResult ShowImportOptionsDialog(HWND owner, EpubImportOptions& options)
{
    OptionsWindow wnd(owner, options);
    try { return wnd.DoModal(); }
    catch(const _com_error& error) { FbePluginSettings::ShowSaveError(owner, error.Error(), LoadImportEpubString(IDS_PLUGIN_SETTINGS_SAVE_ERROR), LoadImportEpubString(IDS_PLUGIN_SETTINGS_CONFLICT)); return ImportOptionsDialogResult::Error; }
}
