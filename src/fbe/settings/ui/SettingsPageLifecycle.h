#pragma once

#include "../SettingsFileTransaction.h"

class ISettingsPage
{
public:
	virtual ~ISettingsPage() {}
	virtual bool Validate() = 0;
	virtual bool PrepareCommit(SettingsFileTransaction&) { return true; }
	virtual void Commit() = 0;
	virtual bool CancelChanges() = 0;
};

inline LRESULT ForwardSettingsDialogCommand(HWND page, WORD command)
{
	return ::SendMessageW(::GetParent(page), WM_COMMAND, MAKEWPARAM(command, BN_CLICKED), 0);
}
