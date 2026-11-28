#include "../../src/common/fb3/Fb3Package.h"
#include "../../src/common/fb3/Fb3Import.h"
#include "../../src/common/fb3/Fb3Schema.h"
#include "../../src/common/fb3/Fb3Export.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>

int wmain(int argc, wchar_t** argv) {
    if (argc < 3 || argc > 5) return 2;
    if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    const bool badImage = wcscmp(argv[1], L"invalid-image-import") == 0;
    const bool damagedImage = wcscmp(argv[1], L"damaged-image-import") == 0;
    const bool badNote = wcscmp(argv[1], L"invalid-note-import") == 0;
    const bool badSchema = wcscmp(argv[1], L"invalid-schema") == 0;
    const bool mismatchedId = wcscmp(argv[1], L"mismatched-id") == 0;
    const bool badExportLink = wcscmp(argv[1], L"export-invalid-link") == 0;
    const bool expected = wcscmp(argv[1], L"invalid") != 0 && !badImage && !damagedImage && !badNote && !badSchema && !mismatchedId && !badExportLink;
    const bool expectMissingCover = wcscmp(argv[1], L"missing-cover") == 0;
    int result = 1;
    try {
        if (argc == 5 && (wcscmp(argv[1], L"export") == 0 || badExportLink)) {
            const auto warnings = FbeFb3::ExportBook(FbeFb3::ReadFb2File(argv[2]), argv[4], argv[3], true);
            for (const auto& warning : warnings) std::fwprintf(stdout, L"%ls\n", warning.c_str());
            if (expected) result = 0;
        } else if (argc == 5) {
            const auto book = FbeFb3::ImportBook(argv[2], argv[3]);
            if (expected) {
                const auto length = static_cast<int>(book.xml.size());
                const int bytes = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, book.xml.data(), length, nullptr, 0, nullptr, nullptr);
                if (bytes <= 0) throw std::runtime_error("Cannot encode the converted FB2.");
                std::string encoded(static_cast<size_t>(bytes), '\0');
                if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, book.xml.data(), length, encoded.data(), bytes, nullptr, nullptr) != bytes)
                    throw std::runtime_error("Cannot encode the converted FB2.");
                std::ofstream output(std::filesystem::path(argv[4]), std::ios::binary);
                output.exceptions(std::ios::failbit | std::ios::badbit);
                output.write(encoded.data(), encoded.size());
                output.close();
                for (const auto& warning : book.warnings) std::fwprintf(stdout, L"%ls\n", warning.c_str());
                result = 0;
            } else std::fwprintf(stderr, L"Invalid conversion was accepted.\n");
        } else if (argc == 4) {
            const FbeFb3::Package book(argv[2]);
            FbeFb3::ValidatePackageSchemas(book, argv[3]);
            if (expected) { std::fwprintf(stdout, L"FB3 normative schema validation passed.\n"); result = 0; }
        } else {
            const FbeFb3::Package book(argv[2]);
            if (!expected) {
                std::fwprintf(stderr, L"Invalid package was accepted.\n");
            } else {
                result = &book.Description() != &book.Body() && !book.Links(book.Description().name).empty() ? 0 : 1;
                if (expectMissingCover && (book.Cover() || book.Warnings().size() != 1 ||
                    book.Warnings().front().find(L"cover part is missing") == std::wstring::npos)) result = 1;
                if (!result) std::fwprintf(stdout, L"Resolved FB3 description and body through OPC relationships.\n");
                for (const auto& warning : book.Warnings()) std::fwprintf(stdout, L"%ls\n", warning.c_str());
            }
        }
    } catch (const FbeFb3::Error& error) {
        std::fwprintf(expected ? stderr : stdout, L"%ls\n", error.message.c_str());
        const bool rightError = (!badImage || error.message.find(L"Invalid FB3 image relationship:") == 0) &&
            (!damagedImage || error.message.find(L"Incomplete or damaged bitmap data in ") == 0) &&
            (!badNote || error.message.find(L"Unresolved or ambiguous FB3 note:") == 0) &&
            (!badSchema || error.message.find(L"FB3 schema validation failed for ") == 0) &&
            (!mismatchedId || error.message == L"The FB3 description and body have different book IDs.") &&
            (!badExportLink || error.message.find(L"Unresolved FB2 internal link or note #") == 0);
        result = expected || !rightError ? 1 : 0;
    } catch (const ATL::CAtlException& error) {
        std::fprintf(stderr, "Unexpected ATL failure: 0x%08lX\n", static_cast<unsigned long>(error.m_hr));
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
    }
    ::CoUninitialize();
    return result;
}
