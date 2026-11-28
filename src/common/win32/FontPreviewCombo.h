#pragma once

#include "FontPreviewCoverage.h"
#include <algorithm>
#include <array>
#include <list>
#include <memory>
#include <new>
#include <string>

// Visual layer only. The dialog still owns its strings, selection and settings.
class FontPreviewCombo final
{
public:
    // Use with CBS_OWNERDRAWFIXED | CBS_HASSTRINGS. An editable combo retains
    // the interface font in its native edit window.
    static void Attach(HWND combo, const wchar_t* locale = nullptr)
    {
        const HWND parent = ::GetParent(combo);
        if (!parent) return;
        const bool russian = locale && (::lstrcmpiW(locale, L"ru-RU") == 0 || ::lstrcmpiW(locale, L"russian") == 0);
        std::unique_ptr<FontPreviewCombo> preview(new (std::nothrow) FontPreviewCombo(combo, parent, russian));
        if (!preview) return;
        const auto id = reinterpret_cast<UINT_PTR>(preview.get());
        const auto data = reinterpret_cast<DWORD_PTR>(preview.get());
        if (!::SetWindowSubclass(parent, ParentProc, id, data)) return;
        if (!::SetWindowSubclass(combo, ComboProc, id, data)) {
            ::RemoveWindowSubclass(parent, ParentProc, id);
            return;
        }
        preview.release()->UpdateMetrics();
    }

private:
    // The plug-ins target Windows 7 headers, which omit this newer message.
    static constexpr UINT DpiChangedAfterParent = 0x02E3;

    struct Face
    {
        std::wstring name;
        HFONT font = nullptr;
        int lineHeight = 0;
        bool readableName = false;
        FontPreviewCoverage::Support coverage = FontPreviewCoverage::Support::None;
    };

    HWND m_combo;
    HWND m_parent;
    const wchar_t* m_partialHint;
    HWND m_popup = nullptr;
    HWND m_tooltip = nullptr;
    int m_hintRow = -1;
    LOGFONTW m_previewFont = {};
    int m_padding = 3;
    int m_sampleWidth = 0;
    unsigned m_coverageGeneration = 0;
    bool m_widthDirty = true;
    // Recently visible faces, including negative results for symbol fonts.
    std::list<Face> m_faces;

    FontPreviewCombo(HWND combo, HWND parent, bool russian)
        : m_combo(combo), m_parent(parent), m_partialHint(russian ?
            L"Шрифт поддерживает не все буквы русского алфавита." :
            L"This font supports only part of the Russian alphabet.") {}
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
            if (face.font) ::DeleteObject(face.font);
        }
        m_faces.clear();
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
        const int pixels = ::GetDeviceCaps(dc, LOGPIXELSY);
        ::SelectObject(dc, old);
        ::ReleaseDC(combo, dc);
        if (!measured) return;

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
        m_padding = (std::max)(3, ::MulDiv(3, pixels, 96));
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
        if (!m_widthDirty) return;
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
        if (::GetMonitorInfoW(::MonitorFromWindow(m_combo, MONITOR_DEFAULTTONEAREST), &monitor))
            width = (std::min)(width, static_cast<int>(monitor.rcWork.right - monitor.rcWork.left));
        ::SendMessageW(m_combo, CB_SETDROPPEDWIDTH, width, 0);
        m_widthDirty = false;
    }

    int SampleLeft(const RECT& row) const
    {
        const int width = row.right - row.left;
        return row.right - (std::min)(m_sampleWidth, width / 2);
    }

    void PrepareDropdown()
    {
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
        tool.cbSize = sizeof(tool);
        tool.uFlags = TTF_TRACK | TTF_ABSOLUTE;
        tool.hwnd = m_popup;
        tool.uId = 1;
        tool.lpszText = const_cast<wchar_t*>(m_partialHint);
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
        if (HIWORD(hit) || ::SendMessageW(m_popup, LB_GETITEMRECT, row, reinterpret_cast<LPARAM>(&area)) == LB_ERR ||
            x < SampleLeft(area) || x >= SampleLeft(area) + 8 * m_padding) {
            HideHint();
            return;
        }
        const auto name = ItemName(static_cast<WPARAM>(row));
        const bool partial = std::any_of(m_faces.begin(), m_faces.end(), [&](const Face& face) {
            return face.coverage == FontPreviewCoverage::Support::Partial && face.name == name;
        });
        if (!partial) { HideHint(); return; }
        if (m_hintRow == row) return;
        HideHint();

        const auto tool = HintTool();
        if (!m_tooltip) {
            m_tooltip = ::CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT, m_popup, nullptr, ::GetModuleHandleW(nullptr), nullptr);
            if (!m_tooltip) return;
            if (!::SendMessageW(m_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool))) {
                ::DestroyWindow(m_tooltip);
                m_tooltip = nullptr;
                return;
            }
        }
        POINT position = {SampleLeft(area) + 6 * m_padding, area.bottom};
        ::ClientToScreen(m_popup, &position);
        ::SendMessageW(m_tooltip, TTM_TRACKPOSITION, 0, MAKELPARAM(position.x, position.y));
        ::SendMessageW(m_tooltip, TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&tool));
        m_hintRow = row;
        TRACKMOUSEEVENT tracking = {sizeof(tracking), TME_LEAVE, m_popup, 0};
        ::TrackMouseEvent(&tracking);
    }

    const Face& PreviewFace(HDC dc, const std::wstring& name)
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
            if (m_faces.back().font) ::DeleteObject(m_faces.back().font);
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
                face.lineHeight = metrics.tmHeight;
            }
            ::SelectObject(dc, old);
        }
        if (!face.font) ::DeleteObject(candidate);
        return face;
    }

    void Draw(const DRAWITEMSTRUCT& item)
    {
        const int saved = ::SaveDC(item.hDC);
        if (!saved) return;
        const bool closed = (item.itemState & ODS_COMBOBOXEDIT) != 0;
        const bool disabled = (item.itemState & (ODS_DISABLED | ODS_GRAYED)) != 0 || !::IsWindowEnabled(m_combo);
        const bool highlighted = !disabled && (item.itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
        COMBOBOXINFO info = {};
        info.cbSize = sizeof(info);
        ::GetComboBoxInfo(m_combo, &info);
        ::SetTextColor(item.hDC, ::GetSysColor(disabled ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT));
        ::SetBkColor(item.hDC, ::GetSysColor(COLOR_WINDOW));
        const HBRUSH brush = reinterpret_cast<HBRUSH>(::SendMessageW(m_parent,
            closed ? WM_CTLCOLOREDIT : WM_CTLCOLORLISTBOX, reinterpret_cast<WPARAM>(item.hDC),
            reinterpret_cast<LPARAM>(closed ? m_combo : info.hwndList)));
        ::FillRect(item.hDC, &item.rcItem, highlighted ? ::GetSysColorBrush(COLOR_HIGHLIGHT) :
            (brush ? brush : ::GetSysColorBrush(COLOR_WINDOW)));
        if (highlighted) ::SetTextColor(item.hDC, ::GetSysColor(COLOR_HIGHLIGHTTEXT));
        ::SetBkMode(item.hDC, TRANSPARENT);
        if (item.itemID != static_cast<UINT>(-1)) {
            const std::wstring name = ItemName(item.itemID);
            const Face& face = PreviewFace(item.hDC, name);
            const int height = item.rcItem.bottom - item.rcItem.top;
            const HFONT preview = face.font && face.lineHeight <= height ? face.font : nullptr;
            ::SelectObject(item.hDC, preview && face.readableName ? preview : InterfaceFont());
            RECT text = item.rcItem;
            ::InflateRect(&text, -m_padding, 0);
            if (!closed) text.right = SampleLeft(item.rcItem) - m_padding;
            ::DrawTextW(item.hDC, name.c_str(), static_cast<int>(name.size()), &text,
                DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
            if (!closed && face.coverage != FontPreviewCoverage::Support::None) {
                const bool partial = face.coverage == FontPreviewCoverage::Support::Partial;
                ::SelectObject(item.hDC, partial || !preview ? InterfaceFont() : preview);
                RECT sample = item.rcItem;
                sample.left = SampleLeft(item.rcItem);
                sample.right -= m_padding;
                if (partial || preview)
                    ::DrawTextW(item.hDC, partial ? L"ⓘ" : FontPreviewCoverage::Sample, -1, &sample,
                        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
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
            if (self.m_popup) ::RemoveWindowSubclass(self.m_popup, PopupProc, id);
            if (self.m_tooltip) ::DestroyWindow(self.m_tooltip);
            if (self.m_parent) ::RemoveWindowSubclass(self.m_parent, ParentProc, id);
            ::RemoveWindowSubclass(combo, ComboProc, id);
            delete &self;
            return ::DefSubclassProc(combo, message, wParam, lParam);
        }
        if (message == CB_ADDSTRING || message == CB_INSERTSTRING ||
            message == CB_DELETESTRING || message == CB_RESETCONTENT) self.m_widthDirty = true;
        if (message == CB_SHOWDROPDOWN && wParam) self.PrepareDropdown();
        const LRESULT result = ::DefSubclassProc(combo, message, wParam, lParam);
        if (message == WM_SETFONT || message == DpiChangedAfterParent || message == WM_FONTCHANGE)
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
