#include "../common/fb3/Fb3Batch.h"
#include "../common/fb3/Fb3Export.h"

namespace {

std::vector<std::wstring> Convert(const std::filesystem::path& source,
    const std::filesystem::path& target, const std::filesystem::path& runtime) {
    return FbeFb3::ExportBook(FbeFb3::ReadFb2File(source), target, runtime, true);
}

}

int wmain(int argc, wchar_t** argv) { return FbeFb3::RunBatch(argc, argv, L".fb2", L".fb3", Convert); }
