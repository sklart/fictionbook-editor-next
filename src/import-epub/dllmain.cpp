#include "stdafx.h"
#include "ImportEPUBModule.h"
#include "RuntimeLocalization.h"

CImportEPUBModule _AtlModule;
HINSTANCE g_hInstance = nullptr;

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hInstance = hInstance;
        InitImportEpubRuntimeStrings();
    }

    return _AtlModule.DllMain(dwReason, lpReserved);
}

STDAPI DllCanUnloadNow(void)
{
    return _AtlModule.DllCanUnloadNow();
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

STDAPI DllRegisterServer(void)
{
    return RegisterPluginServer();
}

STDAPI DllUnregisterServer(void)
{
    return UnregisterPluginServer();
}
