#include "stdafx.h"
#include "ScriptCardParser.h"
#include "ScriptCardBlock.h"
#include <bcrypt.h>
#include <string>

#pragma comment(lib, "bcrypt.lib")

namespace ScriptCards {
namespace {

std::wstring FinalPath(HANDLE file)
{
	const DWORD length = ::GetFinalPathNameByHandleW(file, NULL, 0, FILE_NAME_NORMALIZED);
	if(length == 0) return {};
	std::wstring path(length + 1, L'\0');
	const DWORD written = ::GetFinalPathNameByHandleW(file, &path[0], static_cast<DWORD>(path.size()), FILE_NAME_NORMALIZED);
	if(written == 0 || written > length) return {};
	path.resize(written);
	return path;
}

CString FileSha256(HANDLE file)
{
	BCRYPT_ALG_HANDLE algorithm = NULL;
	BCRYPT_HASH_HANDLE hash = NULL;
	BYTE digest[32] = {};
	CString result;
	if(!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0)))
		return result;
	bool good = BCRYPT_SUCCESS(::BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0));
	BYTE buffer[65536];
	for(;;)
	{
		DWORD read = 0;
		if(!good || !::ReadFile(file, buffer, sizeof(buffer), &read, NULL)) { good = false; break; }
		if(read == 0) break;
		good = BCRYPT_SUCCESS(::BCryptHashData(hash, buffer, read, 0));
	}
	if(good && BCRYPT_SUCCESS(::BCryptFinishHash(hash, digest, sizeof(digest), 0)))
		for(BYTE value : digest) result.AppendFormat(L"%02x", value);
	if(hash != NULL) ::BCryptDestroyHash(hash);
	::BCryptCloseAlgorithmProvider(algorithm, 0);
	return result;
}

CString LinkedScriptSha256(HANDLE file)
{
	LARGE_INTEGER size = {};
	if(!::GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > INT_MAX) return CString();
	std::vector<BYTE> bytes(static_cast<size_t>(size.QuadPart));
	size_t position = 0;
	while(position < bytes.size())
	{
		DWORD read = 0;
		if(!::ReadFile(file, bytes.data() + position,
			static_cast<DWORD>((std::min)(bytes.size() - position, size_t(65536))), &read, NULL) || read == 0)
			return CString();
		position += read;
	}
	// A card-only edit does not change the executable revision of a linked JS.
	return ScriptCardBlock::Sha256WithoutCard(bytes.empty() ? reinterpret_cast<const BYTE*>("") : bytes.data(), bytes.size());
}

}

void VerifyDependencies(Card& card, const CString& resourceRoot)
{
	card.dependencyState = Card::DependenciesState::Unverified;
	if(card.state == State::Invalid || card.state == State::Missing) return;
	HANDLE root = ::CreateFileW(resourceRoot, FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if(root == INVALID_HANDLE_VALUE) return;
	std::wstring rootPath = FinalPath(root);
	::CloseHandle(root);
	if(rootPath.empty()) return;
	while(!rootPath.empty() && rootPath.back() == L'\\') rootPath.pop_back();
	rootPath += L'\\';
	bool changed = false, unverified = !card.dependenciesComplete, invalid = false;
	for(Dependency& dependency : card.dependencies)
	{
		dependency.currentSha256.Empty();
		CString path(resourceRoot);
		if(!path.IsEmpty() && path[path.GetLength() - 1] != L'\\') path += L'\\';
		CString relative(dependency.path);
		relative.Replace(L'/', L'\\');
		path += relative;
		HANDLE file = ::CreateFileW(path, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
			FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if(file == INVALID_HANDLE_VALUE)
		{
			const DWORD error = ::GetLastError();
			if(error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
			{
				dependency.state = dependency.optional ? Dependency::State::OptionalMissing : Dependency::State::Missing;
				if(dependency.optional) unverified = true; else changed = true;
			}
			else { dependency.state = Dependency::State::Unverified; unverified = true; }
			continue;
		}
		const std::wstring finalPath = FinalPath(file);
		const DWORD attributes = ::GetFileAttributesW(path);
		if(finalPath.size() <= rootPath.size() || _wcsnicmp(finalPath.c_str(), rootPath.c_str(), rootPath.size()) != 0 ||
			attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			dependency.state = Dependency::State::Invalid;
			invalid = true;
		}
		else if(dependency.expectedSha256.IsEmpty())
		{
			dependency.state = Dependency::State::Unverified;
			unverified = true;
		}
		else
		{
			dependency.currentSha256 = dependency.path.Right(3).CompareNoCase(L".js") == 0 ?
				LinkedScriptSha256(file) : FileSha256(file);
			if(dependency.currentSha256.IsEmpty()) { dependency.state = Dependency::State::Unverified; unverified = true; }
			else if(dependency.currentSha256 == dependency.expectedSha256) dependency.state = Dependency::State::Current;
			else { dependency.state = Dependency::State::Changed; changed = true; }
		}
		::CloseHandle(file);
	}
	card.dependencyState = invalid ? Card::DependenciesState::Invalid :
		changed ? Card::DependenciesState::Changed :
		unverified ? Card::DependenciesState::Unverified : Card::DependenciesState::Current;
}

}
