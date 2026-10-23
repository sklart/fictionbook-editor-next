#pragma once

#include <atlstr.h>
#include <limits>
#include <type_traits>

namespace FbeNumbers
{
template<class T>
bool ParseUnsignedDecimal(const CString& text, T& value)
{
    static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value, "Unsigned integer required");
    if (text.IsEmpty()) return false;
    const T maximum = (std::numeric_limits<T>::max)();
    T parsed = 0;
    for (int index = 0; index < text.GetLength(); ++index)
    {
        const wchar_t character = text[index];
        if (character < L'0' || character > L'9') return false;
        const T digit = static_cast<T>(character - L'0');
        if (parsed > maximum / 10 || (parsed == maximum / 10 && digit > maximum % 10)) return false;
        parsed = static_cast<T>(parsed * 10 + digit);
    }
    value = parsed;
    return true;
}
}
