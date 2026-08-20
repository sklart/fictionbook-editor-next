#include <windows.h>
#include <dbghelp.h>
#include <iostream>

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2) return 10;
    HANDLE file = ::CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 11;
    HANDLE mapping = ::CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (mapping == NULL) { ::CloseHandle(file); return 12; }
    void* view = ::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (view == NULL) { ::CloseHandle(mapping); ::CloseHandle(file); return 13; }

    MINIDUMP_EXCEPTION_STREAM* exceptionStream = NULL; ULONG exceptionSize = 0;
    MINIDUMP_MODULE_LIST* moduleList = NULL; ULONG modulesSize = 0;
    if (!::MiniDumpReadDumpStream(view, ExceptionStream, NULL, reinterpret_cast<void**>(&exceptionStream), &exceptionSize) ||
        !::MiniDumpReadDumpStream(view, ModuleListStream, NULL, reinterpret_cast<void**>(&moduleList), &modulesSize)) return 14;
    const ULONG64 address = exceptionStream->ExceptionRecord.ExceptionAddress;
    std::wcout << L"exception=0x" << std::hex << address << L"\n";
    for (ULONG index = 0; index < moduleList->NumberOfModules; ++index) {
        const MINIDUMP_MODULE& module = moduleList->Modules[index];
        if (address < module.BaseOfImage || address >= module.BaseOfImage + module.SizeOfImage) continue;
        MINIDUMP_STRING* name = reinterpret_cast<MINIDUMP_STRING*>(reinterpret_cast<BYTE*>(view) + module.ModuleNameRva);
        std::wcout << L"module=" << std::wstring(name->Buffer, name->Length / sizeof(wchar_t)) << L"\n";
        std::wcout << L"base=0x" << std::hex << module.BaseOfImage << L" offset=0x" << (address - module.BaseOfImage) << L"\n";
        return 0;
    }
    return 15;
}
