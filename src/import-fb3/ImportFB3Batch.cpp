#include "../common/fb3/Fb3Batch.h"
#include "../common/fb3/Fb3Import.h"

namespace {

FbeFb3::Diagnostics Convert(const std::filesystem::path& source,
    const std::filesystem::path& target, const std::filesystem::path& runtime) {
    auto book = FbeFb3::ImportBook(source, runtime / L"FictionBook.xsd");
    FbeFb3::WriteUtf8(target, book.xml);
    return std::move(book.diagnostics);
}

}

int wmain(int argc, wchar_t** argv) { return FbeFb3::RunBatch(argc, argv, L".fb3", L".fb2", Convert); }
