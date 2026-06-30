#include "stdafx.h"
#include "ImportEPUBModule.h"
#include "ImportEPUBGuids.h"

namespace
{
    CStringW GuidToString(REFGUID guid)
    {
        wchar_t buffer[64] = {};
        ::StringFromGUID2(guid, buffer, _countof(buffer));
        return CStringW(buffer);
    }

    HRESULT SetStringValue(CRegKey& key, LPCWSTR valueName, LPCWSTR value)
    {
        LSTATUS status = key.SetStringValue(valueName, value);
        return status == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(status);
    }

    HRESULT CreateComRegistrationAtPath(const CStringW& classKeyPath, const CStringW& clsid, const CStringW& modulePath)
    {
        CRegKey classKey;
        LSTATUS status = classKey.Create(HKEY_CURRENT_USER, classKeyPath);
        if (status != ERROR_SUCCESS)
            return HRESULT_FROM_WIN32(status);

        HRESULT hr = SetStringValue(classKey, nullptr, L"FBE EPUB Import Plugin");
        if (FAILED(hr))
            return hr;

        CRegKey inprocKey;
        status = inprocKey.Create(classKey, L"InprocServer32");
        if (status != ERROR_SUCCESS)
            return HRESULT_FROM_WIN32(status);

        hr = SetStringValue(inprocKey, nullptr, modulePath);
        if (FAILED(hr))
            return hr;

        hr = SetStringValue(inprocKey, L"ThreadingModel", L"Apartment");
        if (FAILED(hr))
            return hr;

        CRegKey progIdKey;
        status = progIdKey.Create(HKEY_CURRENT_USER, L"Software\\Classes\\FBE.ImportEPUB");
        if (status == ERROR_SUCCESS)
        {
            SetStringValue(progIdKey, nullptr, L"FBE EPUB Import Plugin");
            CRegKey progIdClsid;
            if (progIdClsid.Create(progIdKey, L"CLSID") == ERROR_SUCCESS)
                SetStringValue(progIdClsid, nullptr, clsid);
        }

        return S_OK;
    }

    HRESULT CreateComRegistration(const CStringW& clsid, const CStringW& modulePath)
    {
        // Per-user COM registration. This avoids requiring administrator rights.
        //
        // A COM in-proc DLL must have the same bitness as the host process.
        // Win32 FBE can load only the Win32 plugin registered in the 32-bit COM
        // view; x64 FBE can load only the x64 plugin registered in the 64-bit COM
        // view. Writing the wrong view is the usual reason for 80040154
        // "Class not registered" when the FBE menu item itself is visible.
        HRESULT hr = CreateComRegistrationAtPath(
            L"Software\\Classes\\CLSID\\" + clsid,
            clsid,
            modulePath);
        if (FAILED(hr))
            return hr;

#if !defined(_WIN64)
        // Be explicit for 32-bit clients on 64-bit Windows.
        hr = CreateComRegistrationAtPath(
            L"Software\\Classes\\Wow6432Node\\CLSID\\" + clsid,
            clsid,
            modulePath);
        if (FAILED(hr))
            return hr;
#endif

        return S_OK;
    }

    HRESULT CreateFbePluginRegistration(const CStringW& clsid, const CStringW& modulePath)
    {
        CStringW pluginKeyPath = L"Software\\FBETeam\\FictionBook Editor\\Plugins\\" + clsid;

        CRegKey pluginKey;
        LSTATUS status = pluginKey.Create(HKEY_CURRENT_USER, pluginKeyPath);
        if (status != ERROR_SUCCESS)
            return HRESULT_FROM_WIN32(status);

        HRESULT hr = SetStringValue(pluginKey, L"Type", L"Import");
        if (FAILED(hr))
            return hr;

        hr = SetStringValue(pluginKey, L"Menu", L"\u0438\u0437 &EPUB...");
        if (FAILED(hr))
            return hr;

        CStringW iconValue = modulePath + L",0";
        hr = SetStringValue(pluginKey, L"Icon", iconValue);
        if (FAILED(hr))
            return hr;

        return S_OK;
    }

    void DeleteKeyIfExists(HKEY root, LPCWSTR subKey)
    {
        CRegKey key;
        if (key.Open(root, subKey, KEY_READ | KEY_WRITE) == ERROR_SUCCESS)
        {
            key.Close();
            CRegKey parent;

            CStringW path(subKey);
            int slash = path.ReverseFind(L'\\');
            if (slash >= 0)
            {
                CStringW parentPath = path.Left(slash);
                if (parent.Open(root, parentPath, KEY_READ | KEY_WRITE) == ERROR_SUCCESS)
                {
                    CStringW childName = path.Mid(slash + 1);
                    parent.RecurseDeleteKey(childName);
                }
            }
        }
    }
}

STDAPI RegisterPluginServer()
{
    wchar_t modulePath[MAX_PATH] = {};
    if (!::GetModuleFileNameW(g_hInstance, modulePath, _countof(modulePath)))
        return HRESULT_FROM_WIN32(::GetLastError());

    const CStringW clsid = GuidToString(CLSID_ImportEPUBPlugin);

    HRESULT hr = CreateComRegistration(clsid, modulePath);
    if (FAILED(hr))
        return hr;

    hr = CreateFbePluginRegistration(clsid, modulePath);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

STDAPI UnregisterPluginServer()
{
    const CStringW clsid = GuidToString(CLSID_ImportEPUBPlugin);

    DeleteKeyIfExists(HKEY_CURRENT_USER, CStringW(L"Software\\FBETeam\\FictionBook Editor\\Plugins\\") + clsid);
    DeleteKeyIfExists(HKEY_CURRENT_USER, CStringW(L"Software\\Classes\\CLSID\\") + clsid);
    DeleteKeyIfExists(HKEY_CURRENT_USER, CStringW(L"Software\\Classes\\Wow6432Node\\CLSID\\") + clsid);
    DeleteKeyIfExists(HKEY_CURRENT_USER, L"Software\\Classes\\FBE.ImportEPUB");

    return S_OK;
}
