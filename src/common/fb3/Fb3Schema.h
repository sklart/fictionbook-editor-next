#pragma once

#include <filesystem>

namespace FbeFb3 {

class Package;

// Validate the two normative XML parts after opening and checking the package.
void ValidatePackageSchemas(const Package& package, const std::filesystem::path& schemaDirectory);

}
