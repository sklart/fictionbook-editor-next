#include "stdafx.h"

#include "StartupTrace.h"
#include "utils.h"
#include "..\\common\\DeploymentContext.h"
#include "../version.h"
#include <algorithm>
#include <vector>
#include <winver.h>
#include "../common/DiagnosticPrivacy.h"
#include "../common/DiagnosticLog.h"

extern "C" { extern const char* build_timestamp; extern const char* build_commit; extern const char* build_configuration; }

namespace
{
	HANDLE traceFile = INVALID_HANDLE_VALUE;
	wchar_t sessionId[40] = {};
	CString sessionStarted, sessionMetadataPath, sessionDirectory, hangEnvironment;
	CRITICAL_SECTION traceLock;
	bool traceLockInitialized = false;
	ULONGLONG startTime = 0, previousTime = 0, writtenBytes = 0, recordSequence = 0;
	ULONGLONG uiComCallCount = 0, uiCheckCommandCount = 0;
	ULONGLONG uiSelectionContainerQueryCount = 0, uiSelectionContextContainerQueryCount = 0, uiSelectionStructConQueryCount = 0, uiSelectionStructTableConQueryCount = 0;
	ULONGLONG xsltTemplateBuildCount = 0, xsdSchemaLoadCount = 0, sourceSerializationCount = 0;
	DWORD lastWriteError = ERROR_SUCCESS;
	CString tracePath, traceBasePath, lastStageCode, lastStageMessage, lastDocumentStage, lastScriptOperationStage, lastScriptFailureStage, lastComFailure, lastHResultFailure, lastDispatchFailure;
	unsigned int traceSegment = 0;
	const ULONGLONG maxTraceSize = 16ULL * 1024ULL * 1024ULL;
	const wchar_t* const diagnosticTraceRegistryPath = L"Software\\FBETeam\\FictionBook Editor Next\\Diagnostics";
	const wchar_t* const diagnosticTraceRegistryValue = L"TraceNextLaunch";

    using OperationCounters=StartupTrace::HangOperationCounters;
    struct ActiveOperation { ULONGLONG token = 0, started = 0; StartupTrace::Operation operation = StartupTrace::Operation::None; };
    constexpr size_t operationCount = static_cast<size_t>(StartupTrace::Operation::Count);
    OperationCounters operationCounters[operationCount];
    ActiveOperation activeOperations[32];
    ULONGLONG nextOperationToken = 0, droppedOperations = 0, lastCompletedMs = 0;
    StartupTrace::Operation lastCompletedOperation = StartupTrace::Operation::None;
    bool lastCompletedSucceeded = false;
    StartupTrace::EditorMode activeEditorMode = StartupTrace::EditorMode::Unknown;
    const wchar_t* OperationName(StartupTrace::Operation operation) {
        switch(operation) {
        case StartupTrace::Operation::Load: return L"load";
        case StartupTrace::Operation::Save: return L"save";
        case StartupTrace::Operation::Validate: return L"validate";
        case StartupTrace::Operation::Script: return L"script";
        case StartupTrace::Operation::ViewSwitch: return L"view-switch";
        default: return L"none";
        }
    }
    const wchar_t* EditorModeName(StartupTrace::EditorMode mode) {
        switch(mode) {
        case StartupTrace::EditorMode::Body: return L"body";
        case StartupTrace::EditorMode::Description: return L"description";
        case StartupTrace::EditorMode::Source: return L"source";
        default: return L"unknown";
        }
    }

	class TraceLock { public: TraceLock() { if (traceLockInitialized) ::EnterCriticalSection(&traceLock); } ~TraceLock() { if (traceLockInitialized) ::LeaveCriticalSection(&traceLock); } };

#include "diagnostics/HangTelemetry.inl"

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
			else if (value.Mid(index, 7).CompareNoCase(L"file://") == 0) start = index;
			if (start < 0 && value[index]==L'/' && index+1<value.GetLength() && iswalpha(value[index+1]) && (index==0 || value[index-1]==L' ' || value[index-1]==L'=' || value[index-1]==L'"')) start=index;
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
		if (redactPaths) value = RedactPathFragments(value);
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

	CString ResolveDiagnosticLogDirectory() { return CString(DeploymentContext::DiagnosticsDirectory().c_str()); }

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
		const ULONGLONG now = ::GetTickCount64(); SYSTEMTIME time = {}; ::GetLocalTime(&time);
		const CString safeCategory = FbeDiagnosticPrivacy::Identifier(category ? category : L"") ? category : L"unknown";
		const CString safeCode = FbeDiagnosticPrivacy::Identifier(code ? code : L"") ? code : L"unknown";
		const CString safeMessage(FbeDiagnosticPrivacy::Details(message).c_str());
		CString line;
		line.Format(L"%04u-%02u-%02u %02u:%02u:%02u.%03u; seq=%llu; elapsed=%llu; delta=%llu; PID=%lu; TID=%lu; session_id=%s; level=%s; category=%s; code=%s; message=%s\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, ++recordSequence, now - startTime, now - previousTime, ::GetCurrentProcessId(), ::GetCurrentThreadId(), sessionId, (LPCWSTR)level, (LPCWSTR)safeCategory, (LPCWSTR)(safeCode.IsEmpty() ? CString(L"-") : safeCode), (LPCWSTR)safeMessage);
		previousTime = now;
		lastStageCode = safeCode;
		lastStageMessage = safeMessage;
		if (safeCategory == L"document")
			lastDocumentStage = safeCode;

		const bool isJavaScriptOperation = safeCategory == L"script" && safeCode.GetLength() >= 4 && safeCode[0] == L'J';
		const bool isDiagnosticOrRestoreEvent = safeCode == L"J900" || safeCode == L"J901" || (message && wcsncmp(message,L"operation=CSS restore",21)==0);
		if (isJavaScriptOperation && !isDiagnosticOrRestoreEvent)
			lastScriptOperationStage = safeCode;
        PublishHangStage(safeCategory,safeCode,isJavaScriptOperation && !isDiagnosticOrRestoreEvent);
		if (traceFile == INVALID_HANDLE_VALUE && wcscmp(level, L"info") == 0) return;
        if (traceFile != INVALID_HANDLE_VALUE) WriteUtf8(line, flush, wcscmp(level, L"error") == 0);
        else if (sessionId[0]) {
            const std::wstring path = std::wstring(sessionDirectory.GetString()) + L"fbe-events-" + sessionId + L".log";
            if (!FbeDiagnostic::WriteUtf8(path, std::wstring(line.GetString()))) lastWriteError = ERROR_WRITE_FAULT;
        }
	}

    CString GetDiagnosticFileVersion(const CString& path)
    {
        if(path.IsEmpty()) return CString(L"unavailable");
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

    CString GetLoadedModuleVersion(const wchar_t* moduleName) {
        HMODULE module=::GetModuleHandle(moduleName); if(!module) return CString(L"not-loaded");
        wchar_t path[MAX_PATH]={}; if(!::GetModuleFileName(module,path,_countof(path))) return CString(L"unavailable");
        return GetDiagnosticFileVersion(CString(path));
    }

	CString ModuleArchitecture(const CString& path)
	{
		DWORD binaryType = 0;
		if (!path.IsEmpty() && ::GetBinaryType(path, &binaryType))
		{
			if (binaryType == SCS_32BIT_BINARY) return CString(L"x86");
			if (binaryType == SCS_64BIT_BINARY) return CString(L"x64");
		}
        HANDLE file=::CreateFile(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file!=INVALID_HANDLE_VALUE) {
            IMAGE_DOS_HEADER dos={}; DWORD read=0; DWORD signature=0; IMAGE_FILE_HEADER header={};
            const bool valid=::ReadFile(file,&dos,sizeof(dos),&read,NULL) && read==sizeof(dos) && dos.e_magic==IMAGE_DOS_SIGNATURE && dos.e_lfanew>0 && dos.e_lfanew<1024*1024;
            bool pe=false;
            if(valid && ::SetFilePointer(file,dos.e_lfanew,NULL,FILE_BEGIN)!=INVALID_SET_FILE_POINTER)
                pe=::ReadFile(file,&signature,sizeof(signature),&read,NULL) && read==sizeof(signature) && signature==IMAGE_NT_SIGNATURE && ::ReadFile(file,&header,sizeof(header),&read,NULL) && read==sizeof(header);
            ::CloseHandle(file);
            if(pe && header.Machine==IMAGE_FILE_MACHINE_I386) return CString(L"x86");
            if(pe && header.Machine==IMAGE_FILE_MACHINE_AMD64) return CString(L"x64");
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
			(LPCWSTR)GetDiagnosticFileVersion(path),
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
	if (sessionId[0]) return;
    GUID id = {}; if (FAILED(::CoCreateGuid(&id))) return;
    wchar_t guid[40] = {}; ::StringFromGUID2(id, guid, _countof(guid));
    wcsncpy_s(sessionId, guid + 1, 36); sessionId[36] = 0;
    ::InitializeCriticalSection(&traceLock); traceLockInitialized = true;
    SYSTEMTIME started = {}; ::GetSystemTime(&started);
    sessionStarted.Format(L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", started.wYear, started.wMonth, started.wDay, started.wHour, started.wMinute, started.wSecond, started.wMilliseconds);
    CString metadataDirectory(DeploymentContext::DiagnosticsDirectory().c_str());
    int created = ::SHCreateDirectoryEx(NULL, metadataDirectory, NULL);
    if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS && DeploymentContext::CurrentMode() != DeploymentContext::Mode::Portable) {
        wchar_t temp[MAX_PATH] = {}; if (::GetTempPath(_countof(temp),temp)) { metadataDirectory = CString(temp) + L"FBE Next Diagnostics\\"; ::SHCreateDirectoryEx(NULL,metadataDirectory,NULL); }
    }
    sessionDirectory = metadataDirectory;
    sessionMetadataPath = metadataDirectory + L"fbe-session-" + sessionId + L".txt";
    CString metadata; metadata.Format(L"session_id=%s\r\nstarted=%s\r\nPID=%lu\r\nversion=%s\r\noutcome=unknown\r\n", sessionId, (LPCWSTR)sessionStarted, ::GetCurrentProcessId(), FBE_VERSION_WSTRING);
    hangEnvironment=BasicEnvironmentReport();
    metadata += hangEnvironment;
    PublishHangOperations();
    HANDLE file = ::CreateFile(sessionMetadataPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if(file==INVALID_HANDLE_VALUE && DeploymentContext::CurrentMode()!=DeploymentContext::Mode::Portable) {
        wchar_t temp[MAX_PATH]={};
        if(::GetTempPath(_countof(temp),temp)) {
            metadataDirectory=CString(temp)+L"FBE Next Diagnostics\\"; ::SHCreateDirectoryEx(NULL,metadataDirectory,NULL);
            sessionDirectory=metadataDirectory; sessionMetadataPath=metadataDirectory+L"fbe-session-"+sessionId+L".txt";
            file=::CreateFile(sessionMetadataPath,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        }
    }
    if (file != INVALID_HANDLE_VALUE) { CStringA bytes(CW2A(metadata, CP_UTF8)); DWORD written = 0; if (!::WriteFile(file, bytes.GetString(), bytes.GetLength(), &written, NULL) || written != static_cast<DWORD>(bytes.GetLength())) lastWriteError = ::GetLastError() ? ::GetLastError() : ERROR_WRITE_FAULT; ::CloseHandle(file); }
    else lastWriteError = ::GetLastError();
    startTime = previousTime = ::GetTickCount64();
    if (!IsEnabledForNextLaunch()) return;
    SYSTEMTIME time = {}; ::GetLocalTime(&time);
    if (!OpenTraceFile(metadataDirectory, time)) { lastWriteError = ::GetLastError(); return; }

	// Retention runs in the main frame diagnostic worker, after UI creation.
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
void StartupTrace::CountUiComCall() { if (Enabled()) ++uiComCallCount; }
ULONGLONG StartupTrace::UiComCallCount() { return Enabled() ? uiComCallCount : 0; }
void StartupTrace::CountUiCheckCommand() { if (Enabled()) ++uiCheckCommandCount; }
ULONGLONG StartupTrace::UiCheckCommandCount() { return Enabled() ? uiCheckCommandCount : 0; }
void StartupTrace::CountUiSelectionContainerQuery() { if (Enabled()) ++uiSelectionContainerQueryCount; }
ULONGLONG StartupTrace::UiSelectionContainerQueryCount() { return Enabled() ? uiSelectionContainerQueryCount : 0; }
void StartupTrace::CountUiSelectionContextContainerQuery() { if (Enabled()) ++uiSelectionContextContainerQueryCount; }
ULONGLONG StartupTrace::UiSelectionContextContainerQueryCount() { return Enabled() ? uiSelectionContextContainerQueryCount : 0; }
void StartupTrace::CountUiSelectionStructConQuery() { if (Enabled()) ++uiSelectionStructConQueryCount; }
ULONGLONG StartupTrace::UiSelectionStructConQueryCount() { return Enabled() ? uiSelectionStructConQueryCount : 0; }
void StartupTrace::CountUiSelectionStructTableConQuery() { if (Enabled()) ++uiSelectionStructTableConQueryCount; }
ULONGLONG StartupTrace::UiSelectionStructTableConQueryCount() { return Enabled() ? uiSelectionStructTableConQueryCount : 0; }
void StartupTrace::CountXsltTemplateBuild() { if (Enabled()) ++xsltTemplateBuildCount; }
ULONGLONG StartupTrace::XsltTemplateBuildCount() { return Enabled() ? xsltTemplateBuildCount : 0; }
void StartupTrace::CountXsdSchemaLoad() { if (Enabled()) ++xsdSchemaLoadCount; }
ULONGLONG StartupTrace::XsdSchemaLoadCount() { return Enabled() ? xsdSchemaLoadCount : 0; }
void StartupTrace::CountSourceSerialization() { if (Enabled()) ++sourceSerializationCount; }
ULONGLONG StartupTrace::SourceSerializationCount() { return Enabled() ? sourceSerializationCount : 0; }
void StartupTrace::Event(const wchar_t* category, const wchar_t* code, const wchar_t* message) { WriteRecord(category, L"info", code, message, false); }
void StartupTrace::Debug(const wchar_t* category, const wchar_t* code, const wchar_t* message) { WriteRecord(category, L"debug", code, message, false); }
void StartupTrace::Warning(const wchar_t* category, const wchar_t* code, const wchar_t* message) { WriteRecord(category, L"warning", code, message, false); }
void StartupTrace::Event(const wchar_t* category, const wchar_t* message) { WriteRecord(category, L"info", L"LEGACY", message, false); }
void StartupTrace::Error(const wchar_t* category, const wchar_t* code, const wchar_t* message) { WriteRecord(category, L"error", code, message, true); }
void StartupTrace::HResult(const wchar_t* category, const wchar_t* code, HRESULT result, const wchar_t* message) { PublishHangHresult(code,result); CString details; details.Format(L"hr=0x%08lX; %s", static_cast<unsigned long>(result), message ? message : L""); WriteRecord(category, FAILED(result) ? L"error" : L"info", code, details, FAILED(result)); if (FAILED(result)) { TraceLock guard; const CString failure = Sanitize(code, 32, false) + L": " + CString(FbeDiagnosticPrivacy::Details(details).c_str()); lastComFailure = failure; lastHResultFailure = failure; if (category && wcscmp(category, L"dispatch") == 0) lastDispatchFailure = failure; } }
void StartupTrace::DispatchResult(const wchar_t* category, const wchar_t* code, HRESULT result, const wchar_t* message) { HResult(category, code, result, message); if (FAILED(result)) { TraceLock guard; CString details; details.Format(L"hr=0x%08lX; %s", static_cast<unsigned long>(result), message ? message : L""); lastDispatchFailure = Sanitize(code, 32, false) + L": " + CString(FbeDiagnosticPrivacy::Details(details).c_str()); } }
void StartupTrace::ComException(const wchar_t* category, const wchar_t* code, HRESULT result,
	const EXCEPINFO* exceptionInfo, IErrorInfo* errorInfo, const wchar_t* message)
{
    PublishHangHresult(code,result);
	CString details;
	details.Format(L"hr=0x%08lX; %s", static_cast<unsigned long>(result), message ? message : L"");
	details += FormatComExceptionMetadata(exceptionInfo, errorInfo);
	WriteRecord(category, L"error", code, details, true);
	{ TraceLock guard; const CString failure = Sanitize(code, 32, false) + L": " + CString(FbeDiagnosticPrivacy::Details(details).c_str()); lastComFailure = failure; lastHResultFailure = failure; }
}
void StartupTrace::ScriptEvent(const wchar_t* code, const wchar_t* message) { CString safeMessage = SanitizeScriptDetails(message); const bool isError = safeMessage.Find(L"level=error") == 0; WriteRecord(L"script", isError ? L"error" : L"info", code, safeMessage, isError); if (isError) { const int marker = safeMessage.Find(L"failed-stage="); if (marker >= 0) { CString stage = safeMessage.Mid(marker + 13); const int separator = stage.Find(L";"); if (separator >= 0) stage = stage.Left(separator); TraceLock guard; lastScriptFailureStage = Sanitize(stage, 32, false); PublishHangScriptFailure(stage); } } }
void StartupTrace::Flush() { TraceLock guard; if (traceFile != INVALID_HANDLE_VALUE && !::FlushFileBuffers(traceFile)) lastWriteError = ::GetLastError(); }
void StartupTrace::EmergencyFlush() { if (traceFile != INVALID_HANDLE_VALUE) ::FlushFileBuffers(traceFile); }
CString StartupTrace::CurrentLogPath() { TraceLock guard; return tracePath; }
CString StartupTrace::CurrentLogDirectory() { TraceLock guard; const CString path = tracePath; const int separator = path.ReverseFind(L'\\'); return separator >= 0 ? path.Left(separator) : (sessionDirectory.IsEmpty() ? ResolveDiagnosticLogDirectory() : sessionDirectory); }
bool StartupTrace::TryGetCrashTraceSnapshot(CrashTraceSnapshot& snapshot)
{
	::ZeroMemory(&snapshot, sizeof(snapshot));
	wcsncpy_s(snapshot.sessionId, sessionId, _TRUNCATE);
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
void StartupTrace::AppendTestStartupBreadcrumb(const char* phase)
{
	if (phase == NULL) return;
	wchar_t testMode[4] = {}, path[32768] = {};
	const DWORD modeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode));
	if (modeLength != 1 || testMode[0] != L'1') return;
	const DWORD pathLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STARTUP_BREADCRUMB", path, _countof(path));
	if (!pathLength || pathLength >= _countof(path)) return;
	HANDLE file = ::CreateFile(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return;
	::SetFilePointer(file, 0, NULL, FILE_END);
	DWORD written = 0;
	::WriteFile(file, phase, static_cast<DWORD>(::lstrlenA(phase)), &written, NULL);
	::WriteFile(file, "\r\n", 2, &written, NULL);
	::FlushFileBuffers(file);
	::CloseHandle(file);
}
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
ULONGLONG StartupTrace::BeginOperation(Operation operation) noexcept
{
    if (!traceLockInitialized || operation <= Operation::None || operation >= Operation::Count) return 0;
    TraceLock guard;
    for (auto& active : activeOperations) if (!active.token) {
        if (++nextOperationToken == 0) ++nextOperationToken;
        active = { nextOperationToken, ::GetTickCount64(), operation };
        auto& counter = operationCounters[static_cast<size_t>(operation)];
        ++counter.started; ++counter.active;
        PublishHangOperations(); return active.token;
    }
    ++droppedOperations; PublishHangOperations();
    return 0;
}
void StartupTrace::EndOperation(ULONGLONG token, bool succeeded) noexcept
{
    if (!token || !traceLockInitialized) return;
    TraceLock guard;
    for (auto& active : activeOperations) if (active.token == token) {
        const ULONGLONG elapsed = ::GetTickCount64() - active.started;
        auto& counter = operationCounters[static_cast<size_t>(active.operation)];
        ++counter.completed; if (!succeeded) ++counter.failed; --counter.active;
        counter.totalMs += elapsed; counter.lastMs = elapsed; counter.maxMs = (std::max)(counter.maxMs, elapsed);
        lastCompletedOperation = active.operation; lastCompletedSucceeded = succeeded; lastCompletedMs = elapsed;
        active = {}; PublishHangOperations(); return;
    }
}
void StartupTrace::SetActiveEditorMode(EditorMode mode) noexcept
{
    if (!traceLockInitialized) return;
    TraceLock guard;
    activeEditorMode = mode >= EditorMode::Unknown && mode <= EditorMode::Source ? mode : EditorMode::Unknown;
    PublishHangOperations();
}
bool StartupTrace::TryGetHangTelemetry(HangTelemetry& snapshot) noexcept
{
    if(!::TryAcquireSRWLockShared(&hangTelemetryLock)) return false;
    snapshot=hangTelemetry; ::ReleaseSRWLockShared(&hangTelemetryLock); return snapshot.sampledAt!=0;
}
CString StartupTrace::HangEnvironmentReport() { return hangEnvironment; }
void StartupTrace::TestHoldTraceLockForWatchdog(DWORD milliseconds)
{
    wchar_t mode[2]={};
    if(::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE",mode,2)!=1 || mode[0]!=L'1' || milliseconds>5000) return;
    TraceLock lock; ::Sleep(milliseconds);
}
CString StartupTrace::ActiveEditorModeName()
{
    TraceLock guard; return CString(EditorModeName(activeEditorMode));
}
CString StartupTrace::OperationStatisticsReport(bool atShutdown)
{
    OperationCounters counters[operationCount]; ActiveOperation latest;
    ULONGLONG dropped = 0, lastMs = 0; Operation last = Operation::None; bool succeeded = false; EditorMode mode;
    {
        TraceLock guard;
        std::copy(std::begin(operationCounters), std::end(operationCounters), std::begin(counters));
        for (const auto& active : activeOperations) if (active.token > latest.token) latest = active;
        dropped = droppedOperations; last = lastCompletedOperation; succeeded = lastCompletedSucceeded; lastMs = lastCompletedMs; mode = activeEditorMode;
    }
    CString text;
    text.Format(L"operation-statistics-source=%s\r\nsession_id=%s\r\nactive-editor-mode=%s\r\nstats-dropped=%llu\r\nstats-last-completed=%s\r\nstats-last-result=%s\r\nstats-last-duration-ms=%llu\r\nstats-in-progress=%s\r\nstats-in-progress-ms=%llu\r\n",
        atShutdown ? L"selected-session-at-shutdown" : L"selected-current-session-at-preview", sessionId, EditorModeName(mode), dropped,
        OperationName(last), last == Operation::None ? L"unknown" : (succeeded ? L"success" : L"failure"), lastMs, OperationName(latest.operation), latest.token ? ::GetTickCount64()-latest.started : 0);
    for (size_t i = 1; i < operationCount; ++i) {
        const auto& c = counters[i]; const wchar_t* name = OperationName(static_cast<Operation>(i));
        text.AppendFormat(L"stats-%s-started=%llu\r\nstats-%s-completed=%llu\r\nstats-%s-failed=%llu\r\nstats-%s-active=%llu\r\nstats-%s-total-ms=%llu\r\nstats-%s-last-ms=%llu\r\nstats-%s-max-ms=%llu\r\n",
            name,c.started,name,c.completed,name,c.failed,name,c.active,name,c.totalMs,name,c.lastMs,name,c.maxMs);
    }
    return text;
}

void StartupTrace::Finish() {
    if (!sessionMetadataPath.IsEmpty()) {
        HANDLE file = ::CreateFile(sessionMetadataPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            SYSTEMTIME now = {}; ::GetSystemTime(&now); CString end;
            end.Format(L"ended=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\r\noutcome=normal\r\n", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
            end += OperationStatisticsReport(true);
            CStringA bytes(CW2A(end, CP_UTF8)); DWORD written = 0; ::WriteFile(file, bytes.GetString(), bytes.GetLength(), &written, NULL); ::CloseHandle(file);
        }
    }
 if (traceFile != INVALID_HANDLE_VALUE) { Event(L"startup", L"S999", L"process shutdown"); Flush(); ::CloseHandle(traceFile); traceFile = INVALID_HANDLE_VALUE; traceBasePath.Empty(); } if (traceLockInitialized) { ::DeleteCriticalSection(&traceLock); traceLockInitialized = false; } }

CString StartupTrace::SessionId() { return CString(sessionId); }
CString StartupTrace::SessionStarted() { return sessionStarted; }
CString StartupTrace::TraceSource() { return IsTraceEnabled(L"FBE_NEXT_TRACE") ? L"FBE_NEXT_TRACE" : (IsEnabledByStoredNextLaunchPreference() ? L"setting" : L"normal"); }
CString StartupTrace::BasicEnvironmentReport()
{
    SYSTEM_INFO info = {}; ::GetNativeSystemInfo(&info);
    OSVERSIONINFOEXW version = {}; version.dwOSVersionInfoSize = sizeof(version);
    typedef LONG (WINAPI* VersionFunction)(OSVERSIONINFOEXW*);
    VersionFunction rtlVersion = reinterpret_cast<VersionFunction>(::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    if (rtlVersion) rtlVersion(&version);
    CString text, commit(build_commit), configuration(build_configuration);
    if(commit.IsEmpty()) commit=L"unknown";
    text.Format(L"environment-source=selected-session-at-start\r\nfbe=%s\r\ncommit=%s\r\nconfiguration=%s\r\nwindows=%lu.%lu.%lu\r\nnative-architecture=%u\r\ndeployment=%s\r\n", FBE_VERSION_WSTRING, (LPCWSTR)commit, (LPCWSTR)configuration, version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber, info.wProcessorArchitecture, DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable ? L"portable" : L"installed");
    const wchar_t* modules[] = { L"mshtml.dll", L"ieframe.dll", L"urlmon.dll", L"jscript.dll", L"msxml6.dll", L"Scintilla.dll", L"Lexilla.dll" };
    text += DescribeDiagnosticModule(L"FBE.exe", true) + L"\r\n";
    for (size_t i = 0; i < _countof(modules); ++i) text += DescribeDiagnosticModule(modules[i]) + L"\r\n";
    return text;
}
// In-process plug-ins use the host backend without extending the COM contract.
extern "C" __declspec(dllexport) void __stdcall FbeRegisterDiagnosticEvent(const wchar_t* source, const wchar_t* code, HRESULT hr)
{
    try { StartupTrace::HResult(source, code, hr); } catch (...) { }
}
