#pragma once

#include <vector>

namespace FbeScripts
{
struct CommandId { CString relativePath; int value; };

class CommandRegistry
{
public:
	CommandRegistry(int capacity, const CString& serialized);
	int Assign(const CString& relativePath);
	bool IsDirty() const { return m_dirty; }
	CString Serialize() const;

private:
	int m_capacity;
	bool m_dirty;
	std::vector<CommandId> m_ids;
};
}
