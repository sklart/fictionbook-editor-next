#pragma once
#include "stdafx.h"

class CImportEPUBModule : public ATL::CAtlDllModuleT<CImportEPUBModule>
{
public:
};

extern CImportEPUBModule _AtlModule;
extern HINSTANCE g_hInstance;

STDAPI RegisterPluginServer();
STDAPI UnregisterPluginServer();
