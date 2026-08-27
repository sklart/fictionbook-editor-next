#pragma once

class ISettingsPage
{
public:
	virtual ~ISettingsPage() {}
	virtual bool Validate() = 0;
	virtual void Commit() = 0;
	virtual bool CancelChanges() = 0;
};
