#include "../../src/common/fb3/Fb3Package.h"

#include <cstdio>
#include <stdexcept>

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    const bool expected = wcscmp(argv[1], L"invalid") != 0;
    const bool expectMissingCover = wcscmp(argv[1], L"missing-cover") == 0;
    int result = 1;
    try {
        const FbeFb3::Package book(argv[2]);
        if (!expected) {
            std::fputs("Invalid package was accepted.\n", stderr);
        } else {
            result = &book.Description() != &book.Body() && !book.Links(book.Description().name).empty() ? 0 : 1;
            if (expectMissingCover && (book.Cover() || book.Warnings().size() != 1 ||
                book.Warnings().front().find(L"cover part is missing") == std::wstring::npos)) result = 1;
            if (!result) std::puts("Resolved FB3 description and body through OPC relationships.");
            for (const auto& warning : book.Warnings()) std::fwprintf(stdout, L"%ls\n", warning.c_str());
        }
    } catch (const FbeFb3::Error& error) {
        std::fwprintf(expected ? stderr : stdout, L"%ls\n", error.message.c_str());
        result = expected ? 1 : 0;
    } catch (const ATL::CAtlException& error) {
        std::fprintf(stderr, "Unexpected ATL failure: 0x%08lX\n", static_cast<unsigned long>(error.m_hr));
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
    }
    ::CoUninitialize();
    return result;
}
