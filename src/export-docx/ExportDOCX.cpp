// PVS-Studio: DllGetClassObject must keep COM ABI REFCLSID/REFIID parameters.
//-V::835
#include "stdafx.h"
#include "resource.h"
#include "ExportDOCX_i.h"
#include "dllmain.h"

#include "utils.h"

// Modern ATL (VS 2022) cannot have both CComModule _Module and CAtlDllModuleT _AtlModule.
// The old FBEditor HTML plugin used CComModule only to get the DLL HINSTANCE.
// In this project we keep the HINSTANCE separately in g_hInstance.
CRegKey	    _Settings;
CString	    _SettingsPath;

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
	return _AtlModule.DllCanUnloadNow();
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

_Check_return_
// PVS-Studio: COM ABI requires DllGetClassObject(REFCLSID, REFIID, LPVOID*).
// Do not change REFCLSID/REFIID to by-value parameters.
//-V835
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID* ppv) //-V835 //-V835
{
	return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// Manual registry helpers

static HRESULT SetRegString(HKEY root, LPCWSTR subkey, LPCWSTR valueName, LPCWSTR value)
{
	CRegKey key;
	LONG rc = key.Create(root, subkey);
	if (rc != ERROR_SUCCESS)
		return HRESULT_FROM_WIN32(rc);

	rc = ::RegSetValueExW(key.m_hKey,
		valueName,
		0,
		REG_SZ,
		reinterpret_cast<const BYTE*>(value),
		static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));

	return HRESULT_FROM_WIN32(rc);
}

static HRESULT RegisterDOCXPluginManually()
{
	wchar_t modulePath[MAX_PATH] = {};
	DWORD len = ::GetModuleFileNameW(g_hInstance, modulePath, _countof(modulePath));
	if (len == 0 || len >= _countof(modulePath))
		return HRESULT_FROM_WIN32(::GetLastError() ? ::GetLastError() : ERROR_INSUFFICIENT_BUFFER);

	// COM class registration.
	HRESULT hr = SetRegString(HKEY_CLASSES_ROOT,
		L"CLSID\\{41494D79-3346-4E8C-A432-51BCD3742FC1}",
		NULL,
		L"ExportDOCXPlugin Class");
	if (FAILED(hr))
		return hr;

	hr = SetRegString(HKEY_CLASSES_ROOT,
		L"CLSID\\{41494D79-3346-4E8C-A432-51BCD3742FC1}\\InprocServer32",
		NULL,
		modulePath);
	if (FAILED(hr))
		return hr;

	hr = SetRegString(HKEY_CLASSES_ROOT,
		L"CLSID\\{41494D79-3346-4E8C-A432-51BCD3742FC1}\\InprocServer32",
		L"ThreadingModel",
		L"Apartment");
	if (FAILED(hr))
		return hr;

	CStringW defaultIcon;
	defaultIcon.Format(L"%s,%d", modulePath, IDI_MAINICON);
	hr = SetRegString(HKEY_CLASSES_ROOT,
		L"CLSID\\{41494D79-3346-4E8C-A432-51BCD3742FC1}\\DefaultIcon",
		NULL,
		defaultIcon);
	if (FAILED(hr))
		return hr;

	// FBEditor plugin registration. This mirrors ExportHTMLPlugin.rgs.
	hr = SetRegString(HKEY_CURRENT_USER,
		L"Software\\FBETeam\\FictionBook Editor\\Plugins\\{41494D79-3346-4E8C-A432-51BCD3742FC1}",
		NULL,
		L"Export FB2 to DOCX");
	if (FAILED(hr))
		return hr;

	hr = SetRegString(HKEY_CURRENT_USER,
		L"Software\\FBETeam\\FictionBook Editor\\Plugins\\{41494D79-3346-4E8C-A432-51BCD3742FC1}",
		L"Type",
		L"Export");
	if (FAILED(hr))
		return hr;

	hr = SetRegString(HKEY_CURRENT_USER,
		L"Software\\FBETeam\\FictionBook Editor\\Plugins\\{41494D79-3346-4E8C-A432-51BCD3742FC1}",
		L"Menu",
		L"To DOCX");
	if (FAILED(hr))
		return hr;

	hr = SetRegString(HKEY_CURRENT_USER,
		L"Software\\FBETeam\\FictionBook Editor\\Plugins\\{41494D79-3346-4E8C-A432-51BCD3742FC1}",
		L"Icon",
		modulePath);
	return hr;
}

static HRESULT UnregisterDOCXPluginManually()
{
	CRegKey hkcr;
	if (hkcr.Open(HKEY_CLASSES_ROOT, L"CLSID") == ERROR_SUCCESS)
		hkcr.RecurseDeleteKey(L"{41494D79-3346-4E8C-A432-51BCD3742FC1}");

	CRegKey plugins;
	if (plugins.Open(HKEY_CURRENT_USER, L"Software\\FBETeam\\FictionBook Editor\\Plugins") == ERROR_SUCCESS)
		plugins.RecurseDeleteKey(L"{41494D79-3346-4E8C-A432-51BCD3742FC1}");

	return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI DllRegisterServer(void)
{
	// Do not use CExportDOCXPlugin::UpdateRegistry/ATL .rgs here.
	// On current ATL it returns DISP_E_EXCEPTION (0x80020009) for this legacy
	// plugin resource. Manual registration is equivalent to ExportHTMLPlugin.rgs.
	return RegisterDOCXPluginManually();
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
	return UnregisterDOCXPluginManually();
}

STDAPI DllInstall(BOOL bInstall, _In_opt_  LPCWSTR pszCmdLine)
{
	HRESULT hr = E_FAIL;
	static const wchar_t szUserSwitch[] = L"user";

	if (pszCmdLine != NULL)
	{
		if (_wcsnicmp(pszCmdLine, szUserSwitch, _countof(szUserSwitch)) == 0)
		{
			ATL::AtlSetPerUserRegistration(true);
		}
	}

	if (bInstall)
	{
		hr = DllRegisterServer();
		if (FAILED(hr))
		{
			DllUnregisterServer();
		}
	}
	else
	{
		hr = DllUnregisterServer();
	}

	return hr;
}

