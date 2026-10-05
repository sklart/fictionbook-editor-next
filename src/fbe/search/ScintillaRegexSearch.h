#pragma once

#include <windows.h>

namespace AU {
namespace Search {

// Finds the reverse result preceding a just-replaced zero-length match. The
// forward fallback is deliberately limited to the protected match's line.
// Returns false only when Scintilla reports a search-status error and
// failOnStatusError is true.
bool FindPreviousSkippingZeroLengthGuard(HWND source, int flags, const char* pattern, int patternLength,
	int zeroLengthGuardFirst, int zeroLengthGuardLast, int rangeEnd, bool failOnStatusError, int& result,
	int* largestForwardRange = nullptr);

} // namespace Search
} // namespace AU
