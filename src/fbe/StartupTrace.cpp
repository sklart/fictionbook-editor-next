#include "stdafx.h"

#include "StartupTrace.h"
#include "../version.h"

namespace
{
	HANDLE traceFile = INVALID_HANDLE_VALUE;
	ULONGLONG startTime = 0;
	ULONGLONG previousTime = 0;
	ULONGLONG writtenBytes = 0;
	ULONGLONG recordSequence = 0;
	bool traceLimitReached = false;
	const ULONGLONG maxTraceSize = 4ULL * 1024ULL * 1024ULL;
	const wchar_t* const diagnosticTraceRegistryPath =
		L"Software\\FBETeam\\FictionBook Editor Next\\Diagnostics";
	const wchar_t* const diagnosticTraceRegistryValue = L"TraceNextLaunch";

	void WriteUtf8(HANDLE file, const wchar_t* text, bool flush)
	{
		if (file == INVALID_HANDLE_VALUE || traceLimitReached)
			return;

		const int byteCount = ::WideCharToMultiByte(
			CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
		if (byteCount <= 1)
			return;

		CStringA utf8;
		char* buffer = utf8.GetBuffer(byteCount);
		::WideCharToMultiByte(
			CP_UTF8, 0, text, -1, buffer, byteCount, NULL, NULL);
		utf8.ReleaseBuffer(byteCount - 1);

		if (writtenBytes + utf8.GetLength() > maxTraceSize)
		{
			static const char limitMessage[] =
				"[trace] size limit reached; further events are omitted\r\n";
			DWORD written = 0;
			::WriteFile(file, limitMessage, sizeof(limitMessage) - 1, &written, NULL);
			writtenBytes += written;
			traceLimitReached = true;
			::FlushFileBuffers(file);
			return;
		}

		DWORD written = 0;
		::WriteFile(file, utf8.GetString(), utf8.GetLength(), &written, NULL);
		writtenBytes += written;
		if (flush)
			::FlushFileBuffers(file);
	}

	bool TryGetNextLaunchPreference(bool& enabled)
	{
		DWORD value = 0;
		DWORD valueSize = sizeof(value);
		const LONG result = ::RegGetValue(
			HKEY_CURRENT_USER,
			diagnosticTraceRegistryPath,
			diagnosticTraceRegistryValue,
			RRF_RT_REG_DWORD,
			NULL,
			&value,
			&valueSize);

		if (result != ERROR_SUCCESS)
			return false;

		enabled = value != 0;
		return true;
	}

	bool IsTraceEnabled(const wchar_t* variable)
	{
		wchar_t enabled[8] = {};
		const DWORD length = ::GetEnvironmentVariable(
			variable, enabled, _countof(enabled));
		return length != 0 && length < _countof(enabled) &&
			!(length == 1 && enabled[0] == L'0');
	}
	const wchar_t* GetProcessorArchitectureName(WORD architecture)
	{
		switch (architecture)
		{
		case PROCESSOR_ARCHITECTURE_AMD64:
			return L"x64";
		case PROCESSOR_ARCHITECTURE_ARM64:
			return L"ARM64";
		case PROCESSOR_ARCHITECTURE_INTEL:
			return L"x86";
		default:
			return L"unknown";
		}
	}

	void WriteRecord(const wchar_t* category, const wchar_t* level,
		const wchar_t* code, const wchar_t* message, bool flush)
	{
		if (traceFile == INVALID_HANDLE_VALUE)
			return;

		const ULONGLONG now = ::GetTickCount64();
		SYSTEMTIME time = {};
		::GetLocalTime(&time);
		CString line;
		line.Format(L"%04u-%02u-%02u %02u:%02u:%02u.%03u; seq=%llu; +%llu; delta=%llu; pid=%lu; tid=%lu; %s; %s; %s; %s\r\n",
			time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
			time.wSecond, time.wMilliseconds, ++recordSequence, now - startTime, now - previousTime,
			::GetCurrentProcessId(), ::GetCurrentThreadId(), category, level,
			code && *code ? code : L"-", message ? message : L"");
		previousTime = now;
		WriteUtf8(traceFile, line, flush);
	}
}

bool StartupTrace::IsEnabledForNextLaunch()
{
	bool enabled = false;
	if (TryGetNextLaunchPreference(enabled))
		return enabled;

	return IsTraceEnabled(L"FBE_NEXT_TRACE");
}

bool StartupTrace::SetEnabledForNextLaunch(bool enabled)
{
	HKEY key = NULL;
	const LONG createResult = ::RegCreateKeyEx(
		HKEY_CURRENT_USER,
		diagnosticTraceRegistryPath,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_SET_VALUE,
		NULL,
		&key,
		NULL);

	if (createResult != ERROR_SUCCESS)
		return false;

	const DWORD value = enabled ? 1 : 0;
	const LONG setResult = ::RegSetValueEx(
		key,
		diagnosticTraceRegistryValue,
		0,
		REG_DWORD,
		reinterpret_cast<const BYTE*>(&value),
		sizeof(value));
	::RegCloseKey(key);

	return setResult == ERROR_SUCCESS;
}
void StartupTrace::Start()
{
	if (!IsEnabledForNextLaunch())
		return;

	wchar_t localAppData[MAX_PATH] = {};
	if (FAILED(::SHGetFolderPath(
		NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL,
		SHGFP_TYPE_CURRENT, localAppData)))
	{
		return;
	}

	CString directory(localAppData);
	directory += L"\\FBE Next";
	::CreateDirectory(directory, NULL);

	startTime = ::GetTickCount64();
	previousTime = startTime;
	recordSequence = 0;
	const CString path = directory + L"\\fbe-trace.log";
	const CString previousPath = directory + L"\\fbe-trace.previous.log";
	::DeleteFile(previousPath);
	::MoveFileEx(path, previousPath, MOVEFILE_REPLACE_EXISTING);
	traceFile = ::CreateFile(
		path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (traceFile != INVALID_HANDLE_VALUE)
	{
		writtenBytes = 0;
		traceLimitReached = false;
		WriteUtf8(traceFile, L"FictionBook Editor diagnostic trace\r\n", false);
		SYSTEM_INFO systemInfo = {};
		::GetNativeSystemInfo(&systemInfo);
		CString environment;
		environment.Format(L"version=%s; process=Win32; os-architecture=%s",
			FBE_VERSION_WSTRING,
			GetProcessorArchitectureName(systemInfo.wProcessorArchitecture));
		WriteRecord(L"environment", L"info", L"E000", environment, false);
		WriteRecord(L"startup", L"info", L"S000", L"process started", false);
	}
}

void StartupTrace::Mark(const wchar_t* stage)
{
	WriteRecord(L"startup", L"info", L"S001", stage, false);
}

bool StartupTrace::Enabled()
{
	return traceFile != INVALID_HANDLE_VALUE;
}

void StartupTrace::Event(const wchar_t* category, const wchar_t* code, const wchar_t* message)
{
	WriteRecord(category, L"info", code, message, false);
}

void StartupTrace::Warning(const wchar_t* category, const wchar_t* code, const wchar_t* message)
{
	WriteRecord(category, L"warning", code, message, false);
}

void StartupTrace::ComException(const wchar_t* category, const wchar_t* code, HRESULT result,
	const EXCEPINFO* exceptionInfo, IErrorInfo* errorInfo, const wchar_t* message)
{
	CString details(message ? message : L"");
	if (exceptionInfo)
		details.AppendFormat(L"; excep.wCode=%u; excep.scode=0x%08lX; excep.description=%s",
			exceptionInfo->wCode, static_cast<unsigned long>(exceptionInfo->scode),
			(LPCWSTR)SanitizeExceptionText(exceptionInfo->bstrDescription));
	if (errorInfo)
		details += L"; IErrorInfo-present";
	WriteRecord(category, L"error", code, details, true);
}
void StartupTrace::Event(const wchar_t* category, const wchar_t* stage)
{
	CString message(stage ? stage : L"");
	CString code;
	const int separator = message.Find(L' ');
	if (separator > 1)
	{
		const CString candidate(message.Left(separator));
		bool isStageCode = candidate[0] >= L'A' && candidate[0] <= L'Z';
		for (int index = 1; isStageCode && index < candidate.GetLength(); ++index)
			isStageCode = candidate[index] >= L'0' && candidate[index] <= L'9';
		if (isStageCode)
		{
			code = candidate;
			message = message.Mid(separator + 1);
		}
	}
	WriteRecord(category, L"info", code, message, false);
}

void StartupTrace::Error(const wchar_t* category, const wchar_t* code,
	const wchar_t* message)
{
	WriteRecord(category, L"error", code, message, true);
}

void StartupTrace::HResult(const wchar_t* category, const wchar_t* code,
	HRESULT result, const wchar_t* message)
{
	CString details;
	details.Format(L"hr=0x%08lX%s%s", static_cast<unsigned long>(result),
		message && *message ? L"; " : L"", message && *message ? message : L"");
	WriteRecord(category, FAILED(result) ? L"error" : L"info", code,
		details, FAILED(result));
}

void StartupTrace::ScriptEvent(const wchar_t* code, const wchar_t* message)
{
	CString safeMessage(message ? message : L"");
	if (safeMessage.GetLength() > 512)
		safeMessage = safeMessage.Left(512) + L"…";
	WriteRecord(L"script", L"info", code, safeMessage, false);
}

CString StartupTrace::NormalizeLogValue(const wchar_t* text, int maximumLength)
{
	CString value(text ? text : L"");
	value.Replace(L"\r", L" "); value.Replace(L"\n", L" "); value.Replace(L"\t", L" ");
	for (int i = 0; i < value.GetLength(); ++i) if (value[i] < L' ') value.SetAt(i, L' ');
	if (maximumLength > 0 && value.GetLength() > maximumLength) value = value.Left(maximumLength) + L"…";
	return value;
}
CString StartupTrace::SanitizeLogText(const wchar_t* text, int maximumLength) { return NormalizeLogValue(text, maximumLength); }
CString StartupTrace::SanitizeExceptionText(const wchar_t* text) { return NormalizeLogValue(text, 256); }
CString StartupTrace::RedactPath(const wchar_t* text) { return NormalizeLogValue(text, 512); }
void StartupTrace::EmergencyFlush() { if (traceFile != INVALID_HANDLE_VALUE) ::FlushFileBuffers(traceFile); }
CString StartupTrace::CurrentLogPath() { return CString(); }
CString StartupTrace::LastStageCode() { return CString(); }
CString StartupTrace::LastStageMessage() { return CString(); }
DWORD StartupTrace::LastWriteError() { return ::GetLastError(); }

void StartupTrace::Flush()
{
	if (traceFile != INVALID_HANDLE_VALUE)
		::FlushFileBuffers(traceFile);
}

void StartupTrace::Finish()
{
	if (traceFile != INVALID_HANDLE_VALUE)
	{
		WriteRecord(L"startup", L"info", L"S999", L"process shutdown", true);
		::CloseHandle(traceFile);
		traceFile = INVALID_HANDLE_VALUE;
	}
}
