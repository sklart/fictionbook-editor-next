#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FbeFb3 {

void ValidateRasterImage(const std::wstring& name, const std::wstring& type, const std::vector<std::uint8_t>& image);

}
