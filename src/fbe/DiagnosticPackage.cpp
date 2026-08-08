#include "stdafx.h"

#include "StartupTrace.h"
#include "utils.h"
#include <string>
#include <vector>

namespace
{
	struct ZipEntry { std::string name; DWORD crc; DWORD size; DWORD offset; };
	class ZipStoreWriter
	{
	public:
		ZipStoreWriter() : file(INVALID_HANDLE_VALUE) {}
		~ZipStoreWriter() { if (file != INVALID_HANDLE_VALUE) ::CloseHandle(file); }
		bool Open(const CString& path) { file = ::CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL); return file != INVALID_HANDLE_VALUE; }
		bool Add(const char* name, const std::vector<BYTE>& bytes)
		{
			if (file == INVALID_HANDLE_VALUE || strlen(name) > 0xffff || bytes.size() > 0xffffffff) return false;
			ZipEntry entry = { name, Crc32(bytes), static_cast<DWORD>(bytes.size()), Tell() };
			Write32(0x04034b50); Write16(20); Write16(0x0800); Write16(0); Write16(0); Write16(0); Write32(entry.crc); Write32(entry.size); Write32(entry.size); Write16(static_cast<WORD>(entry.name.size())); Write16(0); Write(entry.name.data(), static_cast<DWORD>(entry.name.size())); if (entry.size) Write(&bytes[0], entry.size);
			entries.push_back(entry); return ok;
		}
		bool Close()
		{
			const DWORD directory = Tell();
			for (size_t index = 0; index < entries.size(); ++index) { const ZipEntry& entry = entries[index]; Write32(0x02014b50); Write16(20); Write16(20); Write16(0x0800); Write16(0); Write16(0); Write16(0); Write32(entry.crc); Write32(entry.size); Write32(entry.size); Write16(static_cast<WORD>(entry.name.size())); Write16(0); Write16(0); Write16(0); Write16(0); Write32(0); Write32(entry.offset); Write(entry.name.data(), static_cast<DWORD>(entry.name.size())); }
			const DWORD size = Tell() - directory; Write32(0x06054b50); Write16(0); Write16(0); Write16(static_cast<WORD>(entries.size())); Write16(static_cast<WORD>(entries.size())); Write32(size); Write32(directory); Write16(0);
			const bool result = ok; ::CloseHandle(file); file = INVALID_HANDLE_VALUE; return result;
		}
	private:
		HANDLE file; bool ok = true; std::vector<ZipEntry> entries;
		DWORD Tell() const { return ::SetFilePointer(file, 0, NULL, FILE_CURRENT); }
		void Write(const void* data, DWORD size) { DWORD written = 0; if (!size) return; if (!::WriteFile(file, data, size, &written, NULL) || written != size) ok = false; }
		void Write16(WORD value) { BYTE bytes[2] = { static_cast<BYTE>(value), static_cast<BYTE>(value >> 8) }; Write(bytes, 2); }
		void Write32(DWORD value) { BYTE bytes[4] = { static_cast<BYTE>(value), static_cast<BYTE>(value >> 8), static_cast<BYTE>(value >> 16), static_cast<BYTE>(value >> 24) }; Write(bytes, 4); }
		static DWORD Crc32(const std::vector<BYTE>& bytes) { DWORD crc = 0xffffffff; for (size_t index = 0; index < bytes.size(); ++index) { crc ^= bytes[index]; for (int bit = 0; bit < 8; ++bit) crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320 : crc >> 1; } return crc ^ 0xffffffff; }
	};

	bool ReadBytes(const CString& path, std::vector<BYTE>& bytes)
	{
		HANDLE file = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE) return false;
		LARGE_INTEGER size = {}; const bool valid = ::GetFileSizeEx(file, &size) && size.QuadPart >= 0 && size.QuadPart <= 64 * 1024 * 1024;
		if (!valid) { ::CloseHandle(file); return false; }
		bytes.resize(static_cast<size_t>(size.QuadPart)); DWORD read = 0; const bool result = bytes.empty() || (::ReadFile(file, &bytes[0], static_cast<DWORD>(bytes.size()), &read, NULL) && read == bytes.size()); ::CloseHandle(file); return result;
	}

	bool ContainsUnsafeContent(const std::vector<BYTE>& bytes)
	{
		std::string text(bytes.empty() ? "" : reinterpret_cast<const char*>(&bytes[0]), bytes.size()); std::string lower(text); for (size_t index = 0; index < lower.size(); ++index) if (lower[index] >= 'A' && lower[index] <= 'Z') lower[index] = static_cast<char>(lower[index] - 'A' + 'a');
		if (lower.find("<html") != std::string::npos || lower.find("<?xml") != std::string::npos || lower.find("base64") != std::string::npos || lower.find("data:") != std::string::npos || lower.find("file://") != std::string::npos) return true;
		for (size_t index = 0; index + 2 < text.size(); ++index) if (((text[index] >= 'A' && text[index] <= 'Z') || (text[index] >= 'a' && text[index] <= 'z')) && text[index + 1] == ':' && (text[index + 2] == '\\' || text[index + 2] == '/')) return true;
		return false;
	}

	CString ZipName(const CString& value)
	{
		CString name(value); name.Replace(L"\\", L"_"); name.Replace(L"/", L"_"); return name;
	}

	std::vector<BYTE> Utf8(const CString& value)
	{
		const int count = ::WideCharToMultiByte(CP_UTF8, 0, value, value.GetLength(), NULL, 0, NULL, NULL); std::vector<BYTE> bytes(count); if (count) ::WideCharToMultiByte(CP_UTF8, 0, value, value.GetLength(), reinterpret_cast<char*>(&bytes[0]), count, NULL, NULL); return bytes;
	}

	void AddSessionFiles(const CString& currentLog, std::vector<CString>& files)
	{
		const int slash = currentLog.ReverseFind(L'\\'); if (slash < 0) return; const CString directory = currentLog.Left(slash); CString name = currentLog.Mid(slash + 1); const int part = name.Find(L"-part"); if (part >= 0) name = name.Left(part) + L".log"; if (name.Right(4).CompareNoCase(L".log") != 0) return; const CString base = name.Left(name.GetLength() - 4);
		WIN32_FIND_DATA find = {}; HANDLE search = ::FindFirstFile(directory + L"\\" + base + L"*.log", &find); if (search == INVALID_HANDLE_VALUE) return;
		do { if (!(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) files.push_back(directory + L"\\" + find.cFileName); } while (::FindNextFile(search, &find)); ::FindClose(search);
	}

	CString FindLatestCrashText()
	{
		const CString directory(U::GetSettingsDir() + L"Crashes\\"); WIN32_FIND_DATA find = {}; HANDLE search = ::FindFirstFile(directory + L"FBENext-crash-*.txt", &find); if (search == INVALID_HANDLE_VALUE) return CString();
		CString latest; FILETIME latestTime = {};
		do { if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue; if (::CompareFileTime(&find.ftLastWriteTime, &latestTime) > 0) { latestTime = find.ftLastWriteTime; latest = directory + find.cFileName; } } while (::FindNextFile(search, &find)); ::FindClose(search); return latest;
	}

	CString ExtractCategoryLines(const CString& trace, const wchar_t* category)
	{
		CString result; int position = 0; while (position >= 0) { CString line = trace.Tokenize(L"\n", position); if (line.Find(CString(L"category=") + category + L";") >= 0) result += line + L"\r\n"; } return result;
	}
}

bool StartupTrace::CreateDiagnosticPackage(CString& packagePath, CString& error)
{
	packagePath.Empty(); error.Empty(); Flush(); const CString currentLog(CurrentLogPath()); if (currentLog.IsEmpty()) { error = L"No diagnostic trace session is available."; return false; }
	std::vector<CString> files; AddSessionFiles(currentLog, files); if (files.empty()) { error = L"The diagnostic trace session could not be found."; return false; }
	std::vector<std::vector<BYTE> > contents; CString traceText;
	for (size_t index = 0; index < files.size(); ++index) { std::vector<BYTE> bytes; if (!ReadBytes(files[index], bytes) || ContainsUnsafeContent(bytes)) { error = L"Privacy scan rejected a diagnostic trace file."; return false; } contents.push_back(bytes); const int chars = ::MultiByteToWideChar(CP_UTF8, 0, bytes.empty() ? "" : reinterpret_cast<const char*>(&bytes[0]), static_cast<int>(bytes.size()), NULL, 0); if (chars) { CString text; ::MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&bytes[0]), static_cast<int>(bytes.size()), text.GetBuffer(chars), chars); text.ReleaseBuffer(chars); traceText += text; } }
	CString modules(L"FBE.exe\r\nmshtml.dll\r\nieframe.dll\r\nurlmon.dll\r\nwininet.dll\r\njscript.dll\r\njscript9.dll\r\nmsxml6.dll\r\noleaut32.dll\r\nScintilla.dll\r\nLexilla.dll\r\n");
	CString manifest(L"FBE Next diagnostic package\r\nContains only privacy-checked diagnostic records.\r\nCrash dumps, books, settings, recovery files, scripts, images and clipboard data are excluded.\r\n");
	const CString crashTextPath(FindLatestCrashText()); std::vector<BYTE> crashText;
	if (!crashTextPath.IsEmpty() && (!ReadBytes(crashTextPath, crashText) || ContainsUnsafeContent(crashText))) { error = L"Privacy scan rejected the crash report."; return false; }
	std::vector<BYTE> environment = Utf8(ExtractCategoryLines(traceText, L"environment")), typelib = Utf8(ExtractCategoryLines(traceText, L"typelib")), moduleList = Utf8(modules), manifestBytes = Utf8(manifest);
	if (ContainsUnsafeContent(environment) || ContainsUnsafeContent(typelib) || ContainsUnsafeContent(moduleList) || ContainsUnsafeContent(manifestBytes)) { error = L"Privacy scan rejected generated diagnostic content."; return false; }
	SYSTEMTIME now = {}; ::GetLocalTime(&now); CString output; output.Format(L"%s\\FBE-Diagnostics-%04u%02u%02u-%02u%02u%02u.zip", (LPCWSTR)CurrentLogDirectory(), now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
	ZipStoreWriter zip; if (!zip.Open(output)) { error = L"Could not create the diagnostic package."; return false; }
	bool added = zip.Add("package-manifest.txt", manifestBytes) && zip.Add("environment-report.txt", environment) && zip.Add("fbelib-report.txt", typelib) && zip.Add("diagnostic-modules.txt", moduleList);
	if (added && !crashTextPath.IsEmpty()) added = zip.Add("crash/latest-crash-report.txt", crashText);
	for (size_t index = 0; added && index < files.size(); ++index) { CString leaf = files[index].Mid(files[index].ReverseFind(L'\\') + 1); const int length = ::WideCharToMultiByte(CP_UTF8, 0, leaf, leaf.GetLength(), NULL, 0, NULL, NULL); std::string name("trace/"); name.resize(6 + length); ::WideCharToMultiByte(CP_UTF8, 0, leaf, leaf.GetLength(), &name[6], length, NULL, NULL); added = zip.Add(name.c_str(), contents[index]); }
	if (!added || !zip.Close()) { ::DeleteFile(output); error = L"Could not write the diagnostic package."; return false; }
	packagePath = output; Event(L"diagnostic", L"DG130", L"privacy-checked diagnostic package created"); return true;
}
