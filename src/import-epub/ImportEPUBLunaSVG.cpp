// Optional SVG rasterizer adapter for ImportEPUB.
//
// This DLL is deliberately separate from ImportEPUB.dll and ImportEPUBBatch.exe.
// The main importer loads ImportEPUBLunaSVG.dll dynamically only when it exists
// next to the plugin/utility. If this DLL is absent or cannot render a SVG,
// ImportEPUB creates a visible placeholder image instead of leaving a dangling
// FB2 <image> reference.
//
// Build notes:
// - Put LunaSVG headers into thirdparty\lunasvg\include\.
// - Put lunasvg.lib into thirdparty\lunasvg\lib\Win32\Release\ or make it
//   available through vcpkg/MSBuild integration.
// - This wrapper exports a small C ABI so the main importer does not depend on
//   LunaSVG C++ ABI or library layout.

#include <windows.h>
#include <string>
#include <memory>
#include <cstdio>

#include <lunasvg.h>

namespace
{
    std::string WideToUtf8(const wchar_t* value)
    {
        if (!value || !*value)
            return std::string();

        int size = ::WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1)
            return std::string();

        std::string out(static_cast<size_t>(size - 1), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, value, -1, &out[0], size, nullptr, nullptr);
        return out;
    }

    void SetError(wchar_t* buffer, unsigned int bufferChars, const wchar_t* message)
    {
        if (!buffer || bufferChars == 0)
            return;
        buffer[0] = L'\0';
        if (!message)
            return;
        wcsncpy_s(buffer, bufferChars, message, _TRUNCATE);
    }

    bool FileExistsW(const wchar_t* path)
    {
        if (!path || !*path)
            return false;
        DWORD attrs = ::GetFileAttributesW(path);
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }
}

extern "C" __declspec(dllexport) int __stdcall ImportEPUBLunaSvgRenderPng(
    const wchar_t* svgPath,
    const wchar_t* pngPath,
    unsigned int /*maxWidth*/,
    unsigned int /*maxHeight*/,
    wchar_t* errorBuffer,
    unsigned int errorBufferChars)
{
    if (!svgPath || !*svgPath || !pngPath || !*pngPath)
    {
        SetError(errorBuffer, errorBufferChars, L"Empty SVG or PNG path.");
        return 0;
    }

    try
    {
        const std::string svgUtf8 = WideToUtf8(svgPath);
        const std::string pngUtf8 = WideToUtf8(pngPath);
        if (svgUtf8.empty() || pngUtf8.empty())
        {
            SetError(errorBuffer, errorBufferChars, L"Failed to convert paths to UTF-8.");
            return 0;
        }

        auto document = lunasvg::Document::loadFromFile(svgUtf8);
        if (document == nullptr)
        {
            SetError(errorBuffer, errorBufferChars, L"LunaSVG could not load the SVG file.");
            return 0;
        }

        auto bitmap = document->renderToBitmap();
        if (bitmap.isNull())
        {
            SetError(errorBuffer, errorBufferChars, L"LunaSVG returned an empty bitmap.");
            return 0;
        }

        bitmap.writeToPng(pngUtf8);
        if (!FileExistsW(pngPath))
        {
            SetError(errorBuffer, errorBufferChars, L"LunaSVG did not create the output PNG file.");
            return 0;
        }

        SetError(errorBuffer, errorBufferChars, L"");
        return 1;
    }
    catch (...)
    {
        SetError(errorBuffer, errorBufferChars, L"Unhandled exception inside ImportEPUBLunaSVG.dll.");
        return 0;
    }
}
