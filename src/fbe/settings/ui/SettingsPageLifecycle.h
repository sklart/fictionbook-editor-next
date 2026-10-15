#pragma once

#include "../SettingsFileTransaction.h"
#include "../../utils/utils.h"

class ISettingsPage
{
public:
	virtual ~ISettingsPage() {}
	virtual bool Validate() = 0;
	virtual bool PrepareCommit(SettingsFileTransaction&, CString&) { return true; }
	virtual void Commit() = 0;
	virtual bool CancelChanges() = 0;
};

inline LRESULT ForwardSettingsDialogCommand(HWND page, WORD command)
{
	return ::SendMessageW(::GetParent(page), WM_COMMAND, MAKEWPARAM(command, BN_CLICKED), 0);
}

inline bool SettingsPreparationFailed(CString& error, const CString& path, DWORD code = ::GetLastError())
{
    error = path + L"\n" + U::Win32ErrMsg(code ? code : ERROR_WRITE_FAULT);
    return false;
}
