#include "ScintillaRegexSearch.h"

#include <algorithm>

#include "Scintilla.h"

namespace AU {
namespace Search {
namespace {

int PositionAfter(HWND source, int position)
{
	return static_cast<int>(::SendMessage(source, SCI_POSITIONAFTER, position, 0));
}

int PositionBefore(HWND source, int position)
{
	return static_cast<int>(::SendMessage(source, SCI_POSITIONBEFORE, position, 0));
}

} // namespace

bool FindPreviousSkippingZeroLengthGuard(HWND source, int flags, const char* pattern, int patternLength,
	int zeroLengthGuardFirst, int zeroLengthGuardLast, int rangeEnd, bool failOnStatusError, int& result,
	int* largestForwardRange)
{
	auto search = [&](int start, int end)
	{
		::SendMessage(source, SCI_SETTARGETSTART, start, 0);
		::SendMessage(source, SCI_SETTARGETEND, end, 0);
		::SendMessage(source, SCI_SETSEARCHFLAGS, flags, 0);
		::SendMessage(source, SCI_SETSTATUS, SC_STATUS_OK, 0);
		result = static_cast<int>(::SendMessage(source, SCI_SEARCHINTARGET, patternLength, reinterpret_cast<LPARAM>(pattern)));
		return result != -1 || !failOnStatusError || ::SendMessage(source, SCI_GETSTATUS, 0, 0) == SC_STATUS_OK;
	};
	auto isProtectedZeroLengthHit = [&]()
	{
		const int hitStart = static_cast<int>(::SendMessage(source, SCI_GETTARGETSTART, 0, 0));
		const int hitEnd = static_cast<int>(::SendMessage(source, SCI_GETTARGETEND, 0, 0));
		return hitStart == hitEnd && hitStart >= zeroLengthGuardFirst && hitStart <= zeroLengthGuardLast;
	};

	const int guardLine = static_cast<int>(::SendMessage(source, SCI_LINEFROMPOSITION, zeroLengthGuardFirst, 0));
	const int lineStart = static_cast<int>(::SendMessage(source, SCI_POSITIONFROMLINE, guardLine, 0));
	const int lineEnd = static_cast<int>(::SendMessage(source, SCI_GETLINEENDPOSITION, guardLine, 0));
	const int scanBegin = (std::max)(lineStart, rangeEnd);
	if (scanBegin < zeroLengthGuardFirst)
	{
		int scanStart = scanBegin;
		int selectedStart = -1;
		int selectedEnd = -1;
		while (scanStart <= lineEnd)
		{
			if (largestForwardRange != nullptr)
				*largestForwardRange = (std::max)(*largestForwardRange, lineEnd - scanStart);
			if (!search(scanStart, lineEnd))
				return false;
			if (result == -1)
				break;
			const int foundStart = static_cast<int>(::SendMessage(source, SCI_GETTARGETSTART, 0, 0));
			const int foundEnd = static_cast<int>(::SendMessage(source, SCI_GETTARGETEND, 0, 0));
			if (!isProtectedZeroLengthHit() && foundStart < zeroLengthGuardFirst && foundEnd <= zeroLengthGuardFirst)
			{
				selectedStart = foundStart;
				selectedEnd = foundEnd;
			}
			const int cursor = foundEnd > foundStart ? foundEnd : foundStart;
			const int next = PositionAfter(source, cursor);
			if (next == cursor || next > lineEnd)
				break;
			scanStart = next;
		}
		if (selectedStart != -1)
		{
			::SendMessage(source, SCI_SETTARGETSTART, selectedStart, 0);
			::SendMessage(source, SCI_SETTARGETEND, selectedEnd, 0);
			result = selectedStart;
			return true;
		}
	}

	const int previousLineEnd = PositionBefore(source, lineStart);
	if (previousLineEnd <= rangeEnd)
	{
		result = -1;
		return true;
	}
	return search(previousLineEnd, rangeEnd);
}

} // namespace Search
} // namespace AU
