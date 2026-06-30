#include "StdAfx.h"
#include "resource.h"
#include "ExportEPUB_i.h"
#include "dllmain.h"

CExportEPUBModule _AtlModule;

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInstance);
    }
    return _AtlModule.DllMain(dwReason, lpReserved);
}
