#include "stdafx.h"

#include "StartupTrace.h"
#include "utils.h"
#include "..\\common\\DeploymentContext.h"
#include "../version.h"
#include <algorithm>
#include <vector>
#include <winver.h>

extern "C" { extern const char* build_timestamp; extern const char* build_commit; extern const char* build_configuration; }

namespace
{
	HANDLE traceFile = INVALID_HANDLE_VALUE;
	CRITICAL_SECTION traceLock;
	bool traceLockInitialized = false;
	ULONGLONG startTime = 0, previousTime = 0, writtenBytes = 0, recordSequence = 0;
	DWORD lastWriteError = ERROR_SUCCESS;
	CString tracePath, traceBasePath, lastStageCode, lastStageMessage, lastDocumentStage, lastScriptOperationStage, lastScriptFailureStage, lastComFailure, lastHResultFailure, lastDispatchFailure;
	unsigned int traceSegment = 0;
	const ULONGLONG maxTraceSize = 16ULL * 1024ULL * 1024ULL;
	const wchar_t* const diagnosticTraceRegistryPath = L"Software\\FBETeam\\FictionBook Editor Next\\Diagnostics";
	const wchar_t* const diagnosticTraceRegistryValue = L"TraceNextLaunch";

	class TraceLock { public: TraceLock() { if (traceLockInitialized) ::EnterCriticalSection(&traceLock); } ~TraceLock() { if (traceLockInitialized) ::LeaveCriticalSection(&traceLock); } };

	CString TrySnapshot(const CString& value)
	{
		// Crash reporting must not wait behind a trace writer. An empty snapshot
		// is safer than deadlocking the unhandled-exception path.
		if (!traceLockInitialized || !::TryEnterCriticalSection(&traceLock)) return CString();
		CString snapshot(value);
		::LeaveCriticalSection(&traceLock);
		return snapshot;
	}

	CString ReplaceEnvironmentValue(CString value, const wchar_t* variable, const wchar_t* token)
	{
		wchar_t path[MAX_PATH] = {};
		const DWORD length = ::GetEnvironmentVariable(variable, path, _countof(path));
		if (length && length < _countof(path)) value.Replace(path, token);
		return value;
	}

	CString RedactPathFragments(CString value)
	{
		for (int index = 0; index < value.GetLength();)
		{
			int start = -1;
			if (index + 2 < value.GetLength() && ((value[index] >= L'A' && value[index] <= L'Z') || (value[index] >= L'a' && value[index] <= L'z')) && value[index + 1] == L':' && (value[index + 2] == L'\\' || value[index + 2] == L'/')) start = index;
			else if (index + 1 < value.GetLength() && value[index] == L'\\' && value[index + 1] == L'\\') start = index;
			else if (value.Mid(index, 8).CompareNoCase(L"file:///") == 0) start = index;
			if (start < 0) { ++index; continue; }
			int end = start;
			while (end < value.GetLength() && value[end] != L';' && value[end] != L'\r' && value[end] != L'\n' && value[end] != L'\t') ++end;
			value = value.Left(start) + L"[path omitted]" + value.Mid(end);
			index = start + 14;
		}
		return value;
	}

	CString Sanitize(const wchar_t* text, int maximumLength, bool redactPaths)
	{
		CString value(text ? text : L"");
		value.Replace(L"\r", L" "); value.Replace(L"\n", L" "); value.Replace(L"\t", L" ");
		for (int index = 0; index < value.GetLength(); ++index) if (value[index] < L' ') value.SetAt(index, L' ');
		value = ReplaceEnvironmentValue(value, L"USERPROFILE", L"%USERPROFILE%");
		value = ReplaceEnvironmentValue(value, L"LOCALAPPDATA", L"%LOCALAPPDATA%");
		value = ReplaceEnvironmentValue(value, L"APPDATA", L"%APPDATA%");
		value = ReplaceEnvironmentValue(value, L"TEMP", L"%TEMP%");
		value = ReplaceEnvironmentValue(value, L"PROGRAMFILES", L"%PROGRAMFILES%");
		value = ReplaceEnvironmentValue(value, L"PROGRAMDATA", L"%PROGRAMDATA%");
		if (redactPaths) value = RedactPathFragments(value);
		if (maximumLength > 0 && value.GetLength() > maximumLength) value = value.Left(maximumLength) + L"...";
		return value;
	}

	CString SanitizeScriptDetails(const wchar_t* message)
	{
		CString value = Sanitize(message, 512, true);
		CString lower(value);
		lower.MakeLower();
		if (value.Find(L"<") < 0 && value.Find(L">") < 0 && lower.Find(L"base64") < 0 && lower.Find(L"data:") < 0)
			return value;

		// Retain the stable diagnostic fields even when an unsafe detail is removed.
		CString retained;
		int position = 0;
		while (position >= 0)
		{
			CString field = value.Tokenize(L";", position); field.Trim();
			CString fieldLower(field); fieldLower.MakeLower();
			if (fieldLower.Find(L"level=") == 0 || fieldLower.Find(L"failed-stage=") == 0 || fieldLower.Find(L"operation=") == 0 || fieldLower.Find(L"number=") == 0 || fieldLower.Find(L"line=") == 0)
			{
				if (!retained.IsEmpty()) retained += L"; ";
				retained += field;
			}
		}
		if (!retained.IsEmpty()) retained += L"; ";
		return retained + L"details omitted";
	}
	bool WriteAll(const char* bytes, DWORD byteCount)
	{
		while (byteCount)
		{
			DWORD written = 0;
			if (!::WriteFile(traceFile, bytes, byteCount, &written, NULL) || written == 0) { lastWriteError = ::GetLastError(); return false; }
			bytes += written; byteCount -= written; writtenBytes += written;
		}
		return true;
	}

	bool RotateTraceFile()
	{
		CString segmentPath;
		segmentPath.Format(L"%s-part%u.log", (LPCWSTR)traceBasePath, ++traceSegment);
		HANDLE nextFile = ::CreateFile(segmentPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if(nextFile == INVALID_HANDLE_VALUE)
		{
			lastWriteError = ::GetLastError();
			return false;
		}
		if(traceFile != INVALID_HANDLE_VALUE)
		{
			::FlushFileBuffers(traceFile);
			::CloseHandle(traceFile);
		}
		traceFile = nextFile;
		tracePath = segmentPath;
		writtenBytes = 0;
		return true;
	}

	CString ResolveDiagnosticLogDirectory()
	{
		wchar_t base[MAX_PATH] = {};
		if (SUCCEEDED(::SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, base)))
		{
			CString directory(base); directory.TrimRight(L"\\"); directory += L"\\FBE Next\\Diagnostics";
			return directory;
		}
		if (::GetTempPath(_countof(base), base))
		{
			CString directory(base); directory.TrimRight(L"\\"); directory += L"\\FBE Next Diagnostics";
			return directory;
		}
		return CString();
	}

	void ResolveDiagnosticLogDirectories(std::vector<CString>& directories)
	{
		directories.clear();
		wchar_t base[MAX_PATH] = {};
		if (SUCCEEDED(::SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, base)))
		{
			CString directory(base); directory.TrimRight(L"\\"); directory += L"\\FBE Next\\Diagnostics";
			directories.push_back(directory);
		}
		if (::GetTempPath(_countof(base), base))
		{
			CString directory(base); directory.TrimRight(L"\\"); directory += L"\\FBE Next Diagnostics";
			bool known = false;
			for (size_t index = 0; index < directories.size(); ++index) if (directories[index].CompareNoCase(directory) == 0) known = true;
			if (!known) directories.push_back(directory);
		}
	}

	struct DiagnosticLogName
	{
		unsigned int year, month, day, hour, minute, second, millisecond, processId, suffix, segment;
	};

	bool ParseDiagnosticLogNumber(const CString& text, int offset, int length, unsigned int& value)
	{
		if (length <= 0 || offset < 0 || offset + length > text.GetLength()) return false;
		unsigned long parsed = 0;
		for (int index = offset; index < offset + length; ++index)
		{
			if (text[index] < L'0' || text[index] > L'9') return false;
			if (parsed > (ULONG_MAX - static_cast<unsigned long>(text[index] - L'0')) / 10) return false;
			parsed = parsed * 10 + static_cast<unsigned long>(text[index] - L'0');
		}
		value = static_cast<unsigned int>(parsed);
		return true;
	}

	bool ParseDiagnosticLogName(const CString& name, CString& session, DiagnosticLogName& logName)
	{
		session.Empty(); memset(&logName, 0, sizeof(logName));
		if (name.GetLength() < 38 || name.Right(4).CompareNoCase(L".log") != 0) return false;
		const CString stem = name.Left(name.GetLength() - 4);
		if (stem.Left(10).CompareNoCase(L"fbe-trace-") != 0 || stem[18] != L'-' || stem[25] != L'-' || stem[29] != L'-' || stem.Mid(30, 3).CompareNoCase(L"pid") != 0) return false;
		if (!ParseDiagnosticLogNumber(stem, 10, 8, logName.year) || !ParseDiagnosticLogNumber(stem, 19, 6, logName.hour) || !ParseDiagnosticLogNumber(stem, 26, 3, logName.millisecond)) return false;
		logName.month = logName.year % 10000 / 100;
		logName.day = logName.year % 100;
		logName.year /= 10000;
		logName.minute = logName.hour % 10000 / 100;
		logName.second = logName.hour % 100;
		logName.hour /= 10000;
		int position = 33;
		int next = stem.Find(L'-', position);
		const int processIdEnd = next < 0 ? stem.GetLength() : next;
		if (!ParseDiagnosticLogNumber(stem, position, processIdEnd - position, logName.processId)) return false;
		while (next >= 0)
		{
			position = next + 1;
			next = stem.Find(L'-', position);
			const int end = next < 0 ? stem.GetLength() : next;
			if (stem.Mid(position, 4).CompareNoCase(L"part") == 0)
			{
				if (logName.segment != 0 || !ParseDiagnosticLogNumber(stem, position + 4, end - position - 4, logName.segment)) return false;
			}
			else if (logName.suffix == 0)
			{
				if (!ParseDiagnosticLogNumber(stem, position, end - position, logName.suffix)) return false;
			}
			else return false;
		}
		const int partMarker = stem.ReverseFind(L'-');
		session = logName.segment != 0 && partMarker >= 0 ? stem.Left(partMarker) : stem;
		return true;
	}

	int CompareDiagnosticSessionName(const DiagnosticLogName& left, const DiagnosticLogName& right)
	{
		const unsigned int* leftValues = &left.year;
		const unsigned int* rightValues = &right.year;
		for (size_t index = 0; index < 9; ++index)
		{
			if (leftValues[index] < rightValues[index]) return -1;
			if (leftValues[index] > rightValues[index]) return 1;
		}
		return 0;
	}

	ULONGLONG FileTimeValue(const FILETIME& value)
	{
		ULARGE_INTEGER result = {}; result.LowPart = value.dwLowDateTime; result.HighPart = value.dwHighDateTime;
		return result.QuadPart;
	}

	CString FindLatestTrace(const CString& /*ignoredDirectory*/)
	{
		std::vector<CString> directories;
		ResolveDiagnosticLogDirectories(directories);
		CString latestPath, latestSession;
		DiagnosticLogName latestLogName = {};
		bool hasLatestLogName = false;
		ULONGLONG latestWriteTime = 0;
		for (size_t directoryIndex = 0; directoryIndex < directories.size(); ++directoryIndex)
		{
			WIN32_FIND_DATA findData = {};
			HANDLE search = ::FindFirstFile(directories[directoryIndex] + L"\\fbe-trace-*.log", &findData);
			if (search == INVALID_HANDLE_VALUE) continue;
			do
			{
				if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
				CString session; DiagnosticLogName logName = {};
				if (!ParseDiagnosticLogName(findData.cFileName, session, logName)) continue;
				const ULONGLONG writeTime = FileTimeValue(findData.ftLastWriteTime);
				const int sessionOrder = hasLatestLogName ? CompareDiagnosticSessionName(logName, latestLogName) : 1;
				if (sessionOrder > 0 || (sessionOrder == 0 && (logName.segment > latestLogName.segment || (logName.segment == latestLogName.segment && writeTime > latestWriteTime))))
				{
					latestSession = session; latestLogName = logName; hasLatestLogName = true; latestWriteTime = writeTime;
					latestPath = directories[directoryIndex] + L"\\" + findData.cFileName;
				}
			}
			while (::FindNextFile(search, &findData));
			::FindClose(search);
		}
		return latestPath;
	}
	bool OpenTraceFile(const CString& directory, const SYSTEMTIME& time)
	{
		::SHCreateDirectoryEx(NULL, directory, NULL);
		for (unsigned int suffix = 0; suffix != 100; ++suffix)
		{
			CString suffixText;
			if (suffix) suffixText.Format(L"-%u", suffix);
			traceBasePath.Format(L"%s\\fbe-trace-%04u%02u%02u-%02u%02u%02u-%03u-pid%lu%s", (LPCWSTR)directory, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, ::GetCurrentProcessId(), (LPCWSTR)suffixText);
			tracePath = traceBasePath + L".log";
			traceFile = ::CreateFile(tracePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
			if (traceFile != INVALID_HANDLE_VALUE) return true;
		}
		return false;
	}

	void CleanupOldTraceSessions(const CString& directory, const CString& preserveSession, size_t sessionLimit, StartupTrace::DiagnosticLogCleanupResult* cleanup = NULL)
	{
		struct TraceSession { CString name; DiagnosticLogName logName; };
		std::vector<TraceSession> sessions;
		WIN32_FIND_DATA findData = {};
		HANDLE search = ::FindFirstFile(directory + L"\\fbe-trace-*.log", &findData);
		if (search == INVALID_HANDLE_VALUE)
		{
			const DWORD error = ::GetLastError();
			if (cleanup && error != ERROR_FILE_NOT_FOUND) { ++cleanup->filesFailed; cleanup->lastError = error; }
			return;
		}
		do
		{
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
			CString session; DiagnosticLogName logName = {};
			if (!ParseDiagnosticLogName(findData.cFileName, session, logName)) continue;
			bool known = false;
			for (size_t index = 0; index < sessions.size(); ++index) if (sessions[index].name.CompareNoCase(session) == 0) { known = true; break; }
			if (!known) { TraceSession entry = { session, logName }; sessions.push_back(entry); }
		}
		while (::FindNextFile(search, &findData));
		::FindClose(search);
		std::sort(sessions.begin(), sessions.end(), [](const TraceSession& left, const TraceSession& right) { return CompareDiagnosticSessionName(left.logName, right.logName) > 0; });
		for (size_t sessionIndex = sessionLimit; sessionIndex < sessions.size(); ++sessionIndex)
		{
			const CString& session = sessions[sessionIndex].name;
			if (session.CompareNoCase(preserveSession) == 0) continue;
			if (cleanup) ++cleanup->sessionsFound;
			bool sessionHasFiles = false;
			bool sessionHasFailures = false;
			bool sessionHasDeletedFiles = false;
			HANDLE files = ::FindFirstFile(directory + L"\\" + session + L"*.log", &findData);
			if (files == INVALID_HANDLE_VALUE)
			{
				if (cleanup) { ++cleanup->sessionsFailed; cleanup->lastError = ::GetLastError(); }
				continue;
			}
			do
			{
				if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
				CString fileName(findData.cFileName);
				if (fileName != session + L".log" && fileName.Left(session.GetLength() + 5).CompareNoCase(session + L"-part") != 0) continue;
				sessionHasFiles = true;
				const CString filePath = directory + L"\\" + fileName;
				if (::DeleteFile(filePath))
				{
					sessionHasDeletedFiles = true;
					if (cleanup) ++cleanup->filesDeleted;
				}
				else
				{
					sessionHasFailures = true;
					if (cleanup)
					{
						++cleanup->filesFailed;
						cleanup->lastError = ::GetLastError();
					}
				}
			}
			while (::FindNextFile(files, &findData));
			::FindClose(files);
			if (cleanup)
			{
				if (!sessionHasFiles || (!sessionHasDeletedFiles && sessionHasFailures)) ++cleanup->sessionsFailed;
				else if (sessionHasFailures) ++cleanup->sessionsPartiallyDeleted;
				else ++cleanup->sessionsFullyDeleted;
			}
		}
	}
	void WriteUtf8(const CString& text, bool flush, bool force)
	{
		if (traceFile == INVALID_HANDLE_VALUE) return;
		const int byteCount = ::WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), NULL, 0, NULL, NULL);
		if (byteCount <= 0) return;
		CStringA utf8; char* buffer = utf8.GetBuffer(byteCount);
		::WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), buffer, byteCount, NULL, NULL); utf8.ReleaseBuffer(byteCount);
		if(writtenBytes + byteCount > maxTraceSize && !RotateTraceFile() && !force)
		{
			lastWriteError = ERROR_DISK_FULL;
			return;
		}
		WriteAll(utf8, byteCount);
		if (flush && !::FlushFileBuffers(traceFile)) lastWriteError = ::GetLastError();
	}

	void WriteRecord(const wchar_t* category, const wchar_t* level, const wchar_t* code, const wchar_t* message, bool flush)
	{
		TraceLock guard;
		if (traceFile == INVALID_HANDLE_VALUE) return;
		const ULONGLONG now = ::GetTickCount64(); SYSTEMTIME time = {}; ::GetLocalTime(&time);
		const CString safeCategory = Sanitize(category, 64, false), safeCode = Sanitize(code, 32, false), safeMessage = Sanitize(message, 512, true);
		CString line;
		line.Format(L"%04u-%02u-%02u %02u:%02u:%02u.%03u; seq=%llu; elapsed=%llu; delta=%llu; PID=%lu; TID=%lu; level=%s; category=%s; code=%s; message=%s\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, ++recordSequence, now - startTime, now - previousTime, ::GetCurrentProcessId(), ::GetCurrentThreadId(), (LPCWSTR)level, (LPCWSTR)safeCategory, (LPCWSTR)(safeCode.IsEmpty() ? CString(L"-") : safeCode), (LPCWSTR)safeMessage);
		previousTime = now;
		lastStageCode = safeCode;
		lastStageMessage = safeMessage;
		if (safeCategory == L"document")
			lastDocumentStage = safeCode;

		const bool isJavaScriptOperation = safeCategory == L"script" && safeCode.GetLength() >= 4 && safeCode[0] == L'J';
		const bool isDiagnosticOrRestoreEvent = safeCode == L"J900" || safeCode == L"J901" || safeMessage.Find(L"operation=CSS restore") == 0;
		if (isJavaScriptOperation && !isDiagnosticOrRestoreEvent)
			lastScriptOperationStage = safeCode;
		WriteUtf8(line, flush, wcscmp(level, L"error") == 0);
	}

	CString GetLoadedModuleVersion(const wchar_t* moduleName)
	{
		HMODULE module = ::GetModuleHandle(moduleName);
		if (!module) return CString(L"not-loaded");
		wchar_t path[MAX_PATH] = {};
		if (!::GetModuleFileName(module, path, _countof(path))) return CString(L"unavailable");
		HMODULE versionLibrary = ::LoadLibrary(L"version.dll");
		if (!versionLibrary) return CString(L"unavailable");
		typedef DWORD (WINAPI* GetSizeFn)(LPCWSTR, LPDWORD);
		typedef BOOL (WINAPI* GetInfoFn)(LPCWSTR, DWORD, DWORD, LPVOID);
		typedef BOOL (WINAPI* QueryFn)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
		GetSizeFn getSize = reinterpret_cast<GetSizeFn>(::GetProcAddress(versionLibrary, "GetFileVersionInfoSizeW"));
		GetInfoFn getInfo = reinterpret_cast<GetInfoFn>(::GetProcAddress(versionLibrary, "GetFileVersionInfoW"));
		QueryFn query = reinterpret_cast<QueryFn>(::GetProcAddress(versionLibrary, "VerQueryValueW"));
		DWORD ignored = 0, size = getSize ? getSize(path, &ignored) : 0;
		CString result(L"unavailable");
		if (size && getInfo && query)
		{
			std::vector<BYTE> data(size);
			VS_FIXEDFILEINFO* fixed = NULL; UINT fixedSize = 0;
			if (getInfo(path, 0, size, &data[0]) && query(&data[0], L"\\", reinterpret_cast<void**>(&fixed), &fixedSize) && fixed && fixed->dwSignature == VS_FFI_SIGNATURE)
				result.Format(L"%u.%u.%u.%u", HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS), HIWORD(fixed->dwFileVersionLS), LOWORD(fixed->dwFileVersionLS));
		}
		::FreeLibrary(versionLibrary);
		return result;
	}

	CString ModuleArchitecture(const CString& path)
	{
		DWORD binaryType = 0;
		if (!path.IsEmpty() && ::GetBinaryType(path, &binaryType))
		{
			if (binaryType == SCS_32BIT_BINARY) return CString(L"x86");
			if (binaryType == SCS_64BIT_BINARY) return CString(L"x64");
		}
		return CString(L"unknown");
	}

	CString DescribeDiagnosticModule(const wchar_t* moduleName, bool mainExecutable = false)
	{
		HMODULE module = mainExecutable ? ::GetModuleHandle(NULL) : ::GetModuleHandle(moduleName);
		CString path;
		if (module)
		{
			wchar_t buffer[MAX_PATH] = {};
			if (::GetModuleFileName(module, buffer, _countof(buffer))) path = buffer;
		}
		else if (_wcsicmp(moduleName, L"Scintilla.dll") == 0 || _wcsicmp(moduleName, L"Lexilla.dll") == 0)
			path = U::GetProgDirFile(moduleName);
		else
		{
			wchar_t systemDirectory[MAX_PATH] = {};
			if (::GetSystemDirectory(systemDirectory, _countof(systemDirectory))) { path = systemDirectory; path.TrimRight(L"\\"); path += L"\\"; path += moduleName; }
		}
		const bool present = !path.IsEmpty() && ::GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
		CString details;
		details.Format(L"module=%s; present=%d; loaded=%d; file-version=%s; architecture=%s",
			moduleName, present ? 1 : 0, module ? 1 : 0,
			(LPCWSTR)(module ? GetLoadedModuleVersion(mainExecutable ? NULL : moduleName) : CString(L"not-loaded")),
			(LPCWSTR)ModuleArchitecture(path));
		return details;
	}

	const wchar_t* DetectDeployment(const CString& executablePath)
	{
		if (DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable)
			return L"portable";
		const HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
		const REGSAM views[] = { KEY_WOW64_32KEY, KEY_WOW64_64KEY };
		bool foundOtherInstalledLocation = false;
		for (size_t rootIndex = 0; rootIndex < _countof(roots); ++rootIndex) for (size_t viewIndex = 0; viewIndex < _countof(views); ++viewIndex) {
		HKEY key = NULL;
		if (::RegOpenKeyEx(roots[rootIndex], L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FictionBook Editor Next", 0, KEY_QUERY_VALUE | views[viewIndex], &key) != ERROR_SUCCESS) continue;
		wchar_t installedPath[MAX_PATH] = {}; DWORD type = 0, size = sizeof(installedPath);
		const LONG result = ::RegQueryValueEx(key, L"InstallLocation", NULL, &type, reinterpret_cast<BYTE*>(installedPath), &size);
		::RegCloseKey(key);
		if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || !installedPath[0]) continue;
		CString installed(installedPath); wchar_t expanded[MAX_PATH] = {};
		if (type == REG_EXPAND_SZ && ::ExpandEnvironmentStrings(installed, expanded, _countof(expanded))) installed = expanded;
		installed.TrimRight(L"\\");
		CString executableDirectory(executablePath); const int slash = executableDirectory.ReverseFind(L'\\'); if (slash >= 0) executableDirectory = executableDirectory.Left(slash);
		if (installed.CompareNoCase(executableDirectory) == 0) return L"installed";
		foundOtherInstalledLocation = true;
		}
		return foundOtherInstalledLocation ? L"portable" : L"unknown";
	}

	bool IsProcessElevated()
	{
		HANDLE token = NULL; TOKEN_ELEVATION elevation = {}; DWORD size = 0;
		const bool elevated = ::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token) && ::GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated != 0;
		if (token) ::CloseHandle(token);
		return elevated;
	}

	CString ProcessIntegrityLevel()
	{
		HANDLE token = NULL; DWORD size = 0;
		if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) return CString(L"unknown");
		::GetTokenInformation(token, TokenIntegrityLevel, NULL, 0, &size);
		std::vector<BYTE> data(size);
		CString result(L"unknown");
		if (size && ::GetTokenInformation(token, TokenIntegrityLevel, &data[0], size, &size))
		{
			TOKEN_MANDATORY_LABEL* label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(&data[0]);
			DWORD count = *::GetSidSubAuthorityCount(label->Label.Sid);
			DWORD level = *::GetSidSubAuthority(label->Label.Sid, count - 1);
			if (level < SECURITY_MANDATORY_MEDIUM_RID) result = L"low";
			else if (level < SECURITY_MANDATORY_HIGH_RID) result = L"medium";
			else if (level < SECURITY_MANDATORY_SYSTEM_RID) result = L"high";
			else result = L"system";
		}
		::CloseHandle(token);
		return result;
	}

		CString ReadFeatureControlValue(HKEY root, REGSAM view, const CString& feature, const CString& executable)
	{
		CString keyPath(L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\"); keyPath += feature;
		HKEY key = NULL;
		if (::RegOpenKeyEx(root, keyPath, 0, KEY_QUERY_VALUE | view, &key) != ERROR_SUCCESS) return CString(L"not-set");
		DWORD value = 0, size = sizeof(value), type = 0;
		const LONG result = ::RegQueryValueEx(key, executable, NULL, &type, reinterpret_cast<BYTE*>(&value), &size);
		::RegCloseKey(key);
		if (result != ERROR_SUCCESS || type != REG_DWORD || size != sizeof(value)) return CString(L"not-set");
		CString text; text.Format(L"%lu", value); return text;
	}

	void WriteFeatureControlSnapshot()
	{
		wchar_t path[MAX_PATH] = {}; ::GetModuleFileName(NULL, path, _countof(path));
		CString executable(path); const int slash = executable.ReverseFind(L'\\'); if (slash >= 0) executable = executable.Mid(slash + 1);
		const wchar_t* features[] = { L"FEATURE_BROWSER_EMULATION", L"FEATURE_DOCUMENT_COMPATIBLE_MODE", L"FEATURE_LOCALMACHINE_LOCKDOWN", L"FEATURE_BLOCK_LMZ_SCRIPT", L"FEATURE_RESTRICT_ACTIVEXINSTALL", L"FEATURE_ZONE_ELEVATION" };
		for (size_t index = 0; index < _countof(features); ++index)
		{
			const CString feature(features[index]);
			CString details; details.Format(L"feature=%s; hkcu32=%s; hkcu64=%s; hklm32=%s; hklm64=%s", (LPCWSTR)feature,
				(LPCWSTR)ReadFeatureControlValue(HKEY_CURRENT_USER, KEY_WOW64_32KEY, feature, executable), (LPCWSTR)ReadFeatureControlValue(HKEY_CURRENT_USER, KEY_WOW64_64KEY, feature, executable),
				(LPCWSTR)ReadFeatureControlValue(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, feature, executable), (LPCWSTR)ReadFeatureControlValue(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, feature, executable));
			StartupTrace::Event(L"environment", L"E022", details);
		}
	}
void WriteEnvironmentHeader()
	{
		SYSTEM_INFO info = {};
		::GetNativeSystemInfo(&info);
		SYSTEM_INFO processInfo = {};
		::GetSystemInfo(&processInfo);
		OSVERSIONINFOEX version = {};
		version.dwOSVersionInfoSize = sizeof(version);
		::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&version));
		wchar_t exe[MAX_PATH] = {};
		::GetModuleFileName(NULL, exe, _countof(exe));
		const CString executable(exe);
		const wchar_t* deployment = DetectDeployment(executable);
		const bool tempTrace = tracePath.Find(L"\\FBE Next Diagnostics\\") >= 0;
		CString details;
		CString buildTimestamp(build_timestamp), buildCommit(build_commit), buildConfiguration(build_configuration);
		details.Format(L"fbe=%s; build-configuration=%s; build-timestamp=%s; commit=%s; windows=%lu.%lu.%lu; service-pack=%u.%u; process-arch=%u; native-arch=%u; acp=%u; oemcp=%u; system-lcid=0x%04X; ui-language=0x%04X; deployment=%s; elevated=%d; integrity=%s; trace-location=%s; settings-location=%s; exe=%s",
			FBE_VERSION_WSTRING, (LPCWSTR)buildConfiguration, (LPCWSTR)buildTimestamp, (LPCWSTR)buildCommit,
			version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber, version.wServicePackMajor, version.wServicePackMinor, processInfo.wProcessorArchitecture,
			info.wProcessorArchitecture, ::GetACP(), ::GetOEMCP(), ::GetSystemDefaultLCID(),
			::GetUserDefaultUILanguage(), deployment, IsProcessElevated() ? 1 : 0, (LPCWSTR)ProcessIntegrityLevel(), tempTrace ? L"TEMP" : L"LOCALAPPDATA", (LPCWSTR)StartupTrace::RedactPath(U::GetSettingsDir() + L"Settings.xml"), (LPCWSTR)StartupTrace::RedactPath(exe));
		const wchar_t* modules[] = { L"mshtml.dll", L"ieframe.dll", L"urlmon.dll", L"wininet.dll", L"jscript.dll", L"jscript9.dll", L"msxml6.dll", L"oleaut32.dll", L"Scintilla.dll", L"Lexilla.dll" };
		StartupTrace::Event(L"environment", L"E011", DescribeDiagnosticModule(L"FBE.exe", true));
		for (size_t index = 0; index < _countof(modules); ++index) StartupTrace::Event(L"environment", L"E011", DescribeDiagnosticModule(modules[index]));
		StartupTrace::Event(L"environment", L"E010", details);
	}
	bool TryGetNextLaunchPreference(bool& enabled) { if (!DeploymentContext::RegistryPersistenceAllowed()) { const CString marker = CString(DeploymentContext::DataRoot().c_str()) + L"portable.ini"; enabled = ::GetPrivateProfileInt(L"Diagnostics", L"TraceNextLaunch", 0, marker) != 0; return true; } DWORD value = 0, size = sizeof(value); if (::RegGetValue(HKEY_CURRENT_USER, diagnosticTraceRegistryPath, diagnosticTraceRegistryValue, RRF_RT_REG_DWORD, NULL, &value, &size) != ERROR_SUCCESS) return false; enabled = value != 0; return true; }
	bool IsTraceEnabled(const wchar_t* variable) { wchar_t value[8] = {}; DWORD n = ::GetEnvironmentVariable(variable, value, _countof(value)); return n && n < _countof(value) && !(n == 1 && value[0] == L'0'); }
}

bool StartupTrace::IsEnabledForNextLaunch() { bool enabled = false; return IsTraceEnabled(L"FBE_NEXT_TRACE") || (TryGetNextLaunchPreference(enabled) && enabled); }
bool StartupTrace::IsEnabledByStoredNextLaunchPreference() { bool enabled = false; return TryGetNextLaunchPreference(enabled) && enabled; }
bool StartupTrace::SetEnabledForNextLaunch(bool enabled) { if (!DeploymentContext::RegistryPersistenceAllowed()) { const CString marker = CString(DeploymentContext::DataRoot().c_str()) + L"portable.ini"; return ::WritePrivateProfileString(L"Diagnostics", L"TraceNextLaunch", enabled ? L"1" : L"0", marker) != FALSE; } HKEY key = NULL; if (::RegCreateKeyEx(HKEY_CURRENT_USER, diagnosticTraceRegistryPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) return false; DWORD value = enabled ? 1 : 0; LONG result = ::RegSetValueEx(key, diagnosticTraceRegistryValue, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)); ::RegCloseKey(key); return result == ERROR_SUCCESS; }

void StartupTrace::Start()
{
	if (!IsEnabledForNextLaunch()) return;
	::InitializeCriticalSection(&traceLock); traceLockInitialized = true;
	wchar_t base[MAX_PATH] = {};
	const bool portable = DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable;
	const bool haveLocalAppData = !portable && SUCCEEDED(::SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, base));
	SYSTEMTIME time = {}; ::GetLocalTime(&time);
	bool opened = false;
	if (portable)
	{
		CString directory(DeploymentContext::DiagnosticsDirectory().c_str()); directory.TrimRight(L"\\");
		opened = OpenTraceFile(directory, time);
	}
	else if (haveLocalAppData)
	{
		CString directory(base); directory.TrimRight(L"\\"); directory += L"\\FBE Next\\Diagnostics";
		opened = OpenTraceFile(directory, time);
	}
	if (!opened && !portable)
	{
		const DWORD primaryError = ::GetLastError();
		if (!::GetTempPath(_countof(base), base)) { lastWriteError = primaryError; return; }
		CString directory(base); directory.TrimRight(L"\\"); directory += L"\\FBE Next Diagnostics";
		opened = OpenTraceFile(directory, time);
		if (!opened) { lastWriteError = ::GetLastError() ? ::GetLastError() : primaryError; return; }
	}
	std::vector<CString> retentionDirectories; ResolveDiagnosticLogDirectories(retentionDirectories); const CString activeSession = traceBasePath.Mid(traceBasePath.ReverseFind(L'\\') + 1); for (size_t index = 0; index < retentionDirectories.size(); ++index) { const CString preserve = tracePath.Left(retentionDirectories[index].GetLength()).CompareNoCase(retentionDirectories[index]) == 0 ? activeSession : CString(); CleanupOldTraceSessions(retentionDirectories[index], preserve, 10); }
	startTime = previousTime = ::GetTickCount64(); writtenBytes = recordSequence = traceSegment = 0;
	Event(L"environment", L"E000", L"FictionBook Editor diagnostic trace started");
	WriteEnvironmentHeader();
	Event(L"startup", L"S100", L"process started");
}
void StartupTrace::WriteLateEnvironmentHeader()
{
	Event(L"environment", L"E020", L"late environment snapshot after editor and browser initialization");
	const wchar_t* modules[] = { L"mshtml.dll", L"ieframe.dll", L"urlmon.dll", L"wininet.dll", L"jscript.dll", L"jscript9.dll", L"msxml6.dll", L"oleaut32.dll", L"Scintilla.dll", L"Lexilla.dll" };
	Event(L"environment", L"E021", DescribeDiagnosticModule(L"FBE.exe", true));
	for (size_t index = 0; index < _countof(modules); ++index) Event(L"environment", L"E021", DescribeDiagnosticModule(modules[index]));
  WriteFeatureControlSnapshot();
}
bool StartupTrace::Enabled() { return traceFile != INVALID_HANDLE_VALUE; }
void StartupTrace::Event(const wchar_t* category, const wchar_t* code, const wchar_t* message) { WriteRecord(category, L"info", code, message, false); }
void StartupTrace::Warning(const wchar_t* category, const wchar_t* code, const wchar_t* message) { WriteRecord(category, L"warning", code, message, false); }
void StartupTrace::Event(const wchar_t* category, const wchar_t* message) { WriteRecord(category, L"info", L"LEGACY", message, false); }
void StartupTrace::Error(const wchar_t* category, const wchar_t* code, const wchar_t* message) { WriteRecord(category, L"error", code, message, true); }
void StartupTrace::HResult(const wchar_t* category, const wchar_t* code, HRESULT result, const wchar_t* message) { CString details; details.Format(L"hr=0x%08lX; %s", static_cast<unsigned long>(result), message ? message : L""); WriteRecord(category, FAILED(result) ? L"error" : L"info", code, details, FAILED(result)); if (FAILED(result)) { TraceLock guard; const CString failure = Sanitize(code, 32, false) + L": " + Sanitize(details, 256, true); lastComFailure = failure; lastHResultFailure = failure; if (category && wcscmp(category, L"dispatch") == 0) lastDispatchFailure = failure; } }
void StartupTrace::DispatchResult(const wchar_t* category, const wchar_t* code, HRESULT result, const wchar_t* message) { HResult(category, code, result, message); if (FAILED(result)) { TraceLock guard; CString details; details.Format(L"hr=0x%08lX; %s", static_cast<unsigned long>(result), message ? message : L""); lastDispatchFailure = Sanitize(code, 32, false) + L": " + Sanitize(details, 256, true); } }
void StartupTrace::ComException(const wchar_t* category, const wchar_t* code, HRESULT result,
	const EXCEPINFO* exceptionInfo, IErrorInfo* errorInfo, const wchar_t* message)
{
	CString details;
	details.Format(L"hr=0x%08lX; %s", static_cast<unsigned long>(result), message ? message : L"");
	details += FormatComExceptionMetadata(exceptionInfo, errorInfo);
	WriteRecord(category, L"error", code, details, true);
	{ TraceLock guard; const CString failure = Sanitize(code, 32, false) + L": " + Sanitize(details, 256, true); lastComFailure = failure; lastHResultFailure = failure; }
}
void StartupTrace::ScriptEvent(const wchar_t* code, const wchar_t* message) { CString safeMessage = SanitizeScriptDetails(message); const bool isError = safeMessage.Find(L"level=error") == 0; WriteRecord(L"script", isError ? L"error" : L"info", code, safeMessage, isError); if (isError) { const int marker = safeMessage.Find(L"failed-stage="); if (marker >= 0) { CString stage = safeMessage.Mid(marker + 13); const int separator = stage.Find(L";"); if (separator >= 0) stage = stage.Left(separator); TraceLock guard; lastScriptFailureStage = Sanitize(stage, 32, false); } } }
void StartupTrace::Flush() { TraceLock guard; if (traceFile != INVALID_HANDLE_VALUE && !::FlushFileBuffers(traceFile)) lastWriteError = ::GetLastError(); }
void StartupTrace::EmergencyFlush() { if (traceFile != INVALID_HANDLE_VALUE) ::FlushFileBuffers(traceFile); }
CString StartupTrace::CurrentLogPath() { TraceLock guard; return tracePath.IsEmpty() ? FindLatestTrace(CString()) : tracePath; }
CString StartupTrace::CurrentLogDirectory() { TraceLock guard; const CString path = tracePath.IsEmpty() ? FindLatestTrace(CString()) : tracePath; const int separator = path.ReverseFind(L'\\'); return separator >= 0 ? path.Left(separator) : ResolveDiagnosticLogDirectory(); }
StartupTrace::DiagnosticLogCleanupResult StartupTrace::ClearOldLogSessions()
{
	TraceLock guard;
	std::vector<CString> directories;
	ResolveDiagnosticLogDirectories(directories);
	const int separator = traceBasePath.ReverseFind(L'\\');
	const CString activeSession = separator >= 0 ? traceBasePath.Mid(separator + 1) : traceBasePath;
	DiagnosticLogCleanupResult cleanup;
	bool foundDirectory = false;
	for (size_t index = 0; index < directories.size(); ++index)
	{
		if (directories[index].IsEmpty() || ::GetFileAttributes(directories[index]) == INVALID_FILE_ATTRIBUTES) continue;
		foundDirectory = true;
		const CString preserve = tracePath.Left(directories[index].GetLength()).CompareNoCase(directories[index]) == 0 ? activeSession : CString();
		CleanupOldTraceSessions(directories[index], preserve, 0, &cleanup);
	}
	if (!foundDirectory && cleanup.lastError == ERROR_SUCCESS) cleanup.lastError = ERROR_PATH_NOT_FOUND;
	return cleanup;
}
bool StartupTrace::TryGetCrashTraceSnapshot(CrashTraceSnapshot& snapshot)
{
	::ZeroMemory(&snapshot, sizeof(snapshot));
	snapshot.processId = ::GetCurrentProcessId();
	snapshot.threadId = ::GetCurrentThreadId();
	if (!traceLockInitialized || !::TryEnterCriticalSection(&traceLock)) return false;
	snapshot.snapshotAvailable = true;
	snapshot.diagnosticEnabled = traceFile != INVALID_HANDLE_VALUE;
	snapshot.usingTempFallback = tracePath.Find(L"\\FBE Next Diagnostics\\") >= 0;
	const int fileName = tracePath.ReverseFind(L'\\');
	if (fileName >= 0) wcsncpy_s(snapshot.currentLogPath, tracePath.Mid(fileName + 1), _TRUNCATE);
	else wcsncpy_s(snapshot.currentLogPath, tracePath, _TRUNCATE);
	const int separator = tracePath.ReverseFind(L'\\');
	if (separator >= 0) wcsncpy_s(snapshot.currentLogDirectory, tracePath.Left(separator), _TRUNCATE);
	wcsncpy_s(snapshot.lastEventCode, lastStageCode, _TRUNCATE);
	wcsncpy_s(snapshot.lastEventMessage, lastStageMessage, _TRUNCATE);
	wcsncpy_s(snapshot.lastDocumentStage, lastDocumentStage, _TRUNCATE);
	wcsncpy_s(snapshot.lastScriptOperationStage, lastScriptOperationStage, _TRUNCATE);
	wcsncpy_s(snapshot.lastScriptFailureStage, lastScriptFailureStage, _TRUNCATE);
	wcsncpy_s(snapshot.lastHResultFailure, lastHResultFailure, _TRUNCATE);
	wcsncpy_s(snapshot.lastDispatchFailure, lastDispatchFailure, _TRUNCATE);
	::LeaveCriticalSection(&traceLock);
	return true;
}

CString StartupTrace::LastStageCode() { return TrySnapshot(lastStageCode); }
CString StartupTrace::LastStageMessage() { return TrySnapshot(lastStageMessage); }
CString StartupTrace::LastDocumentStage() { return TrySnapshot(lastDocumentStage); }
CString StartupTrace::LastScriptOperationStage() { return TrySnapshot(lastScriptOperationStage); }
CString StartupTrace::LastComFailure() { return TrySnapshot(lastComFailure); }
DWORD StartupTrace::LastWriteError() { TraceLock guard; return lastWriteError; }
CString StartupTrace::NormalizeLogValue(const wchar_t* text, int maximumLength) { return Sanitize(text, maximumLength, false); }
CString StartupTrace::SanitizeLogText(const wchar_t* text, int maximumLength) { return Sanitize(text, maximumLength, true); }
CString StartupTrace::RedactPath(const wchar_t* text) { return Sanitize(text, 512, true); }
CString StartupTrace::FormatComExceptionMetadata(const EXCEPINFO* exceptionInfo, IErrorInfo* errorInfo)
{
	CString details;
	if (exceptionInfo)
	{
		details.AppendFormat(L"; excep.wCode=%u; excep.scode=0x%08lX; excep.source-present=%d; excep.source-length=%u; excep.description-present=%d; excep.description-length=%u; excep.help-present=%d; excep.helpContext=%lu; excep.deferred=%d; details=omitted",
			exceptionInfo->wCode, static_cast<unsigned long>(exceptionInfo->scode), exceptionInfo->bstrSource ? 1 : 0, exceptionInfo->bstrSource ? ::SysStringLen(exceptionInfo->bstrSource) : 0,
			exceptionInfo->bstrDescription ? 1 : 0, exceptionInfo->bstrDescription ? ::SysStringLen(exceptionInfo->bstrDescription) : 0,
			exceptionInfo->bstrHelpFile ? 1 : 0, exceptionInfo->dwHelpContext, exceptionInfo->pfnDeferredFillIn ? 1 : 0);
	}
	if (errorInfo)
	{
		GUID guid = GUID_NULL;
		BSTR source = NULL, description = NULL, helpFile = NULL;
		DWORD helpContext = 0;
		errorInfo->GetGUID(&guid); errorInfo->GetSource(&source); errorInfo->GetDescription(&description);
		errorInfo->GetHelpFile(&helpFile); errorInfo->GetHelpContext(&helpContext);
		wchar_t guidText[64] = {};
		::StringFromGUID2(guid, guidText, _countof(guidText));
		details.AppendFormat(L"; errorInfo.guid=%s; errorInfo.source-present=%d; errorInfo.source-length=%u; errorInfo.description-present=%d; errorInfo.description-length=%u; errorInfo.help-present=%d; errorInfo.helpContext=%lu; details=omitted",
			guidText, source ? 1 : 0, source ? ::SysStringLen(source) : 0, description ? 1 : 0,
			description ? ::SysStringLen(description) : 0, helpFile ? 1 : 0, helpContext);
		::SysFreeString(source); ::SysFreeString(description); ::SysFreeString(helpFile);
	}
	return details;
}
void StartupTrace::Finish() { if (traceFile != INVALID_HANDLE_VALUE) { Event(L"startup", L"S999", L"process shutdown"); Flush(); ::CloseHandle(traceFile); traceFile = INVALID_HANDLE_VALUE; traceBasePath.Empty(); } if (traceLockInitialized) { ::DeleteCriticalSection(&traceLock); traceLockInitialized = false; } }
