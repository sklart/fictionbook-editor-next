#include "../../src/fbe/scripts/BorrowedVariant.h"
#include <cstdio>

__declspec(noinline) int CheckedIncrement(int value) { return value + 1; }

int main()
{
    volatile int control = 1;
    VARIANT original;
    ::VariantInit(&original);
    original.vt = VT_ARRAY | VT_UI1;
    original.parray = ::SafeArrayCreateVector(VT_UI1, 0, 8);
    if (!original.parray) return 1;
    VARIANT alignedBorrowed;
    const bool alignedCopied = FbeAutomation::ReadBorrowedVariant(&original, alignedBorrowed);
    alignas(VARIANT) unsigned char bytes[sizeof(VARIANT) + 4] = {};
    unsigned char* const unaligned = bytes + 4;
    std::memcpy(unaligned, &original, sizeof(original));
    VARIANT borrowed;
    const bool copied = FbeAutomation::ReadBorrowedVariant(unaligned, borrowed);
    LONG upper = -1;
    const bool passed = CheckedIncrement(control) == 2 &&
        alignedCopied && alignedBorrowed.vt == original.vt &&
        alignedBorrowed.parray == original.parray &&
        reinterpret_cast<ULONG_PTR>(unaligned) % alignof(VARIANT) != 0 &&
        copied && borrowed.vt == original.vt &&
        borrowed.parray == original.parray &&
        std::memcmp(unaligned, &original, sizeof(original)) == 0 &&
        ::SafeArrayGetUBound(borrowed.parray, 1, &upper) == S_OK && upper == 7 &&
        !FbeAutomation::ReadBorrowedVariant(nullptr, borrowed) &&
        borrowed.vt == original.vt && borrowed.parray == original.parray;
    // Only the original owns the array; the byte-copy has no lifetime action.
    const HRESULT cleared = ::VariantClear(&original);
    std::puts(passed && SUCCEEDED(cleared) ? "Borrowed unaligned VARIANT: PASS" : "Borrowed unaligned VARIANT: FAIL");
    return passed && SUCCEEDED(cleared) ? 0 : 1;
}
