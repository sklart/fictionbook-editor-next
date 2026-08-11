#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

namespace {

const wchar_t kSettingsRoot[] = L"Software\\FictionBook Editor\\ArchHandler\\";

std::wstring ReadSetting(const std::wstring& archiveType, const wchar_t* name)
{
    const std::wstring key = std::wstring(kSettingsRoot) + archiveType;
    DWORD bytes = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS || !bytes)
        return std::wstring();
    std::vector<wchar_t> value(bytes / sizeof(wchar_t));
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_REG_SZ, nullptr, value.data(), &bytes) != ERROR_SUCCESS)
        return std::wstring();
    return value.data();
}

std::wstring QuoteWindowsArgument(const std::wstring& value)
{
    // This is the inverse of CommandLineToArgvW for one argument.
    std::wstring result(L"\"");
    size_t slashes = 0;
    for (std::wstring::const_iterator it = value.begin(); it != value.end(); ++it) {
        if (*it == L'\\') { ++slashes; continue; }
        if (*it == L'\"') result.append(slashes * 2 + 1, L'\\');
        else result.append(slashes, L'\\');
        result += *it;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    result += L'\"';
    return result;
}

std::wstring ExpandParameters(std::wstring pattern, const std::wstring& archivePath)
{
    const std::wstring quotedPath = QuoteWindowsArgument(archivePath);
    const std::wstring legacyToken = L"\"$1\"";
    size_t position = 0;
    while ((position = pattern.find(legacyToken, position)) != std::wstring::npos) {
        pattern.replace(position, legacyToken.length(), quotedPath);
        position += quotedPath.length();
    }
    position = 0;
    while ((position = pattern.find(L"$1", position)) != std::wstring::npos) {
        pattern.replace(position, 2, quotedPath);
        position += quotedPath.length();
    }
    return pattern;
}

bool IsFB2Archive(const std::wstring& path)
{
    const size_t lastSlash = path.find_last_of(L"\\/");
    const std::wstring name = path.substr(lastSlash == std::wstring::npos ? 0 : lastSlash + 1);
    const size_t lastDot = name.find_last_of(L'.');
    if (lastDot == std::wstring::npos) return false;
    const std::wstring withoutArchiveExtension = name.substr(0, lastDot);
    const size_t priorDot = withoutArchiveExtension.find_last_of(L'.');
    return priorDot != std::wstring::npos && _wcsicmp(withoutArchiveExtension.c_str() + priorDot, L".fb2") == 0;
}

bool IsTestUncPath(const std::wstring& path)
{
    wchar_t value[2] = {};
    return path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\' &&
        GetEnvironmentVariableW(L"ARCHHANDLER_TEST_MODE", value, 2) == 1 && value[0] == L'1';
}

[[noreturn]] void FailLaunch(const std::wstring& program, const std::wstring& archive, DWORD error)
{
    std::wstring message = L"Не удалось открыть архив.\n\nПрограмма: " + program + L"\nАрхив: " + archive + L"\nКод ShellExecute: " + std::to_wstring(error);
    MessageBoxW(nullptr, message.c_str(), L"ArchHandler", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

}

int Run(int argc, wchar_t* argv[])
{
    if (argc != 4 || _wcsicmp(argv[1], L"--type") != 0) {
        MessageBoxW(nullptr, L"Использование: ArchHandler.exe --type rar|zip \"архив\"", L"ArchHandler", MB_OK | MB_ICONERROR);
        return 2;
    }
    std::wstring type(argv[2]);
    for (size_t index = 0; index < type.length(); ++index) type[index] = static_cast<wchar_t>(towlower(type[index]));
    if (type != L"rar" && type != L"zip") {
        MessageBoxW(nullptr, L"Поддерживаются только типы rar и zip.", L"ArchHandler", MB_OK | MB_ICONERROR);
        return 2;
    }
    const std::wstring archive(argv[3]);
    if (GetFileAttributesW(archive.c_str()) == INVALID_FILE_ATTRIBUTES && !IsTestUncPath(archive)) FailLaunch(L"", archive, ERROR_FILE_NOT_FOUND);
    const bool fb2 = IsFB2Archive(archive);
    const std::wstring program = ReadSetting(type, fb2 ? L"FB2Program" : L"ArchiveProgram");
    const std::wstring parameters = ReadSetting(type, fb2 ? L"FB2Parameters" : L"ArchiveParameters");
    if (program.empty() || GetFileAttributesW(program.c_str()) == INVALID_FILE_ATTRIBUTES) FailLaunch(program, archive, ERROR_FILE_NOT_FOUND);
    const INT_PTR launchResult = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", program.c_str(), ExpandParameters(parameters, archive).c_str(), nullptr, SW_SHOWNORMAL));
    if (launchResult <= 32) FailLaunch(program, archive, static_cast<DWORD>(launchResult));
    return 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return 1;
    const int result = Run(argc, argv);
    LocalFree(argv);
    return result;
}
