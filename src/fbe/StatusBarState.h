#pragma once

#include <atlstr.h>

namespace FBEStatusBar
{
enum class ValidationStatus
{
	Unknown,
	Valid,
	Invalid
};

// Owns only logical status-bar state. Window, layout and localization remain
// outside this model.
class State
{
public:
	void QueueMessage(const CString& text) { m_message = text; }
	bool PromoteQueuedMessage(DWORD now, DWORD durationMilliseconds = 5000);
	void SetContext(const CString& text) { m_context = text; }
	void SetTransient(const CString& text, DWORD now, DWORD durationMilliseconds = 5000);
	bool ClearTransientIfExpired(DWORD now);
	void SetValidation(ValidationStatus status) { m_validation = status; }
	ValidationStatus Validation() const { return m_validation; }
	CString EffectiveMainText(bool incrementalSearchActive, bool incrementalSearchFailed,
		const CString& incrementalSearchText) const;
	void ResetForDocument();

private:
	CString m_message;
	CString m_context;
	CString m_transient;
	DWORD m_transientExpiration = 0;
	ValidationStatus m_validation = ValidationStatus::Unknown;
};
}
