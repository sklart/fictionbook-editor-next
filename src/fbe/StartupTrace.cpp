#include "stdafx.h"

#include "StartupTrace.h"
#include "../version.h"

namespace
{
	HANDLE traceFile = INVALID_HANDLE_VALUE;
	ULONGLONG startTime = 0;
	ULONGLONG previousTime = 0;

	void WriteUtf8(HANDLE file, const wchar_t* text)
	{
		if (file == INVALID_HANDLE_VALUE)
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

		DWORD written = 0;
		::WriteFile(file, utf8.GetString(), utf8.GetLength(), &written, NULL);
		::FlushFileBuffers(file);
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

	void WriteMark(const wchar_t* category, const wchar_t* stage)
	{
		if (traceFile == INVALID_HANDLE_VALUE)
			return;

		const ULONGLONG now = ::GetTickCount64();
		CString line;
		line.Format(L"[%s] [+%llu ms, delta %llu ms] %s\r\n",
			category, now - startTime, now - previousTime, stage);
		previousTime = now;
		WriteUtf8(traceFile, line);
	}

}

void StartupTrace::Start()
{
	if (!IsTraceEnabled(L"FBE_NEXT_TRACE"))
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
	const CString path = directory + L"\\fbe-trace.log";
	traceFile = ::CreateFile(
		path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (traceFile != INVALID_HANDLE_VALUE)
	{
		WriteUtf8(traceFile,
			L"FictionBook Editor diagnostic trace\r\n");
		SYSTEM_INFO systemInfo = {};
		::GetNativeSystemInfo(&systemInfo);
		CString environment;
		environment.Format(L"FBE version: %s; process: Win32; OS architecture: %s\r\n",
			FBE_VERSION_WSTRING,
			GetProcessorArchitectureName(systemInfo.wProcessorArchitecture));
		WriteUtf8(traceFile, environment);
		WriteMark(L"startup", L"process started");
	}
}

void StartupTrace::Mark(const wchar_t* stage)
{
	WriteMark(L"startup", stage);
}

bool StartupTrace::Enabled()
{
	return traceFile != INVALID_HANDLE_VALUE;
}

void StartupTrace::Event(const wchar_t* category, const wchar_t* stage)
{
	WriteMark(category, stage);
}

void StartupTrace::Finish()
{
	if (traceFile != INVALID_HANDLE_VALUE)
	{
		WriteMark(L"startup", L"process shutdown");
		::CloseHandle(traceFile);
		traceFile = INVALID_HANDLE_VALUE;
	}
}
