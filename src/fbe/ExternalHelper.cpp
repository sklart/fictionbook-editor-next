#include "stdafx.h"
#include "resource.h"
#include "res1.h"

#include "utils.h"
#include "apputils.h"

#include "FBE.h"
#include "ExternalHelper.h"

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

static const wchar_t* ExternalHelperMethodName(DISPID dispid)
{
	switch (dispid) { case 5: return L"GetStylePath"; case 7: return L"InflateParagraphs"; case 8: return L"GetUUID"; case 12: return L"GetExtendedStyle"; case 19: return L"GetNBSP"; case 22: return L"GetProgramVersion"; case 29: return L"IsDiagnosticTraceEnabled"; case 30: return L"TraceScript"; default: return L"other"; }
}
static bool IsLoadDiagnosticMethod(DISPID dispid) { return dispid == 5 || dispid == 7 || dispid == 8 || dispid == 12 || dispid == 19 || dispid == 22 || dispid == 29 || dispid == 30; }
static CString ExternalHelperArgumentTypes(const DISPPARAMS* parameters)
{
	CString types;
	for (UINT index = 0; parameters && index < parameters->cArgs; ++index) { CString type; type.Format(L"VT_%u", static_cast<unsigned int>(V_VT(&parameters->rgvarg[index]))); if (!types.IsEmpty()) types += L","; types += type; }
	return types;
}
HRESULT ExternalHelper::GetTypeInfoCount(UINT* typeInfoCount)
{
	HRESULT result = IDispatchImpl<IExternalHelper, &IID_IExternalHelper>::GetTypeInfoCount(typeInfoCount); StartupTrace::HResult(L"external", L"XH100", result, L"GetTypeInfoCount"); return result;
}
HRESULT ExternalHelper::GetTypeInfo(UINT typeInfo, LCID lcid, ITypeInfo** resultTypeInfo)
{
	HRESULT result = IDispatchImpl<IExternalHelper, &IID_IExternalHelper>::GetTypeInfo(typeInfo, lcid, resultTypeInfo); CString details; details.Format(L"typeinfo=%u; lcid=%lu", typeInfo, lcid); StartupTrace::HResult(L"external", L"XH110", result, details); return result;
}
HRESULT ExternalHelper::GetIDsOfNames(REFIID riid, LPOLESTR* names, UINT nameCount, LCID lcid, DISPID* dispids)
{
	HRESULT result = IDispatchImpl<IExternalHelper, &IID_IExternalHelper>::GetIDsOfNames(riid, names, nameCount, lcid, dispids); CString details; details.Format(L"lcid=%lu; names=%u; method=%s; dispid=%ld", lcid, nameCount, (names && nameCount && names[0]) ? (LPCWSTR)StartupTrace::SanitizeLogText(names[0], 64) : L"-", (dispids && nameCount) ? static_cast<long>(dispids[0]) : static_cast<long>(DISPID_UNKNOWN)); StartupTrace::HResult(L"external", L"XH120", result, details); return result;
}
HRESULT ExternalHelper::Invoke(DISPID dispid, REFIID riid, LCID lcid, WORD flags, DISPPARAMS* parameters, VARIANT* resultValue, EXCEPINFO* exceptionInfo, UINT* argumentError)
{
	const bool trace = IsLoadDiagnosticMethod(dispid);
	if (trace) { CString details; details.Format(L"dispid=%ld; method=%s; flags=0x%04X; args=%u; types=[%s]", static_cast<long>(dispid), ExternalHelperMethodName(dispid), flags, parameters ? parameters->cArgs : 0, (LPCWSTR)ExternalHelperArgumentTypes(parameters)); StartupTrace::Event(L"external", L"XH130", details); }
	HRESULT callResult = IDispatchImpl<IExternalHelper, &IID_IExternalHelper>::Invoke(dispid, riid, lcid, flags, parameters, resultValue, exceptionInfo, argumentError);
	if (trace || FAILED(callResult)) { CString details; details.Format(L"dispid=%ld; method=%s; result-type=VT_%u; argument-error=%u", static_cast<long>(dispid), ExternalHelperMethodName(dispid), resultValue ? static_cast<unsigned int>(V_VT(resultValue)) : VT_EMPTY, argumentError ? *argumentError : UINT_MAX); StartupTrace::HResult(L"external", FAILED(callResult) ? L"XH140" : L"XH131", callResult, details); }
	return callResult;
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
