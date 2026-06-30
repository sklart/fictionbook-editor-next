#include "StdAfx.h"
#include "resource.h"
#include "ExportEPUB_i.h"
#include "dllmain.h"

STDAPI DllCanUnloadNow(void)
{
    return _AtlModule.DllCanUnloadNow();
}

_Check_return_
// PVS-Studio: COM export signature is fixed by the Windows/ATL API.
//-V835
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID* ppv) //-V835
{
    return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

STDAPI DllRegisterServer(void)
{
    // Register only COM classes and .rgs entries.
    // Do not register the type library here: FBE plugins only need the COM class
    // registration, and registering the generated TLB can fail with 0x80029C4A
    // on machines where dependent/imported type libraries are not registered.
    return _AtlModule.DllRegisterServer(FALSE);
}

STDAPI DllUnregisterServer(void)
{
    return _AtlModule.DllUnregisterServer(FALSE);
}

STDAPI DllInstall(BOOL bInstall, _In_opt_ LPCWSTR pszCmdLine)
{
    HRESULT hr = E_FAIL;
    static const wchar_t szUserSwitch[] = L"user";

    if (pszCmdLine != nullptr) {
        if (_wcsnicmp(pszCmdLine, szUserSwitch, _countof(szUserSwitch)) == 0) {
            ATL::AtlSetPerUserRegistration(true);
        }
    }

    if (bInstall) {
        hr = DllRegisterServer();
        if (FAILED(hr)) {
            DllUnregisterServer();
        }
    } else {
        hr = DllUnregisterServer();
    }
    return hr;
}
