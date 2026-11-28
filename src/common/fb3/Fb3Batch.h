#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace FbeFb3 {

using BatchConvert = std::vector<std::wstring>(*)(const std::filesystem::path& input,
    const std::filesystem::path& temporaryOutput, const std::filesystem::path& runtimeDirectory);

int RunBatch(int argc, wchar_t** argv, const wchar_t* inputExtension, const wchar_t* outputExtension, BatchConvert convert);
std::string Utf8(const std::wstring& text);
void WriteUtf8(const std::filesystem::path& destination, const std::wstring& text);

}
