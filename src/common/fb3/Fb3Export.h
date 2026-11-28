#pragma once

#include "Fb3Diagnostics.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct IStream;

namespace FbeFb3 {

std::vector<std::uint8_t> ReadFb2Stream(IStream& stream);
std::vector<std::uint8_t> ReadFb2File(const std::filesystem::path& source);
Diagnostics ExportBook(const std::vector<std::uint8_t>& fb2,
    const std::filesystem::path& destination, const std::filesystem::path& runtimeDirectory, bool replaceExisting,
    const std::function<void()>& beforePublish = {});

}
