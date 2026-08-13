#pragma once

// Shared low-level writer for exporting embedded binary data.  The temporary
// file lives beside the destination, so a failed write never truncates an
// already exported image.  All APIs used here are available on Windows XP.
namespace BinaryFileSave
{
	inline bool WriteAtomically(const CString& destination, const void* data,
		DWORD byteCount, DWORD* error)
	{
		if (error)
			*error = ERROR_SUCCESS;
		if (destination.IsEmpty() || (byteCount != 0 && data == NULL))
		{
			if (error)
				*error = ERROR_INVALID_PARAMETER;
			return false;
		}

		CString directory(destination);
		const int slash = max(directory.ReverseFind(L'\\'), directory.ReverseFind(L'/'));
		if (slash < 0)
			directory = L".";
		else
			directory = directory.Left(slash + 1);

		wchar_t temporary[MAX_PATH] = {};
		if (::GetTempFileName(directory, L"fbe", 0, temporary) == 0)
		{
			if (error)
				*error = ::GetLastError();
			return false;
		}

		bool saved = false;
		HANDLE file = ::CreateFile(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE)
		{
			if (error)
				*error = ::GetLastError();
		}
		else
		{
			const BYTE* bytes = static_cast<const BYTE*>(data);
			DWORD remaining = byteCount;
			saved = true;
			while (remaining != 0)
			{
				DWORD written = 0;
				const BOOL writeResult = ::WriteFile(file, bytes, remaining, &written, NULL);
				if (!writeResult || written == 0)
				{
					saved = false;
					if (error)
						*error = writeResult ? ERROR_WRITE_FAULT : ::GetLastError();
					break;
				}
				bytes += written;
				remaining -= written;
			}
			if (saved && !::FlushFileBuffers(file))
			{
				saved = false;
				if (error)
					*error = ::GetLastError();
			}
			::CloseHandle(file);
		}

		if (saved && !::MoveFileEx(temporary, destination,
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			saved = false;
			if (error)
				*error = ::GetLastError();
		}
		if (!saved)
			::DeleteFile(temporary);
		return saved;
	}
}
