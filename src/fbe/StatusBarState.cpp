#include "stdafx.h"
#include "StatusBarState.h"

namespace FBEStatusBar
{
bool State::PromoteQueuedMessage(DWORD now, DWORD durationMilliseconds)
{
	if (m_message.IsEmpty()) return false;
	SetTransient(m_message, now, durationMilliseconds);
	m_message.Empty();
	return true;
}

void State::SetTransient(const CString& text, DWORD now, DWORD durationMilliseconds)
{
	m_transient = text;
	m_transientExpiration = now + durationMilliseconds;
}

bool State::ClearTransientIfExpired(DWORD now)
{
	if (m_transient.IsEmpty() || static_cast<LONG>(now - m_transientExpiration) < 0)
		return false;
	m_transient.Empty();
	m_transientExpiration = 0;
	return true;
}

CString State::EffectiveMainText(bool incrementalSearchActive,
	bool incrementalSearchFailed, const CString& incrementalSearchText) const
{
	if (incrementalSearchActive)
		return (incrementalSearchFailed ? L"Failing Incremental Search: " :
			L"Incremental Search: ") + incrementalSearchText;
	return !m_transient.IsEmpty() ? m_transient : m_context;
}

void State::ResetForDocument()
{
	m_message.Empty();
	m_context.Empty();
	m_transient.Empty();
	m_transientExpiration = 0;
	m_validation = ValidationStatus::Unknown;
}
}
