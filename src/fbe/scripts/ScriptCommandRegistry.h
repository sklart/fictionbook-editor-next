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
	CString Serialize() const;

private:
	int m_capacity;
	std::vector<CommandId> m_ids;
};
}
