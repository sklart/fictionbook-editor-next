#include "stdafx.h"
#include "resource.h"
#include "ExportDOCX_i.h"
#include "dllmain.h"
#include "utils.h"

CExportDOCXModule _AtlModule;
HINSTANCE g_hInstance = NULL;

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	hInstance;
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		g_hInstance = hInstance;
		DisableThreadLibraryCalls(hInstance);
		U::InitSettings();
	}
	return _AtlModule.DllMain(dwReason, lpReserved);
}
