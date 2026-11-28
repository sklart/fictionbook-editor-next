#pragma once

#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <array>
#include <functional>
#include <new>
#include <string>

namespace FontPreviewCoverage
{
inline constexpr wchar_t Sample[] = L"Ёё Жж Йй Фф Щщ";
inline constexpr wchar_t Alphabet[] =
    L"АБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"
    L"абвгдежзийклмнопрстуфхцчшщъыьэюяЁё";
static_assert(_countof(Alphabet) - 1 == 66);

enum class Support { None, Partial, Full };
using AlphabetGlyphs = std::array<WORD, _countof(Alphabet) - 1>;

inline bool IsGlyph(WORD glyph)
{
    return glyph != 0 && glyph != 0xffff;
}

inline Support Classify(const AlphabetGlyphs& glyphs)
{
    const auto supported = std::count_if(glyphs.begin(), glyphs.end(), IsGlyph);
    if (supported == 0) return Support::None;
    return static_cast<size_t>(supported) == glyphs.size() ? Support::Full : Support::Partial;
}

inline Support Probe(HDC dc)
{
    AlphabetGlyphs glyphs = {};
    // Read glyph indices from the selected face, before text rendering or font linking.
    if (::GetGlyphIndicesW(dc, Alphabet, static_cast<int>(glyphs.size()), glyphs.data(),
        GGI_MARK_NONEXISTING_GLYPHS) != glyphs.size()) return Support::None;
    return Classify(glyphs);
}

struct Entry
{
    wchar_t family[LF_FACESIZE] = {};
    Support support = Support::None;
};

// Published on the editor's root owner, so plug-in DLLs use the same results.
// Fixed-size values require no cross-module allocation or GDI ownership.
struct Cache
{
    unsigned generation = 1;
    std::array<Entry, 1024> entries = {};

    Support Lookup(HDC dc, const std::wstring& family)
    {
        Entry& entry = entries[std::hash<std::wstring>{}(family) % entries.size()];
        if (::lstrcmpW(entry.family, family.c_str()) != 0) {
            entry.support = Probe(dc);
            ::lstrcpynW(entry.family, family.c_str(), LF_FACESIZE);
        }
        return entry.support;
    }
};

inline constexpr wchar_t CacheProperty[] = L"FBE.FontPreviewCoverage";

inline Cache* ForWindow(HWND window)
{
    return static_cast<Cache*>(::GetPropW(::GetAncestor(window, GA_ROOTOWNER), CacheProperty));
}

inline void Invalidate(HWND root)
{
    if (auto* cache = static_cast<Cache*>(::GetPropW(root, CacheProperty))) {
        cache->entries.fill({});
        ++cache->generation;
    }
}

inline void Install(HWND root)
{
    if (::GetPropW(root, CacheProperty)) return;
    auto* cache = new (std::nothrow) Cache;
    if (cache && !::SetPropW(root, CacheProperty, cache)) delete cache;
}

inline void Uninstall(HWND root)
{
    delete static_cast<Cache*>(::RemovePropW(root, CacheProperty));
}
}
