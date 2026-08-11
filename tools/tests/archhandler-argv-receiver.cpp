#include <windows.h>
#include <string>

int wmain(int argc, wchar_t* argv[])
{
    wchar_t output[MAX_PATH];
    const DWORD length = GetEnvironmentVariableW(L"ARCHHANDLER_TEST_OUTPUT", output, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return 2;

    HANDLE file = CreateFileW(output, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return 3;
    std::wstring allArguments;
    for (int index = 1; index < argc; ++index)
        allArguments += std::wstring(argv[index]) + L'\n';
    const int byteCount = WideCharToMultiByte(CP_UTF8, 0, allArguments.data(), static_cast<int>(allArguments.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(byteCount, '\0');
    WideCharToMultiByte(CP_UTF8, 0, allArguments.data(), static_cast<int>(allArguments.size()), &utf8[0], byteCount, nullptr, nullptr);
    DWORD written = 0;
    const bool succeeded = WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr) && written == utf8.size();
    CloseHandle(file);
    return succeeded ? 0 : 4;
}
