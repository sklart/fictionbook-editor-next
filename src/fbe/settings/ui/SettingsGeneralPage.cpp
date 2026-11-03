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
    HWND m_preview = NULL;
    HWND m_hex = NULL;
    HWND m_colorInfo = NULL;
    HWND m_previewMode = NULL;
    HWND m_recent = NULL;
    HWND m_recentLabel = NULL;
    HWND m_roleList = NULL;
    HWND m_roleDescription = NULL;
    HWND m_resetChanges = NULL;
    CImageList m_previewIcons;
    std::vector<HICON> m_previewScriptIcons;
    std::vector<CString> m_previewScriptNames;
    HFONT m_scaledFont = NULL;
    std::vector<COLORREF> m_recentColors;
    std::vector<ThemeManager::CustomPalette> m_startCustom, m_startPending;
    std::vector<std::wstring> m_startNew, m_startDeleted;
    std::vector<CString> m_startLabels;
    int m_startAccepted = 0;
    bool m_startChanged = false;
    bool m_updatingHex = false;
    COLORREF m_currentColor = 0, m_baseColor = 0;
    CString m_baseDescription;
    int m_hoverRole = -1;
    bool m_layoutActive = false;
    SIZE m_minTrack = {};
    UINT m_testLayoutDpi = 0;
    static constexpr UINT kPreviewSelect = WM_APP + 0x351;
    static constexpr int kPreviewId = 0x7e41;
    static constexpr int kHexId = 0x7e42;
    static constexpr int kPreviewModeId = 0x7e43;
    static constexpr int kRecentId = 0x7e44;
    static constexpr int kRoleListId = 0x7e45;
    static constexpr int kResetChangesId = 0x7e46;
    struct PreviewPart { RECT rect; int role; LPCWSTR text; int background; };

    static std::vector<PreviewPart> PreviewParts(int width, int height, int mode = 0)
    {
        auto rect = [=](int l, int t, int r, int b) -> RECT {
            return { MulDiv(width, l, 1000), MulDiv(height, t, 600), MulDiv(width, r, 1000), MulDiv(height, b, 600) };
        };
        if(mode == 1) return {
            {rect(0,0,1000,600), THEME_COLOR_WINDOW, L"", THEME_COLOR_WINDOW},
            {rect(30,30,970,105), THEME_COLOR_CONTROL, L"Normal", THEME_COLOR_CONTROL},
            {rect(30,125,970,200), THEME_COLOR_HOVER, L"Hover", THEME_COLOR_HOVER},
            {rect(30,220,970,295), THEME_COLOR_PRESSED, L"Pressed", THEME_COLOR_PRESSED},
            {rect(30,315,970,390), THEME_COLOR_FOCUS, L"Focus", THEME_COLOR_FOCUS},
            {rect(30,410,970,485), THEME_COLOR_DISABLED_TEXT, L"Disabled", THEME_COLOR_CONTROL},
            {rect(30,505,970,580), THEME_COLOR_SELECTION_BACKGROUND, L"Selected", THEME_COLOR_SELECTION_BACKGROUND},
            {rect(70,515,930,570), THEME_COLOR_SELECTION_TEXT, L"Selected", THEME_COLOR_SELECTION_BACKGROUND}
        };
        if(mode == 2) return {
            {rect(0,0,1000,600), THEME_COLOR_WINDOW, L"", THEME_COLOR_WINDOW},
            {rect(30,35,970,130), THEME_COLOR_ACCENT, L"Accent", THEME_COLOR_ACCENT},
            {rect(30,150,970,245), THEME_COLOR_ERROR, L"Error", THEME_COLOR_ERROR},
            {rect(30,265,970,360), THEME_COLOR_WARNING, L"Warning", THEME_COLOR_WARNING},
            {rect(30,380,970,475), THEME_COLOR_SUCCESS, L"Success", THEME_COLOR_SUCCESS},
            {rect(30,495,970,575), THEME_COLOR_SECONDARY_TEXT, L"Secondary text", THEME_COLOR_WINDOW}
        };
        return {
            {rect(0,0,1000,600), THEME_COLOR_WINDOW, L"", THEME_COLOR_WINDOW},
            {rect(0,0,1000,65), THEME_COLOR_CONTROL, L"FBE Next", THEME_COLOR_CONTROL},
            {rect(0,68,1000,115), THEME_COLOR_TEXT, L"File   Edit   View   Insert   Scripts", THEME_COLOR_WINDOW},
            {rect(0,118,1000,180), THEME_COLOR_CONTROL, L"", THEME_COLOR_CONTROL},
            {rect(0,181,1000,186), THEME_COLOR_SEPARATOR, L"", THEME_COLOR_SEPARATOR},
            {rect(0,188,1000,238), THEME_COLOR_CONTROL, L"", THEME_COLOR_CONTROL},
            {rect(0,239,1000,244), THEME_COLOR_SEPARATOR, L"", THEME_COLOR_SEPARATOR},
            {rect(0,246,300,545), THEME_COLOR_BORDER, L"", THEME_COLOR_BORDER},
            {rect(5,251,294,540), THEME_COLOR_WINDOW, L"", THEME_COLOR_WINDOW},
            {rect(5,251,294,284), THEME_COLOR_CONTROL, L"Scripts", THEME_COLOR_CONTROL},
            {rect(5,284,294,287), THEME_COLOR_SEPARATOR, L"", THEME_COLOR_SEPARATOR},
            {rect(12,287,288,330), THEME_COLOR_BORDER, L"", THEME_COLOR_BORDER},
            {rect(16,291,284,326), THEME_COLOR_CONTROL, L"", THEME_COLOR_CONTROL},
            {rect(24,294,277,323), THEME_COLOR_SECONDARY_TEXT, L"@find-script", THEME_COLOR_CONTROL},
            {rect(75,336,284,373), THEME_COLOR_SELECTION_BACKGROUND, L"", THEME_COLOR_SELECTION_BACKGROUND},
            {rect(80,339,275,370), THEME_COLOR_SELECTION_TEXT, L"@script0", THEME_COLOR_SELECTION_BACKGROUND},
            {rect(75,375,284,412), THEME_COLOR_HOVER, L"", THEME_COLOR_HOVER},
            {rect(80,378,275,409), THEME_COLOR_TEXT, L"@script1", THEME_COLOR_HOVER},
            {rect(80,417,275,448), THEME_COLOR_TEXT, L"@script2", THEME_COLOR_WINDOW},
            {rect(80,456,275,487), THEME_COLOR_TEXT, L"@script3", THEME_COLOR_WINDOW},
            {rect(320,252,975,294), THEME_COLOR_CONTROL, L"  ID:             \x25BE", THEME_COLOR_CONTROL},
            {rect(320,300,975,340), THEME_COLOR_TEXT, L"Document", THEME_COLOR_WINDOW},
            {rect(320,346,610,388), THEME_COLOR_BORDER, L"", THEME_COLOR_BORDER},
            {rect(324,350,606,384), THEME_COLOR_PRESSED, L"", THEME_COLOR_PRESSED},
            {rect(330,350,420,384), THEME_COLOR_TEXT, L"  OK", THEME_COLOR_PRESSED},
            {rect(630,346,960,388), THEME_COLOR_FOCUS, L"  Focus", THEME_COLOR_FOCUS},
            {rect(320,400,470,440), THEME_COLOR_ACCENT, L"  Accent", THEME_COLOR_ACCENT},
            {rect(485,400,635,440), THEME_COLOR_ERROR, L"  Error", THEME_COLOR_ERROR},
            {rect(650,400,800,440), THEME_COLOR_WARNING, L"  Warning", THEME_COLOR_WARNING},
            {rect(815,400,970,440), THEME_COLOR_SUCCESS, L"  OK", THEME_COLOR_SUCCESS},
            {rect(320,455,610,490), THEME_COLOR_DISABLED_TEXT, L"Disabled", THEME_COLOR_WINDOW},
            {rect(0,548,1000,600), THEME_COLOR_CONTROL, L"  FB2                                           UTF-8", THEME_COLOR_CONTROL}
        };
    }

    static RECT PreviewCaptionButton(int width, int height, int index)
    {
        const int titleHeight = MulDiv(height, 65, 600);
        const int buttonWidth = max(10, titleHeight);
        const int right = width - (2 - index) * buttonWidth;
        return { right - buttonWidth, 0, right, titleHeight };
    }

    void LoadPreviewScriptIcons(UINT dpi)
    {
        for(HICON icon : m_previewScriptIcons) ::DestroyIcon(icon);
        m_previewScriptIcons.clear();
        m_previewScriptNames.clear();
        wchar_t path[MAX_PATH] = {};
        if(!::GetModuleFileNameW(NULL, path, _countof(path))) return;
        CString directory(path); const int slash = directory.ReverseFind(L'\\');
        if(slash < 0) return;
        directory = directory.Left(slash + 1) + L"Scripts\\";
        const wchar_t* names[] = { L"01_Регистр.ico", L"02_Примечания.ico", L"03_Комментарии.ico", L"06_Чистка.ico" };
        const int size = UiMetrics::ScaleForDpi(16, dpi);
        for(const wchar_t* name : names) {
            HICON icon = static_cast<HICON>(::LoadImageW(NULL, directory + name, IMAGE_ICON, size, size, LR_LOADFROMFILE));
            if(icon) {
                m_previewScriptIcons.push_back(icon);
                CString label(name); label = label.Mid(3); label = label.Left(label.GetLength() - 4);
                m_previewScriptNames.push_back(label);
            }
        }
    }

    void PreviewColors(COLORREF* colors) const
    {
        const int index = m_page.m_acceptedPaletteIndex;
        const int customIndex = index - INTERFACE_PALETTE_COUNT;
        const ThemeManager::CustomPalette* custom = customIndex >= 0 && customIndex < static_cast<int>(m_page.m_customPalettes.size()) ?
            &m_page.m_customPalettes[customIndex] : NULL;
        ThemeManager::PreviewColors(static_cast<InterfaceTheme>(m_page.m_acceptedThemeIndex),
            custom ? INTERFACE_PALETTE_CLASSIC : CSettingsGeneralPage::PaletteForRow(index), custom, colors);
    }

    int PreviewHitTest(POINT point) const
    {
        RECT client = {}; ::GetClientRect(m_preview, &client);
        const auto parts = PreviewParts(client.right, client.bottom, static_cast<int>(::SendMessageW(m_previewMode, CB_GETCURSEL, 0, 0)));
        for(auto part = parts.rbegin(); part != parts.rend(); ++part)
            if(::PtInRect(&part->rect, point)) return part->role;
        return -1;
    }

    void PaintPreview(HDC target)
    {
        RECT client = {}; ::GetClientRect(m_preview, &client);
        if(client.right <= 0 || client.bottom <= 0) return;
        HDC dc = ::CreateCompatibleDC(target);
        HBITMAP bitmap = dc ? ::CreateCompatibleBitmap(target, client.right, client.bottom) : NULL;
        if(!dc || !bitmap) { if(bitmap) ::DeleteObject(bitmap); if(dc) ::DeleteDC(dc); return; }
        HGDIOBJ oldBitmap = ::SelectObject(dc, bitmap);
        HFONT font = reinterpret_cast<HFONT>(::SendMessageW(m_resetChanges, WM_GETFONT, 0, 0));
        if(!font) font = reinterpret_cast<HFONT>(::SendMessageW(m_hWnd, WM_GETFONT, 0, 0));
        HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
        COLORREF colors[THEME_COLOR_COUNT] = {}; PreviewColors(colors);
        ::SetBkMode(dc, TRANSPARENT);
        const auto parts = PreviewParts(client.right, client.bottom, static_cast<int>(::SendMessageW(m_previewMode, CB_GETCURSEL, 0, 0)));
        for(const auto& part : parts) {
            HBRUSH brush = ::CreateSolidBrush(colors[part.background]);
            if(brush) { ::FillRect(dc, &part.rect, brush); ::DeleteObject(brush); }
            if(*part.text) {
                ::SetTextColor(dc, colors[part.role == THEME_COLOR_SELECTION_TEXT || part.role == THEME_COLOR_TEXT ||
                    part.role == THEME_COLOR_SECONDARY_TEXT || part.role == THEME_COLOR_DISABLED_TEXT ? part.role : THEME_COLOR_TEXT]);
                RECT text = part.rect; text.left += 4;
                CString localized;
                LPCWSTR caption = part.text;
                const bool scriptName = wcsncmp(part.text, L"@script", 7) == 0 && part.text[7] >= L'0' && part.text[7] <= L'3';
                const int scriptIndex = scriptName ? part.text[7] - L'0' : -1;
                if(scriptIndex >= 0 && scriptIndex < static_cast<int>(m_previewScriptNames.size())) {
                    localized = m_previewScriptNames[scriptIndex]; caption = localized;
                } else if(wcscmp(part.text, L"@find-script") == 0) {
                    localized = FbeLoadRuntimeStringByKey(L"fbe.document_tree.scripts.search", L"Find script..."); caption = localized;
                }
                const wchar_t* samples[] = {L"File   Edit   View   Insert   Scripts", L"Scripts", L"  \x25B8  Category", L"Category", L"  \x25B8  Script", L"Document", L"Normal"};
                int sampleIndex = -1;
                for(int i = 0; i < static_cast<int>(_countof(samples)); ++i) if(wcscmp(part.text, samples[i]) == 0) { sampleIndex = i; break; }
                if(sampleIndex >= 0) {
                    CString labels = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.preview_labels", L"File   Edit   View   Insert   Scripts|Scripts|  \x25B8  Category|Category|  \x25B8  Script|Document|Normal");
                    int offset = 0;
                    for(int i = 0; i <= sampleIndex; ++i) localized = labels.Tokenize(L"|", offset);
                    if(!localized.IsEmpty()) caption = localized;
                }
                if((part.role >= THEME_COLOR_SECONDARY_TEXT && part.role <= THEME_COLOR_SUCCESS) &&
                    sampleIndex < 0 && !scriptName && wcscmp(part.text, L"@find-script") != 0 && wcscmp(part.text, L"  \x25B8") != 0) {
                    m_page.m_paletteRole.GetLBText(part.role, localized);
                    if(!localized.IsEmpty()) caption = localized;
                }
                ::DrawTextW(dc, caption, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            }
        }
        if(::SendMessageW(m_previewMode, CB_GETCURSEL, 0, 0) == 0) {
            HPEN pen = ::CreatePen(PS_SOLID, 1, colors[THEME_COLOR_TEXT]);
            if(pen) {
                HGDIOBJ oldPen = ::SelectObject(dc, pen), oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
                for(int index = 0; index < 3; ++index) {
                    const RECT button = PreviewCaptionButton(client.right, client.bottom, index);
                    const int centerX = (button.left + button.right) / 2, centerY = (button.top + button.bottom) / 2;
                    const int radius = max(2, min(button.right - button.left, button.bottom - button.top) / 5);
                    if(index == 0) {
                        ::MoveToEx(dc, centerX - radius, centerY + radius / 2, NULL);
                        ::LineTo(dc, centerX + radius + 1, centerY + radius / 2);
                    } else if(index == 1) {
                        ::Rectangle(dc, centerX - radius, centerY - radius, centerX + radius + 1, centerY + radius + 1);
                    } else {
                        ::MoveToEx(dc, centerX - radius, centerY - radius, NULL); ::LineTo(dc, centerX + radius + 1, centerY + radius + 1);
                        ::MoveToEx(dc, centerX + radius, centerY - radius, NULL); ::LineTo(dc, centerX - radius - 1, centerY + radius + 1);
                    }
                }
                ::SelectObject(dc, oldBrush); ::SelectObject(dc, oldPen); ::DeleteObject(pen);
            }
        }
        if(::SendMessageW(m_previewMode, CB_GETCURSEL, 0, 0) == 0 && m_previewIcons.m_hImageList) {
            const int size = UiMetrics::ScaleForDpi(16, UiMetrics::DpiForWindow(m_preview));
            const int top = MulDiv(client.bottom, 118, 600), bottom = MulDiv(client.bottom, 180, 600);
            const int gap = max(2, size / 5), left = gap * 2;
            const int saved = ::SaveDC(dc);
            ::IntersectClipRect(dc, 0, top, client.right, bottom);
            for(int index = 0; index < min(6, m_previewIcons.GetImageCount()); ++index)
                m_previewIcons.Draw(dc, index, CPoint(left + index * (size + gap), top + (bottom - top - size) / 2), ILD_TRANSPARENT);
            ::RestoreDC(dc, saved);
        }
        if(::SendMessageW(m_previewMode, CB_GETCURSEL, 0, 0) == 0 && !m_previewScriptIcons.empty()) {
            const int size = UiMetrics::ScaleForDpi(16, UiMetrics::DpiForWindow(m_preview));
            const int top = MulDiv(client.bottom, 188, 600), bottom = MulDiv(client.bottom, 238, 600);
            const int gap = max(2, size / 5), left = gap * 2;
            const int saved = ::SaveDC(dc); ::IntersectClipRect(dc, 0, top, client.right, bottom);
            for(size_t index = 0; index < m_previewScriptIcons.size(); ++index)
                ::DrawIconEx(dc, left + static_cast<int>(index) * (size + gap), top + (bottom - top - size) / 2,
                    m_previewScriptIcons[index], size, size, 0, NULL, DI_NORMAL);
            ::RestoreDC(dc, saved);
        }
        if(::SendMessageW(m_previewMode, CB_GETCURSEL, 0, 0) == 0 && !m_previewScriptIcons.empty()) {
            const int rowTop[] = {336, 375, 414, 453}, rowBottom[] = {373, 412, 451, 490};
            const int iconX = MulDiv(client.right, 48, 1000);
            const int glyphX = MulDiv(client.right, 24, 1000);
            const int saved = ::SaveDC(dc);
            ::IntersectClipRect(dc, 0, MulDiv(client.bottom, 330, 600), MulDiv(client.right, 300, 1000), MulDiv(client.bottom, 545, 600));
            HPEN pen = ::CreatePen(PS_SOLID, 1, colors[THEME_COLOR_SECONDARY_TEXT]);
            HGDIOBJ oldPen = pen ? ::SelectObject(dc, pen) : NULL;
            HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
            for(int row = 0; row < 4 && row < static_cast<int>(m_previewScriptIcons.size()); ++row) {
                const int top = MulDiv(client.bottom, rowTop[row], 600), bottom = MulDiv(client.bottom, rowBottom[row], 600);
                const int iconSize = max(1, min(UiMetrics::ScaleForDpi(16, UiMetrics::DpiForWindow(m_preview)), bottom - top - 2));
                const int middle = (top + bottom) / 2;
                if(pen) {
                    const int radius = max(2, min(4, (bottom - top - 2) / 3));
                    ::Rectangle(dc, glyphX - radius, middle - radius, glyphX + radius + 1, middle + radius + 1);
                    ::MoveToEx(dc, glyphX - radius + 2, middle, NULL); ::LineTo(dc, glyphX + radius - 1, middle);
                    ::MoveToEx(dc, glyphX, middle - radius + 2, NULL); ::LineTo(dc, glyphX, middle + radius - 1);
                }
                ::DrawIconEx(dc, iconX, middle - iconSize / 2, m_previewScriptIcons[row], iconSize, iconSize, 0, NULL, DI_NORMAL);
            }
            ::SelectObject(dc, oldBrush); if(oldPen) ::SelectObject(dc, oldPen); if(pen) ::DeleteObject(pen);
            ::RestoreDC(dc, saved);
        }
        const int selected = CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel();
        for(const auto& part : parts) {
            const bool usesNormalText = *part.text && part.role != THEME_COLOR_SELECTION_TEXT &&
                part.role != THEME_COLOR_SECONDARY_TEXT && part.role != THEME_COLOR_DISABLED_TEXT;
            if(part.role != selected && part.role != m_hoverRole && !(selected == THEME_COLOR_TEXT && usesNormalText)) continue;
            if(selected == THEME_COLOR_TEXT && usesNormalText && part.role != m_hoverRole) {
                RECT marker = {part.rect.left - 3, part.rect.top, part.rect.left - 1, part.rect.bottom};
                ::FillRect(dc, &marker, static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)));
                marker.left = marker.right - 1;
                ::FillRect(dc, &marker, static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)));
                continue;
            }
            // Draw the selection outside the sample so it cannot cross the
            // text in compact rows such as a selected tree item.
            RECT outline = part.rect; ::InflateRect(&outline, 2, 2);
            HPEN darkPen = ::CreatePen(PS_SOLID, 1, RGB(0,0,0));
            HPEN lightPen = ::CreatePen(PS_DOT, 1, RGB(255,255,255));
            if(darkPen && lightPen) {
                HGDIOBJ oldPen = ::SelectObject(dc, darkPen), oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
                ::Rectangle(dc, outline.left, outline.top, outline.right, outline.bottom);
                ::InflateRect(&outline, -1, -1); ::SelectObject(dc, lightPen);
                ::Rectangle(dc, outline.left, outline.top, outline.right, outline.bottom);
                ::SelectObject(dc, oldBrush); ::SelectObject(dc, oldPen);
            }
            if(darkPen) ::DeleteObject(darkPen);
            if(lightPen) ::DeleteObject(lightPen);
        }
        ::BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
        if(oldFont) ::SelectObject(dc, oldFont);
        ::SelectObject(dc, oldBitmap); ::DeleteObject(bitmap); ::DeleteDC(dc);
    }

    static LRESULT CALLBACK PreviewProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data)
    {
        auto* self = reinterpret_cast<CPaletteManagerDialog*>(data);
        if(message == WM_PAINT) { PAINTSTRUCT paint = {}; HDC dc = ::BeginPaint(window, &paint); self->PaintPreview(dc); ::EndPaint(window, &paint); return 0; }
        if(message == WM_ERASEBKGND) return 1;
        if(message == WM_MOUSEMOVE) {
            POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int role = self->PreviewHitTest(point);
            if(role != self->m_hoverRole) {
                self->m_hoverRole = role;
                CString label;
                if(role >= 0) self->m_page.m_paletteRole.GetLBText(role, label);
                if(!label.IsEmpty()) self->m_tooltips.UpdateText(window, label);
                ::InvalidateRect(window, NULL, FALSE);
            }
            TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, window, 0 }; ::TrackMouseEvent(&track);
            return 0;
        }
        if(message == WM_MOUSELEAVE) { self->m_hoverRole = -1; ::InvalidateRect(window, NULL, FALSE); return 0; }
        if(message == WM_LBUTTONDOWN) {
            POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int role = self->PreviewHitTest(point);
            if(role >= 0) ::SendMessageW(self->m_hWnd, kPreviewSelect, role, 0);
            return 0;
        }
        if(message == WM_NCDESTROY) ::RemoveWindowSubclass(window, PreviewProc, 1);
        return ::DefSubclassProc(window, message, wParam, lParam);
    }
    static LRESULT CALLBACK ColorInfoProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data)
    {
        auto* self = reinterpret_cast<CPaletteManagerDialog*>(data);
        if(message == WM_PAINT) {
            PAINTSTRUCT paint = {}; HDC dc = ::BeginPaint(window, &paint);
            RECT bounds = {}; ::GetClientRect(window, &bounds);
            ::FillRect(dc, &bounds, ThemeManager::WindowBrush());
            const int line = max(1, static_cast<int>(bounds.bottom) / 2);
            const int square = max(4, min(line - 4, UiMetrics::ScaleForDpi(20, UiMetrics::DpiForWindow(window))));
            HFONT font = reinterpret_cast<HFONT>(::SendMessageW(window, WM_GETFONT, 0, 0));
            HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
            ::SetBkMode(dc, TRANSPARENT); ::SetTextColor(dc, ThemeManager::TextColor());
            for(int row = 0; row < 2; ++row) {
                RECT swatch = { 2, row * line + 2, 2 + square, row * line + 2 + square };
                HBRUSH brush = ::CreateSolidBrush(row == 0 ? self->m_currentColor : self->m_baseColor);
                if(brush) { ::FillRect(dc, &swatch, brush); ::DeleteObject(brush); }
                ::FrameRect(dc, &swatch, reinterpret_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)));
                RECT text = { square + 8, row * line, bounds.right, (row + 1) * line };
                CString caption;
                if(row == 0) caption = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.current_color", L"Current");
                else caption = self->m_baseDescription;
                ::DrawTextW(dc, caption, -1, &text, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            }
            if(oldFont) ::SelectObject(dc, oldFont);
            ::EndPaint(window, &paint); return 0;
        }
        if(message == WM_NCDESTROY) ::RemoveWindowSubclass(window, ColorInfoProc, 1);
        return ::DefSubclassProc(window, message, wParam, lParam);
    }
    static LRESULT CALLBACK ContrastWarningProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
    {
        if(message == WM_PAINT && ThemeManager::UsesPaletteSurface()) {
            PAINTSTRUCT paint = {}; HDC dc = ::BeginPaint(window, &paint);
            RECT bounds = {}; ::GetClientRect(window, &bounds);
            ::FillRect(dc, &bounds, ThemeManager::WindowBrush());
            const COLORREF background = ThemeManager::WindowColor();
            auto linear = [](BYTE value) { const double v = value / 255.0; return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
            const double luminance = 0.2126 * linear(GetRValue(background)) + 0.7152 * linear(GetGValue(background)) + 0.0722 * linear(GetBValue(background));
            ::SetTextColor(dc, luminance > 0.179 ? RGB(0,0,0) : RGB(255,255,255)); ::SetBkMode(dc, TRANSPARENT);
            HFONT font = reinterpret_cast<HFONT>(::SendMessageW(window, WM_GETFONT, 0, 0));
            HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
            const int length = ::GetWindowTextLengthW(window); CString text;
            if(length > 0) { ::GetWindowTextW(window, text.GetBuffer(length + 1), length + 1); text.ReleaseBuffer(); }
            ::DrawTextW(dc, text, text.GetLength(), &bounds, DT_WORDBREAK | DT_NOPREFIX);
            if(oldFont) ::SelectObject(dc, oldFont);
            ::EndPaint(window, &paint); return 0;
        }
        if(message == WM_NCDESTROY) ::RemoveWindowSubclass(window, ContrastWarningProc, 1);
        return ::DefSubclassProc(window, message, wParam, lParam);
    }
public:
    explicit CPaletteManagerDialog(CSettingsGeneralPage& page) : m_page(page) {}
    ~CPaletteManagerDialog() { for(HICON icon : m_previewScriptIcons) ::DestroyIcon(icon); if(m_scaledFont) ::DeleteObject(m_scaledFont); }
    enum { IDD = IDD_SETTINGS_PALETTE_MANAGER };
    BEGIN_MSG_MAP(CPaletteManagerDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
        MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
        MESSAGE_HANDLER(kPreviewSelect, OnPreviewSelect)
        COMMAND_HANDLER(IDOK, BN_CLICKED, OnOk)
        COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnCancel)
        COMMAND_HANDLER(IDC_PALETTE_ROLE, CBN_SELCHANGE, OnRoleChanged)
        COMMAND_HANDLER(IDC_PALETTE_IMPORT, BN_CLICKED, OnAction)
        COMMAND_HANDLER(IDC_PALETTE_EXPORT, BN_CLICKED, OnAction)
        COMMAND_HANDLER(IDC_PALETTE_RENAME, BN_CLICKED, OnAction)
        COMMAND_HANDLER(IDC_PALETTE_DELETE, BN_CLICKED, OnAction)
        COMMAND_HANDLER(IDC_PALETTE_CHOOSE_COLOR, BN_CLICKED, OnAction)
        COMMAND_HANDLER(IDC_PALETTE_RESET_COLOR, BN_CLICKED, OnAction)
        COMMAND_HANDLER(kHexId, EN_CHANGE, OnHexChanged)
        COMMAND_HANDLER(kPreviewModeId, CBN_SELCHANGE, OnPreviewModeChanged)
        COMMAND_HANDLER(kRecentId, CBN_SELCHANGE, OnRecentChanged)
        COMMAND_HANDLER(kRoleListId, LBN_SELCHANGE, OnRoleListChanged)
        COMMAND_HANDLER(kResetChangesId, BN_CLICKED, OnResetChanges)
    END_MSG_MAP()
    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
    {
        m_page.m_paletteManager = m_hWnd;
        ::SetWindowSubclass(GetDlgItem(IDC_PALETTE_CONTRAST), ContrastWarningProc, 1, 0);
        m_startCustom = m_page.m_customPalettes; m_startPending = m_page.m_pendingPalettes;
        m_startNew = m_page.m_newPaletteIds; m_startDeleted = m_page.m_deletedPaletteIds;
        m_startAccepted = m_page.m_acceptedPaletteIndex; m_startChanged = m_page.m_paletteChanged;
        for(int i = 0; i < m_page.m_interfacePalette.GetCount(); ++i) { CString label; m_page.m_interfacePalette.GetLBText(i, label); m_startLabels.push_back(label); }
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
        m_preview = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
            0, 0, 1, 1, m_hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPreviewId)), _Module.GetModuleInstance(), NULL);
        if(m_preview) ::SetWindowSubclass(m_preview, PreviewProc, 1, reinterpret_cast<DWORD_PTR>(this));
        ToolbarFactory::CreateCommandToolbarImages(m_previewIcons, IDR_MAINFRAME, UiMetrics::DpiForWindow(m_hWnd), 16);
        LoadPreviewScriptIcons(UiMetrics::DpiForWindow(m_hWnd));
        m_previewMode = ::CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 1, 200, m_hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPreviewModeId)), _Module.GetModuleInstance(), NULL);
        CString modes = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.preview_modes", L"Interface|Control states|Messages and accents");
        int position = 0;
        for(int i = 0; i < 4; ++i) { CString mode = modes.Tokenize(L"|", position); if(mode.IsEmpty()) mode = L"All colours"; ::SendMessageW(m_previewMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(mode.GetString())); }
        ::SendMessageW(m_previewMode, CB_SETCURSEL, 0, 0);
        m_roleList = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            0, 0, 1, 1, m_hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRoleListId)), _Module.GetModuleInstance(), NULL);
        for(int role = 0; role < THEME_COLOR_COUNT; ++role) {
            CString label; m_page.m_paletteRole.GetLBText(role, label);
            ::SendMessageW(m_roleList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.GetString()));
        }
        m_hex = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 1, 1, m_hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kHexId)), _Module.GetModuleInstance(), NULL);
        m_roleDescription = ::CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
            0, 0, 1, 1, m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
        m_colorInfo = ::CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 1, 1, m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
        if(m_colorInfo) ::SetWindowSubclass(m_colorInfo, ColorInfoProc, 1, reinterpret_cast<DWORD_PTR>(this));
        m_recentLabel = ::CreateWindowExW(0, L"STATIC", FbeLoadRuntimeStringByKey(L"fbe.settings.palette.recent", L"Recent:"), WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 1, 1, m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
        m_recent = ::CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 1, 200, m_hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRecentId)), _Module.GetModuleInstance(), NULL);
        m_resetChanges = ::CreateWindowExW(0, L"BUTTON", FbeLoadRuntimeStringByKey(L"fbe.settings.palette.reset_changes", L"Reset changes"),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 1, 1, m_hWnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kResetChangesId)), _Module.GetModuleInstance(), NULL);
        HFONT font = reinterpret_cast<HFONT>(::SendMessageW(m_hWnd, WM_GETFONT, 0, 0));
        if(m_hex) { ::SendMessageW(m_hex, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE); ::SendMessageW(m_hex, EM_SETLIMITTEXT, 7, 0); }
        if(m_colorInfo) ::SendMessageW(m_colorInfo, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        if(m_roleDescription) ::SendMessageW(m_roleDescription, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        if(m_previewMode) ::SendMessageW(m_previewMode, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        if(m_roleList) { ::SendMessageW(m_roleList, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE); ::SendMessageW(m_roleList, LB_SETITEMHEIGHT, 0, UiMetrics::ScaleForDpi(26, UiMetrics::DpiForWindow(m_hWnd))); }
        if(m_recent) ::SendMessageW(m_recent, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        if(m_recentLabel) ::SendMessageW(m_recentLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        if(m_resetChanges) ::SendMessageW(m_resetChanges, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        FitLocalizedLayout();
        CComboBox roles(GetDlgItem(IDC_PALETTE_ROLE).m_hWnd);
        for(int i = 0; i < m_page.m_paletteRole.GetCount(); ++i) { CString text; m_page.m_paletteRole.GetLBText(i, text); roles.AddString(text); }
        roles.SetCurSel(m_page.m_paletteRole.GetCurSel());
        m_tooltips.Initialize(m_hWnd);
        m_tooltips.Add(m_preview, L"fbe.settings.tooltip.palette.preview", L"Choose an interface colour by clicking the preview.");
        m_tooltips.Add(m_hex, L"fbe.settings.tooltip.palette.hex", L"Enter a colour as #RRGGBB.");
        m_tooltips.AddDisabledControlArea(m_hex, L"fbe.settings.tooltip.general.palette_builtin", L"Create a copy of the built-in scheme before editing it.");
        m_tooltips.Add(m_recent, L"fbe.settings.tooltip.palette.recent", L"Choose a recently used colour.");
        m_tooltips.Add(m_roleDescription, L"fbe.settings.tooltip.palette.preview", L"Choose an interface colour by clicking the preview.");
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
    void FitLocalizedLayout(bool resizeWindow = true)
    {
        if(m_layoutActive) return;
        m_layoutActive = true;
        const UINT dpi = m_testLayoutDpi ? m_testLayoutDpi : UiMetrics::DpiForWindow(m_hWnd);
        const int margin = UiMetrics::ScaleForDpi(8, dpi), gap = UiMetrics::ScaleForDpi(3, dpi);
        HDC dc = ::GetDC(m_hWnd);
        HFONT font = reinterpret_cast<HFONT>(::SendMessageW(m_resetChanges, WM_GETFONT, 0, 0));
        if(!font) font = reinterpret_cast<HFONT>(::SendMessageW(m_hWnd, WM_GETFONT, 0, 0));
        HGDIOBJ oldFont = dc && font ? ::SelectObject(dc, font) : NULL;
        int buttonTextWidth = 0, labelWidth = 0, fontHeight = UiMetrics::ScaleForDpi(16, dpi);
        if(dc) {
            TEXTMETRICW metrics = {}; if(::GetTextMetricsW(dc, &metrics)) fontHeight = metrics.tmHeight;
            for(int id : {IDC_PALETTE_IMPORT, IDC_PALETTE_EXPORT, IDC_PALETTE_RENAME, IDC_PALETTE_DELETE,
                IDC_PALETTE_CHOOSE_COLOR, IDC_PALETTE_RESET_COLOR, kResetChangesId, IDOK, IDCANCEL}) {
                CString text; GetDlgItemText(id, text); SIZE extent = {};
                if(::GetTextExtentPoint32W(dc, text, text.GetLength(), &extent)) buttonTextWidth = max(buttonTextWidth, static_cast<int>(extent.cx));
            }
            CString text; GetDlgItemText(IDC_PALETTE_ROLE_LABEL, text); SIZE extent = {};
            if(::GetTextExtentPoint32W(dc, text, text.GetLength(), &extent)) labelWidth = extent.cx;
        }
        if(oldFont) ::SelectObject(dc, oldFont); if(dc) ::ReleaseDC(m_hWnd, dc);
        labelWidth += margin; // Leave room for text overhang and static-control padding.
        const int buttonWidth = max(UiMetrics::ScaleForDpi(95, dpi), buttonTextWidth + 2 * margin);
        const int lineHeight = max(UiMetrics::ScaleForDpi(22, dpi), fontHeight + 2 * gap);
        RECT client = {}, window = {}; ::GetClientRect(m_hWnd, &client); ::GetWindowRect(m_hWnd, &window);
        MONITORINFO monitor = { sizeof(monitor) };
        ::GetMonitorInfoW(::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &monitor);
        if(m_testLayoutDpi) monitor.rcWork = { 0, 0, 1366, 728 };
        const int screenWidth = monitor.rcWork.right - monitor.rcWork.left - (window.right - window.left - client.right);
        const int desiredWidth = max(UiMetrics::ScaleForDpi(760, dpi), max(static_cast<int>(client.right), max(2 * buttonWidth + 3 * margin,
            labelWidth + UiMetrics::ScaleForDpi(140, dpi) + 3 * margin)));
        const int width = resizeWindow ? min(desiredWidth, max(1, screenWidth)) : max(1, static_cast<int>(client.right));
        const int right = width - margin;
        int y = margin;
        auto place = [&](int id, int x, int top, int w, int h) {
            ::SetWindowPos(GetDlgItem(id), NULL, x, top, max(1, w), max(1, h), SWP_NOZORDER | SWP_NOACTIVATE);
        };
        place(IDC_PALETTE_CURRENT, margin, y, right - margin, lineHeight); y += lineHeight + gap;
        const int actionWidth = (right - margin - 3 * gap) / 4;
        for(int action = 0; action < 4; ++action) {
            const int ids[] = { IDC_PALETTE_IMPORT, IDC_PALETTE_EXPORT, IDC_PALETTE_RENAME, IDC_PALETTE_DELETE };
            place(ids[action], margin + action * (actionWidth + gap), y, actionWidth, lineHeight);
        }
        y += lineHeight + gap;
        const int workHeight = monitor.rcWork.bottom - monitor.rcWork.top - (window.bottom - window.top - client.bottom);
        const int availablePreviewHeight = workHeight - y - lineHeight - 2 * margin;
        const int previewHeight = resizeWindow ? max(UiMetrics::ScaleForDpi(120, dpi), min(UiMetrics::ScaleForDpi(260, dpi), availablePreviewHeight)) :
            max(UiMetrics::ScaleForDpi(120, dpi), static_cast<int>(client.bottom) - y - lineHeight - 2 * margin);
        const int minimumInspector = max(2 * buttonWidth + gap, labelWidth + UiMetrics::ScaleForDpi(140, dpi) + gap);
        const int previewWidth = min((right - margin) * 2 / 3,
            max(UiMetrics::ScaleForDpi(220, dpi), right - margin - minimumInspector - gap));
        ::SetWindowPos(m_previewMode, NULL, margin, y, previewWidth, 8 * lineHeight, SWP_NOZORDER | SWP_NOACTIVATE);
        ::SetWindowPos(m_preview, NULL, margin, y + lineHeight + gap, previewWidth, previewHeight - lineHeight - gap, SWP_NOZORDER | SWP_NOACTIVATE);
        ::SetWindowPos(m_roleList, NULL, margin, y + lineHeight + gap, previewWidth, previewHeight - lineHeight - gap, SWP_NOZORDER | SWP_NOACTIVATE);
        const int previewBottom = y + previewHeight;
        const int inspectorLeft = margin + previewWidth + gap;
        const int inspectorWidth = right - inspectorLeft;
        const int roleLabelWidth = min(labelWidth, max(1, inspectorWidth - UiMetrics::ScaleForDpi(100, dpi) - gap));
        const int comboLeft = inspectorLeft + roleLabelWidth + gap;
        RECT combo = {}; ::GetWindowRect(GetDlgItem(IDC_PALETTE_ROLE), &combo);
        place(IDC_PALETTE_ROLE_LABEL, inspectorLeft, y, roleLabelWidth, lineHeight);
        place(IDC_PALETTE_ROLE, comboLeft, y, right - comboLeft, combo.bottom - combo.top); y += lineHeight + gap;
        ::SetWindowPos(m_roleDescription, NULL, inspectorLeft, y, inspectorWidth, lineHeight, SWP_NOZORDER | SWP_NOACTIVATE); y += lineHeight + gap;
        ::SetWindowPos(m_hex, NULL, inspectorLeft, y, inspectorWidth, lineHeight, SWP_NOZORDER | SWP_NOACTIVATE); y += lineHeight + gap;
        ::SetWindowPos(m_colorInfo, NULL, inspectorLeft, y, inspectorWidth, 2 * lineHeight, SWP_NOZORDER | SWP_NOACTIVATE); y += 2 * lineHeight + gap;
        ::SetWindowPos(m_recentLabel, NULL, inspectorLeft, y, inspectorWidth / 2, lineHeight, SWP_NOZORDER | SWP_NOACTIVATE);
        ::SetWindowPos(m_recent, NULL, inspectorLeft + inspectorWidth / 2, y, inspectorWidth - inspectorWidth / 2, 7 * lineHeight, SWP_NOZORDER | SWP_NOACTIVATE); y += lineHeight + gap;
        place(IDC_PALETTE_CHOOSE_COLOR, inspectorLeft, y, (inspectorWidth - gap) / 2, lineHeight);
        place(IDC_PALETTE_RESET_COLOR, inspectorLeft + (inspectorWidth + gap) / 2, y, (inspectorWidth - gap) / 2, lineHeight); y += lineHeight + gap;
        place(IDC_PALETTE_CONTRAST, inspectorLeft, y, inspectorWidth, 3 * fontHeight + 2 * margin);
        y = max(previewBottom, y + 3 * fontHeight + 2 * margin) + margin;
        place(kResetChangesId, margin, y, buttonWidth, lineHeight);
        place(IDOK, right - 2 * buttonWidth - gap, y, buttonWidth, lineHeight);
        place(IDCANCEL, right - buttonWidth, y, buttonWidth, lineHeight); y += lineHeight + margin;
        if(resizeWindow) {
            const int targetWidth = width + window.right - window.left - client.right;
            const int targetHeight = y + window.bottom - window.top - client.bottom;
            m_minTrack.cx = min(targetWidth, monitor.rcWork.right - monitor.rcWork.left);
            m_minTrack.cy = min(targetHeight, monitor.rcWork.bottom - monitor.rcWork.top);
            ::SetWindowPos(m_hWnd, NULL, 0, 0, targetWidth, targetHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        m_layoutActive = false;
    }
    LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM parameter, BOOL& handled)
    {
        auto* limits = reinterpret_cast<MINMAXINFO*>(parameter);
        if(limits && m_minTrack.cx > 0 && m_minTrack.cy > 0) {
            limits->ptMinTrackSize.x = m_minTrack.cx; limits->ptMinTrackSize.y = m_minTrack.cy;
            handled = TRUE;
            return 0;
        }
        handled = FALSE;
        return 0;
    }
    LRESULT OnDpiChanged(UINT, WPARAM value, LPARAM parameter, BOOL&)
    {
        const UINT dpi = HIWORD(value) ? HIWORD(value) : 96;
        m_minTrack = {};
        HFONT font = UiMetrics::CreateDialogFontForDpi(dpi);
        if(font) {
            ::SendMessageW(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
            for(HWND child = ::GetWindow(m_hWnd, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
                ::SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
            if(m_scaledFont) ::DeleteObject(m_scaledFont);
            m_scaledFont = font;
        }
        m_previewIcons.Destroy();
        ToolbarFactory::CreateCommandToolbarImages(m_previewIcons, IDR_MAINFRAME, dpi, 16);
        LoadPreviewScriptIcons(dpi);
        if(m_roleList) ::SendMessageW(m_roleList, LB_SETITEMHEIGHT, 0, UiMetrics::ScaleForDpi(26, dpi));
        const RECT* suggested = reinterpret_cast<const RECT*>(parameter);
        if(suggested) ::SetWindowPos(m_hWnd, NULL, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        if(m_preview) { FitLocalizedLayout(true); ::InvalidateRect(m_preview, NULL, FALSE); }
        return 0;
    }
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL& handled)
    {
        if(m_preview && !m_layoutActive) FitLocalizedLayout(false);
        handled = FALSE;
        return 0;
    }
    LRESULT OnPreviewModeChanged(WORD, WORD, HWND, BOOL&)
    {
        m_hoverRole = -1;
        const bool allColors = ::SendMessageW(m_previewMode, CB_GETCURSEL, 0, 0) == 3;
        ::ShowWindow(m_preview, allColors ? SW_HIDE : SW_SHOW);
        ::ShowWindow(m_roleList, allColors ? SW_SHOW : SW_HIDE);
        if(allColors) ::SendMessageW(m_roleList, LB_SETCURSEL, CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel(), 0);
        if(m_preview) ::InvalidateRect(m_preview, NULL, FALSE);
        return 0;
    }
    LRESULT OnRoleListChanged(WORD, WORD, HWND, BOOL&)
    {
        const LRESULT role = ::SendMessageW(m_roleList, LB_GETCURSEL, 0, 0);
        if(role >= 0 && role < THEME_COLOR_COUNT) { BOOL handled = FALSE; OnPreviewSelect(kPreviewSelect, static_cast<WPARAM>(role), 0, handled); }
        return 0;
    }
    LRESULT OnDrawItem(UINT, WPARAM id, LPARAM parameter, BOOL& handled)
    {
        if(id != kRoleListId) { handled = FALSE; return 0; }
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(parameter);
        if(!item || item->itemID >= THEME_COLOR_COUNT) return TRUE;
        COLORREF colors[THEME_COLOR_COUNT] = {}; PreviewColors(colors);
        const bool selected = (item->itemState & ODS_SELECTED) != 0;
        HBRUSH background = ::CreateSolidBrush(ThemeManager::Color(selected ? THEME_COLOR_SELECTION_BACKGROUND : THEME_COLOR_WINDOW));
        if(background) { ::FillRect(item->hDC, &item->rcItem, background); ::DeleteObject(background); }
        RECT swatch = item->rcItem; ::InflateRect(&swatch, -4, -4); swatch.right = min(swatch.right, swatch.left + swatch.bottom - swatch.top);
        HBRUSH fill = ::CreateSolidBrush(colors[item->itemID]);
        if(fill) { ::FillRect(item->hDC, &swatch, fill); ::DeleteObject(fill); }
        ::FrameRect(item->hDC, &swatch, reinterpret_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)));
        CString label; m_page.m_paletteRole.GetLBText(static_cast<int>(item->itemID), label);
        RECT text = item->rcItem; text.left = swatch.right + 8;
        ::SetBkMode(item->hDC, TRANSPARENT); ::SetTextColor(item->hDC, ThemeManager::Color(selected ? THEME_COLOR_SELECTION_TEXT : THEME_COLOR_TEXT));
        ::DrawTextW(item->hDC, label, -1, &text, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        if(item->itemState & ODS_FOCUS) ::DrawFocusRect(item->hDC, &item->rcItem);
        return TRUE;
    }
    void Refresh()
    {
        CString current; const int selected = m_page.m_interfacePalette.GetCurSel();
        if(selected >= 0) m_page.m_interfacePalette.GetLBText(selected, current);
        const int customIndex = m_page.m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT;
        const bool editable = customIndex >= 0 && customIndex < static_cast<int>(m_page.m_customPalettes.size());
        if(editable) {
            LPCWSTR key = L"fbe.settings.interface_theme.light", fallback = L"Light";
            switch(m_page.m_customPalettes[customIndex].base) {
            case ThemeManager::CustomPaletteBase::Dark: key = L"fbe.settings.interface_theme.dark"; fallback = L"Dark"; break;
            case ThemeManager::CustomPaletteBase::Graphite: key = L"fbe.settings.palette.graphite"; fallback = L"Graphite"; break;
            case ThemeManager::CustomPaletteBase::Navy: key = L"fbe.settings.palette.navy"; fallback = L"Dark blue"; break;
            case ThemeManager::CustomPaletteBase::Sepia: key = L"fbe.settings.palette.sepia"; fallback = L"Sepia"; break;
            default: break;
            }
            current += L"   "; current += FbeLoadRuntimeStringByKey(L"fbe.settings.palette.base_color", L"Base:");
            current += L" "; current += FbeLoadRuntimeStringByKey(key, fallback);
        }
        SetDlgItemText(IDC_PALETTE_CURRENT, current);
        m_tooltips.UpdateText(GetDlgItem(IDC_PALETTE_CURRENT), current);
        for(int id : {IDC_PALETTE_EXPORT, IDC_PALETTE_RENAME, IDC_PALETTE_DELETE, IDC_PALETTE_ROLE_LABEL,
            IDC_PALETTE_ROLE, IDC_PALETTE_CHOOSE_COLOR, IDC_PALETTE_RESET_COLOR}) GetDlgItem(id).EnableWindow(editable);
        ::EnableWindow(m_hex, editable);
        ::EnableWindow(m_recent, editable && !m_recentColors.empty());
        CString contrast; m_page.GetDlgItemText(IDC_PALETTE_CONTRAST, contrast); SetDlgItemText(IDC_PALETTE_CONTRAST, contrast);
        RefreshColorInfo();
        if(m_preview) ::InvalidateRect(m_preview, NULL, FALSE);
        if(m_roleList) ::InvalidateRect(m_roleList, NULL, FALSE);
    }
    void RefreshColorInfo()
    {
        const int role = CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel();
        if(role < 0 || role >= THEME_COLOR_COUNT) return;
        const int descriptionIndex[] = {0, 1, 2, 3, 3, 2, 2, 4, 4, 5, 5, 5, 6, 7, 7, 7};
        CString descriptions = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.role_descriptions", L"Window backgrounds|Control backgrounds|Text and labels|Borders and separators|Selected items|Control states|Interface accents|Status messages");
        int offset = 0; CString description;
        for(int i = 0; i <= descriptionIndex[role]; ++i) description = descriptions.Tokenize(L"|", offset);
        if(m_roleDescription) { ::SetWindowTextW(m_roleDescription, description); m_tooltips.UpdateText(m_roleDescription, description); }
        COLORREF colors[THEME_COLOR_COUNT] = {}; PreviewColors(colors);
        const COLORREF current = colors[role];
        m_currentColor = m_baseColor = current;
        CString value; value.Format(L"#%02X%02X%02X", GetRValue(current), GetGValue(current), GetBValue(current));
        m_updatingHex = true; ::SetWindowTextW(m_hex, value); m_updatingHex = false;
        const int index = m_page.m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT;
        CString info;
        if(index >= 0 && index < static_cast<int>(m_page.m_customPalettes.size())) {
            ThemeManager::CustomPalette base = m_page.m_customPalettes[index];
            base.present[role] = false;
            ThemeManager::PreviewColors(static_cast<InterfaceTheme>(m_page.m_acceptedThemeIndex), INTERFACE_PALETTE_CLASSIC, &base, colors);
            m_baseColor = colors[role];
            info.Format(L"%s  #%02X%02X%02X", static_cast<LPCWSTR>(FbeLoadRuntimeStringByKey(L"fbe.settings.palette.base_color", L"Base:")),
                GetRValue(colors[role]), GetGValue(colors[role]), GetBValue(colors[role]));
            info += L" ";
            info += FbeLoadRuntimeStringByKey(m_page.m_customPalettes[index].present[role] ? L"fbe.settings.palette.overridden" : L"fbe.settings.palette.inherited",
                m_page.m_customPalettes[index].present[role] ? L"(custom)" : L"(inherited)");
        }
        else info = FbeLoadRuntimeStringByKey(L"fbe.settings.palette.base_color", L"Base:");
        m_baseDescription = info;
        if(m_colorInfo) ::InvalidateRect(m_colorInfo, NULL, FALSE);
    }
    LRESULT OnHexChanged(WORD, WORD, HWND, BOOL&)
    {
        if(m_updatingHex) return 0;
        wchar_t text[8] = {}; ::GetWindowTextW(m_hex, text, _countof(text));
        if(wcslen(text) != 7 || text[0] != L'#') return 0;
        unsigned value = 0;
        for(int i = 1; i < 7; ++i) {
            const wchar_t ch = text[i];
            const int digit = ch >= L'0' && ch <= L'9' ? ch - L'0' : ch >= L'a' && ch <= L'f' ? ch - L'a' + 10 : ch >= L'A' && ch <= L'F' ? ch - L'A' + 10 : -1;
            if(digit < 0) return 0;
            value = (value << 4) | static_cast<unsigned>(digit);
        }
        const int role = CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel();
        if(role >= 0 && role < THEME_COLOR_COUNT && m_page.StagePaletteColor(static_cast<ThemeColorRole>(role), RGB((value >> 16) & 255, (value >> 8) & 255, value & 255), true)) {
            RecordRecent(RGB((value >> 16) & 255, (value >> 8) & 255, value & 255));
            DWORD start = 0, end = 0; ::SendMessageW(m_hex, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
            RefreshColorInfo();
            ::SendMessageW(m_hex, EM_SETSEL, start, end);
            CString contrast; m_page.GetDlgItemText(IDC_PALETTE_CONTRAST, contrast); SetDlgItemText(IDC_PALETTE_CONTRAST, contrast);
            if(m_preview) ::InvalidateRect(m_preview, NULL, FALSE);
        }
        return 0;
    }
    void RecordRecent(COLORREF color)
    {
        m_recentColors.erase(std::remove(m_recentColors.begin(), m_recentColors.end(), color), m_recentColors.end());
        m_recentColors.insert(m_recentColors.begin(), color);
        if(m_recentColors.size() > 6) m_recentColors.resize(6);
        ::SendMessageW(m_recent, CB_RESETCONTENT, 0, 0);
        for(COLORREF recent : m_recentColors) {
            CString label; label.Format(L"#%02X%02X%02X", GetRValue(recent), GetGValue(recent), GetBValue(recent));
            const LRESULT index = ::SendMessageW(m_recent, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.GetString()));
            if(index >= 0) ::SendMessageW(m_recent, CB_SETITEMDATA, static_cast<WPARAM>(index), recent);
        }
        ::SendMessageW(m_recent, CB_SETCURSEL, 0, 0);
        ::EnableWindow(m_recent, TRUE);
    }
    LRESULT OnRecentChanged(WORD, WORD, HWND, BOOL&)
    {
        const int selected = static_cast<int>(::SendMessageW(m_recent, CB_GETCURSEL, 0, 0));
        const int role = CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel();
        if(selected >= 0 && role >= 0 && role < THEME_COLOR_COUNT) {
            const COLORREF color = static_cast<COLORREF>(::SendMessageW(m_recent, CB_GETITEMDATA, selected, 0));
            if(m_page.StagePaletteColor(static_cast<ThemeColorRole>(role), color, true)) { RecordRecent(color); Refresh(); }
        }
        return 0;
    }
    LRESULT OnResetChanges(WORD, WORD, HWND, BOOL&)
    {
        m_page.m_customPalettes = m_startCustom; m_page.m_pendingPalettes = m_startPending;
        m_page.m_newPaletteIds = m_startNew; m_page.m_deletedPaletteIds = m_startDeleted;
        m_page.m_acceptedPaletteIndex = m_startAccepted; m_page.m_paletteChanged = m_startChanged;
        m_page.m_interfacePalette.ResetContent();
        for(const CString& label : m_startLabels) m_page.m_interfacePalette.AddString(label);
        m_page.m_interfacePalette.SetCurSel(m_startAccepted);
        m_page.UpdatePaletteColorControls();
        m_page.GetDlgItem(IDC_APPEARANCE_ICON_PREVIEW).Invalidate();
        Refresh();
        return 0;
    }
    LRESULT OnRoleChanged(WORD, WORD, HWND, BOOL&) { const int role = CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel(); m_page.m_paletteRole.SetCurSel(role); ::SendMessageW(m_roleList, LB_SETCURSEL, role, 0); RefreshColorInfo(); if(m_preview) ::InvalidateRect(m_preview, NULL, FALSE); return 0; }
    LRESULT OnPreviewSelect(UINT, WPARAM role, LPARAM, BOOL&)
    {
        if(role < THEME_COLOR_COUNT) {
            CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).SetCurSel(static_cast<int>(role));
            m_page.m_paletteRole.SetCurSel(static_cast<int>(role));
            ::SendMessageW(m_roleList, LB_SETCURSEL, role, 0);
            RefreshColorInfo();
            if(m_preview) ::InvalidateRect(m_preview, NULL, FALSE);
        }
        return 0;
    }
    LRESULT OnAction(WORD, WORD id, HWND control, BOOL&)
    {
        COLORREF before[THEME_COLOR_COUNT] = {}; PreviewColors(before);
        const int role = m_page.m_paletteRole.GetCurSel();
        BOOL handled = FALSE;
        switch(id) {
        case IDC_PALETTE_IMPORT: m_page.OnImportPalette(BN_CLICKED, id, control, handled); break;
        case IDC_PALETTE_EXPORT: m_page.OnExportPalette(BN_CLICKED, id, control, handled); break;
        case IDC_PALETTE_RENAME: m_page.OnRenamePalette(BN_CLICKED, id, control, handled); break;
        case IDC_PALETTE_DELETE: m_page.OnDeletePalette(BN_CLICKED, id, control, handled); break;
        case IDC_PALETTE_CHOOSE_COLOR: m_page.OnChoosePaletteColor(BN_CLICKED, id, control, handled); break;
        case IDC_PALETTE_RESET_COLOR: m_page.OnResetPaletteColor(BN_CLICKED, id, control, handled); break;
        }
        if(id == IDC_PALETTE_CHOOSE_COLOR && role >= 0 && role < THEME_COLOR_COUNT) {
            COLORREF after[THEME_COLOR_COUNT] = {}; PreviewColors(after);
            if(after[role] != before[role]) RecordRecent(after[role]);
        }
        Refresh(); return 0;
    }
    bool VerifySimulatedDpiLayout()
    {
        RECT original = {}; ::GetWindowRect(m_hWnd, &original);
        const HFONT dialogFont = reinterpret_cast<HFONT>(::SendMessageW(m_hWnd, WM_GETFONT, 0, 0));
        std::vector<std::pair<HWND, HFONT>> fonts;
        for(HWND child = ::GetWindow(m_hWnd, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
            fonts.emplace_back(child, reinterpret_cast<HFONT>(::SendMessageW(child, WM_GETFONT, 0, 0)));
        bool valid = true;
        UINT failedDpi = 0;
        for(UINT dpi : {96U, 120U, 144U, 192U}) {
            HFONT font = UiMetrics::CreateDialogFontForDpi(dpi);
            if(!font) { valid = false; failedDpi = dpi; break; }
            ::SendMessageW(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
            for(const auto& item : fonts) ::SendMessageW(item.first, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
            m_testLayoutDpi = dpi; FitLocalizedLayout(true);
            RECT frame = {}, client = {}; ::GetWindowRect(m_hWnd, &frame); ::GetClientRect(m_hWnd, &client);
            bool fits = frame.right - frame.left <= 1366 && frame.bottom - frame.top <= 728;
            const int previousRole = CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel();
            for(int role = 0; role < THEME_COLOR_COUNT; ++role) {
                ::SendMessageW(m_hWnd, kPreviewSelect, role, 0);
                wchar_t description[256] = {}; ::GetWindowTextW(m_roleDescription, description, _countof(description));
                RECT area = {}; ::GetClientRect(m_roleDescription, &area);
                HDC descriptionDc = ::GetDC(m_roleDescription);
                HGDIOBJ oldDescriptionFont = descriptionDc ? ::SelectObject(descriptionDc, font) : NULL;
                SIZE measured = {}; const bool measuredOk = descriptionDc && description[0] &&
                    ::GetTextExtentPoint32W(descriptionDc, description, static_cast<int>(wcslen(description)), &measured);
                if(oldDescriptionFont) ::SelectObject(descriptionDc, oldDescriptionFont);
                if(descriptionDc) ::ReleaseDC(m_roleDescription, descriptionDc);
                fits = measuredOk && measured.cx <= area.right && measured.cy <= area.bottom && fits;
            }
            ::SendMessageW(m_hWnd, kPreviewSelect, previousRole, 0);
            ::SetPropW(m_page.m_hWnd, L"FBE.PaletteDpiWidth", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(frame.right - frame.left)));
            ::SetPropW(m_page.m_hWnd, L"FBE.PaletteDpiHeight", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(frame.bottom - frame.top)));
            ::RemovePropW(m_page.m_hWnd, L"FBE.PaletteDpiControl");
            for(const auto& item : fonts) if(::IsWindowVisible(item.first)) {
                RECT bounds = {}; ::GetWindowRect(item.first, &bounds);
                ::MapWindowPoints(NULL, m_hWnd, reinterpret_cast<POINT*>(&bounds), 2);
                if(bounds.left < 0 || bounds.top < 0 || bounds.right > client.right || bounds.bottom > client.bottom)
                    ::SetPropW(m_page.m_hWnd, L"FBE.PaletteDpiControl", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(::GetDlgCtrlID(item.first))));
                fits = bounds.left >= 0 && bounds.top >= 0 && bounds.right <= client.right && bounds.bottom <= client.bottom && fits;
                wchar_t kind[32] = {}; ::GetClassNameW(item.first, kind, _countof(kind));
                const bool button = _wcsicmp(kind, L"Button") == 0, label = _wcsicmp(kind, L"Static") == 0;
                const int length = ::GetWindowTextLengthW(item.first);
                if((button || label) && length > 0) {
                    CString text; ::GetWindowTextW(item.first, text.GetBuffer(length + 1), length + 1); text.ReleaseBuffer();
                    HDC dc = ::GetDC(item.first); HGDIOBJ oldFont = dc ? ::SelectObject(dc, font) : NULL;
                    RECT measured = {0, 0, max(1, static_cast<int>(bounds.right - bounds.left)), 0};
                    const bool wrap = ::GetDlgCtrlID(item.first) == IDC_PALETTE_CONTRAST;
                    if(dc) ::DrawTextW(dc, text, text.GetLength(), &measured, DT_CALCRECT | (wrap ? DT_WORDBREAK : DT_SINGLELINE));
                    if(oldFont) ::SelectObject(dc, oldFont); if(dc) ::ReleaseDC(item.first, dc);
                    const int inset = button ? UiMetrics::ScaleForDpi(12, dpi) : 0;
                    const bool textFits = measured.right + inset <= bounds.right - bounds.left && measured.bottom <= bounds.bottom - bounds.top;
                    if(!textFits) {
                        ::SetPropW(m_page.m_hWnd, L"FBE.PaletteDpiControl", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(::GetDlgCtrlID(item.first))));
                        ::SetPropW(m_page.m_hWnd, L"FBE.PaletteDpiTextWidth", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(measured.right + inset)));
                        ::SetPropW(m_page.m_hWnd, L"FBE.PaletteDpiTextHeight", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(measured.bottom)));
                        ::SetPropW(m_page.m_hWnd, L"FBE.PaletteDpiBoxWidth", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(bounds.right - bounds.left)));
                        ::SetPropW(m_page.m_hWnd, L"FBE.PaletteDpiBoxHeight", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(bounds.bottom - bounds.top)));
                    }
                    fits = textFits && fits;
                }
            }
            ::SendMessageW(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(dialogFont), FALSE);
            for(const auto& item : fonts) ::SendMessageW(item.first, WM_SETFONT, reinterpret_cast<WPARAM>(item.second), FALSE);
            ::DeleteObject(font);
            if(!fits) { valid = false; failedDpi = dpi; break; }
        }
        m_testLayoutDpi = 0; m_minTrack = {};
        ::SetWindowPos(m_hWnd, NULL, original.left, original.top, original.right - original.left, original.bottom - original.top, SWP_NOZORDER | SWP_NOACTIVATE);
        FitLocalizedLayout(true);
        ::SetPropW(m_page.m_hWnd, L"FBE.PaletteDpiFailure", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(failedDpi)));
        return valid;
    }
    bool CapturePaletteExamples(const CString& directory)
    {
        const int index = m_page.m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT;
        if(index < 0 || index >= static_cast<int>(m_page.m_customPalettes.size())) return false;
        const auto originalPalettes = m_page.m_customPalettes, originalPending = m_page.m_pendingPalettes;
        const bool originalChanged = m_page.m_paletteChanged;
        const int originalThemeIndex = m_page.m_acceptedThemeIndex;
        const InterfaceTheme activeTheme = ThemeManager::GetSelectedTheme();
        const InterfacePalette activePalette = ThemeManager::GetNamedPalette();
        ThemeManager::CustomPalette activeCustom;
        const bool hadCustom = ThemeManager::GetCustomPalette(activeCustom);
        auto capture = [&](LPCWSTR name) {
            ::RedrawWindow(m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
            RECT frame = {}; ::GetWindowRect(m_hWnd, &frame); CImage image;
            if(!image.Create(frame.right - frame.left, frame.bottom - frame.top, 32)) return false;
            HDC dc = image.GetDC(); const bool painted = dc && ::PrintWindow(m_hWnd, dc, 0);
            if(dc) image.ReleaseDC();
            return painted && SUCCEEDED(image.Save(directory + L"\\" + name));
        };
        auto selectRole = [&](ThemeColorRole role) { ::SendMessageW(m_hWnd, kPreviewSelect, role, 0); };
        ThemeManager::CustomPalette draft = originalPalettes[index];
        bool ok = ThemeManager::CopyNamedPaletteColors(INTERFACE_PALETTE_CATPPUCCIN_MOCHA, draft);
        draft.name = L"Catppuccin Mocha";
        m_page.m_customPalettes[index] = draft; m_page.m_acceptedThemeIndex = INTERFACE_THEME_DARK;
        ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK); ok = ThemeManager::SetCustomPalette(draft) && ok;
        ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId()); Refresh();
        SetDlgItemText(IDC_PALETTE_CURRENT, L"Catppuccin Mocha");
        selectRole(THEME_COLOR_CONTROL); ok = capture(L"mocha-control.bmp") && ok;
        selectRole(THEME_COLOR_TEXT); ok = capture(L"mocha-text.bmp") && ok;
        selectRole(THEME_COLOR_SELECTION_BACKGROUND); ok = capture(L"mocha-selection.bmp") && ok;
        draft = originalPalettes[index]; draft.name = L"Sepia"; draft.base = ThemeManager::CustomPaletteBase::Sepia;
        for(int role = 0; role < THEME_COLOR_COUNT; ++role) draft.present[role] = false;
        m_page.m_customPalettes[index] = draft; m_page.m_acceptedThemeIndex = INTERFACE_THEME_LIGHT;
        ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT); ok = ThemeManager::SetCustomPalette(draft) && ok;
        ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId()); Refresh(); SetDlgItemText(IDC_PALETTE_CURRENT, L"Sepia");
        selectRole(THEME_COLOR_CONTROL); ::SetWindowTextW(m_hex, L"#D8C9AF");
        ok = capture(L"sepia-custom.bmp") && ok;
        ::SetWindowTextW(m_hex, L"#A0C8D0"); ok = capture(L"sepia-hex.bmp") && ok;
        BOOL handled = FALSE; OnAction(BN_CLICKED, IDC_PALETTE_RESET_COLOR, GetDlgItem(IDC_PALETTE_RESET_COLOR), handled);
        SetDlgItemText(IDC_PALETTE_CURRENT, L"Sepia");
        ok = capture(L"sepia-reset.bmp") && ok;
        m_page.m_customPalettes = originalPalettes; m_page.m_pendingPalettes = originalPending;
        m_page.m_paletteChanged = originalChanged; m_page.m_acceptedThemeIndex = originalThemeIndex;
        ThemeManager::SetSelectedTheme(activeTheme);
        if(hadCustom) ThemeManager::SetCustomPalette(activeCustom); else ThemeManager::SetNamedPalette(activePalette);
        ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId()); Refresh();
        return ok;
    }
    LRESULT OnTimer(UINT, WPARAM id, LPARAM, BOOL&)
    {
        if(id != 0x504D) return 0;
        KillTimer(id);
        const ULONG_PTR mode = reinterpret_cast<ULONG_PTR>(::GetPropW(m_page.m_hWnd, L"FBE.PaletteManagerTest"));
        const bool editable = ::IsWindowEnabled(GetDlgItem(IDC_PALETTE_CHOOSE_COLOR)) != FALSE;
        if(mode == 3) {
            const COLORREF globalBefore = ThemeManager::Color(THEME_COLOR_CONTROL);
            ::SendMessageW(m_hWnd, kPreviewSelect, THEME_COLOR_CONTROL, 0);
            ::SetWindowTextW(m_hex, L"#426891");
            COLORREF colors[THEME_COLOR_COUNT] = {}; PreviewColors(colors);
            const bool changed = editable && colors[THEME_COLOR_CONTROL] == RGB(0x42,0x68,0x91) &&
                ThemeManager::Color(THEME_COLOR_CONTROL) == globalBefore;
            ::SetPropW(m_page.m_hWnd, L"FBE.PaletteManagerResult", reinterpret_cast<HANDLE>(changed ? 1 : 2));
            PostMessage(WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(IDOK).m_hWnd));
            return 0;
        }
        bool passed = mode == 1 && !editable;
        if(mode == 1 && !editable) {
            RECT field = {}; ::GetWindowRect(m_hex, &field);
            POINT center = {(field.left + field.right) / 2, (field.top + field.bottom) / 2};
            ::ScreenToClient(m_hWnd, &center);
            passed = !::IsWindowEnabled(m_hex) && m_tooltips.TextAtOwnerPoint(center) ==
                FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.general.palette_builtin", L"Create a copy of the built-in scheme before editing it.") && passed;
        }
        if(mode == 2 && editable) {
            const CString hexTip = m_tooltips.RegisteredText(m_hex);
            const CString recentTip = m_tooltips.RegisteredText(m_recent);
            const bool tooltipTexts = hexTip == FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.palette.hex", L"Enter a colour as #RRGGBB.") &&
                recentTip == FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.palette.recent", L"Choose a recently used colour.") &&
                hexTip.Find(L"#RRGGBB") >= 0 && hexTip != recentTip;
            const int index = m_page.m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT;
            COLORREF before[THEME_COLOR_COUNT] = {}, after[THEME_COLOR_COUNT] = {};
            ThemeManager::PreviewColors(static_cast<InterfaceTheme>(m_page.m_acceptedThemeIndex), INTERFACE_PALETTE_CLASSIC, &m_page.m_customPalettes[index], before);
            ::SendMessageW(m_hWnd, WM_COMMAND, MAKEWPARAM(IDC_PALETTE_RESET_COLOR, BN_CLICKED),
                reinterpret_cast<LPARAM>(GetDlgItem(IDC_PALETTE_RESET_COLOR).m_hWnd));
            ThemeManager::PreviewColors(static_cast<InterfaceTheme>(m_page.m_acceptedThemeIndex), INTERFACE_PALETTE_CLASSIC, &m_page.m_customPalettes[index], after);
            passed = tooltipTexts && after[THEME_COLOR_WINDOW] != before[THEME_COLOR_WINDOW];
        }
        if(m_preview) {
            RECT bounds = {}; ::GetClientRect(m_preview, &bounds);
            const auto parts = PreviewParts(bounds.right, bounds.bottom);
            POINT hits[THEME_COLOR_COUNT] = {};
            bool found[THEME_COLOR_COUNT] = {};
            int remaining = THEME_COLOR_COUNT;
            for(int y = 0; y < bounds.bottom && remaining; ++y) for(int x = 0; x < bounds.right && remaining; ++x) {
                POINT point = {x, y};
                for(auto part = parts.rbegin(); part != parts.rend(); ++part) if(::PtInRect(&part->rect, point)) {
                    if(!found[part->role]) { found[part->role] = true; hits[part->role] = point; --remaining; }
                    break;
                }
            }
            const int originalRole = CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel();
            COLORREF beforeSelection[THEME_COLOR_COUNT] = {}; PreviewColors(beforeSelection);
            ::RedrawWindow(m_preview, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
            for(const auto& target : {std::make_pair(321, THEME_COLOR_BORDER), std::make_pair(350, THEME_COLOR_TEXT),
                std::make_pair(580, THEME_COLOR_PRESSED)}) {
                const POINT point = {MulDiv(bounds.right, target.first, 1000), MulDiv(bounds.bottom, 365, 600)};
                ::SendMessageW(m_preview, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(point.x, point.y));
                passed = CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel() == target.second && passed;
            }
            for(int role = 0; role < THEME_COLOR_COUNT; ++role) {
                if(!found[role]) { passed = false; continue; }
                ::SendMessageW(m_preview, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(hits[role].x, hits[role].y));
                passed = CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel() == role &&
                    m_page.m_paletteRole.GetCurSel() == role && passed;
            }
            COLORREF afterSelection[THEME_COLOR_COUNT] = {}; PreviewColors(afterSelection);
            for(int role = 0; role < THEME_COLOR_COUNT; ++role) passed = beforeSelection[role] == afterSelection[role] && passed;
            CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).SetCurSel(originalRole);
            m_page.m_paletteRole.SetCurSel(originalRole);
            if(mode == 2 && editable) {
                ::SendMessageW(m_hWnd, kPreviewSelect, THEME_COLOR_TEXT, 0);
                wchar_t textDescription[256] = {}; ::GetWindowTextW(m_roleDescription, textDescription, _countof(textDescription));
                ::RedrawWindow(m_preview, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
                HDC markerDc = ::GetDC(m_preview);
                const int markerX = MulDiv(bounds.right, 320, 1000) - 2;
                const int markerY = MulDiv(bounds.bottom, 320, 600);
                passed = markerDc && ::GetPixel(markerDc, markerX, markerY) == RGB(255,255,255) && passed;
                if(markerDc) ::ReleaseDC(m_preview, markerDc);
                ::SendMessageW(m_hWnd, kPreviewSelect, THEME_COLOR_CONTROL, 0);
                wchar_t controlDescription[256] = {}; ::GetWindowTextW(m_roleDescription, controlDescription, _countof(controlDescription));
                passed = textDescription[0] && controlDescription[0] && wcscmp(textDescription, controlDescription) != 0 && passed;
                ::SetWindowTextW(m_hex, L"#123456");
                COLORREF colors[THEME_COLOR_COUNT] = {}; PreviewColors(colors);
                ::RedrawWindow(m_preview, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
                HDC dc = ::GetDC(m_preview);
                const COLORREF pixel = dc ? ::GetPixel(dc, MulDiv(bounds.right, 900, 1000), MulDiv(bounds.bottom, 145, 600)) : CLR_INVALID;
                bool captionButtons = dc != NULL;
                for(int index = 0; index < 3 && dc; ++index) {
                    const RECT button = PreviewCaptionButton(bounds.right, bounds.bottom, index);
                    const int x = (button.left + button.right) / 2, y = (button.top + button.bottom) / 2;
                    const int radius = max(2, min(button.right - button.left, button.bottom - button.top) / 5);
                    const int sampleY = index == 0 ? y + radius / 2 : index == 1 ? y - radius : y;
                    captionButtons = ::GetPixel(dc, x, sampleY) == colors[THEME_COLOR_TEXT] && captionButtons;
                }
                bool scriptToolbar = dc && m_previewScriptIcons.size() >= 3 &&
                    ::GetPixel(dc, MulDiv(bounds.right, 900, 1000), MulDiv(bounds.bottom, 210, 600)) == colors[THEME_COLOR_CONTROL];
                if(scriptToolbar) {
                    const int size = UiMetrics::ScaleForDpi(16, UiMetrics::DpiForWindow(m_preview));
                    const int gap = max(2, size / 5), top = MulDiv(bounds.bottom, 188, 600), bottom = MulDiv(bounds.bottom, 238, 600);
                    const int iconTop = top + (bottom - top - size) / 2;
                    for(int icon = 0; icon < 3; ++icon) {
                        bool visible = false;
                        for(int y = max(top + 2, iconTop); y < min(bottom - 2, iconTop + size); ++y)
                            for(int x = gap * 2 + icon * (size + gap); x < gap * 2 + icon * (size + gap) + size; ++x)
                                if(::GetPixel(dc, x, y) != colors[THEME_COLOR_CONTROL]) visible = true;
                        scriptToolbar = visible && scriptToolbar;
                    }
                }
                if(dc) ::ReleaseDC(m_preview, dc);
                passed = colors[THEME_COLOR_CONTROL] == RGB(0x12, 0x34, 0x56) && pixel == colors[THEME_COLOR_CONTROL] && captionButtons && scriptToolbar && passed;
                ::SendMessageW(m_hWnd, kPreviewSelect, THEME_COLOR_ACCENT, 0);
                ::SendMessageW(m_recent, CB_SETCURSEL, 0, 0);
                BOOL recentHandled = FALSE; OnRecentChanged(CBN_SELCHANGE, kRecentId, m_recent, recentHandled);
                PreviewColors(colors);
                passed = ::SendMessageW(m_recent, CB_GETCOUNT, 0, 0) > 0 && colors[THEME_COLOR_ACCENT] == RGB(0x12, 0x34, 0x56) && passed;
                ::SetWindowTextW(m_hex, L"#XYZ123");
                PreviewColors(colors);
                passed = colors[THEME_COLOR_ACCENT] == RGB(0x12, 0x34, 0x56) && passed;
                CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).SetCurSel(originalRole);
                m_page.m_paletteRole.SetCurSel(originalRole);
            }
            for(int previewMode = 1; previewMode <= 2; ++previewMode) {
                ::SendMessageW(m_previewMode, CB_SETCURSEL, previewMode, 0);
                const int expected = previewMode == 1 ? THEME_COLOR_HOVER : THEME_COLOR_ERROR;
                POINT sample = {MulDiv(bounds.right, 500, 1000), MulDiv(bounds.bottom, 160, 600)};
                ::SendMessageW(m_preview, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(sample.x, sample.y));
                passed = CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel() == expected && passed;
            }
            ::SendMessageW(m_previewMode, CB_SETCURSEL, 3, 0);
            BOOL handled = FALSE; OnPreviewModeChanged(CBN_SELCHANGE, kPreviewModeId, m_previewMode, handled);
            auto tabOrderValid = [&](bool expectRoleList) {
                std::vector<HWND> forward, backward;
                auto collect = [&](bool reverse, std::vector<HWND>& order) {
                    HWND current = m_previewMode;
                    for(int step = 0; step < 64; ++step) {
                        current = ::GetNextDlgTabItem(m_hWnd, current, reverse ? TRUE : FALSE);
                        if(!current) return false;
                        if(current == m_previewMode) return true;
                        if(std::find(order.begin(), order.end(), current) != order.end()) return false;
                        order.push_back(current);
                    }
                    return false;
                };
                if(!collect(false, forward) || !collect(true, backward) || forward.size() != backward.size() ||
                    !std::equal(forward.begin(), forward.end(), backward.rbegin())) return false;
                for(HWND required : {GetDlgItem(IDOK).m_hWnd, GetDlgItem(IDCANCEL).m_hWnd, m_hex, m_resetChanges})
                    if(::IsWindowVisible(required) && ::IsWindowEnabled(required) && std::find(forward.begin(), forward.end(), required) == forward.end()) return false;
                return (std::find(forward.begin(), forward.end(), m_roleList) != forward.end()) == expectRoleList;
            };
            passed = tabOrderValid(true) && passed;
            const HWND previousFocus = ::GetFocus();
            ::SetFocus(m_previewMode);
            MSG tab = {}; tab.hwnd = m_previewMode; tab.message = WM_KEYDOWN; tab.wParam = VK_TAB;
            const bool tabHandled = ::IsDialogMessageW(m_hWnd, &tab) != FALSE;
            passed = tabHandled && ::GetFocus() == m_roleList && passed;
            ::SendMessageW(m_roleList, LB_SETCURSEL, THEME_COLOR_WARNING - 1, 0);
            ::SendMessageW(m_roleList, WM_KEYDOWN, VK_DOWN, 0);
            ::SendMessageW(m_roleList, WM_KEYUP, VK_DOWN, 0);
            passed = ::IsWindowVisible(m_roleList) && !::IsWindowVisible(m_preview) &&
                ::SendMessageW(m_roleList, LB_GETCURSEL, 0, 0) == THEME_COLOR_WARNING &&
                CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel() == THEME_COLOR_WARNING && passed;
            COLORREF keyboardBefore[THEME_COLOR_COUNT] = {}, keyboardAfter[THEME_COLOR_COUNT] = {};
            PreviewColors(keyboardBefore);
            for(int role = 0; role < THEME_COLOR_COUNT; ++role) {
                const bool first = role == 0;
                ::SendMessageW(m_roleList, LB_SETCURSEL, first ? 1 : role - 1, 0);
                const WPARAM key = first ? VK_UP : VK_DOWN;
                ::SendMessageW(m_roleList, WM_KEYDOWN, key, 0);
                ::SendMessageW(m_roleList, WM_KEYUP, key, 0);
                passed = ::SendMessageW(m_roleList, LB_GETCURSEL, 0, 0) == role &&
                    CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).GetCurSel() == role && passed;
            }
            PreviewColors(keyboardAfter);
            for(int role = 0; role < THEME_COLOR_COUNT; ++role)
                passed = keyboardBefore[role] == keyboardAfter[role] && passed;
            ::SendMessageW(m_previewMode, CB_SETCURSEL, 0, 0);
            OnPreviewModeChanged(CBN_SELCHANGE, kPreviewModeId, m_previewMode, handled);
            passed = !::IsWindowVisible(m_roleList) && ::GetNextDlgTabItem(m_hWnd, m_previewMode, FALSE) != m_roleList && tabOrderValid(false) && passed;
            if(previousFocus && ::IsWindow(previousFocus)) ::SetFocus(previousFocus);
            CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).SetCurSel(originalRole);
            m_page.m_paletteRole.SetCurSel(originalRole);
            RECT window = {}, oldPreview = {}, newPreview = {};
            MINMAXINFO limits = {}; ::SendMessageW(m_hWnd, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&limits));
            passed = limits.ptMinTrackSize.x == m_minTrack.cx && limits.ptMinTrackSize.y == m_minTrack.cy &&
                m_minTrack.cx > 0 && m_minTrack.cy > 0 && passed;
            ::GetWindowRect(m_hWnd, &window); ::GetWindowRect(m_preview, &oldPreview);
            ::SetWindowPos(m_hWnd, NULL, 0, 0, window.right - window.left + 80, window.bottom - window.top + 60,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            ::GetWindowRect(m_preview, &newPreview);
            passed = newPreview.right - newPreview.left > oldPreview.right - oldPreview.left &&
                newPreview.bottom - newPreview.top > oldPreview.bottom - oldPreview.top && passed;
            ::SetWindowPos(m_hWnd, NULL, 0, 0, window.right - window.left, window.bottom - window.top,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            const DWORD gdiBefore = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
            for(int iteration = 0; iteration < 40; ++iteration) {
                ::SendMessageW(m_hWnd, kPreviewSelect, iteration % THEME_COLOR_COUNT, 0);
                ::RedrawWindow(m_preview, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
            }
            const DWORD gdiAfter = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
            passed = gdiAfter <= gdiBefore + 2 && passed;
            CComboBox(GetDlgItem(IDC_PALETTE_ROLE)).SetCurSel(originalRole);
            m_page.m_paletteRole.SetCurSel(originalRole);
            RefreshColorInfo();
            if(mode == 2 && editable) {
                const int originalIndex = m_startAccepted - INTERFACE_PALETTE_COUNT;
                COLORREF expected[THEME_COLOR_COUNT] = {}, actual[THEME_COLOR_COUNT] = {};
                if(originalIndex < 0 || originalIndex >= static_cast<int>(m_startCustom.size())) passed = false;
                else {
                    ThemeManager::PreviewColors(static_cast<InterfaceTheme>(m_page.m_acceptedThemeIndex), INTERFACE_PALETTE_CLASSIC, &m_startCustom[originalIndex], expected);
                    BOOL resetHandled = FALSE; OnResetChanges(BN_CLICKED, kResetChangesId, m_resetChanges, resetHandled);
                    PreviewColors(actual);
                    for(int role = 0; role < THEME_COLOR_COUNT; ++role) passed = expected[role] == actual[role] && passed;
                }
            }
            if(mode == 2) {
                const DWORD beforeDpi = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
                const DWORD beforeDpiUser = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
                passed = VerifySimulatedDpiLayout() && passed;
                RECT suggested = {}; ::GetWindowRect(m_hWnd, &suggested);
                for(int iteration = 0; iteration < 30; ++iteration) {
                    const UINT dpi = iteration % 2 ? 96 : 120;
                    m_testLayoutDpi = dpi;
                    ::SendMessageW(m_hWnd, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), reinterpret_cast<LPARAM>(&suggested));
                    passed = m_scaledFont && reinterpret_cast<HFONT>(::SendMessageW(m_resetChanges, WM_GETFONT, 0, 0)) == m_scaledFont &&
                        ::SendMessageW(m_roleList, LB_GETITEMHEIGHT, 0, 0) == UiMetrics::ScaleForDpi(26, dpi) && passed;
                }
                m_testLayoutDpi = 0;
                const UINT actualDpi = UiMetrics::DpiForWindow(m_hWnd);
                ::SendMessageW(m_hWnd, WM_DPICHANGED, MAKEWPARAM(actualDpi, actualDpi), reinterpret_cast<LPARAM>(&suggested));
                const DWORD afterDpi = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
                const DWORD afterDpiUser = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
                ::SetPropW(m_page.m_hWnd, L"FBE.PaletteGdiBefore", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(beforeDpi)));
                ::SetPropW(m_page.m_hWnd, L"FBE.PaletteGdiAfter", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(afterDpi)));
                ::SetPropW(m_page.m_hWnd, L"FBE.PaletteUserBefore", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(beforeDpiUser)));
                ::SetPropW(m_page.m_hWnd, L"FBE.PaletteUserAfter", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(afterDpiUser)));
                passed = afterDpi <= beforeDpi + 2 && afterDpiUser <= beforeDpiUser + 2 && passed;
            }
            wchar_t capturePath[MAX_PATH] = {};
            const DWORD captureLength = ::GetEnvironmentVariableW(L"FBE_NEXT_PALETTE_PREVIEW_CAPTURE", capturePath, _countof(capturePath));
            if(mode == 2 && captureLength > 0 && captureLength < _countof(capturePath)) {
                RECT frame = {}; ::GetWindowRect(m_hWnd, &frame);
                CImage image;
                if(image.Create(frame.right - frame.left, frame.bottom - frame.top, 32)) {
                    HDC dc = image.GetDC();
                    const bool captured = dc && ::PrintWindow(m_hWnd, dc, 0);
                    if(dc) image.ReleaseDC();
                    if(captured) image.Save(capturePath);
                }
            }
            wchar_t artifactDirectory[MAX_PATH] = {};
            const DWORD artifactLength = ::GetEnvironmentVariableW(L"FBE_NEXT_PALETTE_EXAMPLES_DIR", artifactDirectory, _countof(artifactDirectory));
            if(mode == 2 && artifactLength > 0 && artifactLength < _countof(artifactDirectory))
                passed = CapturePaletteExamples(artifactDirectory) && passed;
        } else passed = false;
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
	m_tooltips.Add(GetDlgItem(IDC_PALETTE_CONTRAST), L"fbe.settings.tooltip.general.palette_contrast", L"Shows the lowest contrast of normal text against the window, control, selection, hover and pressed backgrounds.");
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
	auto contrast = [&](ThemeColorRole foreground, ThemeColorRole background) {
		const double a = luminance(colors[foreground]), b = luminance(colors[background]);
		return ((std::max)(a, b) + 0.05) / ((std::min)(a, b) + 0.05);
	};
	const double minimum = (std::min)({contrast(THEME_COLOR_TEXT, THEME_COLOR_WINDOW), contrast(THEME_COLOR_TEXT, THEME_COLOR_CONTROL),
		contrast(THEME_COLOR_SELECTION_TEXT, THEME_COLOR_SELECTION_BACKGROUND), contrast(THEME_COLOR_TEXT, THEME_COLOR_HOVER),
		contrast(THEME_COLOR_TEXT, THEME_COLOR_PRESSED)});
	CString message = minimum >= 4.5 ?
		FbeLoadRuntimeStringByKey(L"fbe.settings.palette.contrast_ok", L"Text contrast check passed.") :
		FbeLoadRuntimeStringByKey(L"fbe.settings.palette.contrast_low", L"Warning: some text may be difficult to read against its background.");
	CString ratio; ratio.Format(L" (%.2f:1)", minimum); message += ratio;
	SetDlgItemText(IDC_PALETTE_CONTRAST, message);
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
