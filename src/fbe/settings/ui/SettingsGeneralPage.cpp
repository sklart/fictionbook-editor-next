#include "stdafx.h"
#include "../../testing/RuntimeTestScenarioMode.h"
#include "SettingsGeneralPage.h"
#include "..\\..\\Settings.h"
#include "..\\..\\ThemeManager.h"
#include "..\\..\\UiMetrics.h"
#include "../../toolbars/ToolbarFactory.h"
#include "..\\..\\RuntimeLocalization.h"
#include "..\\..\\res1.h"
#include "..\\..\\apputils.h"
#include "..\\..\\..\\common\\ModernFileDialog.h"
#include <initializer_list>
#include <algorithm>
#include <cmath>

extern CSettings _Settings;

namespace
{
const InterfacePalette kPaletteRows[] = {
	INTERFACE_PALETTE_CLASSIC,
	INTERFACE_PALETTE_SEPIA, INTERFACE_PALETTE_CATPPUCCIN_LATTE, INTERFACE_PALETTE_SOLARIZED_LIGHT, INTERFACE_PALETTE_EVERFOREST_LIGHT,
	INTERFACE_PALETTE_GRAPHITE, INTERFACE_PALETTE_NAVY, INTERFACE_PALETTE_CATPPUCCIN_MOCHA, INTERFACE_PALETTE_SOLARIZED_DARK,
	INTERFACE_PALETTE_EVERFOREST_DARK, INTERFACE_PALETTE_ROSE_PINE_MOON, INTERFACE_PALETTE_DRACULA,
	INTERFACE_PALETTE_CATPPUCCIN_FRAPPE, INTERFACE_PALETTE_CATPPUCCIN_MACCHIATO,
	INTERFACE_PALETTE_HIGH_CONTRAST_LIGHT, INTERFACE_PALETTE_HIGH_CONTRAST_DARK
};
static_assert(_countof(kPaletteRows) == INTERFACE_PALETTE_COUNT, "Every built-in palette must be listed");
BOOL CALLBACK FindPreviewMenuBar(HWND window, LPARAM parameter)
{
	wchar_t className[32] = {};
	if(!::GetClassNameW(window, className, _countof(className)) || _wcsicmp(className, TOOLBARCLASSNAMEW) != 0) return TRUE;
	const HMENU menu = reinterpret_cast<HMENU>(::SendMessageW(window, CBRM_GETMENU, 0, 0));
	if(!::IsMenu(menu)) return TRUE;
	*reinterpret_cast<HMENU*>(parameter) = menu;
	return FALSE;
}

struct InterfaceLanguageChoice
{
	DWORD languageId;
	UINT stringId;
	LPCWSTR localeName;
};

const InterfaceLanguageChoice kInterfaceLanguages[] = {
	{ FBE_INTERFACE_LANGUAGE_AUTO, IDS_LANG_SYSTEM_DEFAULT, NULL },
	{ FBE_INTERFACE_LANGUAGE_ENGLISH, IDS_LANG_ENGLISH, L"en-US" },
	{ FBE_INTERFACE_LANGUAGE_RUSSIAN, IDS_LANG_RUSSIAN, L"ru-RU" },
	{ FBE_INTERFACE_LANGUAGE_UKRAINIAN, IDS_LANG_UKRAINIAN, L"uk-UA" },
	{ FBE_INTERFACE_LANGUAGE_GERMAN, IDS_LANG_GERMAN, L"de-DE" },
	{ FBE_INTERFACE_LANGUAGE_FRENCH, IDS_LANG_FRENCH, L"fr-FR" },
	{ FBE_INTERFACE_LANGUAGE_SPANISH, IDS_LANG_SPANISH, L"es-ES" },
	{ FBE_INTERFACE_LANGUAGE_ITALIAN, IDS_LANG_ITALIAN, L"it-IT" },
	{ FBE_INTERFACE_LANGUAGE_POLISH, IDS_LANG_POLISH, L"pl-PL" },
	{ FBE_INTERFACE_LANGUAGE_PORTUGUESE, IDS_LANG_PORTUGUESE, L"pt-PT" },
	{ FBE_INTERFACE_LANGUAGE_DUTCH, IDS_LANG_DUTCH, L"nl-NL" },
	{ FBE_INTERFACE_LANGUAGE_CZECH, IDS_LANG_CZECH, L"cs-CZ" },
	{ FBE_INTERFACE_LANGUAGE_BULGARIAN, IDS_LANG_BULGARIAN, L"bg-BG" },
};

void SetText(HWND window, int controlId, LPCWSTR key, LPCWSTR fallback)
{
	::SetDlgItemText(window, controlId, FbeLoadRuntimeStringByKey(key, fallback));
}
const wchar_t* PaletteId(int index)
{
	return ThemeManager::NamedPaletteId(static_cast<InterfacePalette>(index));
}
}

InterfacePalette CSettingsGeneralPage::PaletteForRow(int row)
{
	return row >= 0 && row < INTERFACE_PALETTE_COUNT ? kPaletteRows[row] : INTERFACE_PALETTE_CLASSIC;
}
int CSettingsGeneralPage::RowForPalette(InterfacePalette palette)
{
	for(int row = 0; row < INTERFACE_PALETTE_COUNT; ++row) if(kPaletteRows[row] == palette) return row;
	return 0;
}

class CPaletteManagerDialog : public CDialogImpl<CPaletteManagerDialog>
{
    CSettingsGeneralPage& m_page;
    CSettingsTooltips m_tooltips;
public:
    explicit CPaletteManagerDialog(CSettingsGeneralPage& page) : m_page(page) {}
    enum { IDD = IDD_SETTINGS_PALETTE_MANAGER };
    BEGIN_MSG_MAP(CPaletteManagerDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        COMMAND_HANDLER(IDOK, BN_CLICKED, OnOk)
        COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnCancel)
        COMMAND_HANDLER(IDC_PALETTE_ROLE, CBN_SELCHANGE, OnRoleChanged)
        COMMAND_HANDLER(IDC_PALETTE_IMPORT, BN_CLICKED, OnAction)
        COMMAND_HANDLER(IDC_PALETTE_EXPORT, BN_CLICKED, OnAction)
        COMMAND_HANDLER(IDC_PALETTE_RENAME, BN_CLICKED, OnAction)
        COMMAND_HANDLER(IDC_PALETTE_DELETE, BN_CLICKED, OnAction)
        COMMAND_HANDLER(IDC_PALETTE_CHOOSE_COLOR, BN_CLICKED, OnAction)
        COMMAND_HANDLER(IDC_PALETTE_RESET_COLOR, BN_CLICKED, OnAction)
    END_MSG_MAP()
    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
    {
        m_page.m_paletteManager = m_hWnd;
        SetWindowText(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.manage", L"Manage colour schemes..."));
        for(const auto& entry : { std::make_pair(IDC_PALETTE_IMPORT, L"fbe.settings.palette.import"),
            std::make_pair(IDC_PALETTE_EXPORT, L"fbe.settings.palette.export"),
            std::make_pair(IDC_PALETTE_RENAME, L"fbe.settings.palette.rename"),
            std::make_pair(IDC_PALETTE_DELETE, L"fbe.settings.palette.delete"),
            std::make_pair(IDC_PALETTE_ROLE_LABEL, L"fbe.settings.palette.role"),
            std::make_pair(IDC_PALETTE_CHOOSE_COLOR, L"fbe.settings.palette.choose_color"),
            std::make_pair(IDC_PALETTE_RESET_COLOR, L"fbe.settings.palette.reset_color") })
            SetDlgItemText(entry.first, FbeLoadRuntimeStringByKey(entry.second));
        SetDlgItemText(IDOK, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_tools_settings.ok", L"OK"));
        SetDlgItemText(IDCANCEL, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_tools_settings.cancel", L"Cancel"));
        FitLocalizedLayout();
        CComboBox roles(GetDlgItem(IDC_PALETTE_ROLE).m_hWnd);
        for(int i = 0; i < m_page.m_paletteRole.GetCount(); ++i) { CString text; m_page.m_paletteRole.GetLBText(i, text); roles.AddString(text); }
        roles.SetCurSel(m_page.m_paletteRole.GetCurSel());
        m_tooltips.Initialize(m_hWnd);
        m_tooltips.Add(GetDlgItem(IDC_PALETTE_CURRENT), L"fbe.settings.tooltip.general.interface_palette", L"Selected colour scheme.");
        for(int id : {IDC_PALETTE_ROLE, IDC_PALETTE_ROLE_LABEL, IDC_PALETTE_CHOOSE_COLOR, IDC_PALETTE_RESET_COLOR}) {
            m_tooltips.Add(GetDlgItem(id), L"fbe.settings.tooltip.general.palette_color", L"Edit a colour in a custom palette.");
            m_tooltips.AddDisabledControlArea(GetDlgItem(id), L"fbe.settings.tooltip.general.palette_builtin", L"Create a copy of the built-in scheme before editing it.");
        }
        for(const auto& item : {std::make_pair(IDC_PALETTE_IMPORT, L"fbe.settings.tooltip.general.palette_import"),
            std::make_pair(IDC_PALETTE_EXPORT, L"fbe.settings.tooltip.general.palette_export"),
            std::make_pair(IDC_PALETTE_RENAME, L"fbe.settings.tooltip.general.palette_rename"),
            std::make_pair(IDC_PALETTE_DELETE, L"fbe.settings.tooltip.general.palette_delete")}) {
            m_tooltips.Add(GetDlgItem(item.first), item.second, L"Manage a custom colour scheme.");
            if(item.first != IDC_PALETTE_IMPORT)
                m_tooltips.AddDisabledControlArea(GetDlgItem(item.first), L"fbe.settings.tooltip.general.palette_builtin", L"Create a copy of the built-in scheme before editing it.");
        }
        ThemeManager::ApplyToWindow(m_hWnd); Refresh(); CenterWindow(GetParent()); PublishAuditMetrics();
        if(RuntimeTests::IsScenario(L"custom-palette-manage") && ::GetPropW(m_page.m_hWnd, L"FBE.PaletteManagerTest")) SetTimer(0x504D, 1);
        return TRUE;
    }
    void PublishAuditMetrics()
    {
        wchar_t enabled[4] = {};
        if(::GetEnvironmentVariableW(L"FBE_NEXT_SETTINGS_LAYOUT_AUDIT", enabled, _countof(enabled)) != 1 || enabled[0] != L'1') return;
        for(HWND child = ::GetWindow(m_hWnd, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT)) {
            wchar_t kind[32] = {}; ::GetClassNameW(child, kind, _countof(kind));
            const bool button = _wcsicmp(kind, L"Button") == 0, label = _wcsicmp(kind, L"Static") == 0;
            if(!button && !label) continue;
            const int length = ::GetWindowTextLengthW(child); if(length <= 0) continue;
            CString text; ::GetWindowTextW(child, text.GetBuffer(length + 1), length + 1); text.ReleaseBuffer();
            RECT bounds = {}; ::GetClientRect(child, &bounds);
            const int inset = max(2, static_cast<int>(UiMetrics::DpiForWindow(child) / 48));
            RECT measured = {0, 0, max(1, static_cast<int>(bounds.right) - 2 * inset), 0};
            HDC dc = ::GetDC(child); if(!dc) continue;
            HFONT font = reinterpret_cast<HFONT>(::SendMessageW(child, WM_GETFONT, 0, 0));
            HGDIOBJ previous = font ? ::SelectObject(dc, font) : NULL;
            ::DrawTextW(dc, text, text.GetLength(), &measured, DT_CALCRECT | (label ? DT_WORDBREAK : DT_SINGLELINE));
            if(previous) ::SelectObject(dc, previous); ::ReleaseDC(child, dc);
            ::SetPropW(child, L"FBE_AUDIT_TEXT_WIDTH", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(measured.right + 1)));
            ::SetPropW(child, L"FBE_AUDIT_TEXT_HEIGHT", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(measured.bottom + 1)));
        }
    }
    void FitLocalizedLayout()
    {
        const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
        const int margin = UiMetrics::ScaleForDpi(8, dpi), gap = UiMetrics::ScaleForDpi(6, dpi);
        HDC dc = ::GetDC(m_hWnd); HFONT font = reinterpret_cast<HFONT>(::SendMessageW(m_hWnd, WM_GETFONT, 0, 0));
        HGDIOBJ oldFont = dc && font ? ::SelectObject(dc, font) : NULL;
        int buttonTextWidth = 0, labelWidth = 0, fontHeight = UiMetrics::ScaleForDpi(16, dpi);
        if(dc) {
            TEXTMETRICW metrics = {}; if(::GetTextMetricsW(dc, &metrics)) fontHeight = metrics.tmHeight;
            for(int id : {IDC_PALETTE_IMPORT, IDC_PALETTE_EXPORT, IDC_PALETTE_RENAME, IDC_PALETTE_DELETE,
                IDC_PALETTE_CHOOSE_COLOR, IDC_PALETTE_RESET_COLOR, IDOK, IDCANCEL}) {
                CString text; GetDlgItemText(id, text); SIZE extent = {};
                if(::GetTextExtentPoint32W(dc, text, text.GetLength(), &extent)) buttonTextWidth = max(buttonTextWidth, static_cast<int>(extent.cx));
            }
            CString text; GetDlgItemText(IDC_PALETTE_ROLE_LABEL, text); SIZE extent = {};
            if(::GetTextExtentPoint32W(dc, text, text.GetLength(), &extent)) labelWidth = extent.cx;
        }
        if(oldFont) ::SelectObject(dc, oldFont); if(dc) ::ReleaseDC(m_hWnd, dc);
        labelWidth += margin; // Leave room for text overhang and static-control padding.
        const int buttonWidth = max(UiMetrics::ScaleForDpi(95, dpi), buttonTextWidth + 2 * margin);
        const int lineHeight = max(UiMetrics::ScaleForDpi(28, dpi), fontHeight + 2 * margin);
        RECT client = {}, window = {}; ::GetClientRect(m_hWnd, &client); ::GetWindowRect(m_hWnd, &window);
        const int width = max(static_cast<int>(client.right), max(2 * buttonWidth + 3 * margin,
            labelWidth + UiMetrics::ScaleForDpi(140, dpi) + 3 * margin));
        const int right = width - margin, mid = width / 2;
        int y = margin;
        auto place = [&](int id, int x, int top, int w, int h) {
            ::SetWindowPos(GetDlgItem(id), NULL, x, top, max(1, w), max(1, h), SWP_NOZORDER | SWP_NOACTIVATE);
        };
        place(IDC_PALETTE_CURRENT, margin, y, right - margin, lineHeight); y += lineHeight + gap;
        place(IDC_PALETTE_IMPORT, margin, y, mid - margin - gap / 2, lineHeight);
        place(IDC_PALETTE_EXPORT, mid + gap / 2, y, right - mid - gap / 2, lineHeight); y += lineHeight + gap;
        place(IDC_PALETTE_RENAME, margin, y, mid - margin - gap / 2, lineHeight);
        place(IDC_PALETTE_DELETE, mid + gap / 2, y, right - mid - gap / 2, lineHeight); y += lineHeight + gap;
        const int comboLeft = margin + labelWidth + gap;
        RECT combo = {}; ::GetWindowRect(GetDlgItem(IDC_PALETTE_ROLE), &combo);
        place(IDC_PALETTE_ROLE_LABEL, margin, y, labelWidth, lineHeight);
        place(IDC_PALETTE_ROLE, comboLeft, y, right - comboLeft, combo.bottom - combo.top); y += lineHeight + gap;
        place(IDC_PALETTE_CHOOSE_COLOR, margin, y, mid - margin - gap / 2, lineHeight);
        place(IDC_PALETTE_RESET_COLOR, mid + gap / 2, y, right - mid - gap / 2, lineHeight); y += lineHeight + gap;
        place(IDC_PALETTE_CONTRAST, margin, y, right - margin, 3 * fontHeight + 2 * margin);
        y += 3 * fontHeight + 3 * margin;
        place(IDOK, right - 2 * buttonWidth - gap, y, buttonWidth, lineHeight);
        place(IDCANCEL, right - buttonWidth, y, buttonWidth, lineHeight); y += lineHeight + margin;
        ::SetWindowPos(m_hWnd, NULL, 0, 0, width + window.right - window.left - client.right,
            y + window.bottom - window.top - client.bottom, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    void Refresh()
    {
        CString current; const int selected = m_page.m_interfacePalette.GetCurSel();
        if(selected >= 0) m_page.m_interfacePalette.GetLBText(selected, current);
        SetDlgItemText(IDC_PALETTE_CURRENT, current);
        m_tooltips.UpdateText(GetDlgItem(IDC_PALETTE_CURRENT), current);
        const int customIndex = m_page.m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT;
        const bool editable = customIndex >= 0 && customIndex < static_cast<int>(m_page.m_customPalettes.size());
        for(int id : {IDC_PALETTE_EXPORT, IDC_PALETTE_RENAME, IDC_PALETTE_DELETE, IDC_PALETTE_ROLE_LABEL,
            IDC_PALETTE_ROLE, IDC_PALETTE_CHOOSE_COLOR, IDC_PALETTE_RESET_COLOR}) GetDlgItem(id).EnableWindow(editable);
        CString contrast; m_page.GetDlgItemText(IDC_PALETTE_CONTRAST, contrast); SetDlgItemText(IDC_PALETTE_CONTRAST, contrast);
    }
    LRESULT OnRoleChanged(WORD, WORD, HWND, BOOL&) { m_page.m_paletteRole.SetCurSel(CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel()); return 0; }
    LRESULT OnAction(WORD, WORD id, HWND control, BOOL&)
    {
        BOOL handled = FALSE;
        switch(id) {
        case IDC_PALETTE_IMPORT: m_page.OnImportPalette(BN_CLICKED, id, control, handled); break;
        case IDC_PALETTE_EXPORT: m_page.OnExportPalette(BN_CLICKED, id, control, handled); break;
        case IDC_PALETTE_RENAME: m_page.OnRenamePalette(BN_CLICKED, id, control, handled); break;
        case IDC_PALETTE_DELETE: m_page.OnDeletePalette(BN_CLICKED, id, control, handled); break;
        case IDC_PALETTE_CHOOSE_COLOR: m_page.OnChoosePaletteColor(BN_CLICKED, id, control, handled); break;
        case IDC_PALETTE_RESET_COLOR: m_page.OnResetPaletteColor(BN_CLICKED, id, control, handled); break;
        }
        Refresh(); return 0;
    }
    LRESULT OnTimer(UINT, WPARAM id, LPARAM, BOOL&)
    {
        if(id != 0x504D) return 0;
        KillTimer(id);
        const ULONG_PTR mode = reinterpret_cast<ULONG_PTR>(::GetPropW(m_page.m_hWnd, L"FBE.PaletteManagerTest"));
        const bool editable = ::IsWindowEnabled(GetDlgItem(IDC_PALETTE_CHOOSE_COLOR)) != FALSE;
        bool passed = mode == 1 && !editable;
        if(mode == 2 && editable) {
            const int index = m_page.m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT;
            COLORREF before[THEME_COLOR_COUNT] = {}, after[THEME_COLOR_COUNT] = {};
            ThemeManager::PreviewColors(static_cast<InterfaceTheme>(m_page.m_acceptedThemeIndex), INTERFACE_PALETTE_CLASSIC, &m_page.m_customPalettes[index], before);
            ::SendMessageW(m_hWnd, WM_COMMAND, MAKEWPARAM(IDC_PALETTE_RESET_COLOR, BN_CLICKED),
                reinterpret_cast<LPARAM>(GetDlgItem(IDC_PALETTE_RESET_COLOR).m_hWnd));
            ThemeManager::PreviewColors(static_cast<InterfaceTheme>(m_page.m_acceptedThemeIndex), INTERFACE_PALETTE_CLASSIC, &m_page.m_customPalettes[index], after);
            passed = after[THEME_COLOR_WINDOW] != before[THEME_COLOR_WINDOW];
        }
        ::SetPropW(m_page.m_hWnd, L"FBE.PaletteManagerResult", reinterpret_cast<HANDLE>(passed ? 1 : 2));
        PostMessage(WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(IDCANCEL).m_hWnd));
        return 0;
    }
    LRESULT OnOk(WORD, WORD, HWND, BOOL&) { m_page.m_paletteManager = NULL; EndDialog(IDOK); return 0; }
    LRESULT OnCancel(WORD, WORD, HWND, BOOL&) { m_page.m_paletteManager = NULL; EndDialog(IDCANCEL); return 0; }
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&) { m_page.m_paletteManager = NULL; EndDialog(IDCANCEL); return 0; }
};

LRESULT CSettingsGeneralPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_SETTINGS_GENERAL);
	m_language = GetDlgItem(IDC_LANG);
	m_originalTheme = _Settings.GetInterfaceTheme();
	m_interfaceTheme = GetDlgItem(IDC_INTERFACE_THEME);
	m_interfacePalette = GetDlgItem(IDC_INTERFACE_PALETTE);
	m_interfacePalette.SetDroppedWidth(UiMetrics::ScaleForDpi(450, UiMetrics::DpiForWindow(m_hWnd)));
	::SetWindowSubclass(m_interfaceTheme, ThemeComboSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
	::SetWindowSubclass(m_interfacePalette, ThemeComboSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
	UpdateThemeComboItemHeight(m_interfaceTheme); UpdateThemeComboItemHeight(m_interfacePalette);
	m_paletteRole = GetDlgItem(IDC_PALETTE_ROLE);
	m_originalPalette = ThemeManager::GetNamedPalette();
	m_originalCustomPaletteActive = ThemeManager::GetCustomPalette(m_originalCustomPalette);
	for(int index = 0; index < INTERFACE_PALETTE_COUNT; ++index)
		if(_Settings.GetInterfacePalette() == PaletteId(index)) m_acceptedPaletteIndex = index;
	m_scriptIconSize = GetDlgItem(IDC_SCRIPT_ICON_SIZE);
	m_mainToolbarIconSize = GetDlgItem(IDC_MAIN_TOOLBAR_ICON_SIZE);
	m_genreCatalog = GetDlgItem(IDC_GENRE_CATALOG);
	m_descriptionLayout = GetDlgItem(IDC_DESCRIPTION_LAYOUT);
	SetText(m_hWnd, IDC_DESCRIPTION_LAYOUT_LABEL, L"fbe.description.layout", L"Description layout:");
	m_descriptionLayout.AddString(FbeLoadRuntimeStringByKey(L"fbe.description.modern", L"Modern"));
	m_descriptionLayout.AddString(FbeLoadRuntimeStringByKey(L"fbe.description.classic", L"Classic"));
	m_descriptionLayout.SetCurSel(_Settings.GetDescriptionLayout() == L"classic" ? 1 : 0);
	m_defaultEncoding = GetDlgItem(IDC_DEFAULT_ENC);
	m_keepEncoding = GetDlgItem(IDC_KEEP);
	m_restorePosition = GetDlgItem(IDC_RESTORE_POS);
	m_updateChannel = GetDlgItem(IDC_UPDATE_CHANNEL);
	m_tooltips.Initialize(m_hWnd);
	m_tooltips.Add(m_language, L"fbe.settings.tooltip.general.language", L"Interface language. System default uses the Windows language.");
	for(HWND control : {m_interfaceTheme.m_hWnd, GetDlgItem(IDC_INTERFACE_THEME_LABEL).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.interface_theme", L"Automatic follows the Windows app theme.");
	for(HWND control : {m_interfacePalette.m_hWnd, GetDlgItem(IDC_INTERFACE_PALETTE_LABEL).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.interface_palette", L"Choose interface colours independently of the Windows mode. High Contrast uses system colours.");
	m_tooltips.Add(GetDlgItem(IDC_INTERFACE_PALETTE_COPY), L"fbe.settings.tooltip.general.palette_copy", L"Create a copy of the selected palette. It is saved when you confirm settings.");
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_MANAGE), L"fbe.settings.tooltip.general.palette_manage", L"Import, export and edit custom colour schemes.");
	m_tooltips.Add(GetDlgItem(IDC_APPEARANCE_THEME_PREVIEW), L"fbe.settings.tooltip.general.interface_palette", L"Preview the selected interface colours.");
	m_tooltips.Add(GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW), L"fbe.settings.tooltip.general.main_toolbar_icon_size", L"Preview the selected toolbar icon sizes.");
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_IMPORT), L"fbe.settings.tooltip.general.palette_import", L"Import a JSON palette. It is saved when you confirm settings.");
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_EXPORT), L"fbe.settings.tooltip.general.palette_export", L"Export the selected custom palette to a JSON file.");
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_RENAME), L"fbe.settings.tooltip.general.palette_rename", L"Rename the selected custom palette. The change is saved when you confirm settings.");
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_DELETE), L"fbe.settings.tooltip.general.palette_delete", L"Delete the selected custom palette when you confirm settings. Cancel keeps it.");
	m_tooltips.AddDisabledControlArea(GetDlgItem(IDC_PALETTE_RENAME), L"fbe.settings.tooltip.general.palette_rename", L"Rename the selected custom palette. The change is saved when you confirm settings.");
	m_tooltips.AddDisabledControlArea(GetDlgItem(IDC_PALETTE_DELETE), L"fbe.settings.tooltip.general.palette_delete", L"Delete the selected custom palette when you confirm settings. Cancel keeps it.");
	for(HWND control : {m_paletteRole.m_hWnd, GetDlgItem(IDC_PALETTE_ROLE_LABEL).m_hWnd, GetDlgItem(IDC_PALETTE_CHOOSE_COLOR).m_hWnd, GetDlgItem(IDC_PALETTE_RESET_COLOR).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.palette_color", L"Edit a colour in a custom palette. Use the base colour to restore inheritance; Cancel discards changes.");
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_CONTRAST), L"fbe.settings.tooltip.general.palette_contrast", L"Checks text against window, control, selection, hover and pressed backgrounds. Disabled text uses a 3:1 threshold.");
	for(HWND control : {m_descriptionLayout.m_hWnd, GetDlgItem(IDC_DESCRIPTION_LAYOUT_LABEL).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.description_layout", L"Choose the Description layout. Modern uses compact, collapsible sections; Classic keeps the familiar FictionBook Editor field order. This does not change FB2 data.");
	for(HWND control : {m_scriptIconSize.m_hWnd, GetDlgItem(IDC_SCRIPT_ICON_SIZE_LABEL).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.script_icon_size", L"Sets the icon size in the script tree and script toolbars. Available sizes: 16, 20, 24 and 32 pixels (scaled for DPI).");
	for(HWND control : {m_mainToolbarIconSize.m_hWnd, GetDlgItem(IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL).m_hWnd})
		m_tooltips.Add(control, L"fbe.settings.tooltip.general.main_toolbar_icon_size", L"Sets the main toolbar icon size independently of script icons. Windows DPI scaling is applied.");
	m_tooltips.Add(m_genreCatalog, L"fbe.settings.tooltip.general.genre_catalog", L"Selects genres available when editing book metadata.");
	m_tooltips.Add(m_restorePosition, L"fbe.settings.tooltip.general.restore_position", L"Restores the saved reading or editing position when reopening a document.");
	m_tooltips.Add(m_defaultEncoding, L"fbe.settings.tooltip.general.default_encoding", L"Encoding used when saving if the original encoding is not kept.");
	m_tooltips.Add(m_keepEncoding, L"fbe.settings.tooltip.general.keep_encoding", L"Keeps the encoding in which the document was opened whenever possible.");
	m_tooltips.Add(m_updateChannel, L"fbe.settings.tooltip.general.update_channel", L"Controls which application versions are offered by the built-in updater.");
	m_tooltips.Add(GetDlgItem(IDC_CREATE_BACKUP_FILE), L"fbe.settings.tooltip.general.backup", L"Creates a .bak copy before saving an existing file.");
	m_tooltips.Add(GetDlgItem(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE), L"fbe.settings.tooltip.general.full_path", L"Shows the complete file path in the application window title.");

	SetText(m_hWnd, IDC_OPTIONS_INTERFACE_GROUP, L"fbe.dialog.idd_options.interface", L"Interface");
	SetText(m_hWnd, IDC_OPTIONS_LANGUAGE_LABEL, L"fbe.dialog.idd_options.language", L"Language:");
	SetText(m_hWnd, IDC_INTERFACE_THEME_LABEL, L"fbe.dialog.idd_options.interface_theme", L"Interface theme:");
	SetText(m_hWnd, IDC_INTERFACE_PALETTE_LABEL, L"fbe.settings.interface_palette", L"Interface palette:");
	SetText(m_hWnd, IDC_INTERFACE_PALETTE_COPY, L"fbe.settings.palette.copy", L"Create copy...");
	SetText(m_hWnd, IDC_PALETTE_MANAGE, L"fbe.settings.palette.manage", L"Manage schemes...");
	SetText(m_hWnd, IDC_PALETTE_IMPORT, L"fbe.settings.palette.import", L"Import...");
	SetText(m_hWnd, IDC_PALETTE_EXPORT, L"fbe.settings.palette.export", L"Export...");
	SetText(m_hWnd, IDC_PALETTE_RENAME, L"fbe.settings.palette.rename", L"Rename...");
	SetText(m_hWnd, IDC_PALETTE_DELETE, L"fbe.settings.palette.delete", L"Delete");
	SetText(m_hWnd, IDC_PALETTE_ROLE_LABEL, L"fbe.settings.palette.role", L"Colour role:");
	SetText(m_hWnd, IDC_PALETTE_CHOOSE_COLOR, L"fbe.settings.palette.choose_color", L"Choose colour...");
	SetText(m_hWnd, IDC_PALETTE_RESET_COLOR, L"fbe.settings.palette.reset_color", L"Use base colour");
	SetText(m_hWnd, IDC_SCRIPT_ICON_SIZE_LABEL, L"fbe.settings.script_icon_size", L"Script icon size:");
	SetText(m_hWnd, IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, L"fbe.settings.main_toolbar_icon_size", L"Main toolbar icon size:");
	SetText(m_hWnd, IDC_OPTIONS_GENRE_CATALOG_LABEL, L"fbe.dialog.idd_options.genre_catalog", L"Genre catalog:");
	SetText(m_hWnd, IDC_SETTINGS_OTHER_OPEN_GROUP, L"fbe.dialog.idd_setting_other.open_file", L"Open file");
	SetText(m_hWnd, IDC_RESTORE_POS, L"fbe.dialog.idd_setting_other.restore_position", L"Restore position");
	SetText(m_hWnd, IDC_SETTINGS_OTHER_SAVE_GROUP, L"fbe.dialog.idd_setting_other.save_file", L"Saving");
	SetText(m_hWnd, IDC_SETTINGS_OTHER_ENCODING, L"fbe.dialog.idd_setting_other.default_encoding", L"Default encoding:");
	SetText(m_hWnd, IDC_KEEP, L"fbe.dialog.idd_setting_other.keep_manual", L"Keep original encoding");
	SetText(m_hWnd, IDC_FBE_NEXT_SAVING_GROUP, L"fbe.dialog.idd_setting_next.saving", L"Backup");
	SetText(m_hWnd, IDC_CREATE_BACKUP_FILE, L"fbe.dialog.idd_setting_next.create_backup_file", L"Create a backup copy (.bak) when saving an existing file");
	SetText(m_hWnd, IDC_FBE_NEXT_WINDOW_TITLE_GROUP, L"fbe.dialog.idd_setting_next.window_title", L"Window title");
	SetText(m_hWnd, IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE, L"fbe.dialog.idd_setting_next.show_full_path_in_window_title", L"Show full file path in the window title");
	SetText(m_hWnd, IDC_FBE_NEXT_UPDATES_GROUP, L"fbe.dialog.idd_setting_next.updates", L"Updates");
	SetText(m_hWnd, IDC_UPDATE_CHANNEL_LABEL, L"fbe.dialog.idd_setting_next.update_channel", L"Update channel:");
	ConfigureInterfaceTheme(m_darkThemeSupported);
	for(int row = 0; row < INTERFACE_PALETTE_COUNT; ++row) {
		const InterfacePalette index = PaletteForRow(row);
		if(index == INTERFACE_PALETTE_CLASSIC) m_interfacePalette.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.classic", L"Classic (Windows mode)"));
		else if(index == INTERFACE_PALETTE_GRAPHITE) m_interfacePalette.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.graphite", L"Graphite"));
		else if(index == INTERFACE_PALETTE_NAVY) m_interfacePalette.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.navy", L"Dark blue"));
		else if(index == INTERFACE_PALETTE_SEPIA) m_interfacePalette.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.sepia", L"Sepia"));
		else if(index == INTERFACE_PALETTE_HIGH_CONTRAST_LIGHT)
			m_interfacePalette.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_palette.high_contrast_light", L"High contrast light"));
		else if(index == INTERFACE_PALETTE_HIGH_CONTRAST_DARK)
			m_interfacePalette.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.source_palette.high_contrast_dark", L"High contrast dark"));
		else m_interfacePalette.AddString(ThemeManager::NamedPaletteName(index));
	}
	m_acceptedPaletteIndex = RowForPalette(static_cast<InterfacePalette>(m_acceptedPaletteIndex));
	for(const ThemeManager::CustomPalette& palette : ThemeManager::ListCustomPalettesInProfile()) {
		const CString label(palette.name.c_str());
		const int index = m_interfacePalette.AddString(label);
		if(index < 0) break;
		m_customPalettes.push_back(palette);
		if(_Settings.GetInterfacePalette() == CString(L"custom:") + palette.id.c_str()) m_acceptedPaletteIndex = index;
	}
	// Keep an unavailable stored preference untouched while showing the usable fallback.
	if(!PaletteItemAvailable(m_acceptedPaletteIndex)) m_acceptedPaletteIndex = INTERFACE_PALETTE_CLASSIC;
	m_interfacePalette.SetCurSel(m_acceptedPaletteIndex);
	CString roles = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.roles", L"Window|Control|Text|Border|Separator|Secondary text|Disabled text|Selection background|Selection text|Hover|Pressed|Focus|Accent|Error|Warning|Success");
	int rolePosition = 0;
	for(int role = 0; role < THEME_COLOR_COUNT; ++role) m_paletteRole.AddString(roles.Tokenize(L"|", rolePosition));
	m_paletteRole.SetCurSel(0);
	UpdatePaletteColorControls();
	FitLocalizedLayout();

	m_language.SetDroppedWidth(320);
	const DWORD currentLanguage = _Settings.GetInterfaceLanguageID();
	int selectedLanguage = 0;
	bool currentLanguagePresent = false;
	wchar_t text[MAX_LOAD_STRING + 1];
	for(int i = 0; i < _countof(kInterfaceLanguages); ++i)
	{
		if(kInterfaceLanguages[i].localeName && !FbeIsRuntimeLocaleInstalled(kInterfaceLanguages[i].localeName))
			continue;
		if(FbeLoadString(_Module.GetResourceInstance(), kInterfaceLanguages[i].stringId, text, MAX_LOAD_STRING))
		{
			const int item = m_language.AddString(text);
			m_language.SetItemData(item, kInterfaceLanguages[i].languageId);
			if(kInterfaceLanguages[i].languageId == currentLanguage) { selectedLanguage = item; currentLanguagePresent = true; }
		}
	}
	if(!currentLanguagePresent)
	{
		for(int i = 0; i < _countof(kInterfaceLanguages); ++i)
			if(kInterfaceLanguages[i].languageId == currentLanguage && FbeLoadString(_Module.GetResourceInstance(), kInterfaceLanguages[i].stringId, text, MAX_LOAD_STRING))
			{
				selectedLanguage = m_language.AddString(text);
				m_language.SetItemData(selectedLanguage, currentLanguage);
				break;
			}
	}
	m_language.SetCurSel(selectedLanguage);
	for(int size : {16, 20, 24, 32}) {
		CString iconText; iconText.Format(L"%d \x00d7 %d", size, size);
		const int index = m_scriptIconSize.AddString(iconText); m_scriptIconSize.SetItemData(index, size);
		if(size == _Settings.GetScriptIconSize()) m_scriptIconSize.SetCurSel(index);
		const int mainIndex = m_mainToolbarIconSize.AddString(iconText); m_mainToolbarIconSize.SetItemData(mainIndex, size);
		if(size == _Settings.GetMainToolbarIconSize()) m_mainToolbarIconSize.SetCurSel(mainIndex);
	}
	m_genreCatalog.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.genre_catalog.standard", L"Standard"));
	m_genreCatalog.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_options.genre_catalog.librusec", L"Librusec"));
	m_genreCatalog.SetCurSel(_Settings.GetGenreCatalog() == GenreCatalog::Librusec ? 1 : 0);

	if(FbeLoadString(_Module.GetResourceInstance(), IDS_ENCODINGS, text, _countof(text)))
	{
		for(TCHAR* encoding = text; *encoding; )
		{
			size_t length = _tcscspn(encoding, _T(","));
			if(encoding[length]) encoding[length++] = _T('\0');
			if(*encoding) m_defaultEncoding.AddString(encoding);
			encoding += length;
		}
	}
	int encodingIndex = m_defaultEncoding.SelectString(0, _Settings.GetDefaultEncoding());
	if(encodingIndex == CB_ERR) encodingIndex = m_defaultEncoding.AddString(_Settings.GetDefaultEncoding());
	m_defaultEncoding.SetCurSel(encodingIndex);
	m_keepEncoding.SetCheck(_Settings.KeepEncoding() ? BST_CHECKED : BST_UNCHECKED);
	m_restorePosition.SetCheck(_Settings.RestoreFilePosition() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CREATE_BACKUP_FILE, _Settings.GetCreateBackupFile() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE, _Settings.GetShowFullPathInWindowTitle() ? BST_CHECKED : BST_UNCHECKED);
	m_updateChannel.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.update_channel.stable", L"Stable versions (recommended)"));
	m_updateChannel.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_next.update_channel.prerelease", L"Prerelease versions (Beta / RC)"));
	m_updateChannel.SetCurSel(_Settings.GetUpdateChannel() == UpdateChannel::Prerelease ? 1 : 0);
	return 1;
}

void CSettingsGeneralPage::FitLocalizedLayout()
{
	const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
	const int margin = UiMetrics::ScaleForDpi(8, dpi);
	const int gap = UiMetrics::ScaleForDpi(8, dpi);
	const int padding = UiMetrics::ScaleForDpi(10, dpi);
	const int rowGap = UiMetrics::ScaleForDpi(2, dpi);
	const int minimumTextHeight = UiMetrics::ScaleForDpi(26, dpi);
	auto bounds = [this](int id) {
		CRect result;
		::GetWindowRect(GetDlgItem(id), &result);
		::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&result), 2);
		return result;
	};
	auto place = [this](int id, const CRect& rect) {
		::SetWindowPos(GetDlgItem(id), NULL, rect.left, rect.top, rect.Width(), rect.Height(),
			SWP_NOZORDER | SWP_NOACTIVATE);
	};
	auto measure = [this](int id, int width, bool wrap) {
		HWND control = GetDlgItem(id);
		CString caption;
		const int length = ::GetWindowTextLengthW(control);
		::GetWindowTextW(control, caption.GetBuffer(length + 1), length + 1);
		caption.ReleaseBuffer();
		HDC dc = ::GetDC(control);
		if(!dc) return CSize(0, 0);
		HFONT font = reinterpret_cast<HFONT>(::SendMessage(control, WM_GETFONT, 0, 0));
		HGDIOBJ previous = font ? ::SelectObject(dc, font) : NULL;
		RECT text = { 0, 0, max(1, width), 0 };
		::DrawTextW(dc, caption, caption.GetLength(), &text,
			DT_CALCRECT | (wrap ? DT_WORDBREAK : DT_SINGLELINE));
		if(previous) ::SelectObject(dc, previous);
		::ReleaseDC(control, dc);
		return CSize(text.right - text.left, text.bottom - text.top);
	};
	int offset = 0;
	auto shift = [&](int groupId, std::initializer_list<int> children) {
		CRect group = bounds(groupId);
		group.OffsetRect(0, offset);
		place(groupId, group);
		for(int id : children) { CRect rect = bounds(id); rect.OffsetRect(0, offset); place(id, rect); }
	};
	auto grow = [&](int groupId, std::initializer_list<int> children) {
		CRect group = bounds(groupId);
		int bottom = group.bottom;
		for(int id : children) bottom = max(bottom, bounds(id).bottom + margin);
		const int growth = bottom - group.bottom;
		if(growth > 0) { group.bottom = bottom; place(groupId, group); offset += growth; }
	};
	auto fitLabelCombo = [&](int labelId, int comboId, int comboLeft, int comboRight) {
		CRect label = bounds(labelId), combo = bounds(comboId);
		label.right = comboLeft - gap;
		const CSize text = measure(labelId, label.Width(), true);
		label.bottom = label.top + max(minimumTextHeight, max(label.Height(), text.cy + padding));
		combo.left = comboLeft;
		combo.right = max(comboLeft, comboRight);
		place(labelId, label); place(comboId, combo);
	};
	auto separateRows = [&](int previousLabelId, int previousComboId, int nextLabelId, int nextComboId) {
		const int previousBottom = max(bounds(previousLabelId).bottom, bounds(previousComboId).bottom);
		CRect label = bounds(nextLabelId), combo = bounds(nextComboId);
		const int move = max(0, previousBottom + rowGap - min(label.top, combo.top));
		if(move > 0) { label.OffsetRect(0, move); combo.OffsetRect(0, move); place(nextLabelId, label); place(nextComboId, combo); }
	};
	auto fitCheckbox = [&](int groupId, int controlId, bool multiline) {
		CRect group = bounds(groupId), check = bounds(controlId);
		check.right = group.right - margin;
		const CSize text = measure(controlId, check.Width() - UiMetrics::ScaleForDpi(24, dpi), multiline);
		const int singleLineHeight = measure(controlId, 10000, false).cy;
		const int minimumHeight = (multiline ? 2 : 1) * singleLineHeight + padding;
		check.bottom = check.top + max(check.Height(), max(text.cy + padding, minimumHeight));
		place(controlId, check);
	};
	shift(IDC_OPTIONS_INTERFACE_GROUP, { IDC_OPTIONS_LANGUAGE_LABEL, IDC_LANG,
		IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG, IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME,
		IDC_INTERFACE_PALETTE_LABEL, IDC_INTERFACE_PALETTE, IDC_INTERFACE_PALETTE_COPY, IDC_PALETTE_IMPORT, IDC_PALETTE_EXPORT, IDC_PALETTE_RENAME, IDC_PALETTE_DELETE, IDC_PALETTE_ROLE_LABEL, IDC_PALETTE_ROLE, IDC_PALETTE_CHOOSE_COLOR, IDC_PALETTE_RESET_COLOR, IDC_PALETTE_CONTRAST, IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE, IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE, IDC_DESCRIPTION_LAYOUT_LABEL, IDC_DESCRIPTION_LAYOUT });
	const CRect interfaceGroup = bounds(IDC_OPTIONS_INTERFACE_GROUP);
	int comboLeft = bounds(IDC_LANG).left;
	for(int id : { IDC_OPTIONS_LANGUAGE_LABEL, IDC_OPTIONS_GENRE_CATALOG_LABEL,
		IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_PALETTE_LABEL, IDC_PALETTE_ROLE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE_LABEL, IDC_DESCRIPTION_LAYOUT_LABEL, IDC_SETTINGS_OTHER_ENCODING, IDC_UPDATE_CHANNEL_LABEL })
		comboLeft = max(comboLeft, bounds(id).left + measure(id, 10000, false).cx + gap);
	comboLeft = min(comboLeft, interfaceGroup.right - margin - UiMetrics::ScaleForDpi(130, dpi));
	const int interfaceComboRight = interfaceGroup.right - margin;
	fitLabelCombo(IDC_OPTIONS_LANGUAGE_LABEL, IDC_LANG, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_INTERFACE_PALETTE_LABEL, IDC_INTERFACE_PALETTE, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_PALETTE_ROLE_LABEL, IDC_PALETTE_ROLE, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE, comboLeft, interfaceComboRight);
	fitLabelCombo(IDC_DESCRIPTION_LAYOUT_LABEL, IDC_DESCRIPTION_LAYOUT, comboLeft, interfaceComboRight);
	separateRows(IDC_OPTIONS_LANGUAGE_LABEL, IDC_LANG, IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG);
	separateRows(IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG, IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME);
	separateRows(IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME, IDC_INTERFACE_PALETTE_LABEL, IDC_INTERFACE_PALETTE);
	CRect copyButton = bounds(IDC_INTERFACE_PALETTE_COPY);
	const int copyButtonHeight = copyButton.Height();
	copyButton.left = comboLeft;
	copyButton.right = interfaceComboRight;
	copyButton.top = max(bounds(IDC_INTERFACE_PALETTE_LABEL).bottom, bounds(IDC_INTERFACE_PALETTE).bottom) + rowGap;
	copyButton.bottom = copyButton.top + max(copyButtonHeight, measure(IDC_INTERFACE_PALETTE_COPY, copyButton.Width(), false).cy + padding);
	place(IDC_INTERFACE_PALETTE_COPY, copyButton);
	CRect importButton = bounds(IDC_PALETTE_IMPORT), exportButton = bounds(IDC_PALETTE_EXPORT);
	const int actionHeight = max(importButton.Height(), exportButton.Height());
	const int actionTop = copyButton.bottom + rowGap;
	const int actionMiddle = comboLeft + (interfaceComboRight - comboLeft) / 2;
	importButton.SetRect(comboLeft, actionTop, actionMiddle - gap / 2, actionTop + actionHeight);
	exportButton.SetRect(actionMiddle + gap / 2, actionTop, interfaceComboRight, actionTop + actionHeight);
	place(IDC_PALETTE_IMPORT, importButton); place(IDC_PALETTE_EXPORT, exportButton);
	CRect renameButton = bounds(IDC_PALETTE_RENAME), deleteButton = bounds(IDC_PALETTE_DELETE);
	const int editHeight = max(renameButton.Height(), deleteButton.Height());
	const int editTop = max(importButton.bottom, exportButton.bottom) + rowGap;
	renameButton.SetRect(comboLeft, editTop, actionMiddle - gap / 2, editTop + editHeight);
	deleteButton.SetRect(actionMiddle + gap / 2, editTop, interfaceComboRight, editTop + editHeight);
	place(IDC_PALETTE_RENAME, renameButton); place(IDC_PALETTE_DELETE, deleteButton);
	separateRows(IDC_PALETTE_RENAME, IDC_PALETTE_DELETE, IDC_PALETTE_ROLE_LABEL, IDC_PALETTE_ROLE);
	CRect choose = bounds(IDC_PALETTE_CHOOSE_COLOR), reset = bounds(IDC_PALETTE_RESET_COLOR);
	const int buttonHeight = max(choose.Height(), reset.Height());
	const int buttonTop = max(bounds(IDC_PALETTE_ROLE_LABEL).bottom, bounds(IDC_PALETTE_ROLE).bottom) + rowGap;
	choose.SetRect(comboLeft, buttonTop, interfaceComboRight, buttonTop + buttonHeight);
	reset.SetRect(comboLeft, choose.bottom + rowGap, interfaceComboRight, choose.bottom + rowGap + buttonHeight);
	place(IDC_PALETTE_CHOOSE_COLOR, choose); place(IDC_PALETTE_RESET_COLOR, reset);
	CRect contrast = bounds(IDC_PALETTE_CONTRAST);
	contrast.left = interfaceGroup.left + margin; contrast.right = interfaceComboRight;
	contrast.top = reset.bottom + rowGap; contrast.bottom = contrast.top + 3 * minimumTextHeight + padding;
	place(IDC_PALETTE_CONTRAST, contrast);
	separateRows(IDC_PALETTE_CONTRAST, IDC_PALETTE_CONTRAST, IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE);
	separateRows(IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE, IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE);
	separateRows(IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE, IDC_DESCRIPTION_LAYOUT_LABEL, IDC_DESCRIPTION_LAYOUT);
	grow(IDC_OPTIONS_INTERFACE_GROUP, { IDC_DESCRIPTION_LAYOUT_LABEL, IDC_DESCRIPTION_LAYOUT });
	if(!m_darkThemeSupported) {
		CRect note = bounds(IDC_INTERFACE_THEME_NOTE);
		note.left = interfaceGroup.left + margin; note.right = interfaceGroup.right - margin;
		note.top = max(bounds(IDC_DESCRIPTION_LAYOUT_LABEL).bottom, bounds(IDC_DESCRIPTION_LAYOUT).bottom) + rowGap;
		note.bottom = note.top + max(minimumTextHeight, measure(IDC_INTERFACE_THEME_NOTE, note.Width(), true).cy + padding);
		place(IDC_INTERFACE_THEME_NOTE, note);
		grow(IDC_OPTIONS_INTERFACE_GROUP, {IDC_INTERFACE_THEME_NOTE});
	}
	shift(IDC_SETTINGS_OTHER_OPEN_GROUP, { IDC_RESTORE_POS });
	fitCheckbox(IDC_SETTINGS_OTHER_OPEN_GROUP, IDC_RESTORE_POS, false);
	grow(IDC_SETTINGS_OTHER_OPEN_GROUP, { IDC_RESTORE_POS });
	shift(IDC_SETTINGS_OTHER_SAVE_GROUP, { IDC_SETTINGS_OTHER_ENCODING, IDC_DEFAULT_ENC, IDC_KEEP });
	const CRect savingGroup = bounds(IDC_SETTINGS_OTHER_SAVE_GROUP);
	fitLabelCombo(IDC_SETTINGS_OTHER_ENCODING, IDC_DEFAULT_ENC, comboLeft, savingGroup.right - margin);
	CRect keep = bounds(IDC_KEEP);
	const int keepMove = max(0, max(bounds(IDC_SETTINGS_OTHER_ENCODING).bottom,
		bounds(IDC_DEFAULT_ENC).bottom) + rowGap - keep.top);
	if(keepMove > 0) { keep.OffsetRect(0, keepMove); place(IDC_KEEP, keep); }
	fitCheckbox(IDC_SETTINGS_OTHER_SAVE_GROUP, IDC_KEEP, false);
	grow(IDC_SETTINGS_OTHER_SAVE_GROUP, { IDC_KEEP });
	shift(IDC_FBE_NEXT_SAVING_GROUP, { IDC_CREATE_BACKUP_FILE });
	fitCheckbox(IDC_FBE_NEXT_SAVING_GROUP, IDC_CREATE_BACKUP_FILE, true);
	grow(IDC_FBE_NEXT_SAVING_GROUP, { IDC_CREATE_BACKUP_FILE });
	shift(IDC_FBE_NEXT_WINDOW_TITLE_GROUP, { IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE });
	fitCheckbox(IDC_FBE_NEXT_WINDOW_TITLE_GROUP, IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE, true);
	grow(IDC_FBE_NEXT_WINDOW_TITLE_GROUP, { IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE });
	shift(IDC_FBE_NEXT_UPDATES_GROUP, { IDC_UPDATE_CHANNEL_LABEL, IDC_UPDATE_CHANNEL });
	const CRect updatesGroup = bounds(IDC_FBE_NEXT_UPDATES_GROUP);
	fitLabelCombo(IDC_UPDATE_CHANNEL_LABEL, IDC_UPDATE_CHANNEL, comboLeft, updatesGroup.right - margin);
	grow(IDC_FBE_NEXT_UPDATES_GROUP, { IDC_UPDATE_CHANNEL_LABEL, IDC_UPDATE_CHANNEL });
	RECT client = {}, window = {};
	::GetClientRect(m_hWnd, &client); ::GetWindowRect(m_hWnd, &window);
	const int requiredHeight = bounds(IDC_FBE_NEXT_UPDATES_GROUP).bottom + margin;
	if(requiredHeight > client.bottom)
		::SetWindowPos(m_hWnd, NULL, 0, 0, window.right - window.left,
			requiredHeight + (window.bottom - window.top - client.bottom),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void CSettingsGeneralPage::MakeAppearanceOnly()
{
    // The common page owns these controls and values after the split.
    for(int id : { IDC_OPTIONS_LANGUAGE_LABEL, IDC_LANG, IDC_OPTIONS_GENRE_CATALOG_LABEL, IDC_GENRE_CATALOG,
        IDC_SETTINGS_OTHER_OPEN_GROUP, IDC_RESTORE_POS, IDC_SETTINGS_OTHER_SAVE_GROUP, IDC_SETTINGS_OTHER_ENCODING,
        IDC_DEFAULT_ENC, IDC_KEEP, IDC_FBE_NEXT_SAVING_GROUP, IDC_CREATE_BACKUP_FILE,
        IDC_FBE_NEXT_WINDOW_TITLE_GROUP, IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE, IDC_FBE_NEXT_UPDATES_GROUP,
        IDC_UPDATE_CHANNEL_LABEL, IDC_UPDATE_CHANNEL }) {
        HWND control = GetDlgItem(id); if(control) ::DestroyWindow(control);
    }
    for(int id : { IDC_PALETTE_IMPORT, IDC_PALETTE_EXPORT, IDC_PALETTE_RENAME, IDC_PALETTE_DELETE,
        IDC_PALETTE_ROLE_LABEL, IDC_PALETTE_ROLE, IDC_PALETTE_CHOOSE_COLOR, IDC_PALETTE_RESET_COLOR, IDC_PALETTE_CONTRAST })
        ::ShowWindow(GetDlgItem(id), SW_HIDE);
    SetText(m_hWnd, IDC_OPTIONS_INTERFACE_GROUP, L"fbe.settings.appearance.colour_group", L"Colour scheme");
    SetText(m_hWnd, IDC_APPEARANCE_TOOLBARS_GROUP, L"fbe.settings.appearance.toolbars_group", L"Toolbars");
    SetText(m_hWnd, IDC_APPEARANCE_DESCRIPTION_GROUP, L"fbe.settings.appearance.description_group", L"Description layout");
    const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
    const int margin = UiMetrics::ScaleForDpi(8, dpi), gap = UiMetrics::ScaleForDpi(6, dpi);
    const int titleHeight = UiMetrics::ScaleForDpi(22, dpi);
    int line = UiMetrics::ScaleForDpi(29, dpi);
    RECT group = {}; ::GetWindowRect(GetDlgItem(IDC_OPTIONS_INTERFACE_GROUP), &group);
    ::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&group), 2);
    const int left = group.left + margin;
    const int labelIds[] = {IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_PALETTE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL,
        IDC_SCRIPT_ICON_SIZE_LABEL, IDC_DESCRIPTION_LAYOUT_LABEL};
    int labelWidth = 0, buttonWidth = 0; HDC dc = ::GetDC(m_hWnd);
    HFONT font = reinterpret_cast<HFONT>(::SendMessage(m_hWnd, WM_GETFONT, 0, 0));
    HGDIOBJ oldFont = dc && font ? ::SelectObject(dc, font) : NULL;
    if(dc) {
        TEXTMETRICW metrics = {};
        if(::GetTextMetricsW(dc, &metrics)) line = max(line, static_cast<int>(metrics.tmHeight) + 2 * margin);
        for(int id : labelIds) {
            CString text; GetDlgItemText(id, text); SIZE extent = {};
            if(::GetTextExtentPoint32W(dc, text, text.GetLength(), &extent)) labelWidth = max(labelWidth, static_cast<int>(extent.cx));
        }
        for(int id : {IDC_INTERFACE_PALETTE_COPY, IDC_PALETTE_MANAGE}) {
            CString text; GetDlgItemText(id, text); SIZE extent = {};
            if(::GetTextExtentPoint32W(dc, text, text.GetLength(), &extent)) buttonWidth = max(buttonWidth, static_cast<int>(extent.cx) + 2 * margin);
        }
    }
    if(oldFont) ::SelectObject(dc, oldFont); if(dc) ::ReleaseDC(m_hWnd, dc);
    const int neededWidth = max(labelWidth + gap + UiMetrics::ScaleForDpi(130, dpi), 2 * buttonWidth + gap) + 2 * margin;
    group.right = max(group.right, group.left + neededWidth);
    const int right = group.right - margin, comboLeft = left + labelWidth + gap;
    int y = group.top + titleHeight;
    auto row = [&](int label, int combo) {
        RECT field = {}; ::GetWindowRect(GetDlgItem(combo), &field);
        ::SetWindowPos(GetDlgItem(label), NULL, left, y, max(1, comboLeft - left - gap), line,
            SWP_NOZORDER | SWP_NOACTIVATE);
        ::SetWindowPos(GetDlgItem(combo), NULL, comboLeft, y, max(1, right - comboLeft), field.bottom - field.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        y += line + gap;
    };
    auto place = [&](int id, int x, int top, int width, int height) {
        ::SetWindowPos(GetDlgItem(id), NULL, x, top, max(1, width), max(1, height), SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    };
    row(IDC_INTERFACE_THEME_LABEL, IDC_INTERFACE_THEME);
    row(IDC_INTERFACE_PALETTE_LABEL, IDC_INTERFACE_PALETTE);
    const int middle = left + (right - left) / 2;
    place(IDC_INTERFACE_PALETTE_COPY, left, y, middle - left - gap / 2, line);
    place(IDC_PALETTE_MANAGE, middle + gap / 2, y, right - middle - gap / 2, line);
    y += line + gap;
    ::ShowWindow(GetDlgItem(IDC_APPEARANCE_THEME_PREVIEW), SW_HIDE);
    if(!m_darkThemeSupported) {
        HWND note = GetDlgItem(IDC_INTERFACE_THEME_NOTE);
        CString caption; GetDlgItemText(IDC_INTERFACE_THEME_NOTE, caption);
        RECT measured = { 0, 0, max(1, right - left), 0 };
        HDC noteDc = ::GetDC(note);
        if(noteDc) {
            HFONT noteFont = reinterpret_cast<HFONT>(::SendMessage(note, WM_GETFONT, 0, 0));
            HGDIOBJ previous = noteFont ? ::SelectObject(noteDc, noteFont) : NULL;
            ::DrawTextW(noteDc, caption, caption.GetLength(), &measured, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            if(previous) ::SelectObject(noteDc, previous);
            ::ReleaseDC(note, noteDc);
        }
        const int height = max(line, static_cast<int>(measured.bottom - measured.top) + margin);
        place(IDC_INTERFACE_THEME_NOTE, left, y, right - left, height); y += height + gap;
    }
    const int colourBottom = y + margin;
    place(IDC_OPTIONS_INTERFACE_GROUP, group.left, group.top, group.right - group.left, colourBottom - group.top);
    const int toolbarTop = colourBottom + gap; y = toolbarTop + titleHeight;
    row(IDC_MAIN_TOOLBAR_ICON_SIZE_LABEL, IDC_MAIN_TOOLBAR_ICON_SIZE);
    row(IDC_SCRIPT_ICON_SIZE_LABEL, IDC_SCRIPT_ICON_SIZE);
    const int iconPreviewHeight = UiMetrics::ScaleForDpi(100, dpi);
    place(IDC_APPEARANCE_ICON_PREVIEW, left, y, right - left, iconPreviewHeight);
    y += iconPreviewHeight + margin;
    place(IDC_APPEARANCE_TOOLBARS_GROUP, group.left, toolbarTop, group.right - group.left, y - toolbarTop);
    const int descriptionTop = y + gap; y = descriptionTop + titleHeight;
    row(IDC_DESCRIPTION_LAYOUT_LABEL, IDC_DESCRIPTION_LAYOUT);
    y += margin;
    place(IDC_APPEARANCE_DESCRIPTION_GROUP, group.left, descriptionTop, group.right - group.left, y - descriptionTop);
    // Group boxes are siblings of their fields. Keep them behind the fields
    // and clip their custom background painting around higher siblings.
    for(int id : {IDC_OPTIONS_INTERFACE_GROUP, IDC_APPEARANCE_TOOLBARS_GROUP, IDC_APPEARANCE_DESCRIPTION_GROUP}) {
        HWND frame = GetDlgItem(id);
        ::SetWindowLongPtrW(frame, GWL_STYLE, ::GetWindowLongPtrW(frame, GWL_STYLE) | WS_CLIPSIBLINGS);
        ::SetWindowPos(frame, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    RECT client = {}, window = {}; ::GetClientRect(m_hWnd, &client); ::GetWindowRect(m_hWnd, &window);
    const int requiredHeight = y + margin, requiredWidth = group.right + margin;
    ::SetWindowPos(m_hWnd, NULL, 0, 0, requiredWidth + window.right - window.left - client.right,
        requiredHeight + window.bottom - window.top - client.bottom, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CSettingsGeneralPage::OnIconPreviewChanged(WORD, WORD, HWND, BOOL&)
{
    GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW).Invalidate();
    return 0;
}

LRESULT CSettingsGeneralPage::OnClickedOK(WORD, WORD, HWND, BOOL&)
{
	return ForwardSettingsDialogCommand(m_hWnd, IDOK);
}

LRESULT CSettingsGeneralPage::OnClickedCancel(WORD, WORD, HWND, BOOL&)
{
	return ForwardSettingsDialogCommand(m_hWnd, IDCANCEL);
}

void CSettingsGeneralPage::UpdateThemeComboItemHeight(HWND window)
{
	HDC dc = ::GetDC(window); if(!dc) return;
	HFONT font = reinterpret_cast<HFONT>(::SendMessage(window, WM_GETFONT, 0, 0));
	HGDIOBJ previous = font ? ::SelectObject(dc, font) : NULL;
	TEXTMETRICW metrics = {}; const bool measured = ::GetTextMetricsW(dc, &metrics) != FALSE;
	if(previous) ::SelectObject(dc, previous);
	::ReleaseDC(window, dc);
	if(measured) ::SendMessage(window, CB_SETITEMHEIGHT, 0, metrics.tmHeight + UiMetrics::ScaleForDpi(6, UiMetrics::DpiForWindow(window)));
}

LRESULT CALLBACK CSettingsGeneralPage::ThemeComboSubclassProc(HWND window, UINT message, WPARAM key, LPARAM parameter, UINT_PTR id, DWORD_PTR data)
{
	CSettingsGeneralPage* page = reinterpret_cast<CSettingsGeneralPage*>(data);
	if(message == WM_NCDESTROY) {
		::RemoveWindowSubclass(window, ThemeComboSubclassProc, id);
		return ::DefSubclassProc(window, message, key, parameter);
	}
	if(message == WM_SETFONT) {
		const LRESULT result = ::DefSubclassProc(window, message, key, parameter);
		UpdateThemeComboItemHeight(window);
		if(page && window == page->m_interfacePalette.m_hWnd)
			::SendMessageW(window, CB_SETDROPPEDWIDTH, UiMetrics::ScaleForDpi(450, UiMetrics::DpiForWindow(window)), 0);
		return result;
	}
	if(page && !page->m_darkThemeSupported && message == WM_KEYDOWN && (key == VK_UP || key == VK_DOWN)) {
		const int count = static_cast<int>(::SendMessage(window, CB_GETCOUNT, 0, 0));
		const int current = static_cast<int>(::SendMessage(window, CB_GETCURSEL, 0, 0));
		const int step = key == VK_DOWN ? 1 : -1;
		const bool mode = window == page->m_interfaceTheme.m_hWnd;
		for(int index = current + step; index >= 0 && index < count; index += step) {
			if(mode ? index != INTERFACE_THEME_DARK : page->PaletteItemAvailable(index)) {
				::SendMessage(window, CB_SETCURSEL, index, 0);
				::SendMessage(page->m_hWnd, WM_COMMAND, MAKEWPARAM(::GetDlgCtrlID(window), CBN_SELCHANGE), reinterpret_cast<LPARAM>(window));
				break;
			}
		}
		return 0;
	}
	return ::DefSubclassProc(window, message, key, parameter);
}

LRESULT CALLBACK CSettingsGeneralPage::ThemeListSubclassProc(HWND window, UINT message, WPARAM key, LPARAM parameter, UINT_PTR id, DWORD_PTR data)
{
	CSettingsGeneralPage* page = reinterpret_cast<CSettingsGeneralPage*>(data);
	if(message == WM_NCDESTROY) {
		::RemoveWindowSubclass(window, ThemeListSubclassProc, id);
		return ::DefSubclassProc(window, message, key, parameter);
	}
	if(page && message == WM_MOUSEMOVE) {
		const LRESULT hit = ::SendMessage(window, LB_ITEMFROMPOINT, 0, parameter);
		const int index = HIWORD(hit) ? -1 : LOWORD(hit);
		const bool themeList = id == 1;
		const bool unavailable = !page->m_darkThemeSupported && index >= 0 &&
			(themeList ? index == INTERFACE_THEME_DARK : !page->PaletteItemAvailable(index));
		const CString text = unavailable ?
			FbeLoadRuntimeStringByKey(L"fbe.settings.palette.unavailable", L"This dark palette requires Windows 10 (1809) or newer.") :
			(themeList ? FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.general.interface_theme", L"Automatic follows the Windows app theme.") :
				FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.general.interface_palette", L"Choose interface colours independently of the Windows mode. High Contrast uses system colours."));
		page->m_tooltips.UpdateText(window, text);
	}
	return ::DefSubclassProc(window, message, key, parameter);
}

bool CSettingsGeneralPage::PaletteItemAvailable(int index) const
{
	if(m_darkThemeSupported) return true;
	if(index >= 0 && index < INTERFACE_PALETTE_COUNT)
		return !ThemeManager::NamedPaletteIsDark(PaletteForRow(index));
	const int custom = index - INTERFACE_PALETTE_COUNT;
	if(custom < 0 || custom >= static_cast<int>(m_customPalettes.size())) return false;
	const auto base = m_customPalettes[custom].base;
	return base != ThemeManager::CustomPaletteBase::Dark && base != ThemeManager::CustomPaletteBase::Graphite && base != ThemeManager::CustomPaletteBase::Navy;
}

LRESULT CSettingsGeneralPage::OnDrawThemeItem(UINT, WPARAM, LPARAM parameter, BOOL& handled)
{
	const DRAWITEMSTRUCT* item = reinterpret_cast<const DRAWITEMSTRUCT*>(parameter);
	if(item && (item->CtlID == IDC_APPEARANCE_THEME_PREVIEW || item->CtlID == IDC_APPEARANCE_ICON_PREVIEW)) {
		const int saved = ::SaveDC(item->hDC); const RECT bounds = item->rcItem;
		COLORREF colors[THEME_COLOR_COUNT] = {};
		const int paletteIndex = m_acceptedPaletteIndex;
		const ThemeManager::CustomPalette* custom = paletteIndex >= INTERFACE_PALETTE_COUNT && paletteIndex - INTERFACE_PALETTE_COUNT < static_cast<int>(m_customPalettes.size()) ? &m_customPalettes[paletteIndex - INTERFACE_PALETTE_COUNT] : NULL;
		ThemeManager::PreviewColors(static_cast<InterfaceTheme>(m_acceptedThemeIndex), custom ? INTERFACE_PALETTE_CLASSIC : PaletteForRow(paletteIndex), custom, colors);
		HBRUSH controlBrush = ::CreateSolidBrush(colors[THEME_COLOR_CONTROL]);
		HBRUSH borderBrush = ::CreateSolidBrush(colors[THEME_COLOR_BORDER]);
		if(!controlBrush || !borderBrush) { if(controlBrush) ::DeleteObject(controlBrush); if(borderBrush) ::DeleteObject(borderBrush); if(saved) ::RestoreDC(item->hDC, saved); return TRUE; }
		::FillRect(item->hDC, &bounds, controlBrush);
		::FrameRect(item->hDC, &bounds, borderBrush);
		const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
		const int padding = UiMetrics::ScaleForDpi(5, dpi);
		if(item->CtlID == IDC_APPEARANCE_THEME_PREVIEW) {
			::FillRect(item->hDC, &bounds, controlBrush);
			::FrameRect(item->hDC, &bounds, borderBrush);
			// Render the real owner window, which is behind this owned settings
			// dialog, so the sample follows the current live preview.
			const HWND owner = ::GetAncestor(m_hWnd, GA_ROOTOWNER);
			RECT window = {};
			if(owner && owner != ::GetAncestor(m_hWnd, GA_ROOT) && ::GetWindowRect(owner, &window)) {
				const int width = window.right - window.left, height = window.bottom - window.top;
				if(width > 0 && height > 0 && width <= 8192 && height <= 8192) {
					HDC memory = ::CreateCompatibleDC(item->hDC);
					HBITMAP bitmap = memory ? ::CreateCompatibleBitmap(item->hDC, width, height) : NULL;
					if(bitmap) {
						HGDIOBJ previous = ::SelectObject(memory, bitmap);
						if(::PrintWindow(owner, memory, 0)) {
							::SetStretchBltMode(item->hDC, HALFTONE);
							::StretchBlt(item->hDC, bounds.left + padding, bounds.top + padding,
								bounds.right - bounds.left - 2 * padding, bounds.bottom - bounds.top - 2 * padding,
								memory, 0, 0, width, (std::max)(1, height / 2), SRCCOPY);
						}
						::SelectObject(memory, previous); ::DeleteObject(bitmap);
					}
					if(memory) ::DeleteDC(memory);
				}
			}
		} else {
			const int mainSelection = m_mainToolbarIconSize.GetCurSel();
			const int scriptSelection = m_scriptIconSize.GetCurSel();
			const int mainSize = mainSelection == CB_ERR ? 16 : static_cast<int>(m_mainToolbarIconSize.GetItemData(mainSelection));
			const int scriptSize = scriptSelection == CB_ERR ? 16 : static_cast<int>(m_scriptIconSize.GetItemData(scriptSelection));
			const int scriptPixels = UiMetrics::ScaleForDpi(scriptSize, dpi);
			const int menuHeight = UiMetrics::ScaleForDpi(22, dpi);
			const int middle = bounds.top + menuHeight + (bounds.bottom - bounds.top - menuHeight) / 2;
			RECT menu = {bounds.left + 1, bounds.top + 1, bounds.right - 1, bounds.top + menuHeight};
			RECT upper = {bounds.left + 1, menu.bottom, bounds.right - 1, middle};
			RECT lower = {bounds.left + 1, middle, bounds.right - 1, bounds.bottom - 1};
			::FillRect(item->hDC, &menu, controlBrush);
			::FillRect(item->hDC, &upper, controlBrush);
			::FillRect(item->hDC, &lower, controlBrush);
			// Match the separate rebar rows and their small drag handles.  Derive
			// both edges from the preview palette, not the currently applied theme.
			auto mix = [](COLORREF a, COLORREF b, int weight) -> COLORREF {
				return RGB((GetRValue(a) * (100 - weight) + GetRValue(b) * weight) / 100,
					(GetGValue(a) * (100 - weight) + GetGValue(b) * weight) / 100,
					(GetBValue(a) * (100 - weight) + GetBValue(b) * weight) / 100);
			};
			const int edge = (std::max)(1, UiMetrics::ScaleForDpi(1, dpi));
			// Rebar band edges use BORDER in the main window. Some palettes set
			// SEPARATOR equal to the panel background, making those edges invisible.
			HBRUSH separatorBrush = ::CreateSolidBrush(colors[THEME_COLOR_BORDER]);
			HBRUSH highlightBrush = ::CreateSolidBrush(mix(colors[THEME_COLOR_CONTROL], colors[THEME_COLOR_TEXT], 18));
			HBRUSH pressedBrush = ::CreateSolidBrush(colors[THEME_COLOR_PRESSED]);
			if(separatorBrush && highlightBrush && pressedBrush) {
				for(int y : {menu.bottom - edge, upper.bottom - edge}) {
					RECT line = {bounds.left + 1, y, bounds.right - 1, y + edge};
					::FillRect(item->hDC, &line, separatorBrush);
				}
				for(const RECT& row : {menu, upper, lower}) {
					for(int y = row.top + padding; y + edge < row.bottom - padding; y += 3 * edge) {
						RECT dot = {bounds.left + edge * 2, y, bounds.left + edge * 3, y + edge};
						::FillRect(item->hDC, &dot, separatorBrush);
					}
				}
			}
			HMENU resourceMenu = NULL;
			const HWND owner = ::GetAncestor(m_hWnd, GA_ROOTOWNER);
			if(owner) ::EnumChildWindows(owner, FindPreviewMenuBar, reinterpret_cast<LPARAM>(&resourceMenu));
			const bool ownsMenu = resourceMenu == NULL;
			if(ownsMenu) resourceMenu = ::LoadMenuW(_Module.GetResourceInstance(), MAKEINTRESOURCEW(IDR_MAINFRAME));
			if(resourceMenu) {
				::SetBkMode(item->hDC, TRANSPARENT); ::SetTextColor(item->hDC, colors[THEME_COLOR_TEXT]);
				HFONT font = reinterpret_cast<HFONT>(::SendMessage(m_hWnd, WM_GETFONT, 0, 0));
				HGDIOBJ oldFont = font ? ::SelectObject(item->hDC, font) : NULL;
				int x = bounds.left + padding;
				const wchar_t* keys[] = { L"fbe.menu.idr_mainframe.popup.file", L"fbe.menu.idr_mainframe.popup.edit",
					L"fbe.menu.idr_mainframe.popup.view", L"fbe.menu.idr_mainframe.popup.insert", L"fbe.menu.idr_mainframe.popup.style",
					L"fbe.menu.idr_mainframe.popup.tools", L"fbe.menu.idr_mainframe.popup.scripts", L"fbe.menu.idr_mainframe.popup.help" };
				for(int index = 0; index < ::GetMenuItemCount(resourceMenu) && x < bounds.right - padding; ++index) {
					wchar_t fallback[128] = {}; ::GetMenuStringW(resourceMenu, index, fallback, _countof(fallback), MF_BYPOSITION);
					const CString caption = index < static_cast<int>(_countof(keys)) ? FbeLoadRuntimeStringByKey(keys[index], fallback) : CString(fallback);
					RECT text = {x, menu.top, bounds.right - padding, menu.bottom};
					RECT measure = text; ::DrawTextW(item->hDC, caption, caption.GetLength(), &measure, DT_SINGLELINE | DT_CALCRECT);
					::DrawTextW(item->hDC, caption, caption.GetLength(), &text, DT_SINGLELINE | DT_VCENTER);
					x += measure.right - measure.left + padding * 2;
				}
				if(oldFont) ::SelectObject(item->hDC, oldFont);
				if(ownsMenu) ::DestroyMenu(resourceMenu);
			}
			CImageList images;
			if(ToolbarFactory::CreateCommandToolbarImages(images, IDR_MAINFRAME, dpi, mainSize)) {
				const int mainPixels = UiMetrics::ScaleForDpi(mainSize, dpi);
				for(int index = 0; index < (std::min)(6, images.GetImageCount()); ++index) {
					const int x = bounds.left + padding + index * (mainPixels + padding);
					const int y = upper.top + (upper.bottom - upper.top - mainPixels) / 2;
					if(index == 4 && separatorBrush && highlightBrush && pressedBrush) {
						RECT button = {x - edge, y - edge, x + mainPixels + edge, y + mainPixels + edge};
						::FillRect(item->hDC, &button, pressedBrush);
						::FrameRect(item->hDC, &button, borderBrush);
						RECT bottom = {button.left + edge, button.bottom - edge, button.right, button.bottom};
						RECT right = {button.right - edge, button.top + edge, button.right, button.bottom};
						::FillRect(item->hDC, &bottom, highlightBrush); ::FillRect(item->hDC, &right, highlightBrush);
					}
					images.Draw(item->hDC, index, CPoint(x, y), ILD_TRANSPARENT);
					if(index == 2 && separatorBrush) {
						RECT divider = {x + mainPixels + padding / 2, upper.top + edge * 2,
							x + mainPixels + padding / 2 + edge, upper.bottom - edge * 2};
						::FillRect(item->hDC, &divider, separatorBrush);
					}
				}
			}
			wchar_t path[MAX_PATH] = {};
			if(::GetModuleFileNameW(NULL, path, _countof(path))) {
				CString directory(path); const int slash = directory.ReverseFind(L'\\');
				if(slash >= 0) {
					directory = directory.Left(slash + 1) + L"Scripts\\";
					const wchar_t* names[] = {L"01_Регистр.ico", L"06_Чистка.ico"};
					for(int index = 0; index < static_cast<int>(_countof(names)); ++index) {
						HICON icon = static_cast<HICON>(::LoadImageW(NULL, directory + names[index], IMAGE_ICON, scriptPixels, scriptPixels, LR_LOADFROMFILE));
						if(icon) { ::DrawIconEx(item->hDC, bounds.left + padding + index * (scriptPixels + padding), lower.top + (lower.bottom - lower.top - scriptPixels) / 2, icon, scriptPixels, scriptPixels, 0, NULL, DI_NORMAL); ::DestroyIcon(icon); }
					}
				}
			}
			if(separatorBrush) ::DeleteObject(separatorBrush);
			if(highlightBrush) ::DeleteObject(highlightBrush);
			if(pressedBrush) ::DeleteObject(pressedBrush);
		}
		if(saved) ::RestoreDC(item->hDC, saved); ::DeleteObject(controlBrush); ::DeleteObject(borderBrush); return TRUE;
	}
	if(!item || (item->CtlID != IDC_INTERFACE_THEME && item->CtlID != IDC_INTERFACE_PALETTE)) { handled = FALSE; return 0; }
	const bool selected = (item->itemState & ODS_SELECTED) != 0;
	const int index = item->itemID == static_cast<UINT>(-1) ? -1 : static_cast<int>(item->itemID);
	const bool available = index < 0 || (item->CtlID == IDC_INTERFACE_THEME ? m_darkThemeSupported || index != INTERFACE_THEME_DARK : PaletteItemAvailable(index));
	const COLORREF background = selected ? ThemeManager::SelectionBackgroundColor() :
		((item->itemState & ODS_COMBOBOXEDIT) && ThemeManager::UsesPaletteSurface()) ? ThemeManager::WindowColor() : ThemeManager::ControlColor();
	const COLORREF foreground = !available ? ThemeManager::DisabledTextColor() : selected ? ThemeManager::SelectionTextColor() : ThemeManager::TextColor();
	::SetBkColor(item->hDC, background); ::SetTextColor(item->hDC, foreground);
	::ExtTextOutW(item->hDC, 0, 0, ETO_OPAQUE, &item->rcItem, NULL, 0, NULL);
	if(index >= 0) {
		CComboBox combo(item->hwndItem); CString text; combo.GetLBText(index, text);
		RECT rect = item->rcItem; rect.left += UiMetrics::ScaleForDpi(4, UiMetrics::DpiForWindow(m_hWnd)); rect.right -= 2;
		if(item->CtlID == IDC_INTERFACE_PALETTE && !(item->itemState & ODS_COMBOBOXEDIT) && (index == 1 || index == 5 || index == 14 || index == INTERFACE_PALETTE_COUNT)) {
			const wchar_t* key = index == 1 ? L"fbe.settings.palette.group.light" : index == 5 ? L"fbe.settings.palette.group.dark" : index == 14 ? L"fbe.settings.palette.group.contrast" : L"fbe.settings.palette.group.custom";
			const wchar_t* fallback = index == 1 ? L"Light themes" : index == 5 ? L"Dark themes" : index == 14 ? L"High contrast themes" : L"Custom themes";
			const CString label = FbeLoadRuntimeStringByKey(key, fallback);
			RECT group = rect; group.left = rect.left + (rect.right - rect.left) / 2;
			::SetTextColor(item->hDC, !available ? ThemeManager::DisabledTextColor() : selected ? ThemeManager::SelectionTextColor() : ThemeManager::SecondaryTextColor());
			::DrawTextW(item->hDC, label, label.GetLength(), &group, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS | DT_NOPREFIX);
			rect.right = group.left - UiMetrics::ScaleForDpi(4, UiMetrics::DpiForWindow(m_hWnd));
			RECT line = item->rcItem; line.bottom = line.top + 1;
			::FillRect(item->hDC, &line, ThemeManager::Brush(THEME_COLOR_BORDER));
			::SetTextColor(item->hDC, foreground);
		}
		::DrawTextW(item->hDC, text, text.GetLength(), &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
	}
	if(item->itemState & ODS_FOCUS) ::DrawFocusRect(item->hDC, &item->rcItem);
	return TRUE;
}

void CSettingsGeneralPage::ConfigureInterfaceTheme(bool darkThemeSupported)
{
	m_darkThemeSupported = darkThemeSupported;
	m_originalTheme = _Settings.GetInterfaceTheme();
	m_preserveUnavailableDark = !darkThemeSupported && m_originalTheme == INTERFACE_THEME_DARK;
	m_interfaceTheme.ResetContent();
	m_interfaceTheme.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.automatic", L"Automatic \x2014 follow Windows"));
	m_interfaceTheme.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.light", L"Light"));
	m_interfaceTheme.AddString(FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.dark", L"Dark"));
	m_acceptedThemeIndex = m_preserveUnavailableDark ? INTERFACE_THEME_LIGHT : static_cast<int>(m_originalTheme);
	m_interfaceTheme.SetCurSel(m_acceptedThemeIndex);
	UpdateThemeNotice();
}

void CSettingsGeneralPage::UpdateThemeNotice()
{
	const bool supported = m_darkThemeSupported;
	CString notice;
	if(!supported) {
		notice = FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.unavailable", L"The dark interface theme requires Windows 10 (1809) or newer. This system uses the standard light appearance.");
		if(m_preserveUnavailableDark) notice += L" " + FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.preserved", L"Your saved dark preference is preserved until you choose another theme.");
	}
	::SetDlgItemText(m_hWnd, IDC_INTERFACE_THEME_NOTE, notice);
	::ShowWindow(GetDlgItem(IDC_INTERFACE_THEME_NOTE), supported ? SW_HIDE : SW_SHOW);
	CString themeTip = FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.general.interface_theme", L"Automatic follows the Windows app theme.");
	CString paletteTip = FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.general.interface_palette", L"Choose interface colours independently of the Windows mode. High Contrast uses system colours.");
	if(!supported) {
		themeTip += L" " + FbeLoadRuntimeStringByKey(L"fbe.settings.interface_theme.unavailable", L"The dark interface theme requires Windows 10 (1809) or newer. This system uses the standard light appearance.");
		paletteTip += L" " + FbeLoadRuntimeStringByKey(L"fbe.settings.palette.unavailable", L"This dark palette requires Windows 10 (1809) or newer.");
		if(!m_unavailableListTooltips) {
			for(HWND combo : {m_interfaceTheme.m_hWnd, m_interfacePalette.m_hWnd}) {
				COMBOBOXINFO info = {}; info.cbSize = sizeof(info);
				if(::GetComboBoxInfo(combo, &info) && info.hwndList) {
					const bool themeList = combo == m_interfaceTheme.m_hWnd;
					m_tooltips.Add(info.hwndList, themeList ? L"fbe.settings.tooltip.general.interface_theme" : L"fbe.settings.tooltip.general.interface_palette",
						themeList ? L"Automatic follows the Windows app theme." : L"Choose interface colours independently of the Windows mode. High Contrast uses system colours.");
					::SetWindowSubclass(info.hwndList, ThemeListSubclassProc, themeList ? 1 : 2, reinterpret_cast<DWORD_PTR>(this));
				}
			}
			m_unavailableListTooltips = true;
		}
	}
	for(HWND control : {m_interfaceTheme.m_hWnd, GetDlgItem(IDC_INTERFACE_THEME_LABEL).m_hWnd}) m_tooltips.UpdateText(control, themeTip);
	for(HWND control : {m_interfacePalette.m_hWnd, GetDlgItem(IDC_INTERFACE_PALETTE_LABEL).m_hWnd}) m_tooltips.UpdateText(control, paletteTip);
	m_interfaceTheme.Invalidate(); m_interfacePalette.Invalidate();
}

LRESULT CSettingsGeneralPage::OnInterfaceThemeChanged(WORD notification, WORD, HWND, BOOL&)
{
	if(m_closing) return 0;
	const int selection = m_interfaceTheme.GetCurSel();
	if(!m_darkThemeSupported && selection == INTERFACE_THEME_DARK) { m_interfaceTheme.SetCurSel(m_acceptedThemeIndex); return 0; }
	if(selection >= INTERFACE_THEME_AUTOMATIC && selection <= INTERFACE_THEME_DARK)
	{
		const bool keepDropdown = !m_cancellingThemeDropdown && notification != CBN_SELENDOK && m_interfaceTheme.GetDroppedState() != FALSE;
		const bool hadFocus = ::GetFocus() == m_interfaceTheme.m_hWnd;
		m_acceptedThemeIndex = selection;
		if(!m_cancellingThemeDropdown && (notification == CBN_SELENDOK || !m_interfaceTheme.GetDroppedState())) {
			m_preserveUnavailableDark = false;
			UpdateThemeNotice();
		}
		GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW).Invalidate();
		if(keepDropdown && !m_interfaceTheme.GetDroppedState()) {
			if(hadFocus) m_interfaceTheme.SetFocus();
			m_reopeningThemeDropdown = true;
			m_interfaceTheme.ShowDropDown(TRUE);
			m_reopeningThemeDropdown = false;
		}
	}
	return 0;
}

LRESULT CSettingsGeneralPage::OnThemeDropdown(WORD, WORD, HWND, BOOL&)
{
	if(!m_reopeningThemeDropdown) {
		m_themeDropdownStart = m_acceptedThemeIndex;
		m_themePreservedBeforeDropdown = m_preserveUnavailableDark;
	}
	return 0;
}

LRESULT CSettingsGeneralPage::OnThemeDropdownCancel(WORD, WORD, HWND, BOOL&)
{
	if(m_closing) return 0;
	if(m_themeDropdownStart < INTERFACE_THEME_AUTOMATIC || m_themeDropdownStart > INTERFACE_THEME_DARK) return 0;
	m_interfaceTheme.SetCurSel(m_themeDropdownStart);
	m_preserveUnavailableDark = m_themePreservedBeforeDropdown;
	BOOL handled = FALSE;
	m_cancellingThemeDropdown = true;
	OnInterfaceThemeChanged(CBN_SELCHANGE, IDC_INTERFACE_THEME, m_interfaceTheme, handled);
	m_cancellingThemeDropdown = false;
	m_themeDropdownStart = -1;
	UpdateThemeNotice();
	return 0;
}

LRESULT CSettingsGeneralPage::OnPaletteDropdown(WORD, WORD, HWND, BOOL&)
{
	if(!m_reopeningPaletteDropdown) {
		m_paletteDropdownStart = m_acceptedPaletteIndex;
		m_paletteChangedBeforeDropdown = m_paletteChanged;
	}
	return 0;
}

LRESULT CSettingsGeneralPage::OnPaletteDropdownCancel(WORD, WORD, HWND, BOOL&)
{
	if(m_closing || m_applyingPalettePreview || m_reopeningPaletteDropdown) return 0;
	if(m_paletteDropdownStart < 0 || m_paletteDropdownStart >= m_interfacePalette.GetCount()) return 0;
	m_interfacePalette.SetCurSel(m_paletteDropdownStart);
	BOOL handled = FALSE;
	m_cancellingPaletteDropdown = true;
	OnInterfacePaletteChanged(CBN_SELCHANGE, IDC_INTERFACE_PALETTE, m_interfacePalette, handled);
	m_cancellingPaletteDropdown = false;
	m_paletteChanged = m_paletteChangedBeforeDropdown;
	m_paletteDropdownStart = -1;
	return 0;
}

LRESULT CSettingsGeneralPage::OnPaletteDropdownAccept(WORD, WORD, HWND, BOOL&)
{
	if(!m_applyingPalettePreview && !m_reopeningPaletteDropdown) m_paletteDropdownStart = -1;
	return 0;
}

LRESULT CSettingsGeneralPage::OnInterfacePaletteChanged(WORD, WORD, HWND, BOOL&)
{
	if(m_closing) return 0;
	const int selection = m_interfacePalette.GetCurSel();
	if(selection < 0 || selection >= INTERFACE_PALETTE_COUNT + static_cast<int>(m_customPalettes.size())) return 0;
	const bool keepDropdown = !m_cancellingPaletteDropdown && m_interfacePalette.GetDroppedState() != FALSE;
	const bool hadFocus = ::GetFocus() == m_interfacePalette.m_hWnd;
	bool applied = false;
	if(selection < INTERFACE_PALETTE_COUNT) {
		if(m_darkThemeSupported || !ThemeManager::NamedPaletteIsDark(PaletteForRow(selection)))
			applied = true;
	} else {
		ThemeManager::CustomPalette palette; std::wstring error;
		const std::wstring& id = m_customPalettes[selection - INTERFACE_PALETTE_COUNT].id;
		bool loaded = false;
		for(const auto& pending : m_pendingPalettes) if(pending.id == id) { palette = pending; loaded = true; break; }
		if(!loaded) loaded = ThemeManager::LoadCustomPaletteFromProfile(id, palette, error);
		if(!loaded) {
			m_interfacePalette.SetCurSel(m_acceptedPaletteIndex);
			CString message = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.load_failed", L"Could not load the selected interface palette.");
			if(!error.empty()) { message += L"\n\n"; message += error.c_str(); }
			MessageBox(message, FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONWARNING);
			return 0;
		}
		const bool dark = palette.base == ThemeManager::CustomPaletteBase::Dark || palette.base == ThemeManager::CustomPaletteBase::Graphite || palette.base == ThemeManager::CustomPaletteBase::Navy;
		if(m_darkThemeSupported || !dark) { m_customPalettes[selection - INTERFACE_PALETTE_COUNT] = palette; applied = true; }
	}
	if(!applied) {
		m_interfacePalette.SetCurSel(m_acceptedPaletteIndex);
		return 0;
	}
	m_acceptedPaletteIndex = selection;
	UpdatePaletteColorControls();
	m_paletteChanged = true;
	m_applyingPalettePreview = true;
	GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW).Invalidate();
	// Retheming native controls can close an open combo popup. Keep keyboard
	// navigation active without replacing the selection captured on opening.
	if(keepDropdown && !m_interfacePalette.GetDroppedState()) {
		if(hadFocus) m_interfacePalette.SetFocus();
		m_reopeningPaletteDropdown = true;
		m_interfacePalette.ShowDropDown(TRUE);
		m_reopeningPaletteDropdown = false;
	}
	m_applyingPalettePreview = false;
	return 0;
}

LRESULT CSettingsGeneralPage::OnPaletteThemeChanged(UINT, WPARAM, LPARAM, BOOL& handled)
{
	if(GetDlgItem(IDC_APPEARANCE_THEME_PREVIEW).IsWindow()) GetDlgItem(IDC_APPEARANCE_THEME_PREVIEW).Invalidate();
	if(GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW).IsWindow()) GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW).Invalidate();
	if(GetDlgItem(IDC_PALETTE_CONTRAST).IsWindow()) UpdatePaletteContrast();
	handled = FALSE;
	return 0;
}

void CSettingsGeneralPage::UpdatePaletteColorControls()
{
	const bool editable = m_acceptedPaletteIndex >= INTERFACE_PALETTE_COUNT && m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT < static_cast<int>(m_customPalettes.size());
	for(int id : {IDC_PALETTE_ROLE_LABEL, IDC_PALETTE_ROLE, IDC_PALETTE_CHOOSE_COLOR, IDC_PALETTE_RESET_COLOR, IDC_PALETTE_EXPORT, IDC_PALETTE_RENAME, IDC_PALETTE_DELETE}) GetDlgItem(id).EnableWindow(editable);
	UpdatePaletteContrast();
}

void CSettingsGeneralPage::UpdatePaletteContrast()
{
	const int customIndex = m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT;
	if(customIndex < 0 || customIndex >= static_cast<int>(m_customPalettes.size())) { SetDlgItemText(IDC_PALETTE_CONTRAST, L""); return; }
	COLORREF colors[THEME_COLOR_COUNT] = {};
	ThemeManager::PreviewColors(static_cast<InterfaceTheme>(m_acceptedThemeIndex), INTERFACE_PALETTE_CLASSIC, &m_customPalettes[customIndex], colors);
	auto luminance = [](COLORREF color) {
		auto channel = [](BYTE value) { const double v = value / 255.0; return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
		return 0.2126 * channel(GetRValue(color)) + 0.7152 * channel(GetGValue(color)) + 0.0722 * channel(GetBValue(color));
	};
	auto enough = [&](ThemeColorRole foreground, ThemeColorRole background, double threshold) {
		const double a = luminance(colors[foreground]), b = luminance(colors[background]);
		return ((std::max)(a, b) + 0.05) / ((std::min)(a, b) + 0.05) >= threshold;
	};
	const bool readable = enough(THEME_COLOR_TEXT, THEME_COLOR_WINDOW, 4.5) && enough(THEME_COLOR_TEXT, THEME_COLOR_CONTROL, 4.5) &&
		enough(THEME_COLOR_SELECTION_TEXT, THEME_COLOR_SELECTION_BACKGROUND, 4.5) && enough(THEME_COLOR_TEXT, THEME_COLOR_HOVER, 4.5) &&
		enough(THEME_COLOR_TEXT, THEME_COLOR_PRESSED, 4.5) && enough(THEME_COLOR_DISABLED_TEXT, THEME_COLOR_CONTROL, 3.0);
	SetDlgItemText(IDC_PALETTE_CONTRAST, readable ?
		FbeLoadRuntimeStringByKey(L"fbe.settings.palette.contrast_ok", L"Text contrast check passed.") :
		FbeLoadRuntimeStringByKey(L"fbe.settings.palette.contrast_low", L"Warning: some text may be difficult to read against its background."));
}

LRESULT CSettingsGeneralPage::OnChoosePaletteColor(WORD, WORD, HWND, BOOL&)
{
	const int role = m_paletteRole.GetCurSel();
	const int index = m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT;
	if(role < 0 || role >= THEME_COLOR_COUNT || index < 0 || index >= static_cast<int>(m_customPalettes.size())) return 0;
	COLORREF colors[THEME_COLOR_COUNT] = {};
	ThemeManager::PreviewColors(static_cast<InterfaceTheme>(m_acceptedThemeIndex), INTERFACE_PALETTE_CLASSIC, &m_customPalettes[index], colors);
	static COLORREF customColors[16] = {};
	CHOOSECOLORW chooser = {};
	chooser.lStructSize = sizeof(chooser); chooser.hwndOwner = m_paletteManager ? m_paletteManager : m_hWnd;
	chooser.rgbResult = colors[role];
	chooser.lpCustColors = customColors; chooser.Flags = CC_RGBINIT | CC_FULLOPEN;
	if(::ChooseColorW(&chooser)) StagePaletteColor(static_cast<ThemeColorRole>(role), chooser.rgbResult, true);
	return 0;
}

LRESULT CSettingsGeneralPage::OnResetPaletteColor(WORD, WORD, HWND, BOOL&)
{
	const int role = m_paletteRole.GetCurSel();
	if(role >= 0 && role < THEME_COLOR_COUNT) StagePaletteColor(static_cast<ThemeColorRole>(role), 0, false);
	return 0;
}

bool CSettingsGeneralPage::StagePaletteCopy(const CString& name)
{
	const int selection = m_interfacePalette.GetCurSel();
	if(selection < 0 || selection >= INTERFACE_PALETTE_COUNT + static_cast<int>(m_customPalettes.size())) return false;
	ThemeManager::CustomPalette copy;
	if(selection >= INTERFACE_PALETTE_COUNT) { copy = m_customPalettes[selection - INTERFACE_PALETTE_COUNT]; }
	else if(ThemeManager::CopyNamedPaletteColors(PaletteForRow(selection), copy)) {}
	else copy.base = PaletteForRow(selection) == INTERFACE_PALETTE_GRAPHITE ? ThemeManager::CustomPaletteBase::Graphite :
		PaletteForRow(selection) == INTERFACE_PALETTE_NAVY ? ThemeManager::CustomPaletteBase::Navy :
		PaletteForRow(selection) == INTERFACE_PALETTE_SEPIA ? ThemeManager::CustomPaletteBase::Sepia :
		ThemeManager::IsDark() ? ThemeManager::CustomPaletteBase::Dark : ThemeManager::CustomPaletteBase::Light;
	GUID guid = {};
	wchar_t text[40] = {};
	if(FAILED(::CoCreateGuid(&guid)) || !::StringFromGUID2(guid, text, _countof(text))) return false;
	CString id(text); id.Trim(L"{}"); id.MakeLower(); id = L"copy-" + id;
	copy.id = static_cast<LPCWSTR>(id); copy.name = static_cast<LPCWSTR>(name);
	ThemeManager::CustomPalette validated; std::wstring error;
	if(!ThemeManager::ParseCustomPalette(ThemeManager::SerializeCustomPalette(copy), validated, error) || validated.name != copy.name) return false;
	const CString label(name);
	const int index = m_interfacePalette.AddString(label);
	if(index < 0) return false;
	m_customPalettes.push_back(copy); m_pendingPalettes.push_back(copy); m_newPaletteIds.push_back(copy.id);
	m_interfacePalette.SetCurSel(index);
	BOOL handled = FALSE;
	OnInterfacePaletteChanged(CBN_SELCHANGE, IDC_INTERFACE_PALETTE, m_interfacePalette, handled);
	return m_acceptedPaletteIndex == index;
}

bool CSettingsGeneralPage::StagePaletteColor(ThemeColorRole role, COLORREF color, bool custom)
{
	const int index = m_interfacePalette.GetCurSel() - INTERFACE_PALETTE_COUNT;
	if(index < 0 || index >= static_cast<int>(m_customPalettes.size()) || role < 0 || role >= THEME_COLOR_COUNT) return false;
	ThemeManager::CustomPalette draft = m_customPalettes[index];
	draft.present[role] = custom;
	draft.colors[role] = color;
	bool found = false;
	for(auto& pending : m_pendingPalettes) if(pending.id == draft.id) { pending = draft; found = true; break; }
	if(!found) m_pendingPalettes.push_back(draft);
	m_customPalettes[index] = draft;
	m_paletteChanged = true;
	UpdatePaletteContrast();
	GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW).Invalidate();
	return true;
}

bool CSettingsGeneralPage::StagePaletteImport(const CString& path, CString& error)
{
	ThemeManager::CustomPalette imported; std::wstring detail;
	if(!ThemeManager::LoadCustomPaletteFromFile(path, imported, detail)) { error = detail.c_str(); return false; }
	for(const auto& palette : m_customPalettes) if(palette.id == imported.id) {
		error = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.id_exists", L"A palette with this ID already exists."); return false;
	}
	if(!ThemeManager::CustomPaletteIdAvailable(imported.id)) {
		error = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.id_exists", L"A palette with this ID already exists or its destination is unavailable."); return false;
	}
	const bool dark = imported.base == ThemeManager::CustomPaletteBase::Dark || imported.base == ThemeManager::CustomPaletteBase::Graphite || imported.base == ThemeManager::CustomPaletteBase::Navy;
	if(dark && !m_darkThemeSupported) {
		error = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.unavailable", L"This dark palette requires Windows 10 (1809) or newer."); return false;
	}
	const CString label(imported.name.c_str());
	const int index = m_interfacePalette.AddString(label);
	if(index < 0) { error = L"Cannot add palette to the list."; return false; }
	m_customPalettes.push_back(imported); m_pendingPalettes.push_back(imported); m_newPaletteIds.push_back(imported.id);
	m_interfacePalette.SetCurSel(index); m_acceptedPaletteIndex = index; m_paletteChanged = true;
	UpdatePaletteColorControls(); GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW).Invalidate();
	return true;
}

LRESULT CSettingsGeneralPage::OnImportPalette(WORD, WORD, HWND, BOOL&)
{
	const CString filter = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.json_filter", L"Interface palettes (*.json)");
	const COMDLG_FILTERSPEC filters[] = {{filter, L"*.json"}};
	ModernFileDialog::Request request; request.fileMustExist = true; request.pathMustExist = true; request.filters = filters; request.filterCount = _countof(filters);
	const auto result = ModernFileDialog::Show(m_paletteManager ? m_paletteManager : m_hWnd, request);
	if(result.outcome == ModernFileDialog::Outcome::Failed) {
		CString detail; detail.Format(L"0x%08X", static_cast<unsigned>(result.error));
		MessageBox(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.load_failed", L"Could not load the selected interface palette.") + L"\n\n" + detail,
			FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONERROR); return 0;
	}
	if(result.outcome != ModernFileDialog::Outcome::Accepted || result.paths.empty()) return 0;
	CString error;
	if(!StagePaletteImport(result.paths.front().c_str(), error)) {
		CString message = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.load_failed", L"Could not load the selected interface palette.");
		if(!error.IsEmpty()) message += L"\n\n" + error;
		MessageBox(message, FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONWARNING);
	}
	return 0;
}

LRESULT CSettingsGeneralPage::OnExportPalette(WORD, WORD, HWND, BOOL&)
{
	const int index = m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT;
	if(index < 0 || index >= static_cast<int>(m_customPalettes.size())) return 0;
	const ThemeManager::CustomPalette palette = m_customPalettes[index];
	const CString filter = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.json_filter", L"Interface palettes (*.json)");
	const COMDLG_FILTERSPEC filters[] = {{filter, L"*.json"}};
	ModernFileDialog::Request request; request.save = true; request.pathMustExist = true; request.overwritePrompt = true;
	request.defaultExtension = L"json"; request.initialFileName = palette.id + L".json"; request.filters = filters; request.filterCount = _countof(filters);
	const auto result = ModernFileDialog::Show(m_paletteManager ? m_paletteManager : m_hWnd, request);
	if(result.outcome == ModernFileDialog::Outcome::Failed) {
		CString detail; detail.Format(L"0x%08X", static_cast<unsigned>(result.error));
		MessageBox(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.export_failed", L"Could not export the interface palette.") + L"\n\n" + detail,
			FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONERROR); return 0;
	}
	if(result.outcome != ModernFileDialog::Outcome::Accepted || result.paths.empty()) return 0;
	std::wstring error;
	if(!ThemeManager::ExportCustomPaletteToFile(result.paths.front().c_str(), palette, error)) {
		CString message = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.export_failed", L"Could not export the interface palette.");
		if(!error.empty()) { message += L"\n\n"; message += error.c_str(); }
		MessageBox(message, FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONERROR);
	}
	return 0;
}

bool CSettingsGeneralPage::StagePaletteRename(const CString& name)
{
	const int selection = m_interfacePalette.GetCurSel();
	const int index = selection - INTERFACE_PALETTE_COUNT;
	if(index < 0 || index >= static_cast<int>(m_customPalettes.size())) return false;
	const ThemeManager::CustomPalette previous = m_customPalettes[index];
	ThemeManager::CustomPalette draft = previous; draft.name = static_cast<LPCWSTR>(name);
	ThemeManager::CustomPalette validated; std::wstring error;
	if(!ThemeManager::ParseCustomPalette(ThemeManager::SerializeCustomPalette(draft), validated, error) || validated.name != draft.name) return false;
	if(previous.name == draft.name) return true;
	const CString oldLabel(previous.name.c_str());
	const CString label(name);
	m_interfacePalette.DeleteString(selection);
	const int inserted = m_interfacePalette.InsertString(selection, label);
	if(inserted != selection) {
		if(inserted >= 0) m_interfacePalette.DeleteString(inserted);
		m_interfacePalette.InsertString(selection, oldLabel); m_interfacePalette.SetCurSel(selection);
		return false;
	}
	m_interfacePalette.SetCurSel(selection);
	m_customPalettes[index] = draft;
	bool found = false;
	for(auto& pending : m_pendingPalettes) if(pending.id == draft.id) { pending = draft; found = true; break; }
	if(!found) m_pendingPalettes.push_back(draft);
	m_paletteChanged = true;
	return true;
}

bool CSettingsGeneralPage::StagePaletteDelete()
{
	const int selection = m_interfacePalette.GetCurSel();
	const int index = selection - INTERFACE_PALETTE_COUNT;
	if(index < 0 || index >= static_cast<int>(m_customPalettes.size())) return false;
	const std::wstring id = m_customPalettes[index].id;
	const auto created = std::find(m_newPaletteIds.begin(), m_newPaletteIds.end(), id);
	if(created == m_newPaletteIds.end()) m_deletedPaletteIds.push_back(id);
	else m_newPaletteIds.erase(created);
	m_pendingPalettes.erase(std::remove_if(m_pendingPalettes.begin(), m_pendingPalettes.end(), [&](const ThemeManager::CustomPalette& value) { return value.id == id; }), m_pendingPalettes.end());
	m_customPalettes.erase(m_customPalettes.begin() + index);
	m_interfacePalette.DeleteString(selection);
	m_interfacePalette.SetCurSel(INTERFACE_PALETTE_CLASSIC);
	BOOL handled = FALSE;
	OnInterfacePaletteChanged(CBN_SELCHANGE, IDC_INTERFACE_PALETTE, m_interfacePalette, handled);
	return true;
}

LRESULT CSettingsGeneralPage::OnRenamePalette(WORD, WORD, HWND, BOOL&)
{
	const int index = m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT;
	if(index < 0 || index >= static_cast<int>(m_customPalettes.size())) return 0;
	const ThemeManager::CustomPalette palette = m_customPalettes[index];
	CString name(palette.name.c_str());
	if(AU::InputBoxOwned(name, FbeLoadRuntimeStringByKey(L"fbe.settings.palette.rename", L"Rename..."),
		FbeLoadRuntimeStringByKey(L"fbe.settings.palette.rename_name", L"New name of the interface palette:"), m_paletteManager ? m_paletteManager : m_hWnd) != IDYES) return 0;
	name.Trim();
	if(!StagePaletteRename(name)) MessageBox(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.rename_failed", L"Could not rename the palette. Enter a name of up to 100 characters without control characters."),
		FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONWARNING);
	return 0;
}

LRESULT CSettingsGeneralPage::OnDeletePalette(WORD, WORD, HWND, BOOL&)
{
	StagePaletteDelete();
	return 0;
}

LRESULT CSettingsGeneralPage::OnManagePalettes(WORD, WORD, HWND, BOOL&)
{
    const auto customBefore = m_customPalettes, pendingBefore = m_pendingPalettes;
    const auto newBefore = m_newPaletteIds, deletedBefore = m_deletedPaletteIds;
    const int acceptedBefore = m_acceptedPaletteIndex;
    const bool changedBefore = m_paletteChanged;
    std::vector<CString> labels;
    for(int i = 0; i < m_interfacePalette.GetCount(); ++i) { CString value; m_interfacePalette.GetLBText(i, value); labels.push_back(value); }
    CPaletteManagerDialog dialog(*this);
    const INT_PTR result = dialog.DoModal(m_hWnd);
    m_paletteManager = NULL;
    if(result != IDOK) {
        m_customPalettes = customBefore; m_pendingPalettes = pendingBefore;
        m_newPaletteIds = newBefore; m_deletedPaletteIds = deletedBefore;
        m_acceptedPaletteIndex = acceptedBefore; m_paletteChanged = changedBefore;
        m_interfacePalette.ResetContent();
        for(const CString& label : labels) m_interfacePalette.AddString(label);
        m_interfacePalette.SetCurSel(acceptedBefore);
        UpdatePaletteColorControls(); GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW).Invalidate();
    }
    return 0;
}

CString CSettingsGeneralPage::SuggestedPaletteCopyName() const
{
	CString original;
	const int selection = m_interfacePalette.GetCurSel();
	if(selection < 0) return CString();
	m_interfacePalette.GetLBText(selection, original);
	CString prefix = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.copy_prefix", L"Copy of ");
	CString base = prefix + original;
	if(base.GetLength() > 96) base = base.Left(96);
	CString name(base);
	for(int number = 2; ; ++number) {
		bool exists = false;
		for(int index = 0; index < m_interfacePalette.GetCount(); ++index) {
			CString existing; m_interfacePalette.GetLBText(index, existing);
			if(existing.CompareNoCase(name) == 0) { exists = true; break; }
		}
		if(!exists) break;
		name.Format(L"%s %d", base.GetString(), number);
	}
	return name;
}

LRESULT CSettingsGeneralPage::OnCopyPalette(WORD, WORD, HWND, BOOL&)
{
	CString name = SuggestedPaletteCopyName();
	if(name.IsEmpty()) return 0;
	if(AU::InputBoxOwned(name, FbeLoadRuntimeStringByKey(L"fbe.settings.palette.copy", L"Create copy..."),
		FbeLoadRuntimeStringByKey(L"fbe.settings.palette.copy_name", L"Name of the new interface palette:"), m_hWnd) != IDYES) return 0;
	name.Trim();
	if(!StagePaletteCopy(name)) MessageBox(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.invalid_name", L"Could not create the palette. Enter a name of up to 100 characters without control characters."),
		FbeLoadRuntimeStringByKey(L"fbe.settings.validation.caption", L"Settings"), MB_OK | MB_ICONWARNING);
	return 0;
}

bool CSettingsGeneralPage::PrepareCommit(SettingsFileTransaction& files, CString& error)
{
	for(const auto& id : m_deletedPaletteIds) {
		std::wstring detail;
		if(!ThemeManager::PrepareCustomPaletteDelete(id, files, detail)) { error = detail.c_str(); return false; }
	}
	for(const auto& palette : m_pendingPalettes) {
		std::wstring detail;
		const bool created = std::find(m_newPaletteIds.begin(), m_newPaletteIds.end(), palette.id) != m_newPaletteIds.end();
		if(!ThemeManager::PrepareCustomPaletteSave(palette, !created, files, detail)) { error = detail.c_str(); return false; }
	}
	return true;
}

void CSettingsGeneralPage::FinishCommit()
{
	if(m_paletteChanged && m_acceptedPaletteIndex >= INTERFACE_PALETTE_COUNT)
		_Settings.SetInterfacePalette(CString(L"custom:") + m_customPalettes[m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT].id.c_str());
	if(m_paletteChanged) ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
}

bool CSettingsGeneralPage::Validate() { return true; }
void CSettingsGeneralPage::Commit()
{
	if(m_paletteChanged) {
		if(m_acceptedPaletteIndex < INTERFACE_PALETTE_COUNT) _Settings.SetInterfacePalette(PaletteId(PaletteForRow(m_acceptedPaletteIndex)));
		else {
			const auto& selected = m_customPalettes[m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT];
			_Settings.SetInterfacePalette(CString(L"custom:") + selected.id.c_str());
			for(const auto& pending : m_pendingPalettes) if(pending.id == selected.id) { ThemeManager::SetCustomPalette(pending); break; }
		}
	}
	const int iconSize = m_scriptIconSize.GetCurSel();
	if(iconSize != CB_ERR) _Settings.SetScriptIconSize(static_cast<int>(m_scriptIconSize.GetItemData(iconSize)));
	const int mainIconSize = m_mainToolbarIconSize.GetCurSel();
	if(mainIconSize != CB_ERR) _Settings.SetMainToolbarIconSize(static_cast<int>(m_mainToolbarIconSize.GetItemData(mainIconSize)));
	const int interfaceTheme = m_interfaceTheme.GetCurSel();
	if(interfaceTheme >= INTERFACE_THEME_AUTOMATIC && interfaceTheme <= INTERFACE_THEME_DARK &&
		(!m_preserveUnavailableDark || interfaceTheme != INTERFACE_THEME_LIGHT))
		_Settings.SetInterfaceTheme(static_cast<InterfaceTheme>(interfaceTheme));
	else if(m_preserveUnavailableDark) ThemeManager::SetSelectedTheme(m_originalTheme);
	_Settings.SetDescriptionLayout(m_descriptionLayout.GetCurSel() == 1 ? L"classic" : L"modern");
}
bool CSettingsGeneralPage::CancelChanges()
{
	m_closing = true;
	m_themeDropdownStart = -1;
	m_paletteDropdownStart = -1;
	m_closing = false;
	return true;
}
