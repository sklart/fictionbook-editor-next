#include "../../src/common/win32/FontPreviewCombo.h"
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
using FontPreviewCoverage::Support;
COLORREF background = RGB(255, 255, 255);
COLORREF foreground = RGB(30, 30, 30);
HBRUSH surface = nullptr;

void Require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

LRESULT CALLBACK HostProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_FONTCHANGE) FontPreviewCoverage::Invalidate(window);
    if (message == WM_DESTROY) FontPreviewCoverage::Uninstall(window);
    if (message == WM_CTLCOLORLISTBOX || message == WM_CTLCOLOREDIT) {
        const auto dc = reinterpret_cast<HDC>(wParam);
        ::SetTextColor(dc, foreground);
        ::SetBkColor(dc, background);
        return reinterpret_cast<LRESULT>(surface);
    }
    return ::DefWindowProcW(window, message, wParam, lParam);
}

class Canvas
{
public:
    Canvas() {
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = Width;
        info.bmiHeader.biHeight = -Height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        void* bits = nullptr;
        m_bitmap = ::CreateDIBSection(m_dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        m_bits = static_cast<const DWORD*>(bits);
        Require(m_bitmap != nullptr, "CreateDIBSection");
        m_old = ::SelectObject(m_dc, m_bitmap);
    }
    ~Canvas() {
        ::SelectObject(m_dc, m_old);
        ::DeleteObject(m_bitmap);
        ::DeleteDC(m_dc);
    }
    void Draw(HWND combo, int index, UINT state = 0, HFONT expectedFont = nullptr,
        Support expectedCoverage = Support::None) const {
        const int height = static_cast<int>(::SendMessageW(combo, CB_GETITEMHEIGHT, state & ODS_COMBOBOXEDIT ? -1 : 0, 0));
        Require(height > 0 && height <= Height, "invalid item height");
        RECT area = {0, 0, Width, height};
        if (expectedFont) {
            wchar_t label[128] = {};
            ::SendMessageW(combo, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(label));
            const int saved = ::SaveDC(m_dc);
            ::FillRect(m_dc, &area, surface);
            ::SetBkMode(m_dc, TRANSPARENT);
            ::SetTextColor(m_dc, foreground);
            ::SelectObject(m_dc, expectedFont);
            HDC windowDc = ::GetDC(combo);
            const int padding = (std::max)(3, ::MulDiv(3, ::GetDeviceCaps(windowDc, LOGPIXELSY), 96));
            ::ReleaseDC(combo, windowDc);
            const auto interfaceFont = reinterpret_cast<HFONT>(::SendMessageW(combo, WM_GETFONT, 0, 0));
            SIZE sampleSize = {};
            ::SelectObject(m_dc, interfaceFont ? interfaceFont : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
            ::GetTextExtentPoint32W(m_dc, FontPreviewCoverage::Sample,
                _countof(FontPreviewCoverage::Sample) - 1, &sampleSize);
            const int sampleWidth = sampleSize.cx + sampleSize.cx * 2 / 5 + 4 * padding;
            const int rightColumn = Width - (std::min)(sampleWidth, Width / 2);
            const bool closed = (state & ODS_COMBOBOXEDIT) != 0;
            RECT nameArea = {padding, 0, (closed ? Width : rightColumn) - padding, height};
            ::SelectObject(m_dc, expectedFont);
            const UINT flags = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
            ::DrawTextW(m_dc, label, -1, &nameArea, flags);
            if (!closed && expectedCoverage != Support::None) {
                RECT sampleArea = {rightColumn, 0, Width - padding, height};
                if (expectedCoverage == Support::Partial) ::SelectObject(m_dc, interfaceFont);
                const wchar_t* sample = expectedCoverage == Support::Partial ? L"ⓘ" : FontPreviewCoverage::Sample;
                RECT measured = sampleArea;
                ::DrawTextW(m_dc, sample, -1, &measured, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
                Require(measured.right <= sampleArea.right, "Cyrillic sample is clipped");
                ::DrawTextW(m_dc, sample, -1, &sampleArea, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            }
            ::RestoreDC(m_dc, saved);
            return;
        }
        DRAWITEMSTRUCT item = {ODT_COMBOBOX, static_cast<UINT>(::GetDlgCtrlID(combo)),
            static_cast<UINT>(index), ODA_DRAWENTIRE, state, combo, m_dc, area, 0};
        Require(::SendMessageW(::GetParent(combo), WM_DRAWITEM, item.CtlID, reinterpret_cast<LPARAM>(&item)) == TRUE,
            "owner-draw item not handled");
    }
    std::vector<DWORD> Pixels(HWND combo, bool closed = false) const {
        const auto height = ::SendMessageW(combo, CB_GETITEMHEIGHT, closed ? static_cast<WPARAM>(-1) : 0, 0);
        ::GdiFlush();
        return {m_bits, m_bits + Width * height};
    }
    DWORD FirstPixel() const { ::GdiFlush(); return m_bits[0] & 0xffffff; }
    static constexpr int Width = 650;
private:
    static constexpr int Height = 96;
    HDC m_dc = ::CreateCompatibleDC(nullptr);
    HBITMAP m_bitmap = nullptr;
    HGDIOBJ m_old = nullptr;
    const DWORD* m_bits = nullptr;
};

int Add(HWND combo, const wchar_t* label)
{
    const auto index = static_cast<int>(::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label)));
    Require(index >= 0, "CB_ADDSTRING failed");
    return index;
}

DWORD DibColor(COLORREF color)
{
    return RGB(GetBValue(color), GetGValue(color), GetRValue(color));
}
}

int main()
{
    try {
        FontPreviewCoverage::AlphabetGlyphs glyphs = {};
        glyphs.fill(7);
        Require(FontPreviewCoverage::Classify(glyphs) == Support::Full, "all 66 glyphs");
        glyphs[64] = 0xffff;
        glyphs[65] = 0;
        Require(FontPreviewCoverage::Alphabet[64] == L'Ё' && FontPreviewCoverage::Alphabet[65] == L'ё' &&
            FontPreviewCoverage::Classify(glyphs) == Support::Partial, "missing Ё and ё");
        glyphs.fill(0xffff);
        Require(FontPreviewCoverage::Classify(glyphs) == Support::None, "no Cyrillic glyphs");

        INITCOMMONCONTROLSEX common = {sizeof(common), ICC_WIN95_CLASSES};
        Require(::InitCommonControlsEx(&common), "InitCommonControlsEx");
        const HINSTANCE instance = ::GetModuleHandleW(nullptr);
        WNDCLASSW hostClass = {};
        hostClass.hInstance = instance;
        hostClass.lpfnWndProc = HostProc;
        hostClass.lpszClassName = L"FbeFontComboProbe";
        Require(::RegisterClassW(&hostClass) != 0, "RegisterClassW");
        const HWND host = ::CreateWindowW(hostClass.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
            0, 0, 800, 600, nullptr, nullptr, instance, nullptr);
        Require(host != nullptr, "host window");
        FontPreviewCoverage::Install(host);
        Canvas canvas;
        const auto resources = [] { return ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS); };
        // Opening Windows' first native combo initializes per-process GDI resources.
        surface = ::CreateSolidBrush(background);
        const HWND native = ::CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VSCROLL | CBS_DROPDOWNLIST,
            0, 0, 165, 300, host, nullptr, instance, nullptr);
        Require(native != nullptr, "native combo");
        Add(native, L"Native text Ёё ⓘ");
        ::SendMessageW(native, CB_SHOWDROPDOWN, TRUE, 0);
        ::SendMessageW(native, CB_SHOWDROPDOWN, FALSE, 0);
        canvas.Draw(native, 0, 0, static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
        ::DestroyWindow(native);
        ::DeleteObject(surface);
        const auto baseline = resources();
        DWORD steadyResources = 0;
        int previousHeight = 0;
        for (const int dpi : {96, 120, 144, 168, 192}) {
            const HWND list = ::CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VSCROLL |
                CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 0, 0, 165, 300, host,
                reinterpret_cast<HMENU>(101), instance, nullptr);
            Require(list != nullptr, "font list");
            FontPreviewCombo::Attach(list);
            LOGFONTW ui = {};
            ui.lfHeight = -::MulDiv(9, dpi, 72);
            ui.lfWeight = FW_NORMAL;
            ui.lfCharSet = DEFAULT_CHARSET;
            wcscpy_s(ui.lfFaceName, L"Segoe UI");
            const HFONT normal = ::CreateFontIndirectW(&ui);
            Require(normal != nullptr, "font creation");
            ::SendMessageW(list, WM_SETFONT, reinterpret_cast<WPARAM>(normal), FALSE);
            const int height = static_cast<int>(::SendMessageW(list, CB_GETITEMHEIGHT, 0, 0));
            Require(height > previousHeight, "popup did not scale with changed UI font");
            previousHeight = height;
            for (int i = 0; i < 3; ++i) {
                background = i == 0 ? RGB(249, 249, 249) : i == 1 ? RGB(33, 37, 45) : RGB(246, 235, 211);
                foreground = i == 1 ? RGB(224, 232, 245) : RGB(39, 34, 30);
                surface = ::CreateSolidBrush(background);
                ::SendMessageW(list, CB_RESETCONTENT, 0, 0);
                for (const wchar_t* name : {L"Arial", L"Times New Roman", L"Consolas", L"Arial Black", L"Wingdings", L"Webdings",
                    L"FBE Missing Font With A Long Readable Family Name", L"FBE Unknown Family"}) {
                    const int index = Add(list, name);
                    const bool fallback = index >= 4;
                    LOGFONTW preview = ui;
                    if (!fallback) wcscpy_s(preview.lfFaceName, name);
                    const HFONT expected = fallback ? normal : ::CreateFontIndirectW(&preview);
                    canvas.Draw(list, index);
                    const auto actual = canvas.Pixels(list);
                    Require(canvas.FirstPixel() == DibColor(background), "theme background");
                    Require(std::any_of(actual.begin(), actual.end(), [&](DWORD pixel) {
                        return (pixel & 0xffffff) != DibColor(background);
                    }), "empty label");
                    canvas.Draw(list, index, 0, expected, fallback ? Support::None : Support::Full);
                    Require(actual == canvas.Pixels(list), fallback ? "fallback label" : "family preview");
                    if (index == 0) {
                        canvas.Draw(list, index, ODS_COMBOBOXEDIT);
                        const auto closed = canvas.Pixels(list, true);
                        canvas.Draw(list, index, ODS_COMBOBOXEDIT, expected, Support::None);
                        Require(closed == canvas.Pixels(list, true), "closed field must show just the face name");
                    }
                    if (!fallback) ::DeleteObject(expected);
                    canvas.Draw(list, index, ODS_SELECTED);
                    Require(canvas.FirstPixel() == DibColor(::GetSysColor(COLOR_HIGHLIGHT)), "selection background");
                }
                ::DeleteObject(surface);
            }
            surface = ::CreateSolidBrush(background);
            const auto cache = FontPreviewCoverage::ForWindow(list);
            Require(cache == FontPreviewCoverage::ForWindow(host), "root-owner cache is shared");
            const unsigned before = cache->generation;
            ::SendMessageW(host, WM_FONTCHANGE, 0, 0);
            Require(cache->generation != before && std::all_of(cache->entries.begin(), cache->entries.end(),
                [](const FontPreviewCoverage::Entry& entry) { return entry.family[0] == L'\0'; }), "font-change invalidation");
            // Render the controlled missing-Ё result through the real owner-draw path.
            const std::wstring partialName = L"Tahoma";
            auto& partial = cache->entries[std::hash<std::wstring>{}(partialName) % cache->entries.size()];
            wcscpy_s(partial.family, partialName.c_str());
            partial.support = Support::Partial;
            const int partialIndex = Add(list, partialName.c_str());
            canvas.Draw(list, partialIndex);
            const auto partialPixels = canvas.Pixels(list);
            LOGFONTW partialLogFont = ui;
            wcscpy_s(partialLogFont.lfFaceName, partialName.c_str());
            const HFONT partialFont = ::CreateFontIndirectW(&partialLogFont);
            Require(partialFont != nullptr, "Tahoma font creation");
            canvas.Draw(list, partialIndex, 0, partialFont, Support::Partial);
            Require(partialPixels == canvas.Pixels(list), "partial support indicator");
            ::DeleteObject(partialFont);

            const int original = static_cast<int>(::SendMessageW(list, CB_SETCURSEL, 2, 0));
            for (int i = 0; i < 600; ++i) {
                const std::wstring name = L"FBE Test Family " + std::to_wstring(i);
                Add(list, name.c_str());
            }
            const auto started = std::chrono::steady_clock::now();
            ::SendMessageW(host, WM_COMMAND, MAKEWPARAM(101, CBN_DROPDOWN), reinterpret_cast<LPARAM>(list));
            const auto prepared = std::chrono::steady_clock::now();
            ::SendMessageW(list, CB_SHOWDROPDOWN, TRUE, 0);
            const auto opened = std::chrono::steady_clock::now();
            ::SendMessageW(list, CB_SHOWDROPDOWN, FALSE, 0);
            Require(::SendMessageW(list, CB_GETDROPPEDWIDTH, 0, 0) > 165, "long name did not widen popup");
            const int count = static_cast<int>(::SendMessageW(list, CB_GETCOUNT, 0, 0));
            for (int index = 0; index < count; ++index) canvas.Draw(list, index, ODS_HOTLIGHT);
            Require(::SendMessageW(list, CB_GETCURSEL, 0, 0) == original, "drawing or opening changed selection");
            Require(::SendMessageW(list, CB_FINDSTRING, static_cast<WPARAM>(-1),
                reinterpret_cast<LPARAM>(L"Consol")) == original, "native prefix search");
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
            const auto prepareTime = std::chrono::duration_cast<std::chrono::microseconds>(prepared - started);
            const auto openTime = std::chrono::duration_cast<std::chrono::microseconds>(opened - prepared);
            std::printf("%d DPI: %d entries, preparation: %lld us, native opening: %lld us, total with row draws: %lld ms\n",
                dpi, count, prepareTime.count(), openTime.count(), elapsed.count());
            ::DestroyWindow(list);
            ::DeleteObject(normal);
            ::DeleteObject(surface);
            std::printf("GDI objects after closing combo: %lu\n", resources());
            if (!steadyResources) steadyResources = resources();
            Require(resources() <= steadyResources, "font GDI resources grew across recreated combos");
        }
        surface = ::CreateSolidBrush(background);
        const HWND editable = ::CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VSCROLL | CBS_AUTOHSCROLL |
            CBS_DROPDOWN | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 0, 0, 165, 300, host, nullptr, instance, nullptr);
        Require(editable != nullptr, "editable combo");
        FontPreviewCombo::Attach(editable);
        Add(editable, L"Arial");
        const wchar_t typed[] = L"Custom font installed on another computer";
        ::SetWindowTextW(editable, typed);
        ::SendMessageW(editable, CB_SHOWDROPDOWN, TRUE, 0);
        ::SendMessageW(editable, CB_SHOWDROPDOWN, FALSE, 0);
        wchar_t actual[128] = {};
        ::GetWindowTextW(editable, actual, _countof(actual));
        Require(wcscmp(actual, typed) == 0, "editable text lost");
        ::DestroyWindow(editable);
        ::DeleteObject(surface);
        std::printf("GDI objects before/after scenario: %lu -> %lu\n", baseline, resources());
        Require(resources() <= steadyResources, "editable combo GDI resources leaked");
        ::DestroyWindow(host);
        std::puts("Font picker runtime checks passed.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
