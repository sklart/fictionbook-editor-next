#pragma once
#include <windows.h>
#include <oleauto.h>
#include <cstring>

namespace FbeAutomation {
// Win32 Automation can supply a VARIANT pointer at four-byte alignment.
// Copy the representation into aligned storage without adopting its contents.
// The destination is borrowed: never VariantClear it or release its members.
inline bool ReadBorrowedVariant(const void* source, VARIANT& destination) noexcept
{
    if (!source) return false;
    std::memcpy(&destination, source, sizeof(destination));
    return true;
}
}
