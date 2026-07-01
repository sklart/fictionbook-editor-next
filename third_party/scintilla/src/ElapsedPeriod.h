// Scintilla source code edit control
/** @file ElapsedPeriod.h
 ** Encapsulate C++ <chrono> to simplify use.
 **/
// Copyright 2018 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef ELAPSEDPERIOD_H
#define ELAPSEDPERIOD_H

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef FindText
#undef FindText
#endif
#else
#include <chrono>
#endif

namespace Scintilla::Internal {

// Simplified access to high precision timing.
class ElapsedPeriod {
#if defined(_WIN32)
	// MSVC STL chrono in current toolchains may pull a direct import of
	// GetSystemTimePreciseAsFileTime, which prevents loading Scintilla.dll on
	// Windows 7. QueryPerformanceCounter is available on supported Windows
	// versions and is enough for Scintilla elapsed-time measurements.
	LARGE_INTEGER frequency {};
	LARGE_INTEGER tp {};
public:
	/// Capture the moment
	ElapsedPeriod() noexcept {
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&tp);
	}
	/// Return duration as floating point seconds
	double Duration(bool reset=false) noexcept {
		LARGE_INTEGER tpNow {};
		QueryPerformanceCounter(&tpNow);
		const double duration =
			static_cast<double>(tpNow.QuadPart - tp.QuadPart) /
			static_cast<double>(frequency.QuadPart);
		if (reset) {
			tp = tpNow;
		}
		return duration;
	}
#else
	using ElapsedClock = std::chrono::steady_clock;
	ElapsedClock::time_point tp;
public:
	/// Capture the moment
	ElapsedPeriod() noexcept : tp(ElapsedClock::now()) {
	}
	/// Return duration as floating point seconds
	double Duration(bool reset=false) noexcept {
		const ElapsedClock::time_point tpNow = ElapsedClock::now();
		const std::chrono::duration<double> duration =
			std::chrono::duration_cast<std::chrono::duration<double>>(tpNow - tp);
		if (reset) {
			tp = tpNow;
		}
		return duration.count();
	}
#endif
};

}

#endif
