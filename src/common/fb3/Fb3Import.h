#pragma once

#include "Fb3Diagnostics.h"

#include <filesystem>
#include <string>
#include <vector>

namespace FbeFb3 {

struct ImportedBook {
    std::wstring xml;
    Diagnostics diagnostics;
};

// The caller supplies the installed FB2 schema and initializes COM.
ImportedBook ImportBook(const std::filesystem::path& source, const std::filesystem::path& fb2Schema);

}
