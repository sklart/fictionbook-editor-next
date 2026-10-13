#pragma once

#include <windows.h>
#include <atlstr.h>
#include <cstddef>

namespace ScriptCardBlock {

struct Span {
	size_t begin = 0;
	size_t end = 0;
};

enum class Status { Missing, Present, Malformed };

// Finds only the version-1 card immediately after a supported BOM (or at byte 0).
// End includes the single line ending inserted with the block; the BOM stays.
Status Find(const BYTE* bytes, size_t length, Span& span);

// Hashes the original byte stream with exactly the detected card span removed.
// Returns empty on malformed cards or cryptographic failure.
CString Sha256WithoutCard(const BYTE* bytes, size_t length);

}
