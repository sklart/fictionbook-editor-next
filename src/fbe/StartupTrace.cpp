#include "stdafx.h"

#include "StartupTrace.h"
#include "../version.h"
#include <algorithm>
#include <vector>

namespace
{
	HANDLE traceFile = INVALID_HANDLE_VALUE;
	CRITICAL_SECTION traceLock;
	bool traceLockInitialized = false;
	ULONGLONG startTime = 0, previousTime = 0, writtenBytes = 0, recordSequence = 0;
	DWORD lastWriteError = ERROR_SUCCESS;
	CString tracePath, traceBasePath, lastStageCode, lastStageMessage, lastDocumentStage, lastScriptOperationStage, lastComFailure;
	unsigned int traceSegment = 0;
	const ULONGLONG maxTraceSize = 16ULL * 1024ULL * 1024ULL;
	const wchar_t* const diagnosticTraceRegistryPath = L"Software\\FBETeam\\FictionBook Editor Next\\Diagnostics";
	const wchar_t* const diagnosticTraceRegistryValue = L"TraceNextLaunch";

	class TraceLock { public: TraceLock() { if (traceLockInitialized) ::EnterCriticalSection(&traceLock); } ~TraceLock() { if (traceLockInitialized) ::LeaveCriticalSection(&traceLock); } };

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
		if(value.Find(L"<") >= 0 || value.Find(L">") >= 0 || lower.Find(L"base64") >= 0 || lower.Find(L"data:") >= 0)
			return L"details omitted";
		return value;
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

	bool OpenTraceFile(const CString& directory, const SYSTEMTIME& time)
	{
		::SHCreateDirectoryEx(NULL, directory, NULL);
		for (unsigned int suffix = 0; suffix != 100; ++suffix)
		{
			CString suffixText;
			if (suffix) suffixText.Format(L"-%u", suffix);
			traceBasePath.Format(L"%s\\fbe-trace-%04u%02u%02u-%02u%02u%02u-pid%lu%s", (LPCWSTR)directory, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, ::GetCurrentProcessId(), (LPCWSTR)suffixText);
			tracePath = traceBasePath + L".log";
			traceFile = ::CreateFile(tracePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
			if (traceFile != INVALID_HANDLE_VALUE) return true;
		}
		return false;
	}

	void CleanupOldTraceSessions(const CString& directory)
	{
		std::vector<CString> sessions;
		WIN32_FIND_DATA findData = {};
		HANDLE search = ::FindFirstFile(directory + L"\\fbe-trace-*.log", &findData);
		if (search == INVALID_HANDLE_VALUE) return;
		do
		{
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
			CString session(findData.cFileName);
			if (session.GetLength() < 5 || session.Right(4).CompareNoCase(L".log") != 0) continue;
			session = session.Left(session.GetLength() - 4);
			const int part = session.Find(L"-part");
			if (part >= 0) session = session.Left(part);
			bool known = false;
			for (size_t index = 0; index < sessions.size(); ++index) if (sessions[index].CompareNoCase(session) == 0) { known = true; break; }
			if (!known) sessions.push_back(session);
		}
		while (::FindNextFile(search, &findData));
		::FindClose(search);
		std::sort(sessions.begin(), sessions.end(), [](const CString& left, const CString& right) { return left.CompareNoCase(right) > 0; });
		for (size_t sessionIndex = 10; sessionIndex < sessions.size(); ++sessionIndex)
		{
			HANDLE files = ::FindFirstFile(directory + L"\\" + sessions[sessionIndex] + L"*.log", &findData);
			if (files == INVALID_HANDLE_VALUE) continue;
			do
			{
				if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
				CString fileName(findData.cFileName);
				if (fileName == sessions[sessionIndex] + L".log" || fileName.Left(sessions[sessionIndex].GetLength() + 5).CompareNoCase(sessions[sessionIndex] + L"-part") == 0)
					::DeleteFile(directory + L"\\" + fileName);
			}
			while (::FindNextFile(files, &findData));
			::FindClose(files);
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
		previousTime = now; lastStageCode = safeCode; lastStageMessage = safeMessage; if (safeCategory == L"document") lastDocumentStage = safeCode; if (safeCategory == L"script") lastScriptOperationStage = safeCode;
		WriteUtf8(line, flush, wcscmp(level, L"error") == 0);
	}

	void WriteEnvironmentHeader()
	{
		SYSTEM_INFO info = {};
		::GetNativeSystemInfo(&info);
		wchar_t exe[MAX_PATH] = {};
		::GetModuleFileName(NULL, exe, _countof(exe));
		CString details;
		details.Format(L"fbe=%s; native-arch=%u; acp=%u; oemcp=%u; exe=%s", FBE_VERSION_WSTRING, info.wProcessorArchitecture, ::GetACP(), ::GetOEMCP(), (LPCWSTR)StartupTrace::RedactPath(exe));
		StartupTrace::Event(L"environment", L"E010", details);
	}
	bool TryGetNextLaunchPreference(bool& enabled) { DWORD value = 0, size = sizeof(value); if (::RegGetValue(HKEY_CURRENT_USER, diagnosticTraceRegistryPath, diagnosticTraceRegistryValue, RRF_RT_REG_DWORD, NULL, &value, &size) != ERROR_SUCCESS) return false; enabled = value != 0; return true; }
	bool IsTraceEnabled(const wchar_t* variable) { wchar_t value[8] = {}; DWORD n = ::GetEnvironmentVariable(variable, value, _countof(value)); return n && n < _countof(value) && !(n == 1 && value[0] == L'0'); }
}

bool StartupTrace::IsEnabledForNextLaunch() { bool enabled = false; return TryGetNextLaunchPreference(enabled) ? enabled : IsTraceEnabled(L"FBE_NEXT_TRACE"); }
bool StartupTrace::SetEnabledForNextLaunch(bool enabled) { HKEY key = NULL; if (::RegCreateKeyEx(HKEY_CURRENT_USER, diagnosticTraceRegistryPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) return false; DWORD value = enabled ? 1 : 0; LONG result = ::RegSetValueEx(key, diagnosticTraceRegistryValue, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)); ::RegCloseKey(key); return result == ERROR_SUCCESS; }

void StartupTrace::Start()
{
	if (!IsEnabledForNextLaunch()) return;
	::InitializeCriticalSection(&traceLock); traceLockInitialized = true;
	wchar_t base[MAX_PATH] = {};
	const bool haveLocalAppData = SUCCEEDED(::SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, base));
	SYSTEMTIME time = {}; ::GetLocalTime(&time);
	bool opened = false;
	if (haveLocalAppData)
	{
		CString directory(base); directory.TrimRight(L"\\"); directory += L"\\FBE Next\\Diagnostics";
		opened = OpenTraceFile(directory, time);
	}
	if (!opened)
	{
		const DWORD primaryError = ::GetLastError();
		if (!::GetTempPath(_countof(base), base)) { lastWriteError = primaryError; return; }
		CString directory(base); directory.TrimRight(L"\\"); directory += L"\\FBE Next Diagnostics";
		opened = OpenTraceFile(directory, time);
		if (!opened) { lastWriteError = ::GetLastError() ? ::GetLastError() : primaryError; return; }
	}
	CleanupOldTraceSessions(CurrentLogDirectory());
	startTime = previousTime = ::GetTickCount64(); writtenBytes = recordSequence = traceSegment = 0;
	Event(L"environment", L"E000", L"FictionBook Editor diagnostic trace started");
	WriteEnvironmentHeader();
	Event(L"startup", L"S100", L"process started");
}
void StartupTrace::Mark(const wchar_t* stage) { Event(L"startup", L"S110", stage); }
bool StartupTrace::Enabled() { return traceFile != INVALID_HANDLE_VALUE; }
void StartupTrace::Event(const wchar_t* category, const wchar_t* code, const wchar_t* message) { WriteRecord(category, L"info", code, message, false); }
void StartupTrace::Warning(const wchar_t* category, const wchar_t* code, const wchar_t* message) { WriteRecord(category, L"warning", code, message, false); }
void StartupTrace::Event(const wchar_t* category, const wchar_t* message) { WriteRecord(category, L"info", L"-", message, false); }
void StartupTrace::Error(const wchar_t* category, const wchar_t* code, const wchar_t* message) { WriteRecord(category, L"error", code, message, true); }
void StartupTrace::HResult(const wchar_t* category, const wchar_t* code, HRESULT result, const wchar_t* message) { CString details; details.Format(L"hr=0x%08lX; %s", static_cast<unsigned long>(result), message ? message : L""); WriteRecord(category, FAILED(result) ? L"error" : L"info", code, details, FAILED(result)); if (FAILED(result)) { TraceLock guard; lastComFailure = Sanitize(code, 32, false) + L": " + Sanitize(details, 256, true); } }
void StartupTrace::ComException(const wchar_t* category, const wchar_t* code, HRESULT result,
	const EXCEPINFO* exceptionInfo, IErrorInfo* errorInfo, const wchar_t* message)
{
	CString details;
	details.Format(L"hr=0x%08lX; %s", static_cast<unsigned long>(result), message ? message : L"");
	if(exceptionInfo)
	{
		details.AppendFormat(L"; excep.wCode=%u; excep.scode=0x%08lX; excep.source=%s; excep.description=%s; excep.help=%d; excep.helpContext=%lu; excep.deferred=%d",
			exceptionInfo->wCode, static_cast<unsigned long>(exceptionInfo->scode),
			(LPCWSTR)SanitizeExceptionText(exceptionInfo->bstrSource),
			(LPCWSTR)SanitizeExceptionText(exceptionInfo->bstrDescription),
			exceptionInfo->bstrHelpFile ? 1 : 0, exceptionInfo->dwHelpContext,
			exceptionInfo->pfnDeferredFillIn ? 1 : 0);
	}
	if(errorInfo)
	{
		GUID guid = GUID_NULL;
		BSTR source = NULL, description = NULL, helpFile = NULL;
		DWORD helpContext = 0;
		errorInfo->GetGUID(&guid);
		errorInfo->GetSource(&source);
		errorInfo->GetDescription(&description);
		errorInfo->GetHelpFile(&helpFile);
		errorInfo->GetHelpContext(&helpContext);
		wchar_t guidText[64] = {};
		::StringFromGUID2(guid, guidText, _countof(guidText));
		details.AppendFormat(L"; errorInfo.guid=%s; errorInfo.source=%s; errorInfo.description=%s; errorInfo.helpContext=%lu; errorInfo.help=%d",
			guidText, (LPCWSTR)SanitizeExceptionText(source),
			(LPCWSTR)SanitizeExceptionText(description), helpContext, helpFile ? 1 : 0);
		::SysFreeString(source);
		::SysFreeString(description);
		::SysFreeString(helpFile);
	}
	WriteRecord(category, L"error", code, details, true);
	{ TraceLock guard; lastComFailure = Sanitize(code, 32, false) + L": " + Sanitize(details, 256, true); }
}
void StartupTrace::ScriptEvent(const wchar_t* code, const wchar_t* message) { CString safeMessage = SanitizeScriptDetails(message); const bool isError = safeMessage.Find(L"level=error") == 0; WriteRecord(L"script", isError ? L"error" : L"info", code, safeMessage, isError); }
void StartupTrace::Flush() { TraceLock guard; if (traceFile != INVALID_HANDLE_VALUE && !::FlushFileBuffers(traceFile)) lastWriteError = ::GetLastError(); }
void StartupTrace::EmergencyFlush() { if (traceFile != INVALID_HANDLE_VALUE) ::FlushFileBuffers(traceFile); }
CString StartupTrace::CurrentLogPath() { TraceLock guard; return tracePath; }
CString StartupTrace::CurrentLogDirectory() { TraceLock guard; const int separator = tracePath.ReverseFind(L'\\'); return separator >= 0 ? tracePath.Left(separator) : CString(); }
CString StartupTrace::LastStageCode() { TraceLock guard; return lastStageCode; }
CString StartupTrace::LastStageMessage() { TraceLock guard; return lastStageMessage; }
CString StartupTrace::LastDocumentStage() { TraceLock guard; return lastDocumentStage; }
CString StartupTrace::LastScriptOperationStage() { TraceLock guard; return lastScriptOperationStage; }
CString StartupTrace::LastComFailure() { TraceLock guard; return lastComFailure; }
DWORD StartupTrace::LastWriteError() { TraceLock guard; return lastWriteError; }
CString StartupTrace::NormalizeLogValue(const wchar_t* text, int maximumLength) { return Sanitize(text, maximumLength, false); }
CString StartupTrace::SanitizeLogText(const wchar_t* text, int maximumLength) { return Sanitize(text, maximumLength, true); }
CString StartupTrace::RedactPath(const wchar_t* text) { return Sanitize(text, 512, true); }
CString StartupTrace::SanitizeExceptionText(const wchar_t* text) { return Sanitize(text, 256, true); }
void StartupTrace::Finish() { if (traceFile != INVALID_HANDLE_VALUE) { Event(L"startup", L"S999", L"process shutdown"); Flush(); ::CloseHandle(traceFile); traceFile = INVALID_HANDLE_VALUE; traceBasePath.Empty(); } if (traceLockInitialized) { ::DeleteCriticalSection(&traceLock); traceLockInitialized = false; } }
