#pragma once

#include "RuntimeLocalization.h"
#include "utils.h"

// Keep the low-level writer free of UI dependencies.  Both image-export entry
// points use this helper after a real filesystem failure.
inline void ShowBinarySaveFailure(HWND owner, const CString& destination, DWORD error)
{
	CString reason = U::Win32ErrMsg(error);
	reason.TrimRight(L"\r\n");
	if (reason.IsEmpty())
		reason = FbeLoadRuntimeStringByKey(
			L"fbe.binary_save.unknown_error", L"Unknown Windows error");

	const CString templateText = FbeLoadRuntimeStringByKey(
		L"fbe.binary_save.failed.message",
		L"Could not save the file.\r\n\r\nFile:\r\n%s\r\n\r\nReason:\r\n%s (Windows error %lu)");
	CString message;
	message.Format(templateText, destination.GetString(), reason.GetString(), error);
	const CString caption = FbeLoadRuntimeStringByKey(L"fbe.binary_save.failed.caption", L"Save image");
	::MessageBox(owner, message, caption, MB_OK | MB_ICONERROR);
}
