#pragma once

#include "FontPreviewCoverage.h"
#include "WindowDpi.h"
#include <algorithm>
#include <array>
#include <list>
#include <limits>
#include <memory>
#include <new>
#include <string>

// Visual layer only. The dialog still owns its strings, selection and settings.
class FontPreviewCombo final
{
public:
    // Use with CBS_OWNERDRAWFIXED | CBS_HASSTRINGS. An editable combo retains
    // the interface font in its native edit window.
    static void Attach(HWND combo)
    {
        const HWND parent = ::GetParent(combo);
        if (!parent) return;
        std::unique_ptr<FontPreviewCombo> preview(new (std::nothrow) FontPreviewCombo(combo, parent));
        if (!preview) return;
        const auto id = reinterpret_cast<UINT_PTR>(preview.get());
        const auto data = reinterpret_cast<DWORD_PTR>(preview.get());
        if (!::SetWindowSubclass(parent, ParentProc, id, data)) return;
        if (!::SetWindowSubclass(combo, ComboProc, id, data)) {
            ::RemoveWindowSubclass(parent, ParentProc, id);
            return;
        }
        ::SetPropW(combo, FontPreviewCoverage::ControlProperty, reinterpret_cast<HANDLE>(1));
        preview.release()->UpdateMetrics();
    }


private:
    // The plug-ins target Windows 7 headers, which omit this newer message.
    static constexpr UINT DpiChangedAfterParent = 0x02E3;

    struct FittedFont
    {
        HFONT font = nullptr;
        int width = -1;
        int height = -1;
        int textWidth = 0;
    };

    struct Face
    {
        std::wstring name;
        HFONT font = nullptr;
        FittedFont rowName, closedName, sample;
        bool readableName = false;
        FontPreviewCoverage::Support coverage = FontPreviewCoverage::Support::None;
    };

    HWND m_combo;
    HWND m_parent;
    std::wstring m_hintText;
    bool m_hintPartial = false;
    HWND m_popup = nullptr;
    HWND m_tooltip = nullptr;
    int m_hintRow = -1;
    LOGFONTW m_previewFont = {};
    int m_padding = 3;
    int m_sampleWidth = 0;
    UINT m_dpi = 0;
    HMONITOR m_monitor = nullptr;
    unsigned m_coverageGeneration = 0;
    bool m_widthDirty = true;
    // Recently visible faces, including negative results for symbol fonts.
    std::list<Face> m_faces;

    FontPreviewCombo(HWND combo, HWND parent) : m_combo(combo), m_parent(parent) {}
    ~FontPreviewCombo() { ClearFaces(); }
    friend struct std::default_delete<FontPreviewCombo>;

    HFONT InterfaceFont() const
    {
        const auto font = reinterpret_cast<HFONT>(::SendMessageW(m_combo, WM_GETFONT, 0, 0));
        return font ? font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
    }

    void ClearFaces()
    {
        for (const Face& face : m_faces) {
            DeleteFonts(face);
        }
        m_faces.clear();
    }

    static void DeleteFonts(const Face& face)
    {
        for (HFONT font : {face.rowName.font, face.closedName.font, face.sample.font}) {
            if (font && font != face.font) ::DeleteObject(font);
        }
        if (face.font) ::DeleteObject(face.font);
    }

    void UpdateMetrics()
    {
        const HWND combo = m_combo;
        HDC dc = ::GetDC(combo);
        if (!dc) return;
        const HFONT font = InterfaceFont();
        const HGDIOBJ old = ::SelectObject(dc, font);
        TEXTMETRICW metrics = {};
        const BOOL measured = ::GetTextMetricsW(dc, &metrics);
        SIZE sample = {};
        ::GetTextExtentPoint32W(dc, FontPreviewCoverage::Sample, _countof(FontPreviewCoverage::Sample) - 1, &sample);
        ::SelectObject(dc, old);
        ::ReleaseDC(combo, dc);
        if (!measured) return;

        HideHint();
        ClearFaces();
        m_widthDirty = true;
        m_previewFont = {};
        ::GetObjectW(font, sizeof(m_previewFont), &m_previewFont);
        const int em = (std::max)(1L, metrics.tmHeight - metrics.tmInternalLeading);
        m_previewFont.lfHeight = -em;
        m_previewFont.lfWidth = 0;
        m_previewFont.lfWeight = FW_NORMAL;
        m_previewFont.lfItalic = FALSE;
        m_previewFont.lfUnderline = FALSE;
        m_previewFont.lfStrikeOut = FALSE;
        m_previewFont.lfCharSet = DEFAULT_CHARSET;
        m_previewFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        m_dpi = FbeWin32::WindowDpi(combo);
        m_padding = (std::max)(3, ::MulDiv(3, m_dpi, 96));
        m_sampleWidth = sample.cx + sample.cx * 2 / 5 + 4 * m_padding;
        ::SendMessageW(combo, CB_SETITEMHEIGHT, 0, (std::max)(2 * em, static_cast<int>(metrics.tmHeight)) + m_padding);
        // Keep the native edit field and surrounding dialog layout compact.
        ::SendMessageW(combo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), metrics.tmHeight);
    }

    std::wstring ItemName(WPARAM index) const
    {
        const LRESULT length = ::SendMessageW(m_combo, CB_GETLBTEXTLEN, index, 0);
        if (length <= 0) return {};
        std::wstring text(static_cast<size_t>(length) + 1, L'\0');
        if (::SendMessageW(m_combo, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(text.data())) == CB_ERR)
            return {};
        text.resize(static_cast<size_t>(length));
        return text;
    }

    void UpdateDropWidth()
    {
        const HMONITOR monitorHandle = ::MonitorFromWindow(m_combo, MONITOR_DEFAULTTONEAREST);
        if (!m_widthDirty && m_monitor == monitorHandle) return;
        HDC dc = ::GetDC(m_combo);
        if (!dc) return;
        const HGDIOBJ old = ::SelectObject(dc, InterfaceFont());
        const LRESULT count = ::SendMessageW(m_combo, CB_GETCOUNT, 0, 0);
        int longest = 0;
        for (LRESULT index = 0; index < count; ++index) {
            const auto text = ItemName(static_cast<WPARAM>(index));
            SIZE size = {};
            ::GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
            longest = (std::max)(longest, static_cast<int>(size.cx));
        }
        ::SelectObject(dc, old);
        ::ReleaseDC(m_combo, dc);

        RECT combo = {};
        ::GetWindowRect(m_combo, &combo);
        MONITORINFO monitor = {};
        monitor.cbSize = sizeof(monitor);
        int width = (std::max)(static_cast<int>(combo.right - combo.left),
            longest + longest / 4 + m_sampleWidth + 6 * m_padding);
        if (::GetMonitorInfoW(monitorHandle, &monitor))
            width = (std::min)(width, static_cast<int>(monitor.rcWork.right - monitor.rcWork.left));
        ::SendMessageW(m_combo, CB_SETDROPPEDWIDTH, width, 0);
        m_widthDirty = false;
        m_monitor = monitorHandle;
    }

    int SampleLeft(const RECT& row) const
    {
        const int width = row.right - row.left;
        return row.right - (std::min)(m_sampleWidth, width / 2);
    }

    void PrepareDropdown()
    {
        if (m_hintRow == -2) HideHint();
        if (m_dpi != FbeWin32::WindowDpi(m_combo)) UpdateMetrics();
        UpdateDropWidth();
        if (m_popup) return;
        COMBOBOXINFO info = {};
        info.cbSize = sizeof(info);
        if (::GetComboBoxInfo(m_combo, &info) && info.hwndList &&
            ::SetWindowSubclass(info.hwndList, PopupProc, reinterpret_cast<UINT_PTR>(this),
                reinterpret_cast<DWORD_PTR>(this))) m_popup = info.hwndList;
    }

    TOOLINFOW HintTool() const
    {
        TOOLINFOW tool = {};
        tool.cbSize = TTTOOLINFOW_V2_SIZE;
        tool.uFlags = TTF_TRACK | TTF_ABSOLUTE;
        tool.hwnd = m_combo;
        tool.uId = 1;
        tool.lpszText = const_cast<wchar_t*>(m_hintText.c_str());
        return tool;
    }

    void HideHint()
    {
        if (m_hintRow == -1) return;
        m_hintRow = -1;
        const auto tool = HintTool();
        ::SendMessageW(m_tooltip, TTM_TRACKACTIVATE, FALSE, reinterpret_cast<LPARAM>(&tool));
    }

    void UpdateHint(LPARAM point)
    {
        const LRESULT hit = ::SendMessageW(m_popup, LB_ITEMFROMPOINT, 0, point);
        const int row = LOWORD(hit);
        const int x = static_cast<short>(LOWORD(point));
        RECT area = {};
        if (HIWORD(hit) || ::SendMessageW(m_popup, LB_GETITEMRECT, row, reinterpret_cast<LPARAM>(&area)) == LB_ERR) {
            HideHint();
            return;
        }
        const auto name = ItemName(static_cast<WPARAM>(row));
        const auto face = std::find_if(m_faces.begin(), m_faces.end(), [&](const Face& entry) { return entry.name == name; });
        if (face == m_faces.end()) { HideHint(); return; }
        const int sampleLeft = SampleLeft(area);
        const bool partial = face->coverage == FontPreviewCoverage::Support::Partial &&
            x >= sampleLeft && x < sampleLeft + 8 * m_padding;
        const RECT label = NameArea(area, false, face->coverage);
        if (!partial && (x < label.left || x >= label.right || face->rowName.textWidth <= label.right - label.left)) {
            HideHint();
            return;
        }
        ShowHint(m_popup, row, partial, name, {partial ? sampleLeft + m_padding : label.left, area.bottom},
            area.bottom - area.top);
    }

    void ShowHint(HWND window, int row, bool partial, const std::wstring& name, POINT position, int rowHeight)
    {
        const auto cache = FontPreviewCoverage::ForWindow(m_combo);
        wchar_t localized[512] = {};
        if (partial && cache && cache->readPartialHint) cache->readPartialHint(localized, _countof(localized));
        const std::wstring text = partial ? localized : name;
        if (text.empty()) { HideHint(); return; }
        if (m_hintRow == row && m_hintPartial == partial && m_hintText == text) return;
        HideHint();
        m_hintText = text;

        const auto tool = HintTool();
        if (!m_tooltip) {
            m_tooltip = ::CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT, m_combo, nullptr, ::GetModuleHandleW(nullptr), nullptr);
            if (!m_tooltip) return;
            if (!::SendMessageW(m_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool))) {
                ::DestroyWindow(m_tooltip);
                m_tooltip = nullptr;
                return;
            }
        }
        const auto palette = cache && cache->readPalette ? cache->readPalette() : FontPreviewCoverage::Palette{};
        ::SendMessageW(m_tooltip, TTM_SETWINDOWTHEME, 0, reinterpret_cast<LPARAM>(palette.selection ? L"" : nullptr));
        ::SendMessageW(m_tooltip, TTM_SETTIPBKCOLOR, palette.selection ? palette.tooltipBackground : ::GetSysColor(COLOR_INFOBK), 0);
        ::SendMessageW(m_tooltip, TTM_SETTIPTEXTCOLOR, palette.selection ? palette.tooltipText : ::GetSysColor(COLOR_INFOTEXT), 0);
        ::ClientToScreen(window, &position);
        MONITORINFO monitor = {};
        monitor.cbSize = sizeof(monitor);
        const BOOL haveMonitor = ::GetMonitorInfoW(::MonitorFromPoint(position, MONITOR_DEFAULTTONEAREST), &monitor);
        int maxWidth = ::MulDiv(420, m_dpi, 96);
        if (haveMonitor) maxWidth = (std::min)(maxWidth, static_cast<int>(monitor.rcWork.right - monitor.rcWork.left));
        ::SendMessageW(m_tooltip, TTM_SETMAXTIPWIDTH, 0, maxWidth);
        ::SendMessageW(m_tooltip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&tool));
        if (haveMonitor) {
            const auto bubble = ::SendMessageW(m_tooltip, TTM_GETBUBBLESIZE, 0, reinterpret_cast<LPARAM>(&tool));
            position.x = (std::max)(monitor.rcWork.left, (std::min)(position.x, monitor.rcWork.right - LOWORD(bubble)));
            if (position.y + HIWORD(bubble) > monitor.rcWork.bottom)
                position.y = (std::max)(monitor.rcWork.top, position.y - rowHeight - HIWORD(bubble));
        }
        ::SendMessageW(m_tooltip, TTM_TRACKPOSITION, 0, MAKELPARAM(position.x, position.y));
        ::SendMessageW(m_tooltip, TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&tool));
        m_hintRow = row;
        m_hintPartial = partial;
        TRACKMOUSEEVENT tracking = {sizeof(tracking), TME_LEAVE, window, 0};
        ::TrackMouseEvent(&tracking);
    }

    void UpdateClosedHint(LPARAM point)
    {
        if ((::GetWindowLongPtrW(m_combo, GWL_STYLE) & 3) != CBS_DROPDOWNLIST ||
            ::SendMessageW(m_combo, CB_GETDROPPEDSTATE, 0, 0)) return;
        COMBOBOXINFO info = {};
        info.cbSize = sizeof(info);
        if (!::GetComboBoxInfo(m_combo, &info)) return;
        const auto name = ItemName(::SendMessageW(m_combo, CB_GETCURSEL, 0, 0));
        const auto face = std::find_if(m_faces.begin(), m_faces.end(), [&](const Face& entry) { return entry.name == name; });
        const POINT mouse = {static_cast<short>(LOWORD(point)), static_cast<short>(HIWORD(point))};
        if (face == m_faces.end() || !::PtInRect(&info.rcItem, mouse) ||
            face->closedName.textWidth <= info.rcItem.right - info.rcItem.left - 2 * m_padding) {
            HideHint();
            return;
        }
        ShowHint(m_combo, -2, false, name, {info.rcItem.left, info.rcItem.bottom}, info.rcItem.bottom - info.rcItem.top);
    }

    Face& PreviewFace(HDC dc, const std::wstring& name)
    {
        using namespace FontPreviewCoverage;
        Cache* cache = ForWindow(m_combo);
        if (cache && cache->generation != m_coverageGeneration) {
            HideHint();
            ClearFaces();
            m_coverageGeneration = cache->generation;
        }
        const auto existing = std::find_if(m_faces.begin(), m_faces.end(), [&](const Face& face) {
            return face.name == name;
        });
        if (existing != m_faces.end()) {
            m_faces.splice(m_faces.begin(), m_faces, existing);
            return m_faces.front();
        }
        if (m_faces.size() == 96) {
            DeleteFonts(m_faces.back());
            m_faces.pop_back();
        }
        m_faces.push_front({name});
        Face& face = m_faces.front();
        if (name.empty() || name.size() >= LF_FACESIZE || name[0] == L'@') return face;

        LOGFONTW request = m_previewFont;
        std::copy_n(name.c_str(), name.size() + 1, request.lfFaceName);
        const HFONT candidate = ::CreateFontIndirectW(&request);
        if (!candidate) return face;
        const HGDIOBJ old = ::SelectObject(dc, candidate);
        if (old) {
            WCHAR actual[LF_FACESIZE] = {};
            TEXTMETRICW metrics = {};
            std::array<WORD, LF_FACESIZE> glyphs = {};
            // Verify the realized face before querying either its name or alphabet.
            // GDI can return mapped symbol indices for letters in symbol fonts.
            if (::GetTextFaceW(dc, LF_FACESIZE, actual) > 0 && ::lstrcmpiW(actual, name.c_str()) == 0 &&
                ::GetTextMetricsW(dc, &metrics) && metrics.tmCharSet != SYMBOL_CHARSET) {
                const auto end = glyphs.begin() + name.size();
                face.readableName = ::GetGlyphIndicesW(dc, name.c_str(), static_cast<int>(name.size()), glyphs.data(),
                    GGI_MARK_NONEXISTING_GLYPHS) == name.size() && std::all_of(glyphs.begin(), end, IsGlyph);
                face.coverage = cache ? cache->Lookup(dc, name) : Probe(dc);
                face.font = candidate;
            }
            ::SelectObject(dc, old);
        }
        if (!face.font) ::DeleteObject(candidate);
        return face;
    }

    RECT NameArea(RECT area, bool closed, FontPreviewCoverage::Support support) const
    {
        const int sampleLeft = SampleLeft(area);
        ::InflateRect(&area, -m_padding, 0);
        if (!closed && support != FontPreviewCoverage::Support::None) area.right = sampleLeft - m_padding;
        return area;
    }

    static RECT Measure(HDC dc, const wchar_t* text)
    {
        RECT bounds = {};
        ::DrawTextW(dc, text, -1, &bounds, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
        return bounds;
    }

    HFONT Fit(HDC dc, HFONT original, FittedFont& result, const wchar_t* text, int width, int height)
    {
        if (result.width == width && result.height == height) return result.font;
        if (result.font && result.font != original) ::DeleteObject(result.font);
        result = {nullptr, width, height, 0};
        const auto previous = ::SelectObject(dc, original ? original : InterfaceFont());
        RECT bounds = Measure(dc, text);
        result.textWidth = bounds.right;
        LOGFONTW request = {};
        if (original && bounds.bottom <= height && bounds.right <= width) {
            result.font = original;
            ::SelectObject(dc, previous);
            return original;
        }
        if (original) ::GetObjectW(original, sizeof(request), &request);
        // Estimate once, then account for rounding and hinting using real GDI extents.
        int size = -request.lfHeight;
        while (original && (bounds.bottom > height || bounds.right > width) && size > 1 && width > 0 && height > 0) {
            const int byWidth = bounds.right > width ? ::MulDiv(size, width, bounds.right) : size;
            const int byHeight = bounds.bottom > height ? ::MulDiv(size, height, bounds.bottom) : size;
            size = (std::max)(1, (std::min)({size - 1, byWidth, byHeight}));
            request.lfHeight = -size;
            const auto candidate = ::CreateFontIndirectW(&request);
            if (!candidate) break;
            ::SelectObject(dc, candidate);
            wchar_t actual[LF_FACESIZE] = {};
            if (::GetTextFaceW(dc, LF_FACESIZE, actual) && ::lstrcmpiW(actual, request.lfFaceName) == 0) {
                bounds = Measure(dc, text);
                if (bounds.bottom <= height && bounds.right <= width) {
                    result.font = candidate;
                    result.textWidth = bounds.right;
                    break;
                }
            }
            ::SelectObject(dc, original);
            ::DeleteObject(candidate);
        }
        if (!result.font && original) {
            ::SelectObject(dc, InterfaceFont());
            result.textWidth = Measure(dc, text).right;
        }
        ::SelectObject(dc, previous);
        return result.font;
    }

    void Draw(const DRAWITEMSTRUCT& item)
    {
        const int saved = ::SaveDC(item.hDC);
        if (!saved) return;
        const bool closed = (item.itemState & ODS_COMBOBOXEDIT) != 0;
        const bool disabled = (item.itemState & (ODS_DISABLED | ODS_GRAYED)) != 0 || !::IsWindowEnabled(m_combo);
        const bool highlighted = !disabled && (item.itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
        const auto cache = FontPreviewCoverage::ForWindow(m_combo);
        const auto palette = cache && cache->readPalette ? cache->readPalette() : FontPreviewCoverage::Palette{};
        COMBOBOXINFO info = {};
        info.cbSize = sizeof(info);
        ::GetComboBoxInfo(m_combo, &info);
        ::SetTextColor(item.hDC, ::GetSysColor(disabled ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT));
        ::SetBkColor(item.hDC, ::GetSysColor(COLOR_WINDOW));
        const HBRUSH brush = reinterpret_cast<HBRUSH>(::SendMessageW(m_parent,
            closed ? WM_CTLCOLOREDIT : WM_CTLCOLORLISTBOX, reinterpret_cast<WPARAM>(item.hDC),
            reinterpret_cast<LPARAM>(closed ? m_combo : info.hwndList)));
        if (!closed || item.itemAction != 0) {
            ::FillRect(item.hDC, &item.rcItem, highlighted ? (palette.selection ? palette.selection : ::GetSysColorBrush(COLOR_HIGHLIGHT)) :
                (brush ? brush : ::GetSysColorBrush(COLOR_WINDOW)));
        }
        if (highlighted) ::SetTextColor(item.hDC, palette.selection ? palette.selectionText : ::GetSysColor(COLOR_HIGHLIGHTTEXT));
        ::SetBkMode(item.hDC, TRANSPARENT);
        if (item.itemID != static_cast<UINT>(-1)) {
            const std::wstring name = ItemName(item.itemID);
            Face& face = PreviewFace(item.hDC, name);
            const int height = item.rcItem.bottom - item.rcItem.top;
            RECT text = NameArea(item.rcItem, closed, face.coverage);
            const HFONT preview = Fit(item.hDC, face.readableName ? face.font : nullptr,
                closed ? face.closedName : face.rowName, name.c_str(), (std::numeric_limits<int>::max)(), height);
            ::SelectObject(item.hDC, preview ? preview : InterfaceFont());
            ::DrawTextW(item.hDC, name.c_str(), static_cast<int>(name.size()), &text,
                DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
            if (!closed && face.coverage != FontPreviewCoverage::Support::None) {
                const bool partial = face.coverage == FontPreviewCoverage::Support::Partial;
                RECT sample = item.rcItem;
                sample.left = SampleLeft(item.rcItem);
                sample.right -= m_padding;
                const HFONT sampleFont = partial ? InterfaceFont() : Fit(item.hDC, face.font, face.sample,
                    FontPreviewCoverage::Sample, sample.right - sample.left, height);
                if (sampleFont) {
                    ::SelectObject(item.hDC, sampleFont);
                    ::DrawTextW(item.hDC, partial ? L"ⓘ" : FontPreviewCoverage::Sample, -1, &sample,
                        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
                }
            }
        }
        if ((item.itemState & (ODS_FOCUS | ODS_NOFOCUSRECT)) == ODS_FOCUS)
            ::DrawFocusRect(item.hDC, &item.rcItem);
        ::RestoreDC(item.hDC, saved);
    }

    static LRESULT CALLBACK ParentProc(HWND parent, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR data)
    {
        auto& self = *reinterpret_cast<FontPreviewCombo*>(data);
        if (message == WM_DRAWITEM && lParam) {
            const auto& item = *reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item.CtlType == ODT_COMBOBOX && item.hwndItem == self.m_combo) {
                self.Draw(item);
                return TRUE;
            }
        }
        if (message == WM_COMMAND && reinterpret_cast<HWND>(lParam) == self.m_combo) {
            if (HIWORD(wParam) == CBN_DROPDOWN) self.PrepareDropdown();
            if (HIWORD(wParam) == CBN_CLOSEUP) self.HideHint();
        }
        if (message == WM_FONTCHANGE) self.UpdateMetrics();
        if (message == WM_NCDESTROY) {
            ::RemoveWindowSubclass(parent, ParentProc, id);
            self.m_parent = nullptr;
        }
        return ::DefSubclassProc(parent, message, wParam, lParam);
    }

    static LRESULT CALLBACK ComboProc(HWND combo, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR data)
    {
        auto& self = *reinterpret_cast<FontPreviewCombo*>(data);
        if (message == WM_NCDESTROY) {
            ::RemovePropW(combo, FontPreviewCoverage::ControlProperty);
            if (self.m_popup) ::RemoveWindowSubclass(self.m_popup, PopupProc, id);
            if (self.m_tooltip) ::DestroyWindow(self.m_tooltip);
            if (self.m_parent) ::RemoveWindowSubclass(self.m_parent, ParentProc, id);
            ::RemoveWindowSubclass(combo, ComboProc, id);
            delete &self;
            return ::DefSubclassProc(combo, message, wParam, lParam);
        }
        if (message == CB_ADDSTRING || message == CB_INSERTSTRING ||
            message == CB_DELETESTRING || message == CB_RESETCONTENT) self.m_widthDirty = true;
        if (message == WM_MOUSEMOVE) self.UpdateClosedHint(lParam);
        if (message == WM_MOUSELEAVE && self.m_hintRow == -2) self.HideHint();
        if (message == CB_SHOWDROPDOWN && wParam) self.PrepareDropdown();
        const LRESULT result = ::DefSubclassProc(combo, message, wParam, lParam);
        DWORD_PTR current = 0;
        if ((message == WM_SETFONT || message == DpiChangedAfterParent || message == WM_FONTCHANGE) &&
            ::GetWindowSubclass(combo, ComboProc, id, &current) && current == data)
            self.UpdateMetrics();
        return result;
    }

    static LRESULT CALLBACK PopupProc(HWND popup, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR data)
    {
        auto& self = *reinterpret_cast<FontPreviewCombo*>(data);
        if (message == WM_MOUSEMOVE) self.UpdateHint(lParam);
        if (message == WM_MOUSELEAVE || message == WM_VSCROLL || message == WM_MOUSEWHEEL ||
            (message == WM_SHOWWINDOW && !wParam)) self.HideHint();
        if (message == WM_NCDESTROY) {
            self.HideHint();
            if (self.m_tooltip) ::DestroyWindow(self.m_tooltip);
            self.m_tooltip = nullptr;
            self.m_popup = nullptr;
            ::RemoveWindowSubclass(popup, PopupProc, id);
        }
        return ::DefSubclassProc(popup, message, wParam, lParam);
    }
};
