#include "stdafx.h"
#include <dbghelp.h>
#include <algorithm>
#include <vector>

#include "CrashHandler.h"
#include "utils.h"
#include "../version.h"

namespace
{
	const size_t MAX_CRASH_REPORTS = 10;
	wchar_t g_crashDirectory[MAX_PATH] = {};

	void CleanupOldReports(const CString& crashDirectory)
	{
		std::vector<CString> reportNames;
		WIN32_FIND_DATA findData = {};
		HANDLE search = ::FindFirstFile(crashDirectory + L"FBE-crash-*.*", &findData);
		if (search == INVALID_HANDLE_VALUE)
			return;

		do
		{
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;

			CString name(findData.cFileName);
			const int extension = name.ReverseFind(L'.');
			if (extension < 0)
				continue;
			const CString suffix(name.Mid(extension));
			if (suffix.CompareNoCase(L".dmp") != 0 && suffix.CompareNoCase(L".txt") != 0)
				continue;

			name.Delete(extension, name.GetLength() - extension);
			bool alreadyAdded = false;
			for (size_t i = 0; i < reportNames.size(); ++i)
			{
				if (reportNames[i].CompareNoCase(name) == 0)
				{
					alreadyAdded = true;
					break;
				}
			}
			if (!alreadyAdded)
				reportNames.push_back(name);
		}
		while (::FindNextFile(search, &findData));
		::FindClose(search);

		std::sort(reportNames.begin(), reportNames.end(),
			[](const CString& left, const CString& right)
			{
				return left.CompareNoCase(right) > 0;
			});

		for (size_t i = MAX_CRASH_REPORTS; i < reportNames.size(); ++i)
		{
			::DeleteFile(crashDirectory + reportNames[i] + L".dmp");
			::DeleteFile(crashDirectory + reportNames[i] + L".txt");
		}
	}

	void WriteTextReport(const wchar_t* path, EXCEPTION_POINTERS* exceptionInfo,
		bool dumpWritten, DWORD dumpError)
	{
		HANDLE report = ::CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (report == INVALID_HANDLE_VALUE)
			return;

		const DWORD exceptionCode = exceptionInfo && exceptionInfo->ExceptionRecord
			? exceptionInfo->ExceptionRecord->ExceptionCode : 0;
		const void* exceptionAddress = exceptionInfo && exceptionInfo->ExceptionRecord
			? exceptionInfo->ExceptionRecord->ExceptionAddress : NULL;

		wchar_t text[1024];
		const int textLength = _snwprintf_s(text, _countof(text), _TRUNCATE,
			L"FictionBook Editor crash report\r\n"
			L"Version: " FBE_VERSION_WSTRING L"\r\n"
			L"Process ID: %lu\r\n"
			L"Exception code: 0x%08lX\r\n"
			L"Exception address: %p\r\n"
			L"Minidump written: %s\r\n"
			L"Minidump error: %lu\r\n",
			::GetCurrentProcessId(), exceptionCode, exceptionAddress,
			dumpWritten ? L"yes" : L"no", dumpWritten ? ERROR_SUCCESS : dumpError);

		const WORD bom = 0xFEFF;
		DWORD written = 0;
		::WriteFile(report, &bom, sizeof(bom), &written, NULL);
		if (textLength > 0)
			::WriteFile(report, text, textLength * sizeof(wchar_t), &written, NULL);
		::FlushFileBuffers(report);
		::CloseHandle(report);
	}

	LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
	{
		if (g_crashDirectory[0] == L'\0')
			return EXCEPTION_EXECUTE_HANDLER;

		SYSTEMTIME localTime;
		::GetLocalTime(&localTime);

		wchar_t basePath[MAX_PATH];
		_snwprintf_s(basePath, _countof(basePath), _TRUNCATE,
			L"%sFBE-crash-%04u%02u%02u-%02u%02u%02u",
			g_crashDirectory, localTime.wYear, localTime.wMonth, localTime.wDay,
			localTime.wHour, localTime.wMinute, localTime.wSecond);

		wchar_t dumpPath[MAX_PATH];
		wchar_t reportPath[MAX_PATH];
		_snwprintf_s(dumpPath, _countof(dumpPath), _TRUNCATE, L"%s.dmp", basePath);
		_snwprintf_s(reportPath, _countof(reportPath), _TRUNCATE, L"%s.txt", basePath);

		bool dumpWritten = false;
		DWORD dumpError = ERROR_SUCCESS;
		HANDLE dump = ::CreateFile(dumpPath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (dump != INVALID_HANDLE_VALUE)
		{
			MINIDUMP_EXCEPTION_INFORMATION dumpInfo = {};
			dumpInfo.ThreadId = ::GetCurrentThreadId();
			dumpInfo.ExceptionPointers = exceptionInfo;
			dumpInfo.ClientPointers = FALSE;

			dumpWritten = ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(),
				dump, MiniDumpNormal, exceptionInfo ? &dumpInfo : NULL, NULL, NULL) != FALSE;
			if (!dumpWritten)
				dumpError = ::GetLastError();
			::FlushFileBuffers(dump);
			::CloseHandle(dump);
		}
		else
		{
			dumpError = ::GetLastError();
		}

		WriteTextReport(reportPath, exceptionInfo, dumpWritten, dumpError);
		return EXCEPTION_EXECUTE_HANDLER;
	}
}

void CrashDiagnostics::Initialize()
{
	const CString crashDirectory(U::GetSettingsDir() + L"Crashes\\");
	if (!::CreateDirectory(crashDirectory, NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS)
		return;

	CleanupOldReports(crashDirectory);
	wcsncpy_s(g_crashDirectory, crashDirectory, _TRUNCATE);
	::SetUnhandledExceptionFilter(UnhandledExceptionHandler);
}
