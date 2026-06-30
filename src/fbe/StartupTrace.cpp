#include "stdafx.h"

#include "StartupTrace.h"

namespace
{
	HANDLE traceFile = INVALID_HANDLE_VALUE;
	ULONGLONG startTime = 0;
	ULONGLONG previousTime = 0;

	void WriteUtf8(const wchar_t* text)
	{
		if (traceFile == INVALID_HANDLE_VALUE)
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
		::WriteFile(traceFile, utf8.GetString(), utf8.GetLength(), &written, NULL);
		::FlushFileBuffers(traceFile);
	}
}

void StartupTrace::Start()
{
	wchar_t enabled[8] = {};
	const DWORD length = ::GetEnvironmentVariable(
		L"FBE_STARTUP_TRACE", enabled, _countof(enabled));
	if (length == 0 || length >= _countof(enabled) ||
		(length == 1 && enabled[0] == L'0'))
	{
		return;
	}

	wchar_t localAppData[MAX_PATH] = {};
	if (FAILED(::SHGetFolderPath(
		NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL,
		SHGFP_TYPE_CURRENT, localAppData)))
	{
		return;
	}

	CString directory(localAppData);
	directory += L"\\FBE";
	::CreateDirectory(directory, NULL);

	const CString path = directory + L"\\startup-trace.log";
	traceFile = ::CreateFile(
		path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (traceFile == INVALID_HANDLE_VALUE)
		return;

	startTime = previousTime = ::GetTickCount64();
	WriteUtf8(L"FictionBook Editor startup trace\r\n");
	Mark(L"process started");
}

void StartupTrace::Mark(const wchar_t* stage)
{
	if (traceFile == INVALID_HANDLE_VALUE)
		return;

	const ULONGLONG now = ::GetTickCount64();
	wchar_t line[256] = {};
	_snwprintf_s(
		line, _countof(line), _TRUNCATE,
		L"[+%llu ms, delta %llu ms] %ls\r\n",
		now - startTime, now - previousTime, stage);
	previousTime = now;
	WriteUtf8(line);
}

void StartupTrace::Finish()
{
	if (traceFile == INVALID_HANDLE_VALUE)
		return;

	Mark(L"process shutdown");
	::CloseHandle(traceFile);
	traceFile = INVALID_HANDLE_VALUE;
}
