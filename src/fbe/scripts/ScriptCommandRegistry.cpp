#include "stdafx.h"
#include "ScriptCommandRegistry.h"

namespace
{
CString Encode(const CString& value) { CString result; for (int i = 0; i < value.GetLength(); ++i) { CString unit; unit.Format(L"%04X", static_cast<unsigned int>(value[i])); result += unit; } return result; }
bool Decode(const CString& encoded, CString& value) { if (encoded.IsEmpty() || encoded.GetLength() % 4 != 0) return false; value.Empty(); for (int i = 0; i < encoded.GetLength(); i += 4) { const CString unit = encoded.Mid(i, 4); if (unit.SpanIncluding(L"0123456789ABCDEFabcdef").GetLength() != 4) return false; value.AppendChar(static_cast<wchar_t>(wcstoul(unit, NULL, 16))); } return true; }
DWORD Hash(const CString& value) { DWORD hash = 2166136261u; for (int i = 0; i < value.GetLength(); ++i) { hash ^= static_cast<DWORD>(value[i]); hash *= 16777619u; } return hash; }
}

namespace FbeScripts
{
CommandRegistry::CommandRegistry(int capacity, const CString& serialized) : m_capacity(capacity), m_dirty(false)
{
	int cursor = 0; CString entry;
	while (!(entry = serialized.Tokenize(L";", cursor)).IsEmpty())
	{
		const int split = entry.Find(L':'); CString path; const int value = split > 0 ? _wtoi(entry.Left(split)) : 0;
		if (value < 1 || value > m_capacity || split <= 0 || !Decode(entry.Mid(split + 1), path)) continue;
		bool duplicate = false; for (size_t i = 0; i < m_ids.size(); ++i) if (m_ids[i].value == value || m_ids[i].uid == path) { duplicate = true; break; }
		if (!duplicate) { CommandId id = { path, value }; m_ids.push_back(id); }
	}
}

int CommandRegistry::Assign(const CString& uid)
{
	for (size_t i = 0; i < m_ids.size(); ++i) if (m_ids[i].uid == uid) return m_ids[i].value;
	const int first = static_cast<int>(Hash(uid) % m_capacity) + 1;
	for (int attempt = 0; attempt < m_capacity; ++attempt) { const int value = ((first - 1 + attempt) % m_capacity) + 1; bool used = false; for (size_t i = 0; i < m_ids.size(); ++i) if (m_ids[i].value == value) { used = true; break; } if (!used) { CommandId id = { uid, value }; m_ids.push_back(id); m_dirty = true; return value; } }
	return -1;
}

void CommandRegistry::MigrateLegacyPath(const CString& relativePath, const CString& uid)
{
	if(relativePath.IsEmpty() || uid.IsEmpty()) return;
	for(size_t i = 0; i < m_ids.size(); ++i)
		if(m_ids[i].uid == relativePath) { m_ids[i].uid = uid; m_dirty = true; return; }
}

CString CommandRegistry::Serialize() const { CString serialized; for (size_t i = 0; i < m_ids.size(); ++i) { CString entry; entry.Format(L"%d:%s", m_ids[i].value, static_cast<LPCWSTR>(Encode(m_ids[i].uid))); if (!serialized.IsEmpty()) serialized += L";"; serialized += entry; } return serialized; }
}
