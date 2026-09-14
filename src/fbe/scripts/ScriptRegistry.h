#pragma once

#include "ScriptDescriptor.h"
#include <vector>

namespace FbeScripts
{
struct ScriptIdentity
{
	CString uid;
	CString relativePath;
	CString fingerprint;
};

// Keeps script identity out of .js files and out of the registry.  Missing
// records are deliberately retained so toolbar and hotkey references can
// reconnect when a script returns.
class ScriptRegistry
{
public:
	ScriptRegistry();
	bool Load();
	bool Resolve(std::vector<ScriptDescriptor>& items);
	const ScriptIdentity* FindByUid(const CString& uid) const;
	const ScriptIdentity* FindByPath(const CString& relativePath) const;

private:
	CString FilePath() const;
	bool Save() const;
	CString Fingerprint(const CString& path) const;
	CString NewUid() const;
	std::vector<ScriptIdentity> m_identities;
};
}
