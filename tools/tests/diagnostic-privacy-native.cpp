#include "../../src/common/DiagnosticLog.h"
#include <cstdio>

int wmain()
{
    using namespace FbeDiagnosticPrivacy;
    if(Details(L"PRIVATE_USER; hr=0x80070005; book=PRIVATE_BOOK; path=C:\\private\\book.fb2; <p>private</p>") != L"hr=0x80070005; details=omitted") return 1;
    if(Details(L"count=book; duration=42; base64=QUFBQUFB") != L"duration=42; details=omitted") return 2;
    if(Identifier(L"A; code=INJECTED") || Identifier(L"C:\\private") || Number(L"42 PRIVATE_BOOK")) return 3;
    if(DeploymentContext::CurrentMode()!=DeploymentContext::Mode::Portable) return 4;
    const std::wstring directory=DeploymentContext::DiagnosticsDirectory();
    if(directory!=DeploymentContext::ExecutableDirectory()+L"Data\\Diagnostics\\") return 5;
    FbeDiagnostic::HResult(L"file-dialog",L"FD203",E_ACCESSDENIED,L"PRIVATE_USER C:\\private\\book.fb2");
    if(::GetFileAttributesW((directory+L"fbe-diagnostic.log").c_str())==INVALID_FILE_ATTRIBUTES) return 6;
    const std::wstring path=directory+L"bounded-test.log";
    HANDLE file=::CreateFileW(path.c_str(),GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL); if(file==INVALID_HANDLE_VALUE) return 7;
    LARGE_INTEGER size={}; size.QuadPart=1024*1024;
    if(!::SetFilePointerEx(file,size,NULL,FILE_BEGIN) || !::SetEndOfFile(file)) { ::CloseHandle(file); return 8; }
    ::CloseHandle(file);
    if(FbeDiagnostic::WriteUtf8(path,L"extra")) return 9;
    WIN32_FILE_ATTRIBUTE_DATA data={}; if(!::GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&data) || data.nFileSizeLow!=1024*1024) return 10;
    std::puts("PASS: typed privacy, standalone portable plug-in path, bounded non-truncating fallback");
    return 0;
}
