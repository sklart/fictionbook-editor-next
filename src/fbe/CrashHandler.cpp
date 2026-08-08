#include "stdafx.h"
#include <dbghelp.h>
#include <algorithm>
#include <vector>

#include "CrashHandler.h"
#include "StartupTrace.h"
#include "utils.h"
#include "../version.h"

#define FBE_WIDEN_TEXT_INNER(value) L##value
#define FBE_WIDEN_TEXT(value) FBE_WIDEN_TEXT_INNER(value)

namespace
{
	const size_t MAX_CRASH_REPORTS = 10;
	wchar_t g_crashDirectory[MAX_PATH] = {};

	void CleanupOldReports(const CString& crashDirectory)
	{
		std::vector<CString> reportNames;
		WIN32_FIND_DATA findData = {};
		HANDLE search = ::FindFirstFile(crashDirectory + L"FBENext-crash-*.*", &findData);
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

		StartupTrace::CrashTraceSnapshot traceSnapshot = {};
		const bool snapshotAvailable = StartupTrace::TryGetCrashTraceSnapshot(traceSnapshot);
		const wchar_t* snapshotState = snapshotAvailable ? L"yes" : L"no";
		const wchar_t* diagnosticState = snapshotAvailable ? (traceSnapshot.diagnosticEnabled ? L"yes" : L"no") : L"unknown";
		const wchar_t* tempFallbackState = snapshotAvailable ? (traceSnapshot.usingTempFallback ? L"yes" : L"no") : L"unknown";
		const wchar_t* unknown = L"unknown";
		CString text;
		text.Format(
			L"FictionBook Editor crash report\r\n"
			L"Version: " FBE_VERSION_WSTRING L"\r\n"
			L"Build name: FictionBook Editor Next Release " FBE_VERSION_WSTRING L"\r\n"
			L"Build timestamp: " FBE_WIDEN_TEXT(__DATE__) L" " FBE_WIDEN_TEXT(__TIME__) L"\r\n"
			L"Process ID: %lu\r\n"
			L"Crash thread ID: %lu\r\n"
			L"Exception code: 0x%08lX\r\n"
			L"Exception address: %p\r\n"
			L"Minidump written: %s\r\n"
			L"Minidump error: %lu\r\n"
			L"Trace snapshot available: %s\r\n"
			L"Diagnostic trace enabled: %s\r\n"
			L"TEMP fallback: %s\r\n",
			::GetCurrentProcessId(), ::GetCurrentThreadId(), exceptionCode, exceptionAddress,
			dumpWritten ? L"yes" : L"no", dumpWritten ? ERROR_SUCCESS : dumpError,
			snapshotState, diagnosticState, tempFallbackState);
		CString line;
		line.Format(L"Diagnostic trace: %s\r\n", snapshotAvailable ? traceSnapshot.currentLogPath : unknown); text += line;
		line.Format(L"Last trace stage: %s; %s\r\n", snapshotAvailable ? traceSnapshot.lastEventCode : unknown, snapshotAvailable ? traceSnapshot.lastEventMessage : unknown); text += line;
		line.Format(L"Last document stage: %s\r\n", snapshotAvailable ? traceSnapshot.lastDocumentStage : unknown); text += line;
		line.Format(L"Last script operation stage: %s\r\n", snapshotAvailable ? traceSnapshot.lastScriptOperationStage : unknown); text += line;
		line.Format(L"Last script failure stage: %s\r\n", snapshotAvailable ? traceSnapshot.lastScriptFailureStage : unknown); text += line;
		line.Format(L"Last HRESULT failure: %s\r\n", snapshotAvailable ? traceSnapshot.lastHResultFailure : unknown); text += line;
		line.Format(L"Last dispatch failure: %s\r\n", snapshotAvailable ? traceSnapshot.lastDispatchFailure : unknown); text += line;

		const WORD bom = 0xFEFF;
		DWORD written = 0;
		::WriteFile(report, &bom, sizeof(bom), &written, NULL);
		if (!text.IsEmpty())
			::WriteFile(report, text, text.GetLength() * sizeof(wchar_t), &written, NULL);
		::FlushFileBuffers(report);
		::CloseHandle(report);
	}

	LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
	{
		StartupTrace::EmergencyFlush();
		if (g_crashDirectory[0] == L'\0')
			return EXCEPTION_EXECUTE_HANDLER;

		SYSTEMTIME localTime;
		::GetLocalTime(&localTime);

		wchar_t basePath[MAX_PATH];
		_snwprintf_s(basePath, _countof(basePath), _TRUNCATE,
			L"%sFBENext-crash-%04u%02u%02u-%02u%02u%02u-%03u-pid%lu",
			g_crashDirectory, localTime.wYear, localTime.wMonth, localTime.wDay,
			localTime.wHour, localTime.wMinute, localTime.wSecond, localTime.wMilliseconds, ::GetCurrentProcessId());

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
