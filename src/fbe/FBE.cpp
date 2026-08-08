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
#include "ExternalHelper.h"

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

static HRESULT FindFunctionDescription(ITypeInfo* typeInfo, MEMBERID memberId, FUNCDESC** functionDescription)
{
	if (!typeInfo || !functionDescription) return E_POINTER;
	*functionDescription = NULL;
	TYPEATTR* attributes = NULL;
	HRESULT result = typeInfo->GetTypeAttr(&attributes);
	if (FAILED(result)) return result;
	for (UINT index = 0; index < attributes->cFuncs; ++index)
	{
		FUNCDESC* candidate = NULL;
		result = typeInfo->GetFuncDesc(index, &candidate);
		if (FAILED(result)) break;
		if (candidate->memid == memberId)
		{
			*functionDescription = candidate;
			result = S_OK;
			break;
		}
		typeInfo->ReleaseFuncDesc(candidate);
	}
	typeInfo->ReleaseTypeAttr(attributes);
	return *functionDescription ? S_OK : (FAILED(result) ? result : DISP_E_MEMBERNOTFOUND);
}

static bool TypeDescMatches(const TYPEDESC& typeDescription, VARTYPE expectedType, bool expectedPointer)
{
	if (expectedPointer)
		return typeDescription.vt == VT_PTR && typeDescription.lptdesc && typeDescription.lptdesc->vt == expectedType;
	return typeDescription.vt == expectedType;
}

static HRESULT ValidateExternalHelperTypeLibrary(ITypeLib* typeLibrary, const wchar_t* phase,
	bool* coreCompatible = NULL, bool* diagnosticCompatible = NULL, bool diagnosticMismatchIsWarning = true)
{
	if (coreCompatible) *coreCompatible = false;
	if (diagnosticCompatible) *diagnosticCompatible = false;
	if (!typeLibrary)
		return E_POINTER;

	TLIBATTR* attributes = NULL;
	HRESULT result = typeLibrary->GetLibAttr(&attributes);
	StartupTrace::HResult(L"typelib", L"TL130", result, L"GetLibAttr");
	if (FAILED(result)) return result;
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

	static const VARTYPE inflateParagraphsTypes[] = { VT_DISPATCH };
	static const VARTYPE getExtendedStyleTypes[] = { VT_BSTR };
	static const VARTYPE traceScriptTypes[] = { VT_BSTR, VT_BSTR };
	struct RequiredMethod { const wchar_t* name; DISPID dispid; bool core; const VARTYPE* types; UINT parameterCount; VARTYPE resultType; };
	const RequiredMethod methods[] = {
		{ L"GetStylePath", 5, true, NULL, 0, VT_BSTR },
		{ L"InflateParagraphs", 7, true, inflateParagraphsTypes, _countof(inflateParagraphsTypes), VT_VOID },
		{ L"GetUUID", 8, true, NULL, 0, VT_BSTR },
		{ L"GetExtendedStyle", 12, true, getExtendedStyleTypes, _countof(getExtendedStyleTypes), VT_I4 },
		{ L"GetNBSP", 19, true, NULL, 0, VT_BSTR },
		{ L"GetProgramVersion", 22, true, NULL, 0, VT_BSTR },
		{ L"IsDiagnosticTraceEnabled", 29, false, NULL, 0, VT_I4 },
		{ L"TraceScript", 30, false, traceScriptTypes, _countof(traceScriptTypes), VT_VOID }
	};
	CString missingCore, missingDiagnostic, wrongCoreDispids, wrongDiagnosticDispids, wrongCoreSignatures, wrongDiagnosticSignatures;
	for (UINT index = 0; index < _countof(methods); ++index)
	{
		LPOLESTR name = const_cast<LPOLESTR>(methods[index].name);
		MEMBERID memberId = DISPID_UNKNOWN;
		HRESULT methodResult = externalHelper->GetIDsOfNames(&name, 1, &memberId);
		CString method; method.Format(L"method=%s; expected-dispid=%ld; actual-dispid=%ld", methods[index].name, static_cast<long>(methods[index].dispid), static_cast<long>(memberId));
		CString& missing = methods[index].core ? missingCore : missingDiagnostic;
		CString& wrongDispids = methods[index].core ? wrongCoreDispids : wrongDiagnosticDispids;
		CString& wrongSignatures = methods[index].core ? wrongCoreSignatures : wrongDiagnosticSignatures;
		if (FAILED(methodResult))
		{
			if (!missing.IsEmpty()) missing += L","; missing += methods[index].name;
			if (methods[index].core) StartupTrace::HResult(L"typelib", L"TL151", methodResult, method);
			else { method += diagnosticMismatchIsWarning ? L"; diagnostic-bridge=degraded" : L"; diagnostic-registration=legacy; internal-bridge=embedded"; if (diagnosticMismatchIsWarning) StartupTrace::Warning(L"typelib", L"TL152", method); else StartupTrace::Event(L"typelib", L"TL152", method); }
			continue;
		}
		if (memberId != methods[index].dispid)
		{
			if (!wrongDispids.IsEmpty()) wrongDispids += L","; wrongDispids += methods[index].name;
			if (methods[index].core) { method += L"; core-incompatible"; StartupTrace::Error(L"typelib", L"TL153", method); }
			else { method += diagnosticMismatchIsWarning ? L"; diagnostic-bridge=degraded" : L"; diagnostic-registration=legacy; internal-bridge=embedded"; if (diagnosticMismatchIsWarning) StartupTrace::Warning(L"typelib", L"TL154", method); else StartupTrace::Event(L"typelib", L"TL154", method); }
			continue;
		}

		FUNCDESC* functionDescription = NULL;
		const HRESULT functionResult = FindFunctionDescription(externalHelper, memberId, &functionDescription);
		bool signatureMatches = SUCCEEDED(functionResult) && functionDescription && functionDescription->invkind == INVOKE_FUNC &&
			functionDescription->cParams == methods[index].parameterCount && functionDescription->elemdescFunc.tdesc.vt == methods[index].resultType;
		if (signatureMatches)
		{
			for (UINT parameter = 0; parameter < methods[index].parameterCount; ++parameter)
			{
				const ELEMDESC& parameterDescription = functionDescription->lprgelemdescParam[parameter];
				// All arguments in the core/diagnostic dispatch contract are [in].
				// The automation return value is represented by elemdescFunc, not a
				// synthetic [out, retval] parameter in this embedded type info.
				if (!TypeDescMatches(parameterDescription.tdesc, methods[index].types[parameter], false) ||
					(parameterDescription.paramdesc.wParamFlags & PARAMFLAG_FIN) == 0 ||
					(parameterDescription.paramdesc.wParamFlags & (PARAMFLAG_FOUT | PARAMFLAG_FRETVAL)) != 0)
				{
					signatureMatches = false; break;
				}
			}
		}
		CString actualSignature;
		if (functionDescription)
		{
			actualSignature.Format(L"; actual-invkind=%u; actual-params=%u; actual-result-vt=%u; actual-result-flags=0x%X; actual-param-vt-flags=", functionDescription->invkind, functionDescription->cParams, functionDescription->elemdescFunc.tdesc.vt, functionDescription->elemdescFunc.paramdesc.wParamFlags);
			for (UINT parameter = 0; parameter < static_cast<UINT>(functionDescription->cParams); ++parameter)
			{
				if (parameter) actualSignature += L",";
				actualSignature.AppendFormat(L"%u/0x%X", functionDescription->lprgelemdescParam[parameter].tdesc.vt, functionDescription->lprgelemdescParam[parameter].paramdesc.wParamFlags);
			}
		}
		if (functionDescription) externalHelper->ReleaseFuncDesc(functionDescription);
		if (!signatureMatches)
		{
			if (!wrongSignatures.IsEmpty()) wrongSignatures += L","; wrongSignatures += methods[index].name;
			method.AppendFormat(L"; signature-hr=0x%08lX; core-compatible=%d", static_cast<unsigned long>(functionResult), methods[index].core ? 0 : 1); method += actualSignature;
			if (methods[index].core) StartupTrace::Error(L"typelib", L"TL160", method);
			else { method += diagnosticMismatchIsWarning ? L"; diagnostic-bridge=degraded" : L"; diagnostic-registration=legacy; internal-bridge=embedded"; if (diagnosticMismatchIsWarning) StartupTrace::Warning(L"typelib", L"TL161", method); else StartupTrace::Event(L"typelib", L"TL161", method); }
		}
		else StartupTrace::Event(L"typelib", L"TL150", method);
	}
	const bool coreIsCompatible = missingCore.IsEmpty() && wrongCoreDispids.IsEmpty() && wrongCoreSignatures.IsEmpty();
	const bool diagnosticIsCompatible = missingDiagnostic.IsEmpty() && wrongDiagnosticDispids.IsEmpty() && wrongDiagnosticSignatures.IsEmpty();
	if (coreCompatible) *coreCompatible = coreIsCompatible;
	if (diagnosticCompatible) *diagnosticCompatible = diagnosticIsCompatible;
	CString missing = missingCore; if (!missingDiagnostic.IsEmpty()) { if (!missing.IsEmpty()) missing += L","; missing += missingDiagnostic; }
	CString wrong = wrongCoreDispids; if (!wrongDiagnosticDispids.IsEmpty()) { if (!wrong.IsEmpty()) wrong += L","; wrong += wrongDiagnosticDispids; }
	CString signatures = wrongCoreSignatures; if (!wrongDiagnosticSignatures.IsEmpty()) { if (!signatures.IsEmpty()) signatures += L","; signatures += wrongDiagnosticSignatures; }
	CString summary;
	summary.Format(L"phase=%s; core-compatible=%d; diagnostic-compatible=%d; missing-methods=%s; wrong-dispids=%s; wrong-signatures=%s", phase, coreIsCompatible ? 1 : 0, diagnosticIsCompatible ? 1 : 0, (LPCWSTR)missing, (LPCWSTR)wrong, (LPCWSTR)signatures);
	if (!coreIsCompatible) { StartupTrace::Error(L"typelib", L"TL155", summary); return E_NOINTERFACE; }
	if (!diagnosticIsCompatible && diagnosticMismatchIsWarning) StartupTrace::Warning(L"typelib", L"TL156", summary);
	else StartupTrace::Event(L"typelib", !diagnosticIsCompatible ? L"TL156" : L"TL157", summary);
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
		if(SUCCEEDED(result)) result = ValidateExternalHelperTypeLibrary(directRegistered, L"registered-direct", NULL, NULL, false);
		if(SUCCEEDED(result))
		{
			::SysFreeString(registeredPath);
			StartupTrace::Event(L"typelib", L"TL158", L"registered FBELib core-compatible; internal diagnostic bridge uses embedded typelib");
			return S_OK;
		}
		StartupTrace::Warning(L"typelib", L"TL159", L"registered FBELib is incompatible; repairing per-user registration");
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
		StartupTrace::HResult(L"typelib", L"TL196", result, L"registry-not-updated");
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
		StartupTrace::Event(L"typelib", L"TL197", L"registry-updated-and-active");
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

static bool IsMainFrameCreateFaultEnabled()
{
	if (!StartupTrace::Enabled()) return false;
	wchar_t testMode[8] = {}, fault[128] = {};
	const DWORD testModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode));
	const DWORD faultLength = ::GetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", fault, _countof(fault));
	return testModeLength && testModeLength < _countof(testMode) && wcscmp(testMode, L"1") == 0 &&
		faultLength && faultLength < _countof(fault) && wcscmp(fault, L"main-frame-create-failure") == 0;
}

int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
	StartupTrace::Event(L"startup", L"S170", L"main window setup started");
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
	StartupTrace::Event(L"startup", L"S175", L"resources and hotkeys initialized");

	StartupTrace::Event(L"startup", L"S190", L"main frame creation begin");
	const bool injectedMainFrameFailure = IsMainFrameCreateFaultEnabled();
	if (injectedMainFrameFailure)
		StartupTrace::Event(L"fault", L"FI014", L"main-frame-create-failure injected");
	if(injectedMainFrameFailure || wndMain.CreateEx() == NULL)
	{
		const DWORD error = injectedMainFrameFailure ? ERROR_GEN_FAILURE : ::GetLastError();
		StartupTrace::HResult(L"startup", L"S191", HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error), L"main frame creation failed");
		ATLTRACE(L"Main window creation failed!\n");
		return 1;
	}
	StartupTrace::Event(L"startup", L"S192", L"main frame created");

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
	StartupTrace::WriteLateEnvironmentHeader();
	StartupTrace::Event(L"startup", L"S230", L"main frame ready");

	int nRet = theLoop.Run();
	StartupTrace::Event(L"startup", L"S900", L"message loop exited");

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
		const DWORD error = ::GetLastError();
		StartupTrace::HResult(L"startup", L"S160", HRESULT_FROM_WIN32(error), L"LoadLibraryEx(Scintilla.dll)");
		ATLTRACE(L"Unable to load Scintilla.dll: %lu\n", error);
		return false;
	}

	g_lexillaModule = LoadApplicationLibrary(L"Lexilla.dll");
	if(g_lexillaModule == NULL)
	{
		const DWORD error = ::GetLastError();
		StartupTrace::HResult(L"startup", L"S161", HRESULT_FROM_WIN32(error), L"LoadLibraryEx(Lexilla.dll)");
		ATLTRACE(L"Unable to load Lexilla.dll: %lu\n", error);
		ResetEditorModules();
		return false;
	}

	g_createLexer = reinterpret_cast<CreateLexerFn>(
		::GetProcAddress(g_lexillaModule, "CreateLexer"));
	if(g_createLexer == NULL)
	{
		const DWORD error = ::GetLastError();
		StartupTrace::HResult(L"startup", L"S162", HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_PROC_NOT_FOUND : error), L"GetProcAddress(CreateLexer)");
		ATLTRACE(L"Lexilla.dll does not export CreateLexer.\n");
		ResetEditorModules();
		return false;
	}

	Scintilla::ILexer5* xmlLexer = g_createLexer("xml");
	if(xmlLexer == NULL)
	{
		StartupTrace::HResult(L"startup", L"S163", HRESULT_FROM_WIN32(ERROR_NOT_FOUND), L"CreateLexer(xml)");
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
  StartupTrace::HResult(L"startup", L"S110", hRes, L"OleInitialize");
  if (FAILED(hRes)) { StartupTrace::Error(L"startup", L"S110", L"OleInitialize is fatal"); StartupTrace::Finish(); return 1; }
  
  // this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
  ::DefWindowProc(NULL, 0, 0, 0L);

  AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES);	// add flags to support other controls

  // init module
  hRes = _Module.Init(ObjectMap, hInstance, &LIBID_FBELib);
  ATLASSERT(SUCCEEDED(hRes));
  StartupTrace::HResult(L"startup", L"S120", hRes, L"_Module.Init");
  if (FAILED(hRes)) { StartupTrace::Finish(); ::OleUninitialize(); return 1; }

  StartupTrace::Event(L"startup", L"S130", L"type library validation started");

  // Installed builds are registered by NSIS. Portable builds register only
  // for the current user, and only when the type library is not available.
  hRes = EnsureTypeLibraryRegisteredForCurrentUser();
  if (FAILED(hRes))
    ATLTRACE(L"Unable to register the FBE type library: 0x%08X\n", hRes);

  // enable web browser hosting
  if (!AtlAxWinInit()) { const DWORD error = ::GetLastError(); hRes = HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error); StartupTrace::HResult(L"startup", L"S125", hRes, L"AtlAxWinInit"); _Module.Term(); ::OleUninitialize(); StartupTrace::Finish(); return 1; }
  StartupTrace::HResult(L"startup", L"S125", S_OK, L"AtlAxWinInit");

  // initialize registry settings
  U::InitSettings();
  StartupTrace::Event(L"startup", L"S140", L"settings initialized");
  CrashDiagnostics::Initialize();
  StartupTrace::Event(L"startup", L"S150", L"crash diagnostics initialized");

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
	StartupTrace::Event(L"startup", L"S164", L"editor modules initialized");

  // register our protocol handler
  IInternetSession *isess = NULL;
  hRes = ::CoInternetGetSession(0, &isess, 0);
  StartupTrace::HResult(L"startup", L"S180", hRes, L"CoInternetGetSession");
  if (SUCCEEDED(hRes)) {
    IClassFactory *cf = NULL;
    hRes = _Module.GetClassObject(CLSID_MemProtocol, IID_IClassFactory, (void**)&cf);
    StartupTrace::HResult(L"startup", L"S181", hRes, L"_Module.GetClassObject(CMemProtocol)");
    if (SUCCEEDED(hRes)) {
      hRes = isess->RegisterNameSpace(cf,CLSID_MemProtocol,L"fbw-internal",0,NULL,0);
      StartupTrace::HResult(L"startup", L"S182", hRes, L"IInternetSession::RegisterNameSpace");
      if (FAILED(hRes)) ATLTRACE("Failed to register protocol handler: %x\n",hRes);
      cf->Release();
    }
    isess->Release();
  }

  // run the main loop
  nRet = Run(lpstrCmdLine, nCmdShow);
out:
  _Module.Term();

  ::OleUninitialize();
  ExternalHelper::FlushTraceSummary();
  StartupTrace::Finish();
  
  return nRet;
}
