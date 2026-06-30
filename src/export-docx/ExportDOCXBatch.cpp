// ExportDOCXBatch.cpp
// Console utility for batch FB2 -> DOCX export through ExportDOCX.dll.
// Build it as Win32 Release together with ExportDOCX.dll.

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <msxml6.h>
#include <shlobj.h>
#include <shellapi.h>
#include <fcntl.h>
#include <io.h>

#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <set>
#include <map>
#include <sstream>
#include <array>
#include <utility>

#include "DocxQuality.h"

namespace {

using ExportFunc = HRESULT (WINAPI *)(LPCWSTR fb2Path, LPCWSTR docxPath);

static HRESULT CallExportFunctionSafely(ExportFunc fn, LPCWSTR fb2Path, LPCWSTR docxPath, DWORD& exceptionCode)
{
    exceptionCode = 0;
    __try {
        return fn(fb2Path, docxPath);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        exceptionCode = ::GetExceptionCode();
        return static_cast<HRESULT>(exceptionCode);
    }
}

static bool IsStructuredExceptionCode(DWORD code)
{
    return (code & 0xF0000000u) == 0xC0000000u;
}

struct Options {
    std::wstring input;
    std::wstring output;
    std::wstring dllPath;
    std::wstring logPath;
    std::wstring inputListPath;
    std::wstring summaryPath;
    std::wstring failedListPath;
    std::wstring okListPath;
    std::wstring htmlReportPath;
    std::wstring inputInvalidListPath;
    std::wstring warningsFilesListPath;
    ULONGLONG minSizeBytes = 0;
    ULONGLONG maxSizeBytes = 0;
    FILETIME modifiedAfter = {};
    FILETIME modifiedBefore = {};
    int limit = 0;
    int retries = 0;
    int progressEvery = 0;
    int timeoutSec = 0;
    bool recurse = false;
    bool noOverwrite = false;
    bool dryRun = false;
    bool flat = false;
    bool quiet = false;
    bool openLog = false;
    bool openHtmlReport = false;
    bool openSummary = false;
    bool openFailedList = false;
    bool openOkList = false;
    bool printFailed = false;
    bool appendLog = false;
    bool skipIfNewer = false;
    bool stopOnError = false;
    bool preserveDates = false;
    bool uniqueNames = false;
    bool hasMinSize = false;
    bool hasMaxSize = false;
    bool hasModifiedAfter = false;
    bool hasModifiedBefore = false;
    bool validateDocx = false;
    bool validateInputXml = true;
    bool isolatedExport = false;
    bool workerExport = false;
};

struct ResultRow {
    std::wstring status;
    std::wstring input;
    std::wstring output;
    std::wstring hr;
    ULONGLONG elapsedMs = 0;
    std::wstring message;
    std::wstring validation;
};

static void InitConsole()
{
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);
}

static void PrintLine(const wchar_t* text)
{
    if (text) {
        std::fwprintf(stdout, L"%ls\n", text);
    }
}

static void PrintLine(const std::wstring& text)
{
    PrintLine(text.c_str());
}

static void PrintLineIf(bool enabled, const std::wstring& text)
{
    if (enabled) PrintLine(text);
}

static bool IEquals(const std::wstring& a, const wchar_t* b)
{
    return _wcsicmp(a.c_str(), b) == 0;
}


static bool IsHelpOption(const std::wstring& a)
{
    return IEquals(a, L"-?") || IEquals(a, L"/?") ||
           IEquals(a, L"-Help") || IEquals(a, L"/Help") ||
           IEquals(a, L"--help");
}

static bool StartsWithI(const std::wstring& s, const wchar_t* prefix)
{
    size_t n = std::wcslen(prefix);
    return s.size() >= n && _wcsnicmp(s.c_str(), prefix, n) == 0;
}

static bool EndsWithI(const std::wstring& s, const wchar_t* suffix)
{
    size_t n = std::wcslen(suffix);
    return s.size() >= n && _wcsicmp(s.c_str() + s.size() - n, suffix) == 0;
}

static bool ContainsI(const std::wstring& s, const wchar_t* needle)
{
    if (!needle || !*needle) return true;
    std::wstring a = s;
    std::wstring b = needle;
    std::transform(a.begin(), a.end(), a.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
    std::transform(b.begin(), b.end(), b.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
    return a.find(b) != std::wstring::npos;
}

static bool IsExpectedNegativeTestPath(const std::wstring& path)
{
    return ContainsI(path, L"\\negative_tests\\") || ContainsI(path, L"/negative_tests/") ||
           StartsWithI(path, L"negative_tests\\") || StartsWithI(path, L"negative_tests/");
}

static std::wstring TrimTrailingSlash(std::wstring s)
{
    while (s.size() > 3 && (s.back() == L'\\' || s.back() == L'/')) {
        s.pop_back();
    }
    return s;
}

static std::wstring TrimString(std::wstring s)
{
    size_t first = 0;
    while (first < s.size() && (s[first] == L' ' || s[first] == L'\t' || s[first] == L'\r' || s[first] == L'\n')) ++first;
    size_t last = s.size();
    while (last > first && (s[last - 1] == L' ' || s[last - 1] == L'\t' || s[last - 1] == L'\r' || s[last - 1] == L'\n')) --last;
    s = s.substr(first, last - first);
    if (s.size() >= 2 && ((s.front() == L'"' && s.back() == L'"') || (s.front() == L'\'' && s.back() == L'\''))) {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

static std::wstring JoinPath(const std::wstring& a, const std::wstring& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    wchar_t last = a.back();
    if (last == L'\\' || last == L'/') return a + b;
    return a + L"\\" + b;
}

static std::wstring DirectoryOf(const std::wstring& path)
{
    size_t p = path.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? std::wstring() : path.substr(0, p);
}

static std::wstring FileNameOf(const std::wstring& path)
{
    size_t p = path.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? path : path.substr(p + 1);
}

static bool ContainsWildcard(const std::wstring& path)
{
    return path.find_first_of(L"*?") != std::wstring::npos;
}

static bool HasExtension(const std::wstring& path, const wchar_t* ext)
{
    size_t slash = path.find_last_of(L"\\/");
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) return false;
    return _wcsicmp(path.c_str() + dot, ext) == 0;
}

static bool HasFb2Extension(const std::wstring& path)
{
    return HasExtension(path, L".fb2");
}

static bool HasDocxExtension(const std::wstring& path)
{
    return HasExtension(path, L".docx");
}

static std::wstring ChangeExtensionToDocx(const std::wstring& fileName)
{
    size_t slash = fileName.find_last_of(L"\\/");
    size_t dot = fileName.find_last_of(L'.');
    if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash)) {
        return fileName.substr(0, dot) + L".docx";
    }
    return fileName + L".docx";
}

static bool FileExists(const std::wstring& path)
{
    DWORD attr = ::GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool DirectoryExists(const std::wstring& path)
{
    DWORD attr = ::GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool GetFileWriteTime(const std::wstring& path, FILETIME& ft)
{
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return false;
    ft = data.ftLastWriteTime;
    return true;
}

static bool OutputIsNewerOrSame(const std::wstring& input, const std::wstring& output)
{
    FILETIME inTime = {}, outTime = {};
    if (!GetFileWriteTime(input, inTime) || !GetFileWriteTime(output, outTime)) return false;
    return ::CompareFileTime(&outTime, &inTime) >= 0;
}
static ULONGLONG FileSizeBytes(const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return 0;
    ULARGE_INTEGER u = {};
    u.HighPart = data.nFileSizeHigh;
    u.LowPart = data.nFileSizeLow;
    return u.QuadPart;
}


static bool ParseSizeValue(const std::wstring& text, ULONGLONG& value)
{
    std::wstring s = TrimString(text);
    if (s.empty()) return false;

    size_t pos = 0;
    while (pos < s.size() && s[pos] >= L'0' && s[pos] <= L'9') ++pos;
    if (pos == 0) return false;

    ULONGLONG base = _wcstoui64(s.substr(0, pos).c_str(), NULL, 10);
    std::wstring suffix = TrimString(s.substr(pos));
    for (auto& ch : suffix) ch = static_cast<wchar_t>(std::towupper(ch));

    if (suffix.empty() || suffix == L"B" || suffix == L"BYTE" || suffix == L"BYTES") {
        value = base;
        return true;
    }

    ULONGLONG mult = 0;
    if (suffix == L"K" || suffix == L"KB" || suffix == L"KIB") mult = 1024ULL;
    else if (suffix == L"M" || suffix == L"MB" || suffix == L"MIB") mult = 1024ULL * 1024ULL;
    else if (suffix == L"G" || suffix == L"GB" || suffix == L"GIB") mult = 1024ULL * 1024ULL * 1024ULL;
    else return false;

    value = base * mult;
    return true;
}

static bool ParseLocalDateToFileTime(const std::wstring& text, bool endOfDay, FILETIME& ft)
{
    int y = 0, m = 0, d = 0;
    if (swscanf_s(text.c_str(), L"%d-%d-%d", &y, &m, &d) != 3) return false;
    if (y < 1601 || m < 1 || m > 12 || d < 1 || d > 31) return false;

    SYSTEMTIME local = {};
    local.wYear = static_cast<WORD>(y);
    local.wMonth = static_cast<WORD>(m);
    local.wDay = static_cast<WORD>(d);
    if (endOfDay) {
        local.wHour = 23;
        local.wMinute = 59;
        local.wSecond = 59;
        local.wMilliseconds = 999;
    }

    SYSTEMTIME utc = {};
    if (!::TzSpecificLocalTimeToSystemTime(NULL, &local, &utc)) return false;
    return ::SystemTimeToFileTime(&utc, &ft) != FALSE;
}

static std::wstring FormatSizeBytes(ULONGLONG value)
{
    wchar_t buf[64] = {};
    std::swprintf(buf, 64, L"%llu", static_cast<unsigned long long>(value));
    return buf;
}

static bool FileMatchesFilters(const Options& opt, const std::wstring& path, std::wstring& reason)
{
    if (opt.hasMinSize || opt.hasMaxSize) {
        ULONGLONG size = FileSizeBytes(path);
        if (opt.hasMinSize && size < opt.minSizeBytes) {
            reason = L"File size is less than MinSize (" + FormatSizeBytes(size) + L" < " + FormatSizeBytes(opt.minSizeBytes) + L")";
            return false;
        }
        if (opt.hasMaxSize && size > opt.maxSizeBytes) {
            reason = L"File size is greater than MaxSize (" + FormatSizeBytes(size) + L" > " + FormatSizeBytes(opt.maxSizeBytes) + L")";
            return false;
        }
    }

    if (opt.hasModifiedAfter || opt.hasModifiedBefore) {
        FILETIME writeTime = {};
        if (!GetFileWriteTime(path, writeTime)) {
            reason = L"Could not read file modification time";
            return false;
        }
        if (opt.hasModifiedAfter && ::CompareFileTime(&writeTime, &opt.modifiedAfter) < 0) {
            reason = L"File modification time is before ModifiedAfter";
            return false;
        }
        if (opt.hasModifiedBefore && ::CompareFileTime(&writeTime, &opt.modifiedBefore) > 0) {
            reason = L"File modification time is after ModifiedBefore";
            return false;
        }
    }

    return true;
}

static bool CopyFileTimes(const std::wstring& sourcePath, const std::wstring& targetPath)
{
    HANDLE src = ::CreateFileW(sourcePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (src == INVALID_HANDLE_VALUE) return false;

    FILETIME creation = {}, access = {}, write = {};
    BOOL got = ::GetFileTime(src, &creation, &access, &write);
    ::CloseHandle(src);
    if (!got) return false;

    HANDLE dst = ::CreateFileW(targetPath.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dst == INVALID_HANDLE_VALUE) return false;
    BOOL set = ::SetFileTime(dst, &creation, &access, &write);
    ::CloseHandle(dst);
    return set != FALSE;
}


static std::wstring FormatLocalTimestamp()
{
    SYSTEMTIME st = {};
    ::GetLocalTime(&st);
    wchar_t buf[64] = {};
    std::swprintf(buf, 64, L"%04u-%02u-%02u %02u:%02u:%02u",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

static bool EnsureDirectory(const std::wstring& path)
{
    if (path.empty()) return true;
    if (DirectoryExists(path)) return true;
    int rc = ::SHCreateDirectoryExW(NULL, path.c_str(), NULL);
    return rc == ERROR_SUCCESS || rc == ERROR_ALREADY_EXISTS || rc == ERROR_FILE_EXISTS;
}

static std::wstring GetModulePath()
{
    wchar_t buf[MAX_PATH] = {};
    DWORD n = ::GetModuleFileNameW(NULL, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::wstring();
    return buf;
}

static std::wstring GetModuleDirectory()
{
    std::wstring path = GetModulePath();
    if (path.empty()) return L".";
    return DirectoryOf(path);
}

static std::wstring GetCurrentDirectoryString()
{
    DWORD need = ::GetCurrentDirectoryW(0, NULL);
    std::wstring result(need ? need : MAX_PATH, L'\0');
    DWORD got = ::GetCurrentDirectoryW(static_cast<DWORD>(result.size()), &result[0]);
    if (got == 0) return L".";
    result.resize(got);
    return result;
}

static std::wstring FindExportDll(const std::wstring& explicitPath)
{
    if (!explicitPath.empty()) {
        return FileExists(explicitPath) ? explicitPath : std::wstring();
    }

    std::wstring exeDir = GetModuleDirectory();
    std::wstring cwd = GetCurrentDirectoryString();

    const std::array<std::wstring, 6> candidates = {
        JoinPath(exeDir, L"ExportDOCX.dll"),
        JoinPath(cwd, L"ExportDOCX.dll"),
        JoinPath(cwd, L"out\\Release\\ExportDOCX.dll"),
        JoinPath(cwd, L"out\\Win32\\Release\\ExportDOCX.dll"),
        JoinPath(exeDir, L"..\\..\\out\\Release\\ExportDOCX.dll"),
        JoinPath(exeDir, L"..\\..\\out\\Win32\\Release\\ExportDOCX.dll")
    };

    for (const auto& c : candidates) {
        if (FileExists(c)) return c;
    }
    return std::wstring();
}


static std::wstring QuoteCommandLineArg(const std::wstring& arg)
{
    if (arg.empty()) return L"\"\"";

    bool needQuotes = false;
    for (wchar_t ch : arg) {
        if (ch == L' ' || ch == L'\t' || ch == L'\n' || ch == L'\r' || ch == L'"') {
            needQuotes = true;
            break;
        }
    }
    if (!needQuotes) return arg;

    std::wstring out;
    out.reserve(arg.size() + 8);
    out += L'"';
    size_t backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out += ch;
            backslashes = 0;
        } else {
            out.append(backslashes, L'\\');
            backslashes = 0;
            out += ch;
        }
    }
    out.append(backslashes * 2, L'\\');
    out += L'"';
    return out;
}

static HRESULT RunExportInWorkerProcess(const std::wstring& fb2Path, const std::wstring& docxPath,
                                        const std::wstring& dllPath, int timeoutSec,
                                        bool& timedOut, DWORD& workerExitCode)
{
    timedOut = false;
    workerExitCode = 0;

    std::wstring exePath = GetModulePath();
    if (exePath.empty()) return HRESULT_FROM_WIN32(::GetLastError());

    std::wstring cmdLine = QuoteCommandLineArg(exePath) + L" " +
                           QuoteCommandLineArg(fb2Path) + L" " +
                           QuoteCommandLineArg(docxPath) + L" -WorkerExport -Quiet";
    if (!dllPath.empty()) cmdLine += L" -Dll " + QuoteCommandLineArg(dllPath);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    BOOL ok = ::CreateProcessW(NULL, mutableCmd.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW,
                               NULL, NULL, &si, &pi);
    if (!ok) return HRESULT_FROM_WIN32(::GetLastError());

    DWORD waitMs = INFINITE;
    if (timeoutSec > 0) {
        unsigned long long requested = static_cast<unsigned long long>(timeoutSec) * 1000ull;
        waitMs = requested > 0xFFFFFFFEull ? 0xFFFFFFFEu : static_cast<DWORD>(requested);
    }

    DWORD waitResult = ::WaitForSingleObject(pi.hProcess, waitMs);
    HRESULT hr = E_FAIL;
    if (waitResult == WAIT_TIMEOUT) {
        timedOut = true;
        workerExitCode = static_cast<DWORD>(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
        ::TerminateProcess(pi.hProcess, workerExitCode);
        ::WaitForSingleObject(pi.hProcess, 5000);
        hr = static_cast<HRESULT>(workerExitCode);
    } else if (waitResult == WAIT_OBJECT_0) {
        if (!::GetExitCodeProcess(pi.hProcess, &workerExitCode)) {
            workerExitCode = static_cast<DWORD>(HRESULT_FROM_WIN32(::GetLastError()));
        }
        hr = static_cast<HRESULT>(workerExitCode);
    } else {
        workerExitCode = static_cast<DWORD>(HRESULT_FROM_WIN32(::GetLastError()));
        hr = static_cast<HRESULT>(workerExitCode);
    }

    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return hr;
}

static int RunWorkerExportOnce(const Options& opt)
{
    std::wstring dllPath = FindExportDll(opt.dllPath);
    if (dllPath.empty()) return static_cast<int>(static_cast<DWORD>(E_FAIL));

    HMODULE dll = ::LoadLibraryW(dllPath.c_str());
    if (!dll) return static_cast<int>(static_cast<DWORD>(HRESULT_FROM_WIN32(::GetLastError())));

    ExportFunc exportFunc = reinterpret_cast<ExportFunc>(::GetProcAddress(dll, "ExportFB2FileToDOCX"));
    if (!exportFunc) {
        HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
        ::FreeLibrary(dll);
        return static_cast<int>(static_cast<DWORD>(hr));
    }

    DWORD exceptionCode = 0;
    HRESULT hr = CallExportFunctionSafely(exportFunc, opt.input.c_str(), opt.output.c_str(), exceptionCode);
    ::FreeLibrary(dll);
    return static_cast<int>(static_cast<DWORD>(hr));
}

static void FindFb2FilesInDirectory(const std::wstring& dir, const std::wstring& mask, bool recurse, std::vector<std::wstring>& out)
{
    WIN32_FIND_DATAW fd = {};
    std::wstring findMask = JoinPath(dir, mask.empty() ? L"*" : mask);
    HANDLE h = ::FindFirstFileW(findMask.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                std::wstring full = JoinPath(dir, fd.cFileName);
                if (HasFb2Extension(full)) out.push_back(full);
            }
        } while (::FindNextFileW(h, &fd));
        ::FindClose(h);
    }

    if (!recurse) return;

    WIN32_FIND_DATAW sub = {};
    std::wstring subMask = JoinPath(dir, L"*");
    HANDLE hs = ::FindFirstFileW(subMask.c_str(), &sub);
    if (hs == INVALID_HANDLE_VALUE) return;

    do {
        const wchar_t* name = sub.cFileName;
        if (std::wcscmp(name, L".") == 0 || std::wcscmp(name, L"..") == 0) continue;
        if (sub.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            FindFb2FilesInDirectory(JoinPath(dir, name), mask, recurse, out);
        }
    } while (::FindNextFileW(hs, &sub));

    ::FindClose(hs);
}

static std::vector<std::wstring> FindFb2Files(const std::wstring& input, bool recurse)
{
    std::vector<std::wstring> result;

    if (ContainsWildcard(input)) {
        std::wstring dir = DirectoryOf(input);
        if (dir.empty()) dir = GetCurrentDirectoryString();
        if (DirectoryExists(dir)) {
            std::wstring mask = FileNameOf(input);
            FindFb2FilesInDirectory(TrimTrailingSlash(dir), mask, recurse, result);
        }
    } else {
        DWORD attr = ::GetFileAttributesW(input.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) return result;

        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            FindFb2FilesInDirectory(TrimTrailingSlash(input), L"*.fb2", recurse, result);
        } else if (HasFb2Extension(input)) {
            result.push_back(input);
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

static void AppendUnique(std::vector<std::wstring>& dst, const std::vector<std::wstring>& src)
{
    dst.insert(dst.end(), src.begin(), src.end());
    std::sort(dst.begin(), dst.end());
    dst.erase(std::unique(dst.begin(), dst.end()), dst.end());
}

static bool ReadTextFileWide(const std::wstring& path, std::wstring& text);

static std::vector<std::wstring> ExpandInputList(const std::wstring& listPath, bool recurse)
{
    std::vector<std::wstring> result;
    std::wstring text;
    if (!ReadTextFileWide(listPath, text)) return result;

    std::wstring baseDir = DirectoryOf(listPath);
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find_first_of(L"\r\n", pos);
        std::wstring line = (end == std::wstring::npos) ? text.substr(pos) : text.substr(pos, end - pos);
        pos = (end == std::wstring::npos) ? text.size() + 1 : end + 1;
        while (pos < text.size() && (text[pos] == L'\r' || text[pos] == L'\n')) ++pos;

        line = TrimString(line);
        if (line.empty() || line[0] == L'#' || line[0] == L';') continue;

        // Relative paths in a list are resolved relative to the list file itself.
        if (line.find(L':') == std::wstring::npos && line.find_first_of(L"\\/") != 0 && !baseDir.empty()) {
            line = JoinPath(baseDir, line);
        }
        AppendUnique(result, FindFb2Files(line, recurse));
    }
    return result;
}

static std::wstring RelativeDirectoryFor(const std::wstring& root, const std::wstring& filePath)
{
    std::wstring cleanRoot = TrimTrailingSlash(root);
    std::wstring fileDir = DirectoryOf(filePath);
    if (cleanRoot.empty() || fileDir.size() <= cleanRoot.size()) return std::wstring();

    if (_wcsnicmp(fileDir.c_str(), cleanRoot.c_str(), cleanRoot.size()) != 0) return std::wstring();

    wchar_t sep = fileDir[cleanRoot.size()];
    if (sep != L'\\' && sep != L'/') return std::wstring();

    return fileDir.substr(cleanRoot.size() + 1);
}

static std::string ToUtf8(const std::wstring& text)
{
    if (text.empty()) return std::string();
    int size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
    if (size <= 0) return std::string();
    std::string out(size, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &out[0], size, NULL, NULL);
    return out;
}

static std::wstring CsvEscape(const std::wstring& s)
{
    std::wstring out = L"\"";
    for (wchar_t ch : s) {
        if (ch == L'\"') out += L"\"\"";
        else out += ch;
    }
    out += L"\"";
    return out;
}

static bool WriteUtf8File(const std::wstring& path, const std::string& bytes)
{
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    ::WriteFile(h, bom, sizeof(bom), &written, NULL);
    BOOL ok = ::WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL);
    ::CloseHandle(h);
    return ok != FALSE;
}

static bool AppendUtf8File(const std::wstring& path, const std::string& bytes)
{
    HANDLE h = ::CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = ::WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL);
    ::CloseHandle(h);
    return ok != FALSE;
}

static bool ReadTextFileWide(const std::wstring& path, std::wstring& text)
{
    text.clear();
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size = {};
    if (!::GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > 32LL * 1024LL * 1024LL) {
        ::CloseHandle(h);
        return false;
    }

    std::vector<unsigned char> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    BOOL ok = bytes.empty() ? TRUE : ::ReadFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &read, NULL);
    ::CloseHandle(h);
    if (!ok) return false;
    bytes.resize(read);

    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        size_t chars = (bytes.size() - 2) / 2;
        text.assign(reinterpret_cast<const wchar_t*>(bytes.data() + 2), chars);
        return true;
    }

    UINT cp = CP_UTF8;
    size_t offset = 0;
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) offset = 3;

    int need = ::MultiByteToWideChar(cp, 0, reinterpret_cast<const char*>(bytes.data() + offset),
                                      static_cast<int>(bytes.size() - offset), NULL, 0);
    if (need <= 0) {
        cp = CP_ACP;
        need = ::MultiByteToWideChar(cp, 0, reinterpret_cast<const char*>(bytes.data() + offset),
                                      static_cast<int>(bytes.size() - offset), NULL, 0);
    }
    if (need <= 0) return false;
    text.assign(need, L'\0');
    ::MultiByteToWideChar(cp, 0, reinterpret_cast<const char*>(bytes.data() + offset),
                          static_cast<int>(bytes.size() - offset), &text[0], need);
    return true;
}


static bool BytesToWideText(const std::vector<unsigned char>& bytes, std::wstring& text)
{
    text.clear();
    if (bytes.empty()) return true;
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        size_t chars = (bytes.size() - 2) / 2;
        text.assign(reinterpret_cast<const wchar_t*>(bytes.data() + 2), chars);
        return true;
    }

    UINT cp = CP_UTF8;
    size_t offset = 0;
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) offset = 3;

    int need = ::MultiByteToWideChar(cp, 0, reinterpret_cast<const char*>(bytes.data() + offset),
                                      static_cast<int>(bytes.size() - offset), NULL, 0);
    if (need <= 0) {
        cp = CP_ACP;
        need = ::MultiByteToWideChar(cp, 0, reinterpret_cast<const char*>(bytes.data() + offset),
                                      static_cast<int>(bytes.size() - offset), NULL, 0);
    }
    if (need <= 0) return false;
    text.assign(need, L'\0');
    ::MultiByteToWideChar(cp, 0, reinterpret_cast<const char*>(bytes.data() + offset),
                          static_cast<int>(bytes.size() - offset), &text[0], need);
    return true;
}

static bool ReadBinaryFile(const std::wstring& path, std::vector<unsigned char>& bytes)
{
    bytes.clear();
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size = {};
    if (!::GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > 256LL * 1024LL * 1024LL) {
        ::CloseHandle(h);
        return false;
    }

    bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    BOOL ok = bytes.empty() ? TRUE : ::ReadFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &read, NULL);
    ::CloseHandle(h);
    if (!ok) return false;
    bytes.resize(read);
    return true;
}

static WORD ReadLe16(const std::vector<unsigned char>& b, size_t off)
{
    if (off + 2 > b.size()) return 0;
    return static_cast<WORD>(b[off] | (b[off + 1] << 8));
}

static DWORD ReadLe32(const std::vector<unsigned char>& b, size_t off)
{
    if (off + 4 > b.size()) return 0;
    return static_cast<DWORD>(b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24));
}

struct ZipEntryInfo {
    std::wstring name;
    WORD method = 0;
    DWORD compressedSize = 0;
    DWORD uncompressedSize = 0;
    DWORD localOffset = 0;
};

using ZipEntryMap = std::map<std::wstring, ZipEntryInfo, std::less<>>;
using WStringMap = std::map<std::wstring, std::wstring, std::less<>>;

static bool ListZipCentralDirectoryEx(const std::wstring& path, ZipEntryMap& entries,
                                      std::vector<unsigned char>* rawBytes, std::wstring& message)
{
    entries.clear();
    message.clear();

    std::vector<unsigned char> bytes;
    if (!ReadBinaryFile(path, bytes)) {
        message = L"Could not read DOCX file";
        return false;
    }
    if (bytes.size() < 22) {
        message = L"DOCX is too small to be a ZIP package";
        return false;
    }

    size_t searchStart = bytes.size() > (65535u + 22u) ? bytes.size() - (65535u + 22u) : 0;
    size_t eocd = std::wstring::npos;
    for (size_t i = bytes.size() - 22; ; --i) {
        if (ReadLe32(bytes, i) == 0x06054b50) {
            eocd = i;
            break;
        }
        if (i == searchStart) break;
    }
    if (eocd == std::wstring::npos) {
        message = L"ZIP end-of-central-directory record was not found";
        return false;
    }

    DWORD cdSize = ReadLe32(bytes, eocd + 12);
    DWORD cdOffset = ReadLe32(bytes, eocd + 16);
    if (cdOffset >= bytes.size() || static_cast<size_t>(cdOffset) + static_cast<size_t>(cdSize) > bytes.size()) {
        message = L"ZIP central-directory offset/size is invalid";
        return false;
    }

    size_t pos = cdOffset;
    size_t end = static_cast<size_t>(cdOffset) + static_cast<size_t>(cdSize);
    while (pos + 46 <= end) {
        if (ReadLe32(bytes, pos) != 0x02014b50) {
            message = L"ZIP central-directory entry signature is invalid";
            return false;
        }
        WORD method = ReadLe16(bytes, pos + 10);
        DWORD compressedSize = ReadLe32(bytes, pos + 20);
        DWORD uncompressedSize = ReadLe32(bytes, pos + 24);
        WORD nameLen = ReadLe16(bytes, pos + 28);
        WORD extraLen = ReadLe16(bytes, pos + 30);
        WORD commentLen = ReadLe16(bytes, pos + 32);
        DWORD localOffset = ReadLe32(bytes, pos + 42);
        size_t namePos = pos + 46;
        if (namePos + nameLen > bytes.size()) {
            message = L"ZIP central-directory entry name is truncated";
            return false;
        }
        std::string name(reinterpret_cast<const char*>(bytes.data() + namePos), nameLen);
        std::wstring wname;
        int need = ::MultiByteToWideChar(CP_UTF8, 0, name.c_str(), static_cast<int>(name.size()), NULL, 0);
        if (need > 0) {
            wname.assign(need, L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, name.c_str(), static_cast<int>(name.size()), &wname[0], need);
        } else {
            need = ::MultiByteToWideChar(CP_ACP, 0, name.c_str(), static_cast<int>(name.size()), NULL, 0);
            if (need > 0) {
                wname.assign(need, L'\0');
                ::MultiByteToWideChar(CP_ACP, 0, name.c_str(), static_cast<int>(name.size()), &wname[0], need);
            }
        }
        if (!wname.empty()) {
            ZipEntryInfo info;
            info.name = wname;
            info.method = method;
            info.compressedSize = compressedSize;
            info.uncompressedSize = uncompressedSize;
            info.localOffset = localOffset;
            entries[wname] = info;
        }
        pos = namePos + nameLen + extraLen + commentLen;
    }

    if (entries.empty()) {
        message = L"ZIP central directory contains no entries";
        return false;
    }
    if (rawBytes) rawBytes->swap(bytes);
    return true;
}

static bool ListZipCentralDirectory(const std::wstring& path, std::set<std::wstring>& entries, std::wstring& message)
{
    entries.clear();
    ZipEntryMap info;
    if (!ListZipCentralDirectoryEx(path, info, NULL, message)) return false;
    for (const auto& kv : info) entries.insert(kv.first);
    return true;
}

static bool ExtractStoredZipEntryText(const std::vector<unsigned char>& bytes, const ZipEntryInfo& info, std::wstring& text)
{
    text.clear();
    if (info.method != 0) return false; // ExportDOCX writes stored entries; compressed third-party DOCX is not decoded here.
    size_t p = info.localOffset;
    if (p + 30 > bytes.size() || ReadLe32(bytes, p) != 0x04034b50) return false;
    WORD nameLen = ReadLe16(bytes, p + 26);
    WORD extraLen = ReadLe16(bytes, p + 28);
    size_t dataPos = p + 30 + nameLen + extraLen;
    if (dataPos + info.uncompressedSize > bytes.size()) return false;
    std::vector<unsigned char> data(bytes.begin() + dataPos, bytes.begin() + dataPos + info.uncompressedSize);
    return BytesToWideText(data, text);
}

static std::wstring NormalizeZipPath(std::wstring path)
{
    std::replace(path.begin(), path.end(), L'\\', L'/');
    std::vector<std::wstring> parts;
    size_t pos = 0;
    while (pos <= path.size()) {
        size_t next = path.find(L'/', pos);
        std::wstring part = (next == std::wstring::npos) ? path.substr(pos) : path.substr(pos, next - pos);
        if (part.empty() || part == L".") {
        } else if (part == L"..") {
            if (!parts.empty()) parts.pop_back();
        } else {
            parts.push_back(part);
        }
        if (next == std::wstring::npos) break;
        pos = next + 1;
    }
    std::wstring out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += L"/";
        out += parts[i];
    }
    return out;
}

static std::wstring RelsBasePath(const std::wstring& relsPath)
{
    if (relsPath == L"_rels/.rels") return L"";
    size_t p = relsPath.find(L"/_rels/");
    if (p == std::wstring::npos) return L"";
    return relsPath.substr(0, p + 1);
}

static bool IsExternalTarget(const std::wstring& target)
{
    return StartsWithI(target, L"http://") || StartsWithI(target, L"https://") ||
           StartsWithI(target, L"mailto:") || StartsWithI(target, L"file:");
}

static void ExtractAttributeValues(const std::wstring& xml, const std::wstring& attrName, std::vector<std::wstring>& values)
{
    size_t p = 0;
    while (true) {
        p = xml.find(attrName, p);
        if (p == std::wstring::npos) break;
        p += attrName.size();
        while (p < xml.size() && iswspace(xml[p])) ++p;
        if (p >= xml.size() || xml[p] != L'=') continue;
        ++p;
        while (p < xml.size() && iswspace(xml[p])) ++p;
        if (p >= xml.size() || (xml[p] != L'"' && xml[p] != L'\'')) continue;
        wchar_t quote = xml[p++];
        size_t e = xml.find(quote, p);
        if (e == std::wstring::npos) break;
        values.emplace_back(xml, p, e - p);
        p = e + 1;
    }
}

static void ExtractRelationshipIds(const std::wstring& relsXml, std::set<std::wstring>& ids)
{
    std::vector<std::wstring> values;
    ExtractAttributeValues(relsXml, L"Id", values);
    for (const auto& v : values) ids.insert(v);
}

static void ValidateRelationshipTargets(const std::wstring& relsPath, const std::wstring& relsXml,
                                        const ZipEntryMap& entries,
                                        std::vector<std::wstring>& errors)
{
    std::wstring base = RelsBasePath(relsPath);
    size_t p = 0;
    while ((p = relsXml.find(L"<Relationship", p)) != std::wstring::npos) {
        size_t e = relsXml.find(L'>', p);
        if (e == std::wstring::npos) break;
        std::wstring tag = relsXml.substr(p, e - p + 1);
        std::vector<std::wstring> targets;
        ExtractAttributeValues(tag, L"Target", targets);
        if (!targets.empty()) {
            std::vector<std::wstring> modes;
            ExtractAttributeValues(tag, L"TargetMode", modes);
            bool external = (!modes.empty() && _wcsicmp(modes[0].c_str(), L"External") == 0) || IsExternalTarget(targets[0]);
            if (!external) {
                std::wstring normalized = targets[0];
                if (!normalized.empty() && normalized[0] == L'/') normalized.erase(0, 1);
                else normalized = base + normalized;
                normalized = NormalizeZipPath(normalized);
                if (entries.find(normalized) == entries.end()) {
                    std::wstring error = L"Relationship target is missing: ";
                    error += relsPath;
                    error += L" -> ";
                    error += normalized;
                    errors.emplace_back(std::move(error));
                }
            }
        }
        p = e + 1;
    }
}

static std::wstring RelsPathForXmlPart(const std::wstring& xmlPart)
{
    size_t slash = xmlPart.find_last_of(L'/');
    if (slash == std::wstring::npos) return L"_rels/" + xmlPart + L".rels";
    return xmlPart.substr(0, slash + 1) + L"_rels/" + xmlPart.substr(slash + 1) + L".rels";
}

static void ValidateRidReferences(const std::wstring& xmlPart, const std::wstring& xml,
                                  const ZipEntryMap& entries,
                                  const WStringMap& relsText,
                                  std::vector<std::wstring>& errors)
{
    std::vector<std::wstring> ids;
    ExtractAttributeValues(xml, L"r:id", ids);
    ExtractAttributeValues(xml, L"r:embed", ids);
    ExtractAttributeValues(xml, L"r:link", ids);
    if (ids.empty()) return;

    std::wstring relsPath = RelsPathForXmlPart(xmlPart);
    auto rt = relsText.find(relsPath);
    if (rt == relsText.end()) {
        std::wstring error = L"XML part uses r:id/r:embed but has no .rels file: ";
        error += xmlPart;
        errors.emplace_back(std::move(error));
        return;
    }
    std::set<std::wstring> relIds;
    ExtractRelationshipIds(rt->second, relIds);
    for (const auto& id : ids) {
        if (relIds.find(id) == relIds.end()) {
            std::wstring error = L"Relationship id is not defined: ";
            error += xmlPart;
            error += L" -> ";
            error += id;
            errors.emplace_back(std::move(error));
        }
    }
}

static bool ValidateDocxPackageBasic(const std::wstring& docxPath, std::wstring& message)
{
    ZipEntryMap entries;
    std::vector<unsigned char> bytes;
    if (!ListZipCentralDirectoryEx(docxPath, entries, &bytes, message)) return false;

    const wchar_t* required[] = {
        L"[Content_Types].xml",
        L"_rels/.rels",
        L"word/document.xml",
        L"word/_rels/document.xml.rels",
        L"word/styles.xml",
        L"word/settings.xml"
    };
    for (const wchar_t* name : required) {
        if (entries.find(name) == entries.end()) {
            message = L"Required DOCX part is missing: ";
            message += name;
            return false;
        }
    }

    std::vector<std::wstring> errors;
    WStringMap xmlText;
    WStringMap relsText;
    int xmlChecked = 0;
    int relsChecked = 0;
    int mediaChecked = 0;

    for (const auto& kv : entries) {
        const std::wstring& name = kv.first;
        const ZipEntryInfo& info = kv.second;
        if (EndsWithI(name, L".xml") || EndsWithI(name, L".rels")) {
            if (info.uncompressedSize == 0) {
                std::wstring error = L"Empty XML/RELS part: ";
                error += name;
                errors.emplace_back(std::move(error));
                continue;
            }
            std::wstring text;
            if (ExtractStoredZipEntryText(bytes, info, text)) {
                if (text.find(L'<') == std::wstring::npos || text.find(L'>') == std::wstring::npos) {
                    std::wstring error = L"XML/RELS part does not look like XML: ";
                    error += name;
                    errors.emplace_back(std::move(error));
                }
                if (EndsWithI(name, L".xml")) {
                    xmlText.emplace(name, std::move(text));
                    ++xmlChecked;
                } else if (EndsWithI(name, L".rels")) {
                    relsText.emplace(name, std::move(text));
                    ++relsChecked;
                }
            } else if (info.method != 0) {
                // Not an error for arbitrary DOCX, but ExportDOCX-generated packages are stored.
                if (EndsWithI(name, L".xml")) ++xmlChecked;
                if (EndsWithI(name, L".rels")) ++relsChecked;
            } else {
                std::wstring error = L"Could not extract XML/RELS part: ";
                error += name;
                errors.emplace_back(std::move(error));
            }
        }
        if (StartsWithI(name, L"word/media/")) {
            ++mediaChecked;
            if (info.uncompressedSize == 0) {
                std::wstring error = L"Empty media part: ";
                error += name;
                errors.emplace_back(std::move(error));
            }
        }
    }

    bool hasFootnotes = entries.find(L"word/footnotes.xml") != entries.end();
    bool hasEndnotes = entries.find(L"word/endnotes.xml") != entries.end();
    if (hasFootnotes && entries.find(L"word/_rels/footnotes.xml.rels") == entries.end())
        errors.emplace_back(L"word/footnotes.xml exists, but word/_rels/footnotes.xml.rels is missing");
    if (hasEndnotes && entries.find(L"word/_rels/endnotes.xml.rels") == entries.end())
        errors.emplace_back(L"word/endnotes.xml exists, but word/_rels/endnotes.xml.rels is missing");

    for (const auto& kv : relsText)
        ValidateRelationshipTargets(kv.first, kv.second, entries, errors);
    for (const auto& kv : xmlText)
        ValidateRidReferences(kv.first, kv.second, entries, relsText, errors);

    auto ct = xmlText.find(L"[Content_Types].xml");
    if (ct != xmlText.end()) {
        const std::wstring& c = ct->second;
        if (mediaChecked > 0) {
            if (c.find(L"Extension=\"png\"") == std::wstring::npos && c.find(L"image/png") == std::wstring::npos)
                errors.emplace_back(L"[Content_Types].xml has no PNG image content type/default");
            if (c.find(L"Extension=\"jpg\"") == std::wstring::npos && c.find(L"Extension=\"jpeg\"") == std::wstring::npos && c.find(L"image/jpeg") == std::wstring::npos)
                errors.emplace_back(L"[Content_Types].xml has no JPEG image content type/default");
        }
    }

    if (!errors.empty()) {
        message = L"INVALID: ";
        size_t maxErrors = std::min<size_t>(errors.size(), 5);
        for (size_t i = 0; i < maxErrors; ++i) {
            if (i) message += L"; ";
            message += errors[i];
        }
        if (errors.size() > maxErrors) message += L"; ...";
        return false;
    }

    message = L"OK: DOCX package structure and relationships look valid; XML parts checked=" +
              std::to_wstring(xmlChecked) + L", rels checked=" + std::to_wstring(relsChecked) +
              L", media=" + std::to_wstring(mediaChecked);
    return true;
}

static std::wstring ExportReportPathForDocx(const std::wstring& docxPath)
{
    std::wstring report = docxPath;
    size_t slash = report.find_last_of(L"\\/");
    size_t dot = report.find_last_of(L'.');
    if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash)) report.erase(dot);
    report += L"_export_report.txt";
    return report;
}

static bool ExportReportWarningsText(const std::wstring& docxPath, std::wstring& warnings)
{
    warnings.clear();
    std::wstring reportPath = ExportReportPathForDocx(docxPath);
    std::wstring text;
    if (!ReadTextFileWide(reportPath, text)) return false;

    size_t p = text.find(L"Предупреждения:");
    if (p == std::wstring::npos) p = text.find(L"Warnings:");
    if (p == std::wstring::npos) return false;

    p = text.find_first_of(L"\r\n", p);
    if (p == std::wstring::npos) return false;
    while (p < text.size() && (text[p] == L'\r' || text[p] == L'\n')) ++p;
    std::wstring tail = text.substr(p);
    size_t nextSection = tail.find(L"\r\n\r\n");
    if (nextSection != std::wstring::npos) tail = tail.substr(0, nextSection);
    tail = TrimString(tail);
    if (tail.empty() || _wcsicmp(tail.c_str(), L"нет") == 0 || _wcsicmp(tail.c_str(), L"none") == 0)
        return false;
    warnings = tail;
    return !warnings.empty();
}

static bool ExportReportHasWarnings(const std::wstring& docxPath, std::wstring& message)
{
    message.clear();
    std::wstring warnings;
    if (!ExportReportWarningsText(docxPath, warnings)) return false;

    size_t dash = warnings.find(L"- ");
    if (dash != std::wstring::npos) {
        size_t eol = warnings.find_first_of(L"\r\n", dash);
        message = (eol == std::wstring::npos) ? warnings.substr(dash) : warnings.substr(dash, eol - dash);
    } else {
        size_t eol = warnings.find_first_of(L"\r\n");
        message = (eol == std::wstring::npos) ? warnings : warnings.substr(0, eol);
    }
    return true;
}

static std::wstring HResultHex(HRESULT hr)
{
    wchar_t buf[32] = {};
    std::swprintf(buf, 32, L"0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

static std::wstring FormatWin32Error(DWORD error)
{
    if (error == ERROR_SUCCESS) return L"0";

    LPWSTR buffer = NULL;
    DWORD len = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, NULL);
    std::wstring text;
    if (len && buffer) {
        text.assign(buffer, len);
        while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L'.' || text.back() == L' ')) {
            text.pop_back();
        }
        ::LocalFree(buffer);
    }

    wchar_t code[32] = {};
    std::swprintf(code, 32, L"%lu", static_cast<unsigned long>(error));
    if (text.empty()) return std::wstring(L"Win32 error ") + code;
    return std::wstring(L"Win32 error ") + code + L": " + text;
}


static std::wstring BstrToWString(BSTR value)
{
    return value ? std::wstring(value, ::SysStringLen(value)) : std::wstring();
}

static bool ValidateInputFb2Xml(const std::wstring& path, std::wstring& message, HRESULT& parseHr)
{
    message.clear();
    parseHr = S_OK;

    HRESULT hrCo = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool needUninit = SUCCEEDED(hrCo);
    if (FAILED(hrCo) && hrCo != RPC_E_CHANGED_MODE) {
        parseHr = hrCo;
        message = L"Could not initialize COM for XML pre-validation: " + HResultHex(hrCo);
        return false;
    }

    bool valid = false;
    IXMLDOMDocument2* doc = NULL;
    CLSID clsid = {};
    HRESULT hr = ::CLSIDFromProgID(L"Msxml2.DOMDocument.6.0", &clsid);
    if (SUCCEEDED(hr)) {
        hr = ::CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&doc));
    }

    if (FAILED(hr) || doc == NULL) {
        parseHr = FAILED(hr) ? hr : E_FAIL;
        message = L"Could not create MSXML DOMDocument.6.0 for XML pre-validation: " + HResultHex(parseHr);
    } else {
        doc->put_async(VARIANT_FALSE);
        doc->put_validateOnParse(VARIANT_FALSE);
        doc->put_resolveExternals(VARIANT_FALSE);

        VARIANT fileName;
        ::VariantInit(&fileName);
        fileName.vt = VT_BSTR;
        fileName.bstrVal = ::SysAllocString(path.c_str());
        VARIANT_BOOL ok = VARIANT_FALSE;
        hr = doc->load(fileName, &ok);
        ::VariantClear(&fileName);

        if (SUCCEEDED(hr) && ok == VARIANT_TRUE) {
            valid = true;
            message = L"OK";
        } else {
            IXMLDOMParseError* err = NULL;
            HRESULT errHr = doc->get_parseError(&err);
            if (SUCCEEDED(errHr) && err != NULL) {
                long line = 0;
                long col = 0;
                long code = 0;
                BSTR reason = NULL;
                BSTR srcText = NULL;
                err->get_line(&line);
                err->get_linepos(&col);
                err->get_errorCode(&code);
                err->get_reason(&reason);
                err->get_srcText(&srcText);
                parseHr = code != 0 ? static_cast<HRESULT>(code) : (FAILED(hr) ? hr : E_FAIL);
                message = L"Input FB2 is not well-formed XML";
                if (line > 0) message += L" at line " + std::to_wstring(line);
                if (col > 0) message += L", column " + std::to_wstring(col);
                std::wstring reasonText = TrimString(BstrToWString(reason));
                if (!reasonText.empty()) message += L": " + reasonText;
                std::wstring src = TrimString(BstrToWString(srcText));
                if (!src.empty()) {
                    if (src.size() > 180) src = src.substr(0, 180) + L"...";
                    message += L"; source: " + src;
                }
                if (reason) ::SysFreeString(reason);
                if (srcText) ::SysFreeString(srcText);
                err->Release();
            } else {
                parseHr = FAILED(hr) ? hr : E_FAIL;
                message = L"Input FB2 is not well-formed XML: " + HResultHex(parseHr);
            }
        }
        doc->Release();
    }

    if (needUninit) ::CoUninitialize();
    return valid;
}

static std::wstring GetInputRoot(const std::wstring& input)
{
    if (ContainsWildcard(input)) {
        std::wstring dir = DirectoryOf(input);
        return dir.empty() ? GetCurrentDirectoryString() : TrimTrailingSlash(dir);
    }

    DWORD attr = ::GetFileAttributesW(input.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) return TrimTrailingSlash(input);
    return DirectoryOf(input);
}

static std::wstring DefaultLogPath(const Options& opt, bool inputIsSet)
{
    if (!opt.logPath.empty()) return opt.logPath;

    if (!opt.output.empty()) {
        if (HasDocxExtension(opt.output)) {
            std::wstring dir = DirectoryOf(opt.output);
            return JoinPath(dir, L"ExportDOCX_batch_log.csv");
        }
        return JoinPath(opt.output, L"ExportDOCX_batch_log.csv");
    }

    if (inputIsSet) {
        std::wstring dir = DirectoryOf(opt.input);
        return JoinPath(dir, L"ExportDOCX_batch_log.csv");
    }

    return L"ExportDOCX_batch_log.csv";
}

static std::wstring MakeOutputPath(const Options& opt, const std::wstring& root, const std::wstring& file, bool inputIsSingleFile)
{
    if (inputIsSingleFile && HasDocxExtension(opt.output)) {
        return opt.output;
    }

    std::wstring targetDir = opt.output;
    if (!inputIsSingleFile && opt.recurse && !opt.flat) {
        std::wstring relDir = RelativeDirectoryFor(root, file);
        if (!relDir.empty()) targetDir = JoinPath(targetDir, relDir);
    }

    return JoinPath(targetDir, ChangeExtensionToDocx(FileNameOf(file)));
}
static bool ContainsPathNoCase(const std::vector<std::wstring>& paths, const std::wstring& path)
{
    for (const auto& p : paths) {
        if (_wcsicmp(p.c_str(), path.c_str()) == 0) return true;
    }
    return false;
}

static std::wstring MakeUniqueOutputPath(const std::wstring& candidate, const std::vector<std::wstring>& usedOutputs)
{
    if (!ContainsPathNoCase(usedOutputs, candidate) && !FileExists(candidate)) {
        return candidate;
    }

    std::wstring dir = DirectoryOf(candidate);
    std::wstring name = FileNameOf(candidate);
    std::wstring base = name;
    std::wstring ext;

    size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        base = name.substr(0, dot);
        ext = name.substr(dot);
    }

    for (int i = 2; i < 10000; ++i) {
        std::wstring nextName = base + L"_" + std::to_wstring(i) + ext;
        std::wstring nextPath = JoinPath(dir, nextName);
        if (!ContainsPathNoCase(usedOutputs, nextPath) && !FileExists(nextPath)) {
            return nextPath;
        }
    }

    return candidate;
}



static std::wstring HtmlEscape(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size() + 32);
    for (wchar_t ch : s) {
        switch (ch) {
        case L'&': out += L"&amp;"; break;
        case L'<': out += L"&lt;"; break;
        case L'>': out += L"&gt;"; break;
        case L'\"': out += L"&quot;"; break;
        default: out += ch; break;
        }
    }
    return out;
}


static std::wstring PathToFileUrl(std::wstring path)
{
    if (path.empty()) return std::wstring();
    std::replace(path.begin(), path.end(), L'\\', L'/');

    std::wstring out;
    out.reserve(path.size() + 16);
    if (path.size() >= 2 && path[1] == L':') {
        out = L"file:///";
    } else if (path.size() >= 2 && path[0] == L'/' && path[1] == L'/') {
        out = L"file:";
    } else {
        out = L"file:///";
    }

    for (wchar_t ch : path) {
        switch (ch) {
        case L'%': out += L"%25"; break;
        case L' ': out += L"%20"; break;
        case L'#': out += L"%23"; break;
        case L'?': out += L"%3F"; break;
        default: out += ch; break;
        }
    }
    return out;
}

static void AppendHtmlPathCell(std::wstring& html, const std::wstring& path)
{
    html += L"<td>";
    if (!path.empty()) {
        html += L"<a href=\"" + HtmlEscape(PathToFileUrl(path)) + L"\">" + HtmlEscape(path) + L"</a>";
    }
    html += L"</td>";
}
static void AppendHtmlRow(std::wstring& html, const ResultRow& row)
{
    html += L"<tr class=\"" + HtmlEscape(row.status) + L"\">";
    html += L"<td>" + HtmlEscape(row.status) + L"</td>";
    AppendHtmlPathCell(html, row.input);
    AppendHtmlPathCell(html, row.output);
    html += L"<td>" + HtmlEscape(row.hr) + L"</td>";
    html += L"<td>" + std::to_wstring(row.elapsedMs) + L"</td>";
    html += L"<td>" + HtmlEscape(row.message) + L"</td>";
    html += L"<td>" + HtmlEscape(row.validation) + L"</td>";
    html += L"</tr>\r\n";
}
static bool WriteHtmlReport(const std::wstring& path, const Options& opt, const std::wstring& logPath,
                            const std::wstring& dllPath, const std::wstring& warningsSummaryPath,
                            const std::wstring& warningsFilesPath,
                            int total, int ok, int fail, int timeouts, int skipped,
                            int upToDate, int planned, int filtered, int okWarnings, int docxInvalid,
                            int inputInvalid, int expectedFail, int unexpectedFail, const std::vector<ResultRow>& rows)
{
    std::wstring html;
    html += L"<!doctype html><html><head><meta charset=\"utf-8\"><title>ExportDOCXBatch report</title>";
    html += L"<style>body{font-family:Segoe UI,Arial,sans-serif;margin:24px;}table{border-collapse:collapse;width:100%;font-size:13px;}th,td{border:1px solid #ccc;padding:4px 6px;vertical-align:top;}th{background:#eee;position:sticky;top:0;}a{color:#0645ad;text-decoration:none}a:hover{text-decoration:underline}tr.OK{background:#e9ffe9;}tr.OK_WITH_WARNINGS{background:#fff8d8;}tr.DOCX_INVALID,tr.FAIL,tr.TIMEOUT{background:#ffe9e9;}tr.INPUT_INVALID{background:#fff0e0;}tr.SKIP,tr.UPTODATE,tr.FILTERED{background:#f7f7f7;}tr.DRYRUN{background:#eef5ff}.summary td:first-child{font-weight:bold;width:220px}</style>";
    html += L"</head><body>";
    html += L"<h1>ExportDOCXBatch report</h1>";
    html += L"<table class=\"summary\">";
    html += L"<tr><td>Generated</td><td>" + HtmlEscape(FormatLocalTimestamp()) + L"</td></tr>";
    html += L"<tr><td>Input</td><td>" + HtmlEscape(opt.input) + L"</td></tr>";
    if (!opt.inputListPath.empty()) html += L"<tr><td>Input list</td><td>" + HtmlEscape(opt.inputListPath) + L"</td></tr>";
    html += L"<tr><td>Output</td><td>" + HtmlEscape(opt.output) + L"</td></tr>";
    if (!dllPath.empty()) html += L"<tr><td>DLL</td><td>" + HtmlEscape(dllPath) + L"</td></tr>";
    html += L"<tr><td>CSV log</td><td>" + HtmlEscape(logPath) + L"</td></tr>";
    const int okClean = ok - okWarnings;
    html += L"<tr><td>Total</td><td>" + std::to_wstring(total) + L"</td></tr>";
    html += L"<tr><td>SUCCESS_TOTAL</td><td>" + std::to_wstring(ok) + L"</td></tr>";
    html += L"<tr><td>OK_CLEAN</td><td>" + std::to_wstring(okClean) + L"</td></tr>";
    html += L"<tr><td>OK_WITH_WARNINGS</td><td>" + std::to_wstring(okWarnings) + L"</td></tr>";
    html += L"<tr><td>DOCX_INVALID</td><td>" + std::to_wstring(docxInvalid) + L"</td></tr>";
    html += L"<tr><td>INPUT_INVALID</td><td>" + std::to_wstring(inputInvalid) + L"</td></tr>";
    html += L"<tr><td>FAIL</td><td>" + std::to_wstring(fail) + L"</td></tr>";
    html += L"<tr><td>TIMEOUT</td><td>" + std::to_wstring(timeouts) + L"</td></tr>";
    html += L"<tr><td>Expected FAIL</td><td>" + std::to_wstring(expectedFail) + L"</td></tr>";
    html += L"<tr><td>Unexpected FAIL</td><td>" + std::to_wstring(unexpectedFail) + L"</td></tr>";
    html += L"<tr><td>SKIP</td><td>" + std::to_wstring(skipped) + L"</td></tr>";
    html += L"<tr><td>UPTODATE</td><td>" + std::to_wstring(upToDate) + L"</td></tr>";
    html += L"<tr><td>DRYRUN/PLANNED</td><td>" + std::to_wstring(planned) + L"</td></tr>";
    html += L"<tr><td>FILTERED</td><td>" + std::to_wstring(filtered) + L"</td></tr>";
    html += L"</table>";
    if (!warningsSummaryPath.empty()) html += L"<p>Warnings summary: <a href=\"" + HtmlEscape(PathToFileUrl(warningsSummaryPath)) + L"\">" + HtmlEscape(warningsSummaryPath) + L"</a></p>";
    html += L"<p>Input/Output paths are local file links. Depending on browser security settings, opening local links may be restricted.</p>";
    html += L"<h2>Files</h2><table><tr><th>Status</th><th>Input</th><th>Output</th><th>HRESULT</th><th>Elapsed ms</th><th>Message</th><th>Validation</th></tr>\r\n";
    for (const auto& row : rows) AppendHtmlRow(html, row);
    html += L"</table></body></html>\r\n";

    EnsureDirectory(DirectoryOf(path));
    return WriteUtf8File(path, ToUtf8(html));
}

static void Usage()
{
    PrintLine(L"ExportDOCXBatch.exe - batch FB2 to DOCX export utility");
    PrintLine(L"");
    PrintLine(L"Usage:");
    PrintLine(L"  ExportDOCXBatch.exe \"D:\\Books\" \"D:\\Books_DOCX\"");
    PrintLine(L"  ExportDOCXBatch.exe \"D:\\Books\" \"D:\\Books_DOCX\" -Recurse");
    PrintLine(L"  ExportDOCXBatch.exe \"D:\\Books\\*.fb2\" \"D:\\Books_DOCX\" -DryRun");
    PrintLine(L"  ExportDOCXBatch.exe \"D:\\Book.fb2\" \"D:\\Books_DOCX\"");
    PrintLine(L"  ExportDOCXBatch.exe \"D:\\Book.fb2\" \"D:\\Books_DOCX\\Book.docx\"");
    PrintLine(L"  ExportDOCXBatch.exe -Help");
    PrintLine(L"");
    PrintLine(L"Options:");
    PrintLine(L"  -Recurse          search for FB2 files in subfolders");
    PrintLine(L"  -NoOverwrite      do not overwrite existing DOCX files");
    PrintLine(L"  -DryRun           show the export plan without creating DOCX files");
    PrintLine(L"  -Flat             with -Recurse, place all DOCX files into one output folder");
    PrintLine(L"  -Quiet            suppress per-file console output");
    PrintLine(L"  -OpenLog          open the CSV log after processing");
    PrintLine(L"  -OpenHtmlReport   open the HTML report after processing (alias: -OpenHtml)");
    PrintLine(L"  -OpenSummary      open the TXT summary after processing");
    PrintLine(L"  -OpenFailedList   open the failed-files list after processing");
    PrintLine(L"  -OpenOkList       open the successful-files list after processing");
    PrintLine(L"  -OpenReports      open all created reports and lists");
    PrintLine(L"  -PrintFailed      print failed FB2 files to the console at the end");
    PrintLine(L"  -ValidateDocx     perform a basic DOCX package check after each successful export");
    PrintLine(L"  -NoInputValidation do not pre-check FB2 XML before export");
    PrintLine(L"  -IsolatedExport   run each FB2 export in a separate worker process");
    PrintLine(L"  -TimeoutSec <N>   with -IsolatedExport, terminate a stuck worker after N seconds; 0 = no timeout");
    PrintLine(L"                   long runs also write ExportDOCX_current_file.txt and ExportDOCX_progress_state.txt next to the CSV log");
    PrintLine(L"  -AppendLog        append to an existing CSV log");
    PrintLine(L"  -Resume           resume processing: implies -NoOverwrite and -AppendLog");
    PrintLine(L"  -SkipIfNewer      skip DOCX files that are newer than or same age as FB2");
    PrintLine(L"  -StopOnError      stop after the first export error");
    PrintLine(L"  -Retries <N>      retry a failed export N times before marking it as FAIL");
    PrintLine(L"  -ProgressEvery <N> print progress every N files, even with -Quiet");
    PrintLine(L"  -PreserveDates    copy the FB2 timestamp to the created DOCX");
    PrintLine(L"  -UniqueNames      generate unique DOCX names to avoid overwriting collisions");
    PrintLine(L"  -Limit <N>        process at most N files");
    PrintLine(L"  -List <path>      add FB2 files from a TXT list of paths/masks; use input '-' for list-only mode");
    PrintLine(L"  -FailedList <path> write a TXT list of FB2 files that ended with FAIL");
    PrintLine(L"  -InputInvalidList <path> write a TXT list of FB2 files that are invalid XML");
    PrintLine(L"  -WarningsFilesList <path> write a TXT list of FB2 files exported with warnings");
    PrintLine(L"  -OkList <path>    write a TXT list of successfully exported FB2 files (alias: -SuccessList)");
    PrintLine(L"  -Summary <path>   write a short TXT batch summary (alias: -Report)");
    PrintLine(L"  -HtmlReport <path> write an HTML batch report (alias: -Html)");
    PrintLine(L"  -MinSize <N>      process only FB2 files not smaller than N; suffixes: KB, MB, GB");
    PrintLine(L"  -MaxSize <N>      process only FB2 files not larger than N; suffixes: KB, MB, GB");
    PrintLine(L"  -ModifiedAfter yyyy-mm-dd   process only FB2 files modified on or after this date (alias: -After)");
    PrintLine(L"  -ModifiedBefore yyyy-mm-dd  process only FB2 files modified on or before this date (alias: -Before)");
    PrintLine(L"  -Dll <path>       use the specified ExportDOCX.dll");
    PrintLine(L"  -Log <path>       write the CSV log to the specified file");
    PrintLine(L"");
    PrintLine(L"By default, the DLL is searched next to the EXE, in the current directory, and in out\\Release.");
}

static int NonNegativeInt(const std::wstring& value)
{
    int v = _wtoi(value.c_str());
    return v < 0 ? 0 : v;
}

static bool ReadValueArg(int argc, wchar_t** argv, int& i, const std::wstring& a, const wchar_t* prefixEq, std::wstring& value)
{
    size_t prefixLen = std::wcslen(prefixEq);
    if (a.size() > prefixLen && StartsWithI(a, prefixEq)) {
        value = a.substr(prefixLen);
        return true;
    }
    if (i + 1 >= argc) return false;
    value = argv[++i];
    return !value.empty();
}

static bool ParseArgs(int argc, wchar_t** argv, Options& opt)
{
    if (argc < 3) return false;
    opt.input = argv[1];
    opt.output = argv[2];

    for (int i = 3; i < argc; ++i) {
        std::wstring a = argv[i];
        if (IEquals(a, L"-Recurse") || IEquals(a, L"/Recurse") || IEquals(a, L"-R") || IEquals(a, L"/R")) {
            opt.recurse = true;
        } else if (IEquals(a, L"-NoOverwrite") || IEquals(a, L"/NoOverwrite")) {
            opt.noOverwrite = true;
        } else if (IEquals(a, L"-DryRun") || IEquals(a, L"/DryRun") || IEquals(a, L"-WhatIf") || IEquals(a, L"/WhatIf")) {
            opt.dryRun = true;
        } else if (IEquals(a, L"-Flat") || IEquals(a, L"/Flat")) {
            opt.flat = true;
        } else if (IEquals(a, L"-Quiet") || IEquals(a, L"/Quiet") || IEquals(a, L"-Q") || IEquals(a, L"/Q")) {
            opt.quiet = true;
        } else if (IEquals(a, L"-OpenLog") || IEquals(a, L"/OpenLog")) {
            opt.openLog = true;
        } else if (IEquals(a, L"-OpenHtmlReport") || IEquals(a, L"/OpenHtmlReport") || IEquals(a, L"-OpenHtml") || IEquals(a, L"/OpenHtml")) {
            opt.openHtmlReport = true;
        } else if (IEquals(a, L"-OpenSummary") || IEquals(a, L"/OpenSummary")) {
            opt.openSummary = true;
        } else if (IEquals(a, L"-OpenFailedList") || IEquals(a, L"/OpenFailedList")) {
            opt.openFailedList = true;
        } else if (IEquals(a, L"-OpenOkList") || IEquals(a, L"/OpenOkList")) {
            opt.openOkList = true;
        } else if (IEquals(a, L"-OpenReports") || IEquals(a, L"/OpenReports")) {
            opt.openLog = true;
            opt.openHtmlReport = true;
            opt.openSummary = true;
            opt.openFailedList = true;
            opt.openOkList = true;
        } else if (IEquals(a, L"-PrintFailed") || IEquals(a, L"/PrintFailed")) {
            opt.printFailed = true;
        } else if (IEquals(a, L"-ValidateDocx") || IEquals(a, L"/ValidateDocx")) {
            opt.validateDocx = true;
        } else if (IEquals(a, L"-NoInputValidation") || IEquals(a, L"/NoInputValidation") ||
                   IEquals(a, L"-SkipInputValidation") || IEquals(a, L"/SkipInputValidation")) {
            opt.validateInputXml = false;
        } else if (IEquals(a, L"-IsolatedExport") || IEquals(a, L"/IsolatedExport") ||
                   IEquals(a, L"-Isolate") || IEquals(a, L"/Isolate")) {
            opt.isolatedExport = true;
        } else if (IEquals(a, L"-WorkerExport") || IEquals(a, L"/WorkerExport")) {
            opt.workerExport = true;
            opt.quiet = true;
        } else if (IEquals(a, L"-TimeoutSec") || IEquals(a, L"/TimeoutSec") ||
                   IEquals(a, L"-Timeout") || IEquals(a, L"/Timeout")) {
            std::wstring value;
            if (!ReadValueArg(argc, argv, i, a, L"-TimeoutSec=", value)) return false;
            opt.timeoutSec = NonNegativeInt(value);
        } else if (StartsWithI(a, L"-TimeoutSec=") || StartsWithI(a, L"/TimeoutSec:") ||
                   StartsWithI(a, L"-Timeout=") || StartsWithI(a, L"/Timeout:")) {
            size_t p = a.find_first_of(L"=:");
            opt.timeoutSec = (p == std::wstring::npos) ? 0 : NonNegativeInt(a.substr(p + 1));
        } else if (IEquals(a, L"-AppendLog") || IEquals(a, L"/AppendLog")) {
            opt.appendLog = true;
        } else if (IEquals(a, L"-Resume") || IEquals(a, L"/Resume")) {
            opt.noOverwrite = true;
            opt.appendLog = true;
        } else if (IEquals(a, L"-SkipIfNewer") || IEquals(a, L"/SkipIfNewer")) {
            opt.skipIfNewer = true;
        } else if (IEquals(a, L"-StopOnError") || IEquals(a, L"/StopOnError")) {
            opt.stopOnError = true;
        } else if (IEquals(a, L"-PreserveDates") || IEquals(a, L"/PreserveDates")) {
            opt.preserveDates = true;
        } else if (IEquals(a, L"-UniqueNames") || IEquals(a, L"/UniqueNames")) {
            opt.uniqueNames = true;
        } else if (IEquals(a, L"-Retries") || IEquals(a, L"/Retries") || IEquals(a, L"-Retry") || IEquals(a, L"/Retry")) {
            std::wstring value;
            if (!ReadValueArg(argc, argv, i, a, L"-Retries=", value)) return false;
            opt.retries = NonNegativeInt(value);
        } else if (StartsWithI(a, L"-Retries=") || StartsWithI(a, L"/Retries:") || StartsWithI(a, L"-Retry=") || StartsWithI(a, L"/Retry:")) {
            size_t p = a.find_first_of(L"=:");
            opt.retries = (p == std::wstring::npos) ? 0 : NonNegativeInt(a.substr(p + 1));
        } else if (IEquals(a, L"-ProgressEvery") || IEquals(a, L"/ProgressEvery")) {
            std::wstring value;
            if (!ReadValueArg(argc, argv, i, a, L"-ProgressEvery=", value)) return false;
            opt.progressEvery = NonNegativeInt(value);
        } else if (StartsWithI(a, L"-ProgressEvery=") || StartsWithI(a, L"/ProgressEvery:")) {
            size_t p = a.find_first_of(L"=:");
            opt.progressEvery = (p == std::wstring::npos) ? 0 : NonNegativeInt(a.substr(p + 1));
        } else if (IEquals(a, L"-Limit") || IEquals(a, L"/Limit")) {
            std::wstring value;
            if (!ReadValueArg(argc, argv, i, a, L"-Limit=", value)) return false;
            opt.limit = NonNegativeInt(value);
        } else if (StartsWithI(a, L"-Limit=") || StartsWithI(a, L"/Limit:")) {
            size_t p = a.find_first_of(L"=:");
            opt.limit = (p == std::wstring::npos) ? 0 : NonNegativeInt(a.substr(p + 1));
        } else if (IEquals(a, L"-List") || IEquals(a, L"/List")) {
            if (!ReadValueArg(argc, argv, i, a, L"-List=", opt.inputListPath)) return false;
        } else if (StartsWithI(a, L"-List=") || StartsWithI(a, L"/List:")) {
            size_t p = a.find_first_of(L"=:");
            opt.inputListPath = (p == std::wstring::npos) ? std::wstring() : a.substr(p + 1);
            if (opt.inputListPath.empty()) return false;
        } else if (IEquals(a, L"-FailedList") || IEquals(a, L"/FailedList")) {
            if (!ReadValueArg(argc, argv, i, a, L"-FailedList=", opt.failedListPath)) return false;
        } else if (StartsWithI(a, L"-FailedList=") || StartsWithI(a, L"/FailedList:")) {
            size_t p = a.find_first_of(L"=:");
            opt.failedListPath = (p == std::wstring::npos) ? std::wstring() : a.substr(p + 1);
            if (opt.failedListPath.empty()) return false;
        } else if (IEquals(a, L"-InputInvalidList") || IEquals(a, L"/InputInvalidList") ||
                   IEquals(a, L"-InvalidInputList") || IEquals(a, L"/InvalidInputList")) {
            if (!ReadValueArg(argc, argv, i, a, L"-InputInvalidList=", opt.inputInvalidListPath)) return false;
        } else if (StartsWithI(a, L"-InputInvalidList=") || StartsWithI(a, L"/InputInvalidList:") ||
                   StartsWithI(a, L"-InvalidInputList=") || StartsWithI(a, L"/InvalidInputList:")) {
            size_t p = a.find_first_of(L"=:");
            opt.inputInvalidListPath = (p == std::wstring::npos) ? std::wstring() : a.substr(p + 1);
            if (opt.inputInvalidListPath.empty()) return false;
        } else if (IEquals(a, L"-WarningsFilesList") || IEquals(a, L"/WarningsFilesList") ||
                   IEquals(a, L"-WarningsList") || IEquals(a, L"/WarningsList")) {
            if (!ReadValueArg(argc, argv, i, a, L"-WarningsFilesList=", opt.warningsFilesListPath)) return false;
        } else if (StartsWithI(a, L"-WarningsFilesList=") || StartsWithI(a, L"/WarningsFilesList:") ||
                   StartsWithI(a, L"-WarningsList=") || StartsWithI(a, L"/WarningsList:")) {
            size_t p = a.find_first_of(L"=:");
            opt.warningsFilesListPath = (p == std::wstring::npos) ? std::wstring() : a.substr(p + 1);
            if (opt.warningsFilesListPath.empty()) return false;
        } else if (IEquals(a, L"-OkList") || IEquals(a, L"/OkList") || IEquals(a, L"-SuccessList") || IEquals(a, L"/SuccessList")) {
            if (!ReadValueArg(argc, argv, i, a, L"-OkList=", opt.okListPath)) return false;
        } else if (StartsWithI(a, L"-OkList=") || StartsWithI(a, L"/OkList:") || StartsWithI(a, L"-SuccessList=") || StartsWithI(a, L"/SuccessList:")) {
            size_t p = a.find_first_of(L"=:");
            opt.okListPath = (p == std::wstring::npos) ? std::wstring() : a.substr(p + 1);
            if (opt.okListPath.empty()) return false;
        } else if (IEquals(a, L"-Summary") || IEquals(a, L"/Summary") || IEquals(a, L"-Report") || IEquals(a, L"/Report")) {
            if (!ReadValueArg(argc, argv, i, a, L"-Summary=", opt.summaryPath)) return false;
        } else if (StartsWithI(a, L"-Summary=") || StartsWithI(a, L"/Summary:") || StartsWithI(a, L"-Report=") || StartsWithI(a, L"/Report:")) {
            size_t p = a.find_first_of(L"=:");
            opt.summaryPath = (p == std::wstring::npos) ? std::wstring() : a.substr(p + 1);
            if (opt.summaryPath.empty()) return false;
        } else if (IEquals(a, L"-HtmlReport") || IEquals(a, L"/HtmlReport") || IEquals(a, L"-Html") || IEquals(a, L"/Html")) {
            if (!ReadValueArg(argc, argv, i, a, L"-HtmlReport=", opt.htmlReportPath)) return false;
        } else if (StartsWithI(a, L"-HtmlReport=") || StartsWithI(a, L"/HtmlReport:") || StartsWithI(a, L"-Html=") || StartsWithI(a, L"/Html:")) {
            size_t p = a.find_first_of(L"=:");
            opt.htmlReportPath = (p == std::wstring::npos) ? std::wstring() : a.substr(p + 1);
            if (opt.htmlReportPath.empty()) return false;
        } else if (IEquals(a, L"-MinSize") || IEquals(a, L"/MinSize")) {
            std::wstring value;
            if (!ReadValueArg(argc, argv, i, a, L"-MinSize=", value)) return false;
            if (!ParseSizeValue(value, opt.minSizeBytes)) return false;
            opt.hasMinSize = true;
        } else if (StartsWithI(a, L"-MinSize=") || StartsWithI(a, L"/MinSize:")) {
            size_t p = a.find_first_of(L"=:");
            if (p == std::wstring::npos || !ParseSizeValue(a.substr(p + 1), opt.minSizeBytes)) return false;
            opt.hasMinSize = true;
        } else if (IEquals(a, L"-MaxSize") || IEquals(a, L"/MaxSize")) {
            std::wstring value;
            if (!ReadValueArg(argc, argv, i, a, L"-MaxSize=", value)) return false;
            if (!ParseSizeValue(value, opt.maxSizeBytes)) return false;
            opt.hasMaxSize = true;
        } else if (StartsWithI(a, L"-MaxSize=") || StartsWithI(a, L"/MaxSize:")) {
            size_t p = a.find_first_of(L"=:");
            if (p == std::wstring::npos || !ParseSizeValue(a.substr(p + 1), opt.maxSizeBytes)) return false;
            opt.hasMaxSize = true;
        } else if (IEquals(a, L"-ModifiedAfter") || IEquals(a, L"/ModifiedAfter") || IEquals(a, L"-After") || IEquals(a, L"/After")) {
            std::wstring value;
            if (!ReadValueArg(argc, argv, i, a, L"-ModifiedAfter=", value)) return false;
            if (!ParseLocalDateToFileTime(value, false, opt.modifiedAfter)) return false;
            opt.hasModifiedAfter = true;
        } else if (StartsWithI(a, L"-ModifiedAfter=") || StartsWithI(a, L"/ModifiedAfter:") || StartsWithI(a, L"-After=") || StartsWithI(a, L"/After:")) {
            size_t p = a.find_first_of(L"=:");
            if (p == std::wstring::npos || !ParseLocalDateToFileTime(a.substr(p + 1), false, opt.modifiedAfter)) return false;
            opt.hasModifiedAfter = true;
        } else if (IEquals(a, L"-ModifiedBefore") || IEquals(a, L"/ModifiedBefore") || IEquals(a, L"-Before") || IEquals(a, L"/Before")) {
            std::wstring value;
            if (!ReadValueArg(argc, argv, i, a, L"-ModifiedBefore=", value)) return false;
            if (!ParseLocalDateToFileTime(value, true, opt.modifiedBefore)) return false;
            opt.hasModifiedBefore = true;
        } else if (StartsWithI(a, L"-ModifiedBefore=") || StartsWithI(a, L"/ModifiedBefore:") || StartsWithI(a, L"-Before=") || StartsWithI(a, L"/Before:")) {
            size_t p = a.find_first_of(L"=:");
            if (p == std::wstring::npos || !ParseLocalDateToFileTime(a.substr(p + 1), true, opt.modifiedBefore)) return false;
            opt.hasModifiedBefore = true;
        } else if (IEquals(a, L"-Dll") || IEquals(a, L"/Dll")) {
            if (!ReadValueArg(argc, argv, i, a, L"-Dll=", opt.dllPath)) return false;
        } else if (StartsWithI(a, L"-Dll=") || StartsWithI(a, L"/Dll:")) {
            size_t p = a.find_first_of(L"=:");
            opt.dllPath = (p == std::wstring::npos) ? std::wstring() : a.substr(p + 1);
            if (opt.dllPath.empty()) return false;
        } else if (IEquals(a, L"-Log") || IEquals(a, L"/Log")) {
            if (!ReadValueArg(argc, argv, i, a, L"-Log=", opt.logPath)) return false;
        } else if (StartsWithI(a, L"-Log=") || StartsWithI(a, L"/Log:")) {
            size_t p = a.find_first_of(L"=:");
            opt.logPath = (p == std::wstring::npos) ? std::wstring() : a.substr(p + 1);
            if (opt.logPath.empty()) return false;
        } else if (IsHelpOption(a)) {
            return false;
        } else {
            std::wstring msg = L"Unknown option: " + a;
            PrintLine(msg);
            return false;
        }
    }
    return true;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    InitConsole();

    for (int i = 1; i < argc; ++i) {
        if (IsHelpOption(argv[i])) {
            Usage();
            return 0;
        }
    }

    Options opt;
    if (!ParseArgs(argc, argv, opt)) {
        Usage();
        return 1;
    }

    if (opt.workerExport) {
        return RunWorkerExportOnce(opt);
    }

    bool inputPlaceholder = (opt.input == L"-");
    bool inputWildcard = !inputPlaceholder && ContainsWildcard(opt.input);
    DWORD inputAttr = inputPlaceholder ? FILE_ATTRIBUTE_DIRECTORY : (inputWildcard ? FILE_ATTRIBUTE_DIRECTORY : ::GetFileAttributesW(opt.input.c_str()));
    if (inputPlaceholder && opt.inputListPath.empty()) {
        PrintLine(L"Input '-' is allowed only together with -List <path>.");
        return 1;
    }
    if (!inputPlaceholder && !inputWildcard && inputAttr == INVALID_FILE_ATTRIBUTES) {
        PrintLine(L"Input file or folder was not found.");
        return 1;
    }

    bool inputIsDir = inputPlaceholder || inputWildcard || ((inputAttr & FILE_ATTRIBUTE_DIRECTORY) != 0);
    bool inputIsSingleFile = !inputIsDir && opt.inputListPath.empty();
    if (!inputPlaceholder && !inputIsDir && !HasFb2Extension(opt.input)) {
        PrintLine(L"The input file must have the .fb2 extension.");
        return 1;
    }

    if (inputIsSingleFile && HasDocxExtension(opt.output)) {
        std::wstring outDir = DirectoryOf(opt.output);
        if (!EnsureDirectory(outDir)) {
            PrintLine(L"Could not create the output folder.");
            return 1;
        }
    } else if (!EnsureDirectory(opt.output)) {
        PrintLine(L"Could not create the output folder.");
        return 1;
    }

    std::vector<std::wstring> files;
    if (!inputPlaceholder) {
        files = FindFb2Files(opt.input, opt.recurse);
    }
    if (!opt.inputListPath.empty()) {
        if (!FileExists(opt.inputListPath)) {
            PrintLine(L"TXT list was not found: " + opt.inputListPath);
            return 1;
        }
        AppendUnique(files, ExpandInputList(opt.inputListPath, opt.recurse));
    }
    if (files.empty()) {
        PrintLine(L"No FB2 files found.");
        return 1;
    }

    std::wstring root = inputPlaceholder ? DirectoryOf(opt.inputListPath) : GetInputRoot(opt.input);
    std::wstring logPath = DefaultLogPath(opt, true);
    std::wstring logDir = DirectoryOf(logPath);
    if (!EnsureDirectory(logDir)) {
        PrintLine(L"Could not create the folder for the CSV log.");
        return 1;
    }
    if (!opt.appendLog || !FileExists(logPath) || FileSizeBytes(logPath) == 0) {
        WriteUtf8File(logPath, "Status;Input;Output;HRESULT;ElapsedMs;Message;Validation\r\n");
    }

    std::vector<ResultRow> rows;
    int filtered = 0;
    if (opt.hasMinSize || opt.hasMaxSize || opt.hasModifiedAfter || opt.hasModifiedBefore) {
        std::vector<std::wstring> kept;
        for (const auto& file : files) {
            std::wstring reason;
            if (FileMatchesFilters(opt, file, reason)) {
                kept.push_back(file);
            } else {
                ++filtered;
                std::wstring outFile = MakeOutputPath(opt, root, file, inputIsSingleFile);
                std::wstring line = L"FILTERED;" + CsvEscape(file) + L";" + CsvEscape(outFile) + L";0x00000000;0;" + CsvEscape(reason) + L";\"\"\r\n";
                AppendUtf8File(logPath, ToUtf8(line));
                rows.push_back(ResultRow{L"FILTERED", file, outFile, L"0x00000000", 0, reason});
                PrintLineIf(!opt.quiet, L"[FILTERED] " + file + L" - " + reason);
            }
        }
        files.swap(kept);
    }

    if (opt.limit > 0 && static_cast<int>(files.size()) > opt.limit) {
        files.resize(static_cast<size_t>(opt.limit));
    }

    HMODULE dll = NULL;
    ExportFunc exportFunc = NULL;
    std::wstring dllPath;

    if (!opt.dryRun && !files.empty()) {
        dllPath = FindExportDll(opt.dllPath);
        if (dllPath.empty()) {
            if (!opt.dllPath.empty()) {
                PrintLine(L"The specified ExportDOCX.dll was not found: " + opt.dllPath);
            } else {
                PrintLine(L"ExportDOCX.dll was not found. Build Release|Win32 first, or place the DLL next to ExportDOCXBatch.exe.");
            }
            return 1;
        }

        if (!opt.isolatedExport) {
            dll = ::LoadLibraryW(dllPath.c_str());
            if (!dll) {
                DWORD err = ::GetLastError();
                PrintLine(L"Could not load ExportDOCX.dll. Make sure the EXE and DLL have the same architecture: Win32.");
                PrintLine(FormatWin32Error(err));
                return 1;
            }

            exportFunc = reinterpret_cast<ExportFunc>(::GetProcAddress(dll, "ExportFB2FileToDOCX"));
            if (!exportFunc) {
                DWORD err = ::GetLastError();
                PrintLine(L"ExportDOCX.dll does not export the ExportFB2FileToDOCX function.");
                PrintLine(FormatWin32Error(err));
                ::FreeLibrary(dll);
                return 1;
            }
        }

        PrintLineIf(!opt.quiet, L"DLL: " + dllPath);
        if (opt.isolatedExport) {
            PrintLineIf(!opt.quiet, L"Isolated export: enabled; timeout=" + std::to_wstring(opt.timeoutSec) + L" sec");
        }
    } else if (opt.dryRun) {
        PrintLineIf(!opt.quiet, L"Dry-run: export is not performed; DOCX files are not created.");
    } else {
        PrintLineIf(!opt.quiet, L"After applying filters, no files remain for export.");
    }

    int total = 0;
    int ok = 0;
    int fail = 0;
    int timeouts = 0;
    int okWarnings = 0;
    int docxInvalid = 0;
    int inputInvalid = 0;
    int expectedFail = 0;
    int unexpectedFail = 0;
    int skipped = 0;
    int upToDate = 0;
    int planned = 0;
    std::vector<std::wstring> usedOutputPaths;
    std::wstring failedListText;
    std::wstring inputInvalidListText;
    std::wstring okListText;
    std::wstring warningsSummaryText;
    std::wstring warningsFilesText;
    std::wstring warningsSummaryPath = JoinPath(DirectoryOf(logPath), L"warnings_summary.txt");
    std::wstring warningsFilesPath = opt.warningsFilesListPath.empty() ? JoinPath(DirectoryOf(logPath), L"warnings_files.txt") : opt.warningsFilesListPath;
    if (opt.inputInvalidListPath.empty()) opt.inputInvalidListPath = JoinPath(DirectoryOf(logPath), L"input_invalid.txt");
    // Files below are intentionally updated for every processed input file.
    // They make long unattended runs easier to diagnose after a power loss,
    // a forced termination, or an unexpected process crash.
    std::wstring currentFilePath = JoinPath(DirectoryOf(logPath), L"ExportDOCX_current_file.txt");
    std::wstring progressStatePath = JoinPath(DirectoryOf(logPath), L"ExportDOCX_progress_state.txt");

    for (const auto& file : files) {
        ++total;
        WriteUtf8File(currentFilePath, ToUtf8(L"Started: " + FormatLocalTimestamp() + L"\r\nIndex: " + std::to_wstring(total) + L"/" + std::to_wstring(files.size()) + L"\r\nInput: " + file + L"\r\n"));
        WriteUtf8File(progressStatePath, ToUtf8(L"Running\r\nStarted: " + FormatLocalTimestamp() + L"\r\nProcessedBeforeCurrent: " + std::to_wstring(total - 1) + L"\r\nTotalQueued: " + std::to_wstring(files.size()) + L"\r\nCurrent: " + file + L"\r\n"));
        std::wstring outFile = MakeOutputPath(opt, root, file, inputIsSingleFile);
        if (opt.uniqueNames) {
            outFile = MakeUniqueOutputPath(outFile, usedOutputPaths);
        }
        usedOutputPaths.push_back(outFile);
        EnsureDirectory(DirectoryOf(outFile));

        if (opt.noOverwrite && FileExists(outFile)) {
            ++skipped;
            std::wstring line = L"SKIP;" + CsvEscape(file) + L";" + CsvEscape(outFile) + L";0x00000000;0;Already exists;\"\"\r\n";
            AppendUtf8File(logPath, ToUtf8(line));
            rows.push_back(ResultRow{L"SKIP", file, outFile, L"0x00000000", 0, L"Already exists"});
            PrintLineIf(!opt.quiet, L"[SKIP] " + file);
            continue;
        }

        if (opt.skipIfNewer && FileExists(outFile) && OutputIsNewerOrSame(file, outFile)) {
            ++upToDate;
            std::wstring line = L"UPTODATE;" + CsvEscape(file) + L";" + CsvEscape(outFile) + L";0x00000000;0;Output is newer or same age;\"\"\r\n";
            AppendUtf8File(logPath, ToUtf8(line));
            rows.push_back(ResultRow{L"UPTODATE", file, outFile, L"0x00000000", 0, L"Output is newer or same age"});
            PrintLineIf(!opt.quiet, L"[UPTODATE] " + file);
            continue;
        }

        if (opt.dryRun) {
            ++planned;
            std::wstring line = L"DRYRUN;" + CsvEscape(file) + L";" + CsvEscape(outFile) + L";0x00000000;0;Planned;\"\"\r\n";
            AppendUtf8File(logPath, ToUtf8(line));
            rows.push_back(ResultRow{L"DRYRUN", file, outFile, L"0x00000000", 0, L"Planned"});
            PrintLineIf(!opt.quiet, L"[DRYRUN] " + file + L" -> " + outFile);
            continue;
        }

        if (opt.validateInputXml) {
            std::wstring inputValidationMsg;
            HRESULT inputValidationHr = S_OK;
            if (!ValidateInputFb2Xml(file, inputValidationMsg, inputValidationHr)) {
                ++inputInvalid;
                inputInvalidListText += file + L"\r\n";
                std::wstring hrText = HResultHex(inputValidationHr);
                std::wstring line = L"INPUT_INVALID;" + CsvEscape(file) + L";" + CsvEscape(outFile) + L";" + hrText + L";0;" + CsvEscape(inputValidationMsg) + L";\"\"\r\n";
                AppendUtf8File(logPath, ToUtf8(line));
                rows.push_back(ResultRow{L"INPUT_INVALID", file, outFile, hrText, 0, inputValidationMsg});
                warningsSummaryText += L"[" + file + L"]\r\nINPUT_INVALID: " + inputValidationMsg + L"\r\n\r\n";
                PrintLineIf(!opt.quiet, L"[INPUT_INVALID] " + file + L" - " + inputValidationMsg);
                continue;
            }
        }

        PrintLineIf(!opt.quiet, L"[" + std::to_wstring(total) + L"/" + std::to_wstring(files.size()) + L"] " + file + L" -> " + outFile);
        ULONGLONG elapsed = 0;
        HRESULT hr = E_FAIL;
        DWORD lastExceptionCode = 0;
        int attemptsUsed = 0;
        int maxAttempts = 1 + opt.retries;
        bool lastTimedOut = false;
        for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
            attemptsUsed = attempt;
            ULONGLONG t0 = ::GetTickCount64();
            if (opt.isolatedExport) {
                bool timedOut = false;
                DWORD workerExitCode = 0;
                hr = RunExportInWorkerProcess(file, outFile, dllPath, opt.timeoutSec, timedOut, workerExitCode);
                elapsed += (::GetTickCount64() - t0);
                if (timedOut) {
                    lastTimedOut = true;
                    PrintLineIf(!opt.quiet, L"  Timeout: worker process exceeded " + std::to_wstring(opt.timeoutSec) + L" sec");
                    break;
                }
                if (IsStructuredExceptionCode(workerExitCode)) {
                    lastExceptionCode = workerExitCode;
                    PrintLineIf(!opt.quiet, L"  Structured exception in worker: " + HResultHex(static_cast<HRESULT>(workerExitCode)));
                    break;
                }
            } else {
                DWORD exceptionCode = 0;
                hr = CallExportFunctionSafely(exportFunc, file.c_str(), outFile.c_str(), exceptionCode);
                elapsed += (::GetTickCount64() - t0);
                if (exceptionCode != 0) {
                    lastExceptionCode = exceptionCode;
                    PrintLineIf(!opt.quiet, L"  Structured exception: " + HResultHex(static_cast<HRESULT>(exceptionCode)));
                    break;
                }
            }
            if (SUCCEEDED(hr)) break;
            if (attempt < maxAttempts) {
                PrintLineIf(!opt.quiet, L"  Retry " + std::to_wstring(attempt) + L"/" + std::to_wstring(opt.retries) + L" after error " + HResultHex(hr));
            }
        }
        std::wstring hrHex = HResultHex(hr);

        if (SUCCEEDED(hr)) {
            std::wstring status = L"OK";
            std::wstring msg;
            std::wstring validationMsg;
            bool validPackage = true;
            if (attemptsUsed > 1) msg += L"Succeeded after " + std::to_wstring(attemptsUsed) + L" attempts";
            if (opt.preserveDates) {
                bool preserved = CopyFileTimes(file, outFile);
                if (!msg.empty()) msg += L"; ";
                msg += preserved ? L"Preserved file timestamp" : L"Could not preserve file timestamp";
            }
            if (opt.validateDocx) {
                validPackage = ValidateDocxPackageBasic(outFile, validationMsg);
                if (!validPackage) status = L"DOCX_INVALID";
            }
            std::wstring warnMsg;
            std::wstring warnFull;
            if (validPackage && ExportReportHasWarnings(outFile, warnMsg)) {
                status = L"OK_WITH_WARNINGS";
                if (!validationMsg.empty()) validationMsg += L"; ";
                validationMsg += warnMsg;
                if (ExportReportWarningsText(outFile, warnFull))
                    warningsSummaryText += L"[" + file + L"]\r\n" + warnFull + L"\r\n\r\n";
                warningsFilesText += file + L"\r\n";
            }
            if (status == L"DOCX_INVALID") {
                ++docxInvalid;
                ++fail;
                ++unexpectedFail;
                failedListText += file + L"\r\n";
                warningsSummaryText += L"[" + file + L"]\r\nDOCX_INVALID: " + validationMsg + L"\r\n\r\n";
            } else {
                ++ok;
                if (status == L"OK_WITH_WARNINGS") ++okWarnings;
                okListText += file + L"\r\n";
            }
            std::wstring line = status + L";" + CsvEscape(file) + L";" + CsvEscape(outFile) + L";" + hrHex + L";" + std::to_wstring(elapsed) + L";" + CsvEscape(msg) + L";" + CsvEscape(validationMsg) + L"\r\n";
            AppendUtf8File(logPath, ToUtf8(line));
            rows.push_back(ResultRow{status, file, outFile, hrHex, elapsed, msg, validationMsg});
            if (status == L"DOCX_INVALID" && opt.stopOnError) {
                PrintLineIf(!opt.quiet, L"Stopping after DOCX validation error (-StopOnError)." );
                break;
            }
        } else {
            ++fail;
            bool expectedNegativeFail = IsExpectedNegativeTestPath(file);
            if (expectedNegativeFail) ++expectedFail; else ++unexpectedFail;
            failedListText += file + L"\r\n";
            std::wstring status = lastTimedOut ? L"TIMEOUT" : L"FAIL";
            std::wstring msg = expectedNegativeFail ? L"Expected negative test failed" : L"Export function failed";
            if (lastTimedOut) {
                ++timeouts;
                msg = L"Export timed out after " + std::to_wstring(opt.timeoutSec) + L" seconds; worker process was terminated";
            } else if (lastExceptionCode != 0) {
                msg = std::wstring(expectedNegativeFail ? L"Expected negative test raised a structured exception " : L"Export function raised a structured exception ") + HResultHex(static_cast<HRESULT>(lastExceptionCode));
            } else if (attemptsUsed > 1) {
                msg += L" after " + std::to_wstring(attemptsUsed) + L" attempts";
            }
            if (lastTimedOut)
                warningsSummaryText += L"[" + file + L"]\r\nTIMEOUT: " + hrHex + L" - " + msg + L"\r\n\r\n";
            else if (expectedNegativeFail)
                warningsSummaryText += L"[" + file + L"]\r\nExpected FAIL: " + hrHex + L"\r\n\r\n";
            else
                warningsSummaryText += L"[" + file + L"]\r\nUnexpected FAIL: " + hrHex + L" - " + msg + L"\r\n\r\n";
            std::wstring line = status + L";" + CsvEscape(file) + L";" + CsvEscape(outFile) + L";" + hrHex + L";" + std::to_wstring(elapsed) + L";" + CsvEscape(msg) + L";""\r\n";
            AppendUtf8File(logPath, ToUtf8(line));
            rows.push_back(ResultRow{status, file, outFile, hrHex, elapsed, msg});
            PrintLineIf(!opt.quiet, L"  Error: " + hrHex);
            if (opt.stopOnError && !expectedNegativeFail) {
                PrintLineIf(!opt.quiet, L"Stopping after the first unexpected error (-StopOnError).");
                break;
            }
        }

        if (opt.progressEvery > 0 && (total % opt.progressEvery == 0 || total == static_cast<int>(files.size()))) {
            PrintLine(L"Progress: " + std::to_wstring(total) + L"/" + std::to_wstring(files.size()) +
                      L" OK=" + std::to_wstring(ok) + L" OK_WARN=" + std::to_wstring(okWarnings) +
                      L" DOCX_INVALID=" + std::to_wstring(docxInvalid) + L" INPUT_INVALID=" + std::to_wstring(inputInvalid) +
                      L" FAIL=" + std::to_wstring(fail) + L" TIMEOUT=" + std::to_wstring(timeouts) +
                      L" SKIP=" + std::to_wstring(skipped) + L" UPTODATE=" + std::to_wstring(upToDate) +
                      L" FILTERED=" + std::to_wstring(filtered));
        }
    }

    PrintLine(L"");
    if (opt.dryRun) {
        PrintLine(L"Dry-run finished. Total=" + std::to_wstring(total) +
                  L" PLANNED=" + std::to_wstring(planned) +
                  L" SKIP=" + std::to_wstring(skipped) +
                  L" UPTODATE=" + std::to_wstring(upToDate) +
                  L" FILTERED=" + std::to_wstring(filtered));
    } else {
        PrintLine(L"Done. Total=" + std::to_wstring(total) +
                  L" SUCCESS_TOTAL=" + std::to_wstring(ok) +
                  L" OK_CLEAN=" + std::to_wstring(ok - okWarnings) +
                  L" OK_WARN=" + std::to_wstring(okWarnings) +
                  L" DOCX_INVALID=" + std::to_wstring(docxInvalid) +
                  L" INPUT_INVALID=" + std::to_wstring(inputInvalid) +
                  L" FAIL=" + std::to_wstring(fail) +
                  L" TIMEOUT=" + std::to_wstring(timeouts) +
                  L" SKIP=" + std::to_wstring(skipped) +
                  L" UPTODATE=" + std::to_wstring(upToDate) +
                  L" FILTERED=" + std::to_wstring(filtered));
    }
    PrintLine(L"Log: " + logPath);
    const int okClean = ok - okWarnings;
    WriteUtf8File(currentFilePath, ToUtf8(L"Finished: " + FormatLocalTimestamp() + L"\r\nTotal: " + std::to_wstring(total) + L"\r\nSUCCESS_TOTAL: " + std::to_wstring(ok) + L"\r\nOK_CLEAN: " + std::to_wstring(okClean) + L"\r\nOK_WITH_WARNINGS: " + std::to_wstring(okWarnings) + L"\r\nDOCX_INVALID: " + std::to_wstring(docxInvalid) + L"\r\nINPUT_INVALID: " + std::to_wstring(inputInvalid) + L"\r\nFAIL: " + std::to_wstring(fail) + L"\r\nTIMEOUT: " + std::to_wstring(timeouts) + L"\r\nUnexpected FAIL: " + std::to_wstring(unexpectedFail) + L"\r\n"));
    WriteUtf8File(progressStatePath, ToUtf8(L"Finished\r\nFinished: " + FormatLocalTimestamp() + L"\r\nTotal: " + std::to_wstring(total) + L"\r\nSUCCESS_TOTAL: " + std::to_wstring(ok) + L"\r\nOK_CLEAN: " + std::to_wstring(okClean) + L"\r\nOK_WITH_WARNINGS: " + std::to_wstring(okWarnings) + L"\r\nDOCX_INVALID: " + std::to_wstring(docxInvalid) + L"\r\nINPUT_INVALID: " + std::to_wstring(inputInvalid) + L"\r\nFAIL: " + std::to_wstring(fail) + L"\r\nTIMEOUT: " + std::to_wstring(timeouts) + L"\r\nExpected FAIL: " + std::to_wstring(expectedFail) + L"\r\nUnexpected FAIL: " + std::to_wstring(unexpectedFail) + L"\r\n"));

    if (!opt.failedListPath.empty()) {
        EnsureDirectory(DirectoryOf(opt.failedListPath));
        WriteUtf8File(opt.failedListPath, ToUtf8(failedListText));
        PrintLine(L"Failed list: " + opt.failedListPath);
    }

    if (!opt.inputInvalidListPath.empty()) {
        EnsureDirectory(DirectoryOf(opt.inputInvalidListPath));
        WriteUtf8File(opt.inputInvalidListPath, ToUtf8(inputInvalidListText));
        PrintLine(L"Input invalid list: " + opt.inputInvalidListPath);
    }

    if (!opt.okListPath.empty()) {
        EnsureDirectory(DirectoryOf(opt.okListPath));
        WriteUtf8File(opt.okListPath, ToUtf8(okListText));
        PrintLine(L"OK list: " + opt.okListPath);
    }

    if (opt.printFailed && !failedListText.empty()) {
        PrintLine(L"");
        PrintLine(L"Failed files:");
        PrintLine(failedListText);
    }

    EnsureDirectory(DirectoryOf(warningsSummaryPath));
    if (warningsSummaryText.empty()) warningsSummaryText = L"No warnings.\r\n";
    WriteUtf8File(warningsSummaryPath, ToUtf8(warningsSummaryText));
    PrintLine(L"Warnings summary: " + warningsSummaryPath);

    EnsureDirectory(DirectoryOf(warningsFilesPath));
    WriteUtf8File(warningsFilesPath, ToUtf8(warningsFilesText));
    PrintLine(L"Warnings files: " + warningsFilesPath);

    if (!opt.summaryPath.empty()) {
        EnsureDirectory(DirectoryOf(opt.summaryPath));
        std::wstring summary;
        summary += L"ExportDOCXBatch summary\r\n";
        summary += L"Generated: " + FormatLocalTimestamp() + L"\r\n";
        summary += L"Input: " + opt.input + L"\r\n";
        if (!opt.inputListPath.empty()) summary += L"Input list: " + opt.inputListPath + L"\r\n";
        summary += L"Output: " + opt.output + L"\r\n";
        if (!dllPath.empty()) summary += L"DLL: " + dllPath + L"\r\n";
        summary += L"Log: " + logPath + L"\r\n";
        summary += L"Total: " + std::to_wstring(total) + L"\r\n";
        summary += L"SUCCESS_TOTAL: " + std::to_wstring(ok) + L"\r\n";
        summary += L"OK_CLEAN: " + std::to_wstring(okClean) + L"\r\n";
        summary += L"OK_WITH_WARNINGS: " + std::to_wstring(okWarnings) + L"\r\n";
        summary += L"DOCX_INVALID: " + std::to_wstring(docxInvalid) + L"\r\n";
        summary += L"INPUT_INVALID: " + std::to_wstring(inputInvalid) + L"\r\n";
        summary += L"FAIL: " + std::to_wstring(fail) + L"\r\n";
        summary += L"TIMEOUT: " + std::to_wstring(timeouts) + L"\r\n";
        summary += L"Expected FAIL: " + std::to_wstring(expectedFail) + L"\r\n";
        summary += L"Unexpected FAIL: " + std::to_wstring(unexpectedFail) + L"\r\n";
        summary += L"SKIP: " + std::to_wstring(skipped) + L"\r\n";
        summary += L"UPTODATE: " + std::to_wstring(upToDate) + L"\r\n";
        summary += L"FILTERED: " + std::to_wstring(filtered) + L"\r\n";
        summary += L"DRYRUN/PLANNED: " + std::to_wstring(planned) + L"\r\n";
        summary += L"Retries: " + std::to_wstring(opt.retries) + L"\r\n";
        summary += std::wstring(L"PreserveDates: ") + (opt.preserveDates ? L"yes" : L"no") + L"\r\n";
        summary += std::wstring(L"UniqueNames: ") + (opt.uniqueNames ? L"yes" : L"no") + L"\r\n";
        summary += std::wstring(L"ValidateDocx: ") + (opt.validateDocx ? L"yes" : L"no") + L"\r\n";
        summary += std::wstring(L"IsolatedExport: ") + (opt.isolatedExport ? L"yes" : L"no") + L"\r\n";
        summary += L"TimeoutSec: " + std::to_wstring(opt.timeoutSec) + L"\r\n";
        if (opt.hasMinSize) summary += L"MinSizeBytes: " + FormatSizeBytes(opt.minSizeBytes) + L"\r\n";
        if (opt.hasMaxSize) summary += L"MaxSizeBytes: " + FormatSizeBytes(opt.maxSizeBytes) + L"\r\n";
        if (!opt.failedListPath.empty()) summary += L"FailedList: " + opt.failedListPath + L"\r\n";
        if (!opt.inputInvalidListPath.empty()) summary += L"InputInvalidList: " + opt.inputInvalidListPath + L"\r\n";
        if (!opt.okListPath.empty()) summary += L"OkList: " + opt.okListPath + L"\r\n";
        summary += L"WarningsSummary: " + warningsSummaryPath + L"\r\n";
        summary += L"WarningsFiles: " + warningsFilesPath + L"\r\n";
        summary += L"CurrentFileMarker: " + currentFilePath + L"\r\n";
        summary += L"ProgressState: " + progressStatePath + L"\r\n";
        summary += L"ExitCode: " + std::to_wstring(unexpectedFail > 0 ? 2 : 0) + L"\r\n";
        WriteUtf8File(opt.summaryPath, ToUtf8(summary));
        PrintLine(L"Summary: " + opt.summaryPath);
    }

    if (!opt.htmlReportPath.empty()) {
        if (WriteHtmlReport(opt.htmlReportPath, opt, logPath, dllPath, warningsSummaryPath, warningsFilesPath, total, ok, fail, timeouts, skipped, upToDate, planned, filtered, okWarnings, docxInvalid, inputInvalid, expectedFail, unexpectedFail, rows)) {
            PrintLine(L"HTML report: " + opt.htmlReportPath);
        } else {
            PrintLine(L"Could not write HTML report: " + opt.htmlReportPath);
        }
    }

    if (opt.openLog) {
        ::ShellExecuteW(NULL, L"open", logPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
    if (opt.openHtmlReport && !opt.htmlReportPath.empty()) {
        ::ShellExecuteW(NULL, L"open", opt.htmlReportPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
    if (opt.openSummary && !opt.summaryPath.empty()) {
        ::ShellExecuteW(NULL, L"open", opt.summaryPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
    if (opt.openFailedList && !opt.failedListPath.empty()) {
        ::ShellExecuteW(NULL, L"open", opt.failedListPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
    if (opt.openOkList && !opt.okListPath.empty()) {
        ::ShellExecuteW(NULL, L"open", opt.okListPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    if (dll) ::FreeLibrary(dll);
    return unexpectedFail > 0 ? 2 : 0;
}
