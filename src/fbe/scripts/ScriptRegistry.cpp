#include "stdafx.h"
#include "ScriptRegistry.h"
#include "../toolbars/PortableToolbarStore.h"
#include "..\\..\\common\\DeploymentContext.h"
#include <rpc.h>

#pragma comment(lib, "Rpcrt4.lib")

namespace
{
CString Escape(const CString& value) { CString result(value); result.Replace(L"&", L"&amp;"); result.Replace(L"\"", L"&quot;"); result.Replace(L"<", L"&lt;"); result.Replace(L">", L"&gt;"); return result; }
CString Unescape(const CString& value) { CString result(value); result.Replace(L"&quot;", L"\""); result.Replace(L"&lt;", L"<"); result.Replace(L"&gt;", L">"); result.Replace(L"&amp;", L"&"); return result; }
bool Attribute(const CString& tag, const wchar_t* name, CString& value) { CString key(name); key += L"=\""; const int first = tag.Find(key); if(first < 0) return false; const int begin = first + key.GetLength(); const int end = tag.Find(L'\"', begin); if(end < 0) return false; value = Unescape(tag.Mid(begin, end - begin)); return true; }

// Блокировка относится к профилю, а не к процессу: старый экземпляр не должен
// перезаписать назначения, уже опубликованные другим окном редактора.
class RegistryLock
{
public:
	explicit RegistryLock(const CString& path) : m_file(INVALID_HANDLE_VALUE)
	{
		for(int attempt = 0; attempt < 100; ++attempt)
		{
			m_file = ::CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
				OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
			if(m_file != INVALID_HANDLE_VALUE) break;
			if(::GetLastError() != ERROR_SHARING_VIOLATION && ::GetLastError() != ERROR_ACCESS_DENIED) break;
			::Sleep(20);
		}
	}
	~RegistryLock() { if(m_file != INVALID_HANDLE_VALUE) ::CloseHandle(m_file); }
	bool Acquired() const { return m_file != INVALID_HANDLE_VALUE; }
private:
	RegistryLock(const RegistryLock&) = delete;
	RegistryLock& operator=(const RegistryLock&) = delete;
	HANDLE m_file;
};
}

namespace FbeScripts
{
ScriptRegistry::ScriptRegistry() {}

CString ScriptRegistry::FilePath() const { return CString(DeploymentContext::SettingsDirectory().c_str()) + L"ScriptRegistry.xml"; }

bool ScriptRegistry::Load()
{
	HANDLE file = ::CreateFile(FilePath(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) { if(::GetLastError() != ERROR_FILE_NOT_FOUND) return false; m_identities.clear(); return true; }
	const DWORD length = ::GetFileSize(file, NULL);
	if(length == INVALID_FILE_SIZE || length > 1024 * 1024 || length % sizeof(wchar_t)) { ::CloseHandle(file); return false; }
	std::vector<wchar_t> text(length / sizeof(wchar_t) + 1, 0); DWORD read = 0; const bool readOk = ::ReadFile(file, &text[0], length, &read, NULL) != FALSE && read == length; ::CloseHandle(file);
	if(!readOk) return false;
	CString xml(&text[0]); if(xml.Find(L"<Scripts version=\"1\">") < 0 || xml.Find(L"</Scripts>") < 0) return false;
	std::vector<ScriptIdentity> parsed;
	int cursor = 0;
	while((cursor = xml.Find(L"<Script ", cursor)) >= 0) {
		const int end = xml.Find(L'>', cursor); if(end < 0) return false; CString tag = xml.Mid(cursor, end - cursor + 1), uid, path, fingerprint;
		if(!Attribute(tag, L"uid", uid) || !Attribute(tag, L"path", path) || !Attribute(tag, L"fingerprint", fingerprint) || uid.IsEmpty() || path.IsEmpty()) return false;
		for(size_t i = 0; i < parsed.size(); ++i) if(parsed[i].uid == uid || parsed[i].relativePath == path) return false;
		ScriptIdentity identity = {}; identity.uid = uid; identity.relativePath = path; identity.fingerprint = fingerprint;
		Attribute(tag, L"name", identity.name); parsed.push_back(identity);
		cursor = end + 1;
	}
	m_identities.swap(parsed);
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
	const CString directory(DeploymentContext::SettingsDirectory().c_str());
	if(!::CreateDirectoryW(directory, NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS) return false;
	RegistryLock lock(FilePath() + L".lock");
	if(!lock.Acquired()) return false;
	ScriptRegistry next;
	if(!next.Load()) return false;
	bool changed = false;
	// Не теряем отсутствующие на диске записи текущего сеанса. При конфликте
	// принимаем последнюю запись с диска, прочитанную уже под блокировкой.
	for(const auto& identity : m_identities)
		if(!next.FindByUid(identity.uid) && !next.FindByPath(identity.relativePath)) { next.m_identities.push_back(identity); changed = true; }

	// Самодостаточные назначения позволяют восстановить связь после утраты
	// реестра. Не угадываем соответствие при конфликте UID или пути.
	PortableToolbarLayout layout;
	if(PortableToolbarStore::Load(layout))
		for(const auto& toolbar : layout.scriptToolbars)
			for(const auto& item : toolbar.items)
				if(!item.scriptUid.IsEmpty() && !item.relativePath.IsEmpty() &&
					!next.FindByUid(item.scriptUid) && !next.FindByPath(item.relativePath))
				{
					ScriptIdentity identity = {}; identity.uid = item.scriptUid;
					identity.relativePath = item.relativePath; identity.name = item.scriptName;
					next.m_identities.push_back(identity); changed = true;
				}

	std::vector<ScriptDescriptor> resolved = items;
	std::vector<size_t> matches(resolved.size(), next.m_identities.size());
	const size_t unmatched = next.m_identities.size();
	std::vector<bool> claimed(next.m_identities.size(), false);
	// Сначала закрепляем ВСЕ существующие пути. Иначе новый файл с тем же
	// содержимым может забрать UID файла, до которого обход ещё не дошёл.
	for(size_t index = 0; index < resolved.size(); ++index)
		if(!resolved[index].isFolder)
			for(size_t i = 0; i < next.m_identities.size(); ++i)
				if(!claimed[i] && next.m_identities[i].relativePath == resolved[index].relativePath)
				{ matches[index] = i; claimed[i] = true; break; }
	for(size_t index = 0; index < resolved.size(); ++index) {
		ScriptDescriptor& script = resolved[index]; if(script.isFolder || script.relativePath.IsEmpty()) continue;
		const CString fingerprint = Fingerprint(script.path);
		size_t matched = matches[index];
		if(matched == unmatched && !fingerprint.IsEmpty())
		{
			size_t candidate = next.m_identities.size();
			for(size_t i = 0; i < next.m_identities.size(); ++i)
				if(!claimed[i] && next.m_identities[i].fingerprint == fingerprint)
				{
					if(candidate != next.m_identities.size()) { candidate = next.m_identities.size(); break; }
					candidate = i;
				}
			matched = candidate;
		}
		else if(matched == unmatched) matched = next.m_identities.size();
		if(matched == next.m_identities.size())
		{
			ScriptIdentity identity = { NewUid(), script.relativePath, fingerprint, script.name };
			if(identity.uid.IsEmpty()) return false;
			next.m_identities.push_back(identity);
			claimed.push_back(true);
			matched = next.m_identities.size() - 1;
			changed = true;
		}
		else
		{
			claimed[matched] = true;
			if(next.m_identities[matched].relativePath != script.relativePath || next.m_identities[matched].fingerprint != fingerprint || next.m_identities[matched].name != script.name)
			{
				next.m_identities[matched].relativePath = script.relativePath;
				next.m_identities[matched].fingerprint = fingerprint;
				next.m_identities[matched].name = script.name;
				changed = true;
			}
		}
		script.uid = next.m_identities[matched].uid;
	}
	if(changed && !next.Save()) return false;
	m_identities.swap(next.m_identities);
	items.swap(resolved);
	return true;
}

bool ScriptRegistry::Save() const
{
	const CString directory(DeploymentContext::SettingsDirectory().c_str()); if(!::CreateDirectory(directory, NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS) return false;
	CString xml(L"<Scripts version=\"1\">\r\n"); for(size_t i = 0; i < m_identities.size(); ++i) xml.AppendFormat(L"  <Script uid=\"%s\" path=\"%s\" fingerprint=\"%s\" name=\"%s\" />\r\n", static_cast<LPCWSTR>(Escape(m_identities[i].uid)), static_cast<LPCWSTR>(Escape(m_identities[i].relativePath)), static_cast<LPCWSTR>(Escape(m_identities[i].fingerprint)), static_cast<LPCWSTR>(Escape(m_identities[i].name))); xml += L"</Scripts>\r\n";
	if(xml.GetLength() > 512 * 1024) return false;
	const CString path = FilePath(), temporary = path + L"." + NewUid() + L".tmp";
	HANDLE file = ::CreateFile(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL); if(file == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	const DWORD bytes = static_cast<DWORD>(xml.GetLength() * sizeof(wchar_t));
	bool ok = ::WriteFile(file, xml, bytes, &written, NULL) != FALSE && written == bytes;
	if (ok && !::FlushFileBuffers(file))
		ok = false;
	::CloseHandle(file);
	if(!ok || !::MoveFileEx(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { ::DeleteFile(temporary); return false; } return true;
}

const ScriptIdentity* ScriptRegistry::FindByUid(const CString& uid) const { for(size_t i = 0; i < m_identities.size(); ++i) if(m_identities[i].uid == uid) return &m_identities[i]; return NULL; }
const ScriptIdentity* ScriptRegistry::FindByPath(const CString& relativePath) const { for(size_t i = 0; i < m_identities.size(); ++i) if(m_identities[i].relativePath == relativePath) return &m_identities[i]; return NULL; }
}
