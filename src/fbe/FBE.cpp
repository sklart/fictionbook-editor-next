// FBE.cpp : main source file for FBE.exe
//

#include "stdafx.h"
#include <locale.h>
#include <sys/stat.h>

#include "resource.h"

#include "utils.h"
#include "Settings.h"
#include "apputils.h"

#include "FBEView.h"
#include "FBDoc.h"
#include "TreeView.h"
#include "ContainerWnd.h"
#include "Scintilla.h"
#include "ILexer.h"
#include "EditorEngine.h"
#include "MainFrm.h"
#include "MemProtocol.h"
#include "CrashHandler.h"
#include "StartupTrace.h"
#include "RuntimeLocalization.h"

// typelib interfaces
#include "FBE.h"

// implementation
#include "ExternalHelper.h"

// typelib guids
#include "fbe_i.c"

#define	DEFINE_CLSID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
		EXTERN_C const CLSID DECLSPEC_SELECTANY name \
		= { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

// {7301FF90-9029-4819-B778-19D9999DB419}
DEFINE_CLSID(CLSID_MemProtocol, 0x7301ff90, 0x9029, 0x4819, 0xb7, 0x78, 0x19, 0xd9, 0x99, 0x9d, 0xb4, 0x19);

CAppModule _Module;
extern CElementDescMnr _EDMnr;

BEGIN_OBJECT_MAP(ObjectMap)
	OBJECT_ENTRY(CLSID_MemProtocol, CMemProtocol)
END_OBJECT_MAP()

CSettings _Settings;
CSimpleArray<CString> _ARGV;

static void ConfigureDllSearchPath()
{
	// Keep the current working directory out of the legacy DLL search order.
	// This API is available on every Windows version supported by FBE.
	::SetDllDirectory(L"");
}

static HMODULE LoadApplicationLibrary(const CString& fileName)
{
	const CString libraryPath = U::GetProgDirFile(fileName);

	// LOAD_LIBRARY_SEARCH_* requires KB2533623 on Windows 7. Fall back to
	// loading the absolute path when the updated loader API is unavailable.
	HMODULE library = ::LoadLibraryEx(
		libraryPath,
		NULL,
		LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (library == NULL && ::GetLastError() == ERROR_INVALID_PARAMETER)
		library = ::LoadLibrary(libraryPath);

	return library;
}

static HRESULT ValidateExternalHelperTypeLibrary(ITypeLib* typeLibrary, const wchar_t* phase)
{
	if (!typeLibrary)
		return E_POINTER;

	TLIBATTR* attributes = NULL;
	HRESULT result = typeLibrary->GetLibAttr(&attributes);
	StartupTrace::HResult(L"typelib", L"TL130", result, L"GetLibAttr");
	if (FAILED(result))
		return result;
	CString details;
	details.Format(L"phase=%s; libid=%08lX; version=%u.%u; lcid=%lu; syskind=%u; typeinfo=%u", phase,
		attributes->guid.Data1, attributes->wMajorVerNum, attributes->wMinorVerNum, attributes->lcid,
		attributes->syskind, typeLibrary->GetTypeInfoCount());
	StartupTrace::Event(L"typelib", L"TL131", details);
	typeLibrary->ReleaseTLibAttr(attributes);

	CComPtr<ITypeInfo> externalHelper;
	result = typeLibrary->GetTypeInfoOfGuid(IID_IExternalHelper, &externalHelper);
	StartupTrace::HResult(L"typelib", L"TL140", result, L"GetTypeInfoOfGuid(IID_IExternalHelper)");
	if (FAILED(result)) return result;
	const wchar_t* const requiredMethods[] = { L"GetStylePath", L"GetNBSP", L"GetUUID", L"GetExtendedStyle", L"InflateParagraphs", L"GetProgramVersion", L"IsDiagnosticTraceEnabled", L"TraceScript" };
	for (int index = 0; index < _countof(requiredMethods); ++index)
	{
		LPOLESTR name = const_cast<LPOLESTR>(requiredMethods[index]);
		MEMBERID memberId = DISPID_UNKNOWN;
		result = externalHelper->GetIDsOfNames(&name, 1, &memberId);
		CString method; method.Format(L"method=%s; dispid=%ld", requiredMethods[index], static_cast<long>(memberId));
		StartupTrace::HResult(L"typelib", L"TL150", result, method);
		if (FAILED(result)) return result;
	}
	return S_OK;
}

static HRESULT EnsureTypeLibraryRegisteredForCurrentUser()
{
	StartupTrace::Event(L"typelib", L"TL100", L"registered FBELib validation started");
	const CString modulePath = U::GetModulePath();
	if(modulePath.IsEmpty()) return HRESULT_FROM_WIN32(::GetLastError());

	// Do not prime the OLEAUT cache with LoadRegTypeLib before checking the
	// physical registration. This is essential for a portable repair.
	LPOLESTR registeredPath = NULL;
	HRESULT result = ::QueryPathOfRegTypeLib(LIBID_FBELib, 1, 0, LOCALE_SYSTEM_DEFAULT, &registeredPath);
	StartupTrace::HResult(L"typelib", L"TL120", result, L"QueryPathOfRegTypeLib before repair");
	CComPtr<ITypeLib> directRegistered;
	if(SUCCEEDED(result) && registeredPath)
	{
		CString details;
		details.Format(L"registered-path=%s; matches-current=%d", (LPCWSTR)StartupTrace::RedactPath(registeredPath), modulePath.CompareNoCase(registeredPath) == 0 ? 1 : 0);
		StartupTrace::Event(L"typelib", L"TL121", details);
		result = ::LoadTypeLibEx(registeredPath, REGKIND_NONE, &directRegistered);
		StartupTrace::HResult(L"typelib", L"TL122", result, L"LoadTypeLibEx(registered path)");
		if(SUCCEEDED(result)) result = ValidateExternalHelperTypeLibrary(directRegistered, L"registered-direct");
		if(SUCCEEDED(result))
		{
			::SysFreeString(registeredPath);
			StartupTrace::Event(L"typelib", L"TL199", L"registered FBELib is compatible without LoadRegTypeLib");
			return S_OK;
		}
		StartupTrace::Warning(L"typelib", L"TL151", L"registered FBELib is incompatible; repairing per-user registration");
	}
	if(registeredPath) { ::SysFreeString(registeredPath); registeredPath = NULL; }

	CComPtr<ITypeLib> embedded;
	result = ::LoadTypeLibEx((LPCOLESTR)(LPCTSTR)modulePath, REGKIND_NONE, &embedded);
	StartupTrace::HResult(L"typelib", L"TL160", result, L"LoadTypeLibEx(current FBE.exe)");
	if(FAILED(result)) return result;
	result = ValidateExternalHelperTypeLibrary(embedded, L"embedded");
	StartupTrace::HResult(L"typelib", L"TL170", result, L"embedded FBELib validation");
	if(FAILED(result)) return result;
	result = ::RegisterTypeLibForUser(embedded, (LPOLESTR)(LPCTSTR)modulePath, NULL);
	StartupTrace::HResult(L"typelib", L"TL180", result, L"RegisterTypeLibForUser");
	if(FAILED(result)) return result;

	result = ::QueryPathOfRegTypeLib(LIBID_FBELib, 1, 0, LOCALE_SYSTEM_DEFAULT, &registeredPath);
	StartupTrace::HResult(L"typelib", L"TL190", result, L"QueryPathOfRegTypeLib after repair");
	CComPtr<ITypeLib> repairedDirect;
	if(SUCCEEDED(result) && registeredPath)
	{
		result = ::LoadTypeLibEx(registeredPath, REGKIND_NONE, &repairedDirect);
		StartupTrace::HResult(L"typelib", L"TL191", result, L"LoadTypeLibEx(repaired path)");
		if(SUCCEEDED(result)) result = ValidateExternalHelperTypeLibrary(repairedDirect, L"registered-direct-after-repair");
	}
	if(registeredPath) { ::SysFreeString(registeredPath); registeredPath = NULL; }
	if(FAILED(result))
	{
		StartupTrace::HResult(L"typelib", L"TL199", result, L"registry-not-updated");
		return result;
	}

	// The first LoadRegTypeLib happens only after a direct repaired check.
	CComPtr<ITypeLib> activeRegistered;
	HRESULT loadRegResult = ::LoadRegTypeLib(LIBID_FBELib, 1, 0, LOCALE_SYSTEM_DEFAULT, &activeRegistered);
	StartupTrace::HResult(L"typelib", L"TL192", loadRegResult, L"first LoadRegTypeLib after repair");
	if(SUCCEEDED(loadRegResult)) loadRegResult = ValidateExternalHelperTypeLibrary(activeRegistered, L"loadreg-after-repair");
	if(FAILED(loadRegResult))
		StartupTrace::Warning(L"typelib", L"TL193", L"registry-updated-loadreg-stale; typelib-cache-stale=1");
	else
		StartupTrace::Event(L"typelib", L"TL199", L"registry-updated-and-active");
	return S_OK;
}// External helpers
IDispatchPtr  CFBEView::CreateHelper()
{
	CComObject<ExternalHelper> *obj;
	if(FAILED(CComObject<ExternalHelper>::CreateInstance(&obj)))
		obj = NULL;
	else
		obj->SetDocumentFilePathSource(m_document_filename, m_document_namevalid);
	return obj;
}

// Command line parser
static void ParseCommandLine(LPTSTR cmd, CSimpleArray<CString>& args)
{
	TCHAR* p=cmd;
	int len= _tcslen(p);
	TCHAR* e = p + len;

	for (;;)
	{
		// Skip ws
		while(p < e && (unsigned)*p <= 32)
			++p;
		if(p >= e)
			break;

		// Process argument
		CString arg;
		TCHAR* buf = arg.GetBuffer(e - p);
		TCHAR* q = buf;
		bool fQuote = false;
		while(p < e)
		{
			if(fQuote)
			{
				if(*p == L'"')
				{
					// Possible end of arg
					if(p + 1 < e && p[1] == L'"')
					{
						// Literal quote
						*q++ = L'"';
						++p;
					}
					else
						fQuote = false;
					}
				else
					*q++ = *p; // normal char
			}
			else
			{
				if(*p <= 32) // end of arg
					break;
				if(*p == L'"') // quoted part
					fQuote = true;
				else // normal text
				*q++ = *p;
			}
			++p;
		}
		arg.ReleaseBuffer(q - buf);
		args.Add(arg);
	}
}

HINSTANCE resLib;

int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
	StartupTrace::Mark(L"main window setup started");
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);
	CMainFrame wndMain;

	resLib = LoadApplicationLibrary(_Settings.GetInterfaceLanguageDllName());
	if(resLib)
	ATL::_AtlBaseModule.SetResourceInstance(resLib);
	else
	ATL::_AtlBaseModule.SetResourceInstance(ATL::_AtlBaseModule.GetModuleInstance());

	FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName());

	HookSysDialogs();

	U::InitKeycodes();
	U::InitSettingsHotkeyGroups();
	StartupTrace::Mark(L"resources and hotkeys initialized");

	if(wndMain.CreateEx() == NULL)
	{
		ATLTRACE(L"Main window creation failed!\n");
		return 0;
	}
	StartupTrace::Mark(L"main window created");

	WINDOWPLACEMENT wpl;
	if(_Settings.GetWindowPosition(wpl))
	{
		wndMain.SetWindowPlacement(&wpl);
		wndMain.ShowWindow(wpl.showCmd);
	}
	else
	{
		wndMain.ShowWindow(nCmdShow);
		wndMain.GetWindowPlacement(&wpl);
		_Settings.SetWindowPosition(wpl);
	}
	StartupTrace::Mark(L"main window shown");

	int nRet = theLoop.Run();
	StartupTrace::Mark(L"message loop exited");

	_Module.RemoveMessageLoop();

	// exit CBT hook
	UnhookSysDialogs();

	return nRet;
}

typedef Scintilla::ILexer5* (__stdcall *CreateLexerFn)(const char* name);

static HMODULE g_scintillaModule = NULL;
static HMODULE g_lexillaModule = NULL;
static CreateLexerFn g_createLexer = NULL;

static void ResetEditorModules()
{
	g_createLexer = NULL;
	if(g_lexillaModule != NULL)
	{
		::FreeLibrary(g_lexillaModule);
		g_lexillaModule = NULL;
	}
	if(g_scintillaModule != NULL)
	{
		::FreeLibrary(g_scintillaModule);
		g_scintillaModule = NULL;
	}
}

bool LoadEditor()
{
	g_scintillaModule = LoadApplicationLibrary(L"Scintilla.dll");
	if(g_scintillaModule == NULL)
	{
		ATLTRACE(L"Unable to load Scintilla.dll: %lu\n", ::GetLastError());
		return false;
	}

	g_lexillaModule = LoadApplicationLibrary(L"Lexilla.dll");
	if(g_lexillaModule == NULL)
	{
		ATLTRACE(L"Unable to load Lexilla.dll: %lu\n", ::GetLastError());
		ResetEditorModules();
		return false;
	}

	g_createLexer = reinterpret_cast<CreateLexerFn>(
		::GetProcAddress(g_lexillaModule, "CreateLexer"));
	if(g_createLexer == NULL)
	{
		ATLTRACE(L"Lexilla.dll does not export CreateLexer.\n");
		ResetEditorModules();
		return false;
	}

	Scintilla::ILexer5* xmlLexer = g_createLexer("xml");
	if(xmlLexer == NULL)
	{
		ATLTRACE(L"Lexilla.dll does not provide the XML lexer.\n");
		ResetEditorModules();
		return false;
	}
	xmlLexer->Release();
	return true;
}

Scintilla::ILexer5* CreateEditorLexer(const char* name)
{
	return g_createLexer != NULL ? g_createLexer(name) : NULL;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	int nRet=1;

  ConfigureDllSearchPath();
  StartupTrace::Start();

#if 1
#ifdef _DEBUG
  _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
  _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
  _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
  _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDERR );
  _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
  _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
#endif
#endif

  // initialize RNG
  srand((unsigned int)time(NULL));

  // switch to user's locale
  setlocale(LC_CTYPE,"");
  setlocale(LC_COLLATE,"");

  // initialize COM/OLE
  HRESULT hRes = ::OleInitialize(NULL);
  ATLASSERT(SUCCEEDED(hRes));
  
  // this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
  ::DefWindowProc(NULL, 0, 0, 0L);

  AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES);	// add flags to support other controls

  // init module
  hRes = _Module.Init(ObjectMap, hInstance, &LIBID_FBELib);
  ATLASSERT(SUCCEEDED(hRes));
  StartupTrace::Mark(L"COM and application module initialized");

  // Installed builds are registered by NSIS. Portable builds register only
  // for the current user, and only when the type library is not available.
  hRes = EnsureTypeLibraryRegisteredForCurrentUser();
  if (FAILED(hRes))
    ATLTRACE(L"Unable to register the FBE type library: 0x%08X\n", hRes);

  // enable web browser hosting
  AtlAxWinInit();

  // initialize registry settings
  U::InitSettings();
  StartupTrace::Mark(L"settings initialized");
  CrashDiagnostics::Initialize();
  StartupTrace::Mark(L"crash diagnostics initialized");

  // parse command line
  ParseCommandLine(lpstrCmdLine,_ARGV);
  if (!AU::ParseCmdLineArgs())
    goto out;
  
  // load xml source editor
  if (!LoadEditor()) 
  {
	  wchar_t msg[MAX_LOAD_STRING + 1];
	  wchar_t cpt[MAX_LOAD_STRING + 1];
	  FbeLoadString(_Module.GetResourceInstance(), IDS_SCINTILLA_LOAD_ERR_MSG, msg, MAX_LOAD_STRING);
	  FbeLoadString(_Module.GetResourceInstance(), IDS_ERRMSGBOX_CAPTION, cpt, MAX_LOAD_STRING);      
    ::MessageBox(NULL, msg, cpt,MB_OK|MB_ICONERROR);
    goto out;
  }
  StartupTrace::Mark(L"Scintilla and Lexilla loaded");

  // register our protocol handler
  IInternetSession *isess;
  if (SUCCEEDED(::CoInternetGetSession(0, &isess, 0))) {
    IClassFactory *cf;
    if (SUCCEEDED(_Module.GetClassObject(CLSID_MemProtocol, IID_IClassFactory, (void**)&cf))) {
      HRESULT hr=isess->RegisterNameSpace(cf,CLSID_MemProtocol,L"fbw-internal",0,NULL,0);
      if (FAILED(hr))
	      ATLTRACE("Failed to register protocol handler: %x\n",hr);
      cf->Release();
    }
    isess->Release();
  }

  // run the main loop
  StartupTrace::Mark(L"protocol handler initialized");
  nRet = Run(lpstrCmdLine, nCmdShow);
out:
  _Module.Term();

  ::OleUninitialize();
  StartupTrace::Finish();
  
  return nRet;
}
