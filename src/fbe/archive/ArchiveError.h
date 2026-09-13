#pragma once

namespace FbeArchive
{
enum class ErrorCode
{
	None, OpenFailed, UnsupportedFormat, Corrupted, Encrypted,
	NoFictionBookEntries, EntryNotFound, EntryAmbiguous, EntryReadFailed,
	EntryTooLarge, WriteFailed, ReplaceFailed, ModifiedExternally
};

struct Error
{
	ErrorCode code = ErrorCode::None;
	DWORD systemError = ERROR_SUCCESS;
};
}
