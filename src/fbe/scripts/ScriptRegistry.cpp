#include "stdafx.h"
#include "ScriptRegistry.h"
#include "..\\..\\common\\DeploymentContext.h"
#include <rpc.h>

#pragma comment(lib, "Rpcrt4.lib")

namespace
{
CString Escape(const CString& value) { CString result(value); result.Replace(L"&", L"&amp;"); result.Replace(L"\"", L"&quot;"); result.Replace(L"<", L"&lt;"); result.Replace(L">", L"&gt;"); return result; }
CString Unescape(const CString& value) { CString result(value); result.Replace(L"&quot;", L"\""); result.Replace(L"&lt;", L"<"); result.Replace(L"&gt;", L">"); result.Replace(L"&amp;", L"&"); return result; }
bool Attribute(const CString& tag, const wchar_t* name, CString& value) { CString key(name); key += L"=\""; const int first = tag.Find(key); if(first < 0) return false; const int begin = first + key.GetLength(); const int end = tag.Find(L'\"', begin); if(end < 0) return false; value = Unescape(tag.Mid(begin, end - begin)); return true; }
}

namespace FbeScripts
{
ScriptRegistry::ScriptRegistry() {}

CString ScriptRegistry::FilePath() const { return CString(DeploymentContext::SettingsDirectory().c_str()) + L"ScriptRegistry.xml"; }

bool ScriptRegistry::Load()
{
	m_identities.clear();
	HANDLE file = ::CreateFile(FilePath(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) return ::GetLastError() == ERROR_FILE_NOT_FOUND;
	const DWORD length = ::GetFileSize(file, NULL);
	if(length == INVALID_FILE_SIZE || length > 1024 * 1024 || length % sizeof(wchar_t)) { ::CloseHandle(file); return false; }
	std::vector<wchar_t> text(length / sizeof(wchar_t) + 1, 0); DWORD read = 0; const bool readOk = ::ReadFile(file, &text[0], length, &read, NULL) != FALSE && read == length; ::CloseHandle(file);
	if(!readOk) return false;
	CString xml(&text[0]); if(xml.Find(L"<Scripts version=\"1\">") < 0 || xml.Find(L"</Scripts>") < 0) return false;
	int cursor = 0;
	while((cursor = xml.Find(L"<Script ", cursor)) >= 0) {
		const int end = xml.Find(L'>', cursor); if(end < 0) return false; CString tag = xml.Mid(cursor, end - cursor + 1), uid, path, fingerprint;
		if(!Attribute(tag, L"uid", uid) || !Attribute(tag, L"path", path) || !Attribute(tag, L"fingerprint", fingerprint) || uid.IsEmpty() || path.IsEmpty()) return false;
		bool duplicate = false; for(size_t i = 0; i < m_identities.size(); ++i) if(m_identities[i].uid == uid || m_identities[i].relativePath == path) { duplicate = true; break; }
		if(!duplicate) { ScriptIdentity identity = { uid, path, fingerprint }; m_identities.push_back(identity); }
		cursor = end + 1;
	}
	return true;
}

CString ScriptRegistry::Fingerprint(const CString& path) const
{
	HANDLE file = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); if(file == INVALID_HANDLE_VALUE) return CString();
	unsigned __int64 hash = 1469598103934665603ULL; BYTE buffer[4096]; DWORD read = 0;
	while(::ReadFile(file, buffer, sizeof(buffer), &read, NULL) && read) for(DWORD i = 0; i < read; ++i) { hash ^= buffer[i]; hash *= 1099511628211ULL; }
	::CloseHandle(file); CString fingerprint; fingerprint.Format(L"%016I64X", hash); return fingerprint;
}

CString ScriptRegistry::NewUid() const
{
	UUID uuid = {}; const RPC_STATUS created = ::UuidCreate(&uuid); if(created != RPC_S_OK && created != RPC_S_UUID_LOCAL_ONLY) return CString(); RPC_WSTR string = NULL; if(::UuidToString(&uuid, &string) != RPC_S_OK || string == NULL) return CString(); CString uid(reinterpret_cast<wchar_t*>(string)); ::RpcStringFree(&string); return uid;
}

bool ScriptRegistry::Resolve(std::vector<ScriptDescriptor>& items)
{
	bool changed = false;
	for(size_t index = 0; index < items.size(); ++index) {
		ScriptDescriptor& script = items[index]; if(script.isFolder || script.relativePath.IsEmpty()) continue;
		const CString fingerprint = Fingerprint(script.path); ScriptIdentity* matched = NULL;
		for(size_t i = 0; i < m_identities.size(); ++i) if(m_identities[i].relativePath == script.relativePath) { matched = &m_identities[i]; break; }
		if(matched == NULL && !fingerprint.IsEmpty()) { ScriptIdentity* candidate = NULL; for(size_t i = 0; i < m_identities.size(); ++i) if(m_identities[i].fingerprint == fingerprint) { if(candidate != NULL) { candidate = NULL; break; } candidate = &m_identities[i]; } matched = candidate; }
		if(matched == NULL) { ScriptIdentity identity = { NewUid(), script.relativePath, fingerprint }; if(identity.uid.IsEmpty()) return false; m_identities.push_back(identity); matched = &m_identities.back(); changed = true; }
		else if(matched->relativePath != script.relativePath || matched->fingerprint != fingerprint) { matched->relativePath = script.relativePath; matched->fingerprint = fingerprint; changed = true; }
		script.uid = matched->uid;
	}
	return !changed || Save();
}

bool ScriptRegistry::Save() const
{
	const CString directory(DeploymentContext::SettingsDirectory().c_str()); if(!::CreateDirectory(directory, NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS) return false;
	CString xml(L"<Scripts version=\"1\">\r\n"); for(size_t i = 0; i < m_identities.size(); ++i) xml.AppendFormat(L"  <Script uid=\"%s\" path=\"%s\" fingerprint=\"%s\" />\r\n", static_cast<LPCWSTR>(Escape(m_identities[i].uid)), static_cast<LPCWSTR>(Escape(m_identities[i].relativePath)), static_cast<LPCWSTR>(Escape(m_identities[i].fingerprint))); xml += L"</Scripts>\r\n";
	const CString path = FilePath(), temporary = path + L".tmp"; HANDLE file = ::CreateFile(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); if(file == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0; const DWORD bytes = static_cast<DWORD>(xml.GetLength() * sizeof(wchar_t)); const bool ok = ::WriteFile(file, xml, bytes, &written, NULL) != FALSE && written == bytes; if(ok) ::FlushFileBuffers(file); ::CloseHandle(file);
	if(!ok || !::MoveFileEx(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { ::DeleteFile(temporary); return false; } return true;
}

const ScriptIdentity* ScriptRegistry::FindByUid(const CString& uid) const { for(size_t i = 0; i < m_identities.size(); ++i) if(m_identities[i].uid == uid) return &m_identities[i]; return NULL; }
const ScriptIdentity* ScriptRegistry::FindByPath(const CString& relativePath) const { for(size_t i = 0; i < m_identities.size(); ++i) if(m_identities[i].relativePath == relativePath) return &m_identities[i]; return NULL; }
}
