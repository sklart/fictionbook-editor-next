#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace FbeFb3 {

struct ImportedBook {
    std::wstring xml;
    std::vector<std::wstring> warnings;
};

// The caller supplies the installed FB2 schema and initializes COM.
ImportedBook ImportBook(const std::filesystem::path& source, const std::filesystem::path& fb2Schema);

}
