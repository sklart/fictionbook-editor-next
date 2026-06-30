#pragma once

class CExportEPUBModule : public ATL::CAtlDllModuleT<CExportEPUBModule>
{
public:
    DECLARE_LIBID(LIBID_ExportEPUBLib)
};

extern class CExportEPUBModule _AtlModule;
