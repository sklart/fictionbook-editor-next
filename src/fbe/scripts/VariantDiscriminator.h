#pragma once

#include <cstring>

// The x86 COM caller may provide a VARIANT on a four-byte boundary even
// when the compiler gives VARIANT an eight-byte alignment requirement.
inline VARTYPE ReadVariantDiscriminator(const void* value) noexcept
{
    VARTYPE type = VT_EMPTY;
    if (value) std::memcpy(&type, value, sizeof(type));
    return type;
}
