#include <windows.h>
#include "../../src/common/PluginSettingsStore.h"

int wmain(int argc, wchar_t** argv)
{
    if(argc != 3 || DeploymentContext::CurrentMode() != DeploymentContext::Mode::Installed) return 1;
    FbePluginSettings::Store writer(L"isolation-probe", argv[2]);
    if(writer.SetDWORDValue(L"value", 12345) != ERROR_SUCCESS || FAILED(writer.Commit())) return 2;
    FbePluginSettings::Store reader(L"isolation-probe", argv[2]);
    DWORD value = 0;
    return reader.QueryDWORDValue(L"value", value) == ERROR_SUCCESS && value == 12345 ? 0 : 3;
}
