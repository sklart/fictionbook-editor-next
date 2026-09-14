#pragma once

#include <vector>

namespace FbeScripts
{
struct CommandId { CString uid; int value; };

class CommandRegistry
{
public:
	CommandRegistry(int capacity, const CString& serialized);
	int Assign(const CString& uid);
	// Rebind a pre-UID persisted key while preserving its runtime slot.
	void MigrateLegacyPath(const CString& relativePath, const CString& uid);
	bool IsDirty() const { return m_dirty; }
	CString Serialize() const;

private:
	int m_capacity;
	bool m_dirty;
	std::vector<CommandId> m_ids;
};
}
