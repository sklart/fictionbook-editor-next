#include "stdafx.h"
#include "resource.h"
#include "res1.h"

#include "utils.h"
#include "apputils.h"

#include "FBE.h"
#include "ExternalHelper.h"
#include <map>

__declspec(thread) bool ExternalHelper::s_traceScriptActive = false;
CComAutoCriticalSection ExternalHelper::s_embeddedTypeInfoLock;
CComPtr<ITypeInfo> ExternalHelper::s_embeddedTypeInfo;
namespace
{
	CComAutoCriticalSection g_dispatchTraceLock;
	std::map<DISPID, unsigned long> g_successfulNameLookups, g_successfulInvokes, g_failedNameLookups, g_failedInvokes;
	std::map<ULONGLONG, unsigned long> g_uniqueLookupFailures, g_uniqueInvokeFailures;

	bool IsExternalTraceVerbose()
	{
		wchar_t value[8] = {};
		const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TRACE_VERBOSE", value, _countof(value));
		return length && length < _countof(value) && !(length == 1 && value[0] == L'0');
	}

	bool RecordFirstCall(std::map<DISPID, unsigned long>& calls, DISPID dispid)
	{
		CComCritSecLock<CComAutoCriticalSection> lock(g_dispatchTraceLock);
		return ++calls[dispid] == 1;
	}

	bool RecordFirstFailure(std::map<DISPID, unsigned long>& calls, std::map<ULONGLONG, unsigned long>& uniqueFailures, DISPID dispid, HRESULT result)
	{
		CComCritSecLock<CComAutoCriticalSection> lock(g_dispatchTraceLock);
		++calls[dispid];
		const ULONGLONG key = (static_cast<ULONGLONG>(static_cast<ULONG>(dispid)) << 32) | static_cast<ULONG>(result);
		return ++uniqueFailures[key] == 1;
	}

	unsigned long CallCount(const std::map<DISPID, unsigned long>& calls, DISPID dispid)
	{
		std::map<DISPID, unsigned long>::const_iterator found = calls.find(dispid);
		return found == calls.end() ? 0 : found->second;
	}
}

HRESULT ExternalHelper::GetEmbeddedTypeInfo(ITypeInfo** resultTypeInfo)
{
	if (!resultTypeInfo)
		return E_POINTER;
	*resultTypeInfo = NULL;

	CComCritSecLock<CComAutoCriticalSection> lock(s_embeddedTypeInfoLock);
	if (!s_embeddedTypeInfo)
	{
		wchar_t modulePath[MAX_PATH] = {};
		if (::GetModuleFileName(NULL, modulePath, _countof(modulePath)) == 0)
			return HRESULT_FROM_WIN32(::GetLastError());

		CComPtr<ITypeLib> typeLibrary;
		HRESULT result = ::LoadTypeLibEx(modulePath, REGKIND_NONE, &typeLibrary);
		StartupTrace::HResult(L"external", L"XH090", result, L"LoadTypeLibEx(current FBE.exe, REGKIND_NONE)");
		if (FAILED(result))
			return result;

		result = typeLibrary->GetTypeInfoOfGuid(IID_IExternalHelper, &s_embeddedTypeInfo);
		StartupTrace::HResult(L"external", L"XH091", result, L"GetTypeInfoOfGuid(IID_IExternalHelper) from embedded typelib");
		if (FAILED(result))
			s_embeddedTypeInfo.Release();
	}
	if (!s_embeddedTypeInfo)
		return E_NOINTERFACE;

	*resultTypeInfo = s_embeddedTypeInfo;
	(*resultTypeInfo)->AddRef();
	return S_OK;
}

#define MENU_BASE 5000

struct Genre
{
	int		groupid;
	CString	id;
	CString	text;
};

static CSimpleArray<CString> g_genre_groups;
static CSimpleArray<Genre> g_genres;

struct DescElement
{
	int groupid;
	CString text;
};

static CSimpleMap<CString, DescElement> g_desc_elements;

static DISPID ExternalHelperMethodDispid(const wchar_t* method)
{
	if (!method) return DISPID_UNKNOWN;
	static const wchar_t* const names[] = { L"", L"BeginUndoUnit", L"EndUndoUnit", L"inflateBlock", L"GenrePopup", L"GetStylePath", L"GetBinarySize", L"InflateParagraphs", L"GetUUID", L"MsgBox", L"AskYesNo", L"SaveBinary", L"GetExtendedStyle", L"DescShowElement", L"DescShowMenu", L"IsFastMode", L"SetStyleEx", L"GetImageDimsByPath", L"GetImageDimsByData", L"GetNBSP", L"GetViewWidth", L"GetViewHeight", L"GetProgramVersion", L"InputBox", L"GetModalResult", L"SetStatusBarText", L"GetDocumentFilePath", L"GetDocumentFileName", L"GetDocumentDirectory", L"IsDiagnosticTraceEnabled", L"TraceScript" };
	for (DISPID dispid = 1; dispid < static_cast<DISPID>(_countof(names)); ++dispid)
		if (wcscmp(method, names[dispid]) == 0) return dispid;
	return DISPID_UNKNOWN;
}

static const wchar_t* ExternalHelperMethodName(DISPID dispid)
{
	static const wchar_t* const names[] = { L"other", L"BeginUndoUnit", L"EndUndoUnit", L"inflateBlock", L"GenrePopup", L"GetStylePath", L"GetBinarySize", L"InflateParagraphs", L"GetUUID", L"MsgBox", L"AskYesNo", L"SaveBinary", L"GetExtendedStyle", L"DescShowElement", L"DescShowMenu", L"IsFastMode", L"SetStyleEx", L"GetImageDimsByPath", L"GetImageDimsByData", L"GetNBSP", L"GetViewWidth", L"GetViewHeight", L"GetProgramVersion", L"InputBox", L"GetModalResult", L"SetStatusBarText", L"GetDocumentFilePath", L"GetDocumentFileName", L"GetDocumentDirectory", L"IsDiagnosticTraceEnabled", L"TraceScript" };
	return dispid > 0 && dispid < static_cast<DISPID>(_countof(names)) ? names[dispid] : names[0];
}
static bool IsLoadDiagnosticMethod(DISPID dispid) { return dispid == 5 || dispid == 6 || dispid == 7 || dispid == 8 || dispid == 12 || dispid == 13 || dispid == 17 || dispid == 18 || dispid == 19 || dispid == 22 || dispid == 29; }
static CString ExternalHelperArgumentTypes(const DISPPARAMS* parameters)
{
	CString types;
	for (UINT index = 0; parameters && index < parameters->cArgs; ++index) { const VARTYPE variantType = V_VT(&parameters->rgvarg[index]); CString type; type.Format(L"VT_%u", static_cast<unsigned int>(variantType & VT_TYPEMASK)); if (variantType & VT_BYREF) type += L"|VT_BYREF"; if (variantType & VT_ARRAY) type += L"|VT_ARRAY"; if (!types.IsEmpty()) types += L","; types += type; }
	return types;
}

void ExternalHelper::FlushTraceSummary()
{
	if (!StartupTrace::Enabled()) return;
	CComCritSecLock<CComAutoCriticalSection> lock(g_dispatchTraceLock);
	std::map<DISPID, unsigned long> lookupMethods(g_successfulNameLookups);
	for (std::map<DISPID, unsigned long>::const_iterator it = g_failedNameLookups.begin(); it != g_failedNameLookups.end(); ++it) lookupMethods[it->first] += 0;
	for (std::map<DISPID, unsigned long>::const_iterator it = lookupMethods.begin(); it != lookupMethods.end(); ++it)
	{
		const unsigned long successes = CallCount(g_successfulNameLookups, it->first);
		const unsigned long failures = CallCount(g_failedNameLookups, it->first);
		const unsigned long suppressed = (IsExternalTraceVerbose() ? 0 : (successes > 0 ? successes - 1 : 0) + (failures > 0 ? failures - 1 : 0));
		CString details; details.Format(L"method=%s; operation=GetIDsOfNames; success-count=%lu; failure-count=%lu; suppressed-count=%lu", ExternalHelperMethodName(it->first), successes, failures, suppressed);
		StartupTrace::Event(L"external", L"XH190", details);
	}
	std::map<DISPID, unsigned long> invokeMethods(g_successfulInvokes);
	for (std::map<DISPID, unsigned long>::const_iterator it = g_failedInvokes.begin(); it != g_failedInvokes.end(); ++it) invokeMethods[it->first] += 0;
	for (std::map<DISPID, unsigned long>::const_iterator it = invokeMethods.begin(); it != invokeMethods.end(); ++it)
	{
		const unsigned long successes = CallCount(g_successfulInvokes, it->first);
		const unsigned long failures = CallCount(g_failedInvokes, it->first);
		const unsigned long suppressed = (IsExternalTraceVerbose() ? 0 : (successes > 0 ? successes - 1 : 0) + (failures > 0 ? failures - 1 : 0));
		CString details; details.Format(L"method=%s; operation=Invoke; success-count=%lu; failure-count=%lu; suppressed-count=%lu", ExternalHelperMethodName(it->first), successes, failures, suppressed);
		StartupTrace::Event(L"external", L"XH191", details);
	}
}

HRESULT ExternalHelper::GetTypeInfoCount(UINT* typeInfoCount)
{
	if (!typeInfoCount)
		return E_POINTER;
	*typeInfoCount = 0;
	CComPtr<ITypeInfo> typeInfo;
	HRESULT result = GetEmbeddedTypeInfo(&typeInfo);
	if (SUCCEEDED(result))
		*typeInfoCount = 1;
	StartupTrace::HResult(L"external", L"XH100", result, L"GetTypeInfoCount (embedded typelib)");
	return result;
}
HRESULT ExternalHelper::GetTypeInfo(UINT typeInfo, LCID lcid, ITypeInfo** resultTypeInfo)
{
	if (!resultTypeInfo)
		return E_POINTER;
	*resultTypeInfo = NULL;
	HRESULT result = typeInfo == 0 ? GetEmbeddedTypeInfo(resultTypeInfo) : DISP_E_BADINDEX;
	CString details;
	details.Format(L"typeinfo=%u; lcid=%lu; source=embedded", typeInfo, lcid);
	StartupTrace::HResult(L"external", L"XH110", result, details);
	return result;
}
HRESULT ExternalHelper::GetIDsOfNames(REFIID riid, LPOLESTR* names, UINT nameCount, LCID lcid, DISPID* dispids)
{
	if (dispids)
		for (UINT index = 0; index < nameCount; ++index)
			dispids[index] = DISPID_UNKNOWN;
	if (riid != IID_NULL)
		return DISP_E_UNKNOWNINTERFACE;
	if (!names || !dispids || nameCount == 0)
		return E_INVALIDARG;

	CComPtr<ITypeInfo> typeInfo;
	HRESULT result = GetEmbeddedTypeInfo(&typeInfo);
	if (SUCCEEDED(result))
		result = typeInfo->GetIDsOfNames(names, nameCount, dispids);

	CString details;
	details.Format(L"lcid=%lu; names=%u; method=%s; dispid=%ld; source=embedded", lcid, nameCount,
		names[0] ? (LPCWSTR)StartupTrace::SanitizeLogText(names[0], 64) : L"-",
		SUCCEEDED(result) ? static_cast<long>(dispids[0]) : static_cast<long>(DISPID_UNKNOWN));
	const DISPID methodDispid = SUCCEEDED(result) ? dispids[0] : ExternalHelperMethodDispid(names[0]);
	const bool firstFailure = FAILED(result) && RecordFirstFailure(g_failedNameLookups, g_uniqueLookupFailures, methodDispid, result);
	const bool logLookup = (FAILED(result) && firstFailure) || IsExternalTraceVerbose() || (SUCCEEDED(result) && RecordFirstCall(g_successfulNameLookups, dispids[0]));
	if (logLookup) StartupTrace::HResult(L"external", L"XH120", result, details);
	return result;
}
HRESULT ExternalHelper::Invoke(DISPID dispid, REFIID riid, LCID lcid, WORD flags, DISPPARAMS* parameters, VARIANT* resultValue, EXCEPINFO* exceptionInfo, UINT* argumentError)
{
	if (riid != IID_NULL)
		return DISP_E_UNKNOWNINTERFACE;
	if (argumentError)
		*argumentError = UINT_MAX;

	const bool trace = IsLoadDiagnosticMethod(dispid);
	const bool knownMethod = wcscmp(ExternalHelperMethodName(dispid), L"other") != 0;
	const bool verbose = IsExternalTraceVerbose();
	const bool logInvokeBegin = trace && verbose;
	CString begin;
	begin.Format(L"dispid=%ld; method=%s; flags=0x%04X; lcid=%lu; args=%u; types=[%s]", static_cast<long>(dispid), ExternalHelperMethodName(dispid), flags, lcid, parameters ? parameters->cArgs : 0, (LPCWSTR)ExternalHelperArgumentTypes(parameters));
	if (logInvokeBegin)
		StartupTrace::Event(L"external", L"XH130", begin);

	CComPtr<ITypeInfo> typeInfo;
	HRESULT result = GetEmbeddedTypeInfo(&typeInfo);
	if (SUCCEEDED(result))
		result = typeInfo->Invoke(static_cast<IExternalHelper*>(this), dispid, flags, parameters, resultValue, exceptionInfo, argumentError);

	CComPtr<IErrorInfo> errorInfo;
	if (FAILED(result))
		::GetErrorInfo(0, &errorInfo);
	if (exceptionInfo && exceptionInfo->pfnDeferredFillIn)
		exceptionInfo->pfnDeferredFillIn(exceptionInfo);

	const bool firstSuccessfulInvoke = SUCCEEDED(result) && knownMethod && RecordFirstCall(g_successfulInvokes, dispid);
	const bool firstFailure = FAILED(result) && RecordFirstFailure(g_failedInvokes, g_uniqueInvokeFailures, dispid, result);
	const bool logInvokeResult = (trace && (verbose || firstSuccessfulInvoke)) || firstFailure;
	if (logInvokeResult)
	{
		if (!logInvokeBegin && trace)
			StartupTrace::Event(L"external", L"XH130", begin);
		CString argumentText;
		if (!argumentError || *argumentError == UINT_MAX) argumentText = L"none";
		else argumentText.Format(L"%u", *argumentError);
		CString details;
		details.Format(L"dispid=%ld; method=%s; result-type=VT_%u; argument-error=%s; excep.wCode=%u; excep.scode=0x%08lX; excep.helpContext=%lu", static_cast<long>(dispid), ExternalHelperMethodName(dispid), resultValue ? static_cast<unsigned int>(V_VT(resultValue)) : VT_EMPTY, (LPCWSTR)argumentText, exceptionInfo ? exceptionInfo->wCode : 0, static_cast<unsigned long>(exceptionInfo ? exceptionInfo->scode : S_OK), exceptionInfo ? exceptionInfo->dwHelpContext : 0);
		if (exceptionInfo && exceptionInfo->bstrSource) details += L"; excep.source=" + StartupTrace::SanitizeExceptionText(exceptionInfo->bstrSource);
		if (exceptionInfo && exceptionInfo->bstrDescription) details += L"; excep.description=" + StartupTrace::SanitizeExceptionText(exceptionInfo->bstrDescription);
		if (exceptionInfo && exceptionInfo->bstrHelpFile) details += L"; excep.help=1";
		if (errorInfo)
		{
			BSTR source = NULL, description = NULL, help = NULL;
			DWORD helpContext = 0; GUID guid = {};
			errorInfo->GetGUID(&guid); errorInfo->GetSource(&source); errorInfo->GetDescription(&description); errorInfo->GetHelpFile(&help); errorInfo->GetHelpContext(&helpContext);
			details.AppendFormat(L"; errorInfo.guid=%08lX; errorInfo.source=%s; errorInfo.description=%s; errorInfo.help=%d; errorInfo.helpContext=%lu", guid.Data1, (LPCWSTR)StartupTrace::SanitizeExceptionText(source), (LPCWSTR)StartupTrace::SanitizeExceptionText(description), help ? 1 : 0, helpContext);
			::SysFreeString(source); ::SysFreeString(description); ::SysFreeString(help);
		}
		StartupTrace::HResult(L"external", FAILED(result) ? L"XH140" : L"XH131", result, details);
	}
	return result;
}
static CString GetCurrentDocumentFilePath(const CString* filename, const bool* namevalid)
{
	if (filename == NULL || namevalid == NULL || !*namevalid || filename->IsEmpty())
		return CString();

	return U::GetFullPathName(*filename);
}

HRESULT ExternalHelper::GetDocumentFilePath(BSTR* path)
{
	if (path == NULL)
		return E_POINTER;

	*path = GetCurrentDocumentFilePath(m_document_filename, m_document_namevalid).AllocSysString();
	return S_OK;
}

HRESULT ExternalHelper::GetDocumentFileName(BSTR* name)
{
	if (name == NULL)
		return E_POINTER;

	const CString path = GetCurrentDocumentFilePath(m_document_filename, m_document_namevalid);
	const int separator = path.ReverseFind(L'\\');
	const CString result = path.IsEmpty() ? CString() : (separator >= 0 ? path.Mid(separator + 1) : path);
	*name = result.AllocSysString();
	return S_OK;
}

HRESULT ExternalHelper::GetDocumentDirectory(BSTR* directory)
{
	if (directory == NULL)
		return E_POINTER;

	const CString path = GetCurrentDocumentFilePath(m_document_filename, m_document_namevalid);
	if (path.IsEmpty())
	{
		*directory = ::SysAllocString(L"");
		return S_OK;
	}

	const int separator = path.ReverseFind(L'\\');
	CString result;
	if (separator == 2 && path.GetLength() >= 3 && path[1] == L':')
		result = path.Left(3);
	else if (separator > 0)
		result = path.Left(separator);
	*directory = result.AllocSysString();
	return S_OK;
}
struct Lang
{
	CString id;
	CString text;
};

static CSimpleArray<CString> g_lang_groups;
static CSimpleArray<Lang> g_langs;

static void FillDescElements()
{
	g_desc_elements.RemoveAll();
	DescElement elem;
	elem.groupid = 1;
	wchar_t buf[MAX_LOAD_STRING + 1];
	if(FbeLoadString(_Module.GetResourceInstance(), IDS_DMS_TI, buf, MAX_LOAD_STRING))
		elem.text = buf;
	g_desc_elements.Add(L"ti_group", elem);
	if(FbeLoadString(_Module.GetResourceInstance(), IDS_DMS_GENRE_M, buf, MAX_LOAD_STRING))
		elem.text = buf;
	g_desc_elements.Add(L"ti_genre_match", elem);
	if(FbeLoadString(_Module.GetResourceInstance(), IDS_DMS_KW, buf, MAX_LOAD_STRING))
		elem.text = buf;
	g_desc_elements.Add(L"ti_kw", elem);
	if(FbeLoadString(_Module.GetResourceInstance(), IDS_DMS_AUTHOR, buf, MAX_LOAD_STRING))
		elem.text = buf;
	g_desc_elements.Add(L"ti_nic_mail_web", elem);
	elem.groupid = 2;
	if(FbeLoadString(_Module.GetResourceInstance(), IDS_DMS_DI, buf, MAX_LOAD_STRING))
		elem.text = buf;
	g_desc_elements.Add(L"di_group", elem);
	if(FbeLoadString(_Module.GetResourceInstance(), IDS_DMS_ID, buf, MAX_LOAD_STRING))
		elem.text = buf;
	g_desc_elements.Add(L"di_id", elem);
	elem.groupid = 0;
	if(FbeLoadString(_Module.GetResourceInstance(), IDS_DMS_STI, buf, MAX_LOAD_STRING))
		elem.text = buf;
	g_desc_elements.Add(L"sti_all", elem);
	if(FbeLoadString(_Module.GetResourceInstance(), IDS_DMS_CI, buf, MAX_LOAD_STRING))
		elem.text = buf;
	g_desc_elements.Add(L"ci_all", elem);
}

// genre list helper
static void LoadGenres()
{
	FILE *fp;
  CString file_name = _Settings.GetLocalizedGenresFileName();
  // Modification by Pilgrim 
  try{
	fp=_tfopen(U::GetProgDirFile(file_name), _T("rb"));
  }catch(...){
  }

  if(!fp){
	  U::MessageBox(MB_OK|MB_ICONERROR, IDR_MAINFRAME, IDS_GENRES_LIST_MSG, file_name);
	  return;
  }

  g_genre_groups.RemoveAll();
  g_genres.RemoveAll();

  char	  buffer[1024];
  while (fgets(buffer,sizeof(buffer),fp)) {
    int	  l=strlen(buffer);
    if (l>0 && buffer[l-1]=='\n') 
      buffer[--l]='\0';
    if (l>0 && buffer[l-1]=='\r')
      buffer[--l]='\0';

    if (buffer[0] && buffer[0]!=' ') {
	  CA2W tmp(buffer, 65001);
      CString name(tmp);
      name.Replace(_T("&"),_T("&&"));
      g_genre_groups.Add(name);
    } else {
      char  *p=strchr(buffer+1,' ');
      if (!p || p==buffer+1)
	continue;
      *p++='\0';
      Genre   g;
      g.groupid=g_genre_groups.GetSize()-1;
      g.id=buffer+1;
	  CA2W tmp(p, 65001);
      g.text.SetString(tmp);
      g.text.Replace(_T("&"),_T("&&"));
      g_genres.Add(g);
    }
  }
	fclose(fp);
}

static HMENU MakeGenresMenu()
{
	CMenu ret;
	ret.CreatePopupMenu();

	CMenu cur;
	int g=-1;
	for(int i=0; i < g_genres.GetSize(); ++i)
	{
		if(g_genres[i].groupid != g)
		{
			g = g_genres[i].groupid;
			cur.Detach();
			cur.CreatePopupMenu();
			ret.AppendMenu(MF_POPUP | MF_STRING, (UINT)(HMENU)cur, g_genre_groups[g]);
		}
	cur.AppendMenu(MF_STRING, i + MENU_BASE, g_genres[i].text);
	}
	cur.Detach();

	return ret.Detach();
}

static HMENU MakeDescComponentsMenu()
{
	CMenu ret;
	ret.CreatePopupMenu();

	CMenu cur;
	int g=-1;
	for (int i=0;i<g_desc_elements.GetSize();++i) 
	{
		const DescElement& descElement = g_desc_elements.GetValueAt(i);
		const CString& descKey = g_desc_elements.GetKeyAt(i);
		if(descElement.groupid==0)
		{
			ret.AppendMenu(MF_STRING,i+MENU_BASE,descElement.text);
			bool ext = _Settings.GetExtElementStyle(descKey);
			if(ext)
			{
				ret.CheckMenuItem(i+MENU_BASE, MF_CHECKED);
			}
			else
			{
				ret.CheckMenuItem(i+MENU_BASE, MF_UNCHECKED);
			}
			continue;
		}
		
		if (descElement.groupid!=g) 
		{
			g=descElement.groupid;
			cur.Detach();
			cur.CreatePopupMenu();
			ret.AppendMenu(MF_POPUP|MF_STRING,(UINT)(HMENU)cur,descElement.text);
			continue;
		}
		cur.AppendMenu(MF_STRING,i+MENU_BASE,descElement.text);
		bool ext = _Settings.GetExtElementStyle(descKey);
		if(ext)
		{
			cur.CheckMenuItem(i + MENU_BASE, MF_CHECKED);
		}
		else
		{
			cur.CheckMenuItem(i + MENU_BASE, MF_UNCHECKED);
		}
	}
	cur.Detach();

	return ret.Detach();
}

HRESULT ExternalHelper::GenrePopup(IDispatch *obj,LONG x,LONG y,BSTR *name)
{
	LoadGenres();
	CMenu popup;
	popup.Attach(MakeGenresMenu());
	if(popup)
	{
		UINT cmd = popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, x, y, ::GetActiveWindow());
		popup.DestroyMenu();
		cmd -= MENU_BASE;
		if(cmd < (UINT)g_genres.GetSize())
		{
			*name = g_genres[cmd].id.AllocSysString();
			return S_OK;
		}
	}
	*name = NULL;
	return S_OK;
}

// Modification by Pilgrim

// lang list helper
/*static void	    LoadLangs() {
	FILE	  *fp;
	try{
	  fp=_tfopen(U::GetProgDirFile(_T("languages.txt")),_T("rb"));
    }catch(...){
	}

	if(!fp){
		U::MessageBox(MB_OK|MB_ICONERROR,_T("FBE"),
			  _T("�� ���� ����� ����-������ ������ '%s'."),_T("languages.txt"));
		return;
	}

	g_lang_groups.RemoveAll();
	g_langs.RemoveAll();

	char	  buffer[1024];
	while (fgets(buffer,sizeof(buffer),fp)) {
		int	  l=strlen(buffer);
		if (l>0 && buffer[l-1]=='\n')
			buffer[--l]='\0';
		if (l>0 && buffer[l-1]=='\r')
			buffer[--l]='\0';

		char  *p=strchr(buffer+1,'|');
		if (!p || p==buffer+1)
			continue;
		*p++='\0';
		Lang   g;
		g.text=buffer;
		g.id=p;
		g.id.Replace(_T("&"),_T("&&"));
		g_langs.Add(g);
	}
	fclose(fp);
}*/

static CMenu MakeLangsMenu()
{
	CMenu cur;
	cur.CreatePopupMenu();

	for(int i = 0;i < g_langs.GetSize(); ++i)
	{
		cur.AppendMenu(MF_STRING, i + MENU_BASE, g_langs[i].text);
	}

	return cur.Detach();
}

static CMenu MakeExtendMenu()
{
	CMenu cur;
	cur.CreatePopupMenu();

	for (int i = 0; i < g_langs.GetSize(); ++i)
	{
		cur.AppendMenu(MF_STRING, i + MENU_BASE, g_langs[i].text);
	}

	return cur.Detach();
}

/*HRESULT	ExternalHelper::LangPopup(IDispatch *obj,LONG x,LONG y,BSTR *name) {
	LoadLangs();
	CMenu	  popup=MakeLangsMenu();
	if (popup) {
		UINT  cmd=popup.TrackPopupMenu(
			TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,
			x,y,::GetActiveWindow()
			);
		popup.DestroyMenu();
		cmd-=MENU_BASE;
		if (cmd<(UINT)g_langs.GetSize()) {
			*name=g_langs[cmd].id.AllocSysString();
			return S_OK;
		}
	}
	*name=NULL;
	return S_OK;
}

HRESULT	ExternalHelper::SrcLangPopup(IDispatch *obj,LONG x,LONG y,BSTR *name) {
	LoadLangs();
	CMenu	  popup=MakeLangsMenu();
	if (popup) {
		UINT  cmd=popup.TrackPopupMenu(
			TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,
			x,y,::GetActiveWindow()
			);
		popup.DestroyMenu();
		cmd-=MENU_BASE;
		if (cmd<(UINT)g_langs.GetSize()) {
			*name=g_langs[cmd].id.AllocSysString();
			return S_OK;
		}
	}
	*name=NULL;
	return S_OK;
}

HRESULT	ExternalHelper::STILangPopup(IDispatch *obj,LONG x,LONG y,BSTR *name) {
	LoadLangs();
	CMenu	  popup=MakeLangsMenu();
	if (popup) {
		UINT  cmd=popup.TrackPopupMenu(
			TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,
			x,y,::GetActiveWindow()
			);
		popup.DestroyMenu();
		cmd-=MENU_BASE;
		if (cmd<(UINT)g_langs.GetSize()) {
			*name=g_langs[cmd].id.AllocSysString();
			return S_OK;
		}
	}
	*name=NULL;
	return S_OK;
}

HRESULT	ExternalHelper::STISrcLangPopup(IDispatch *obj,LONG x,LONG y,BSTR *name) {
	LoadLangs();
	CMenu	  popup=MakeLangsMenu();
	if (popup) {
		UINT  cmd=popup.TrackPopupMenu(
			TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,
			x,y,::GetActiveWindow()
			);
		popup.DestroyMenu();
		cmd-=MENU_BASE;
		if (cmd<(UINT)g_langs.GetSize()) {
			*name=g_langs[cmd].id.AllocSysString();
			return S_OK;
		}
	}
	*name=NULL;
	return S_OK;
}*/

HRESULT ExternalHelper::DescShowMenu(IDispatch *obj, LONG x,LONG y, BSTR* element_id)
{
	FillDescElements();
	CMenu popup;
	popup.Attach(MakeDescComponentsMenu());
	if(popup)
	{
		UINT cmd = popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, x, y, ::GetActiveWindow());
		if(!cmd)
		{
			popup.DestroyMenu();
			return S_OK;
		}
		
		popup.DestroyMenu();
		cmd -= MENU_BASE;
		if(cmd < (UINT)g_desc_elements.GetSize()) 
		{
			DescElement elem = g_desc_elements.GetValueAt(cmd);
			*element_id = g_desc_elements.GetKeyAt(cmd).AllocSysString();
			return S_OK;
		}
	}

	return S_OK;
}
