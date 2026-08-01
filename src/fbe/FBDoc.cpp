// Doc.cpp: implementation of the Doc class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include "res1.h"

#include "utils.h"
#include "apputils.h"

#include "FBDoc.h"
#include "Scintilla.h"
#include "Settings.h"
#include "ElementDescMnr.h"
#include "StartupTrace.h"
#include <new>
#include <vector>

extern CElementDescMnr _EDMnr;

extern CSettings _Settings;

namespace FB {

// Журнал не содержит имён и путей книг: для диагностики достаточно факта
// наличия файла и результата операции.
static void TraceDocumentEvent(const wchar_t* code, const wchar_t* operation, const CString& filename)
{
	CString trace;
	trace.Format(L"%s; file-present=%d", operation, filename.IsEmpty() ? 0 : 1);
	StartupTrace::Event(L"document", code, trace);
}

static void TraceRecoveryEvent(const wchar_t* code, const wchar_t* operation, const CString& filename)
{
	CString trace;
	trace.Format(L"%s; file-present=%d", operation, filename.IsEmpty() ? 0 : 1);
	StartupTrace::Event(L"recovery", code, trace);
}

static void TraceOptionalDiagnosticWarning(const wchar_t* code, HRESULT result, const wchar_t* operation)
{
	CString details;
	details.Format(L"hr=0x%08lX; %s unavailable", static_cast<unsigned long>(result), operation);
	StartupTrace::Warning(L"diagnostic", code, details);
}

static void TraceScriptStageSnapshot(const wchar_t* code, const wchar_t* name, BSTR stage)
{
	CString details;
	details.Format(L"%s=%s", name, (LPCWSTR)StartupTrace::SanitizeLogText(stage, 32));
	// Keep the actual J stage intact for crash diagnostics: this is a C++ query result.
	StartupTrace::Event(L"document", code, details);
}

static bool DispatchHasMember(IDispatch* dispatch, const wchar_t* name)
{
	if (!dispatch || !name || !*name) return false;
	LPOLESTR names[1] = { const_cast<LPOLESTR>(name) };
	DISPID dispid = DISPID_UNKNOWN;
	return SUCCEEDED(dispatch->GetIDsOfNames(IID_NULL, names, 1, LOCALE_SYSTEM_DEFAULT, &dispid));
}

static void TraceHtmlDocumentState(MSHTML::IHTMLDocument2Ptr document)
{
	try
	{
		if (!document)
		{
			StartupTrace::Error(L"webbrowser", L"WB300", L"HTML document unavailable");
			return;
		}
		MSHTML::IHTMLDocument4Ptr document4(document);
		_bstr_t url(document4 ? document4->URLUnencoded : L"");
		_bstr_t readyState(document->readyState);
		MSHTML::IHTMLDocument5Ptr document5(document);
		MSHTML::IHTMLDocument6Ptr document6(document);
		_bstr_t compatMode(document5 ? document5->compatMode : L"(unknown)");
		_variant_t documentModeValue(document6 ? document6->documentMode : _variant_t());
		long documentMode = documentModeValue.vt == VT_I4 ? documentModeValue.lVal : -1;
		_bstr_t charset(document->charset);
		MSHTML::IHTMLDocument3Ptr document3(document);
		const bool hasBody = (bool)document->body;
		const bool hasCss = document3 && (bool)document3->getElementById(L"css");
		const bool hasFbwDesc = document3 && (bool)document3->getElementById(L"fbw_desc");
		const bool hasFbwBody = document3 && (bool)document3->getElementById(L"fbw_body");
		const bool hasUserCmd = document3 && (bool)document3->getElementById(L"userCmd");
		MSHTML::IHTMLWindow2Ptr window(document->parentWindow);
		MSHTML::IOmNavigatorPtr navigator = window ? window->navigator : NULL;
		_bstr_t userAgent(navigator ? navigator->userAgent : L"");
		_bstr_t appVersion(navigator ? navigator->appVersion : L"");
		IDispatchPtr external = window ? window->external : NULL;
		UINT externalTypeInfoCount = 0;
		const HRESULT externalTypeInfoResult = external ? external->GetTypeInfoCount(&externalTypeInfoCount) : E_NOINTERFACE;
		const wchar_t* const externalType = external ? L"object" : L"undefined";
		IDispatchPtr script(MSHTML::IHTMLDocumentPtr(document)->Script);
		const bool hasApiLoadFB2 = DispatchHasMember(script, L"apiLoadFB2");
		const bool hasApiSetDiagnosticTrace = DispatchHasMember(script, L"apiSetDiagnosticTraceEnabled");
		const bool hasApiOperationStage = DispatchHasMember(script, L"apiGetDiagnosticOperationStage");
		const bool hasApiFailureStage = DispatchHasMember(script, L"apiGetDiagnosticFailureStage");
		const bool hasApiBridgeState = DispatchHasMember(script, L"apiGetDiagnosticTraceBridgeState");
		CString trace;
		trace.Format(L"url=%s; ready-state=%s; document-mode=%ld; compat-mode=%s; charset=%s; body=%d; css=%d; fbw_desc=%d; fbw_body=%d; userCmd=%d; user-agent=%s; app-version=%s; external=%d; typeof-window.external=%s; external-typeinfo=%u; external-typeinfo-hr=0x%08lX; apiLoadFB2=%d; apiSetDiagnosticTraceEnabled=%d; apiGetDiagnosticOperationStage=%d; apiGetDiagnosticFailureStage=%d; apiGetDiagnosticTraceBridgeState=%d",
			(LPCWSTR)StartupTrace::RedactPath((const wchar_t*)url), (const wchar_t*)readyState,
			documentMode, (const wchar_t*)compatMode, (const wchar_t*)charset,
			hasBody ? 1 : 0, hasCss ? 1 : 0, hasFbwDesc ? 1 : 0, hasFbwBody ? 1 : 0, hasUserCmd ? 1 : 0, (LPCWSTR)StartupTrace::SanitizeLogText((const wchar_t*)userAgent, 256), (LPCWSTR)StartupTrace::SanitizeLogText((const wchar_t*)appVersion, 256),
			external ? 1 : 0, externalType, externalTypeInfoCount, static_cast<unsigned long>(externalTypeInfoResult), hasApiLoadFB2 ? 1 : 0,
			hasApiSetDiagnosticTrace ? 1 : 0, hasApiOperationStage ? 1 : 0, hasApiFailureStage ? 1 : 0, hasApiBridgeState ? 1 : 0);
		StartupTrace::Event(L"webbrowser", L"WB310", trace);
	}
	catch (_com_error& error)
	{
		StartupTrace::HResult(L"webbrowser", L"WB320", error.Error(), L"HTML document state");
	}
}// namespaces
const _bstr_t	  FBNS(L"http://www.gribuser.ru/xml/fictionbook/2.0");
const _bstr_t	  XLINKNS(L"http://www.w3.org/1999/xlink");
const _bstr_t	  NEWLINE(L"\n");

// document list
CSimpleMap<Doc*,Doc*>	Doc::m_active_docs;
Doc* FB::Doc::m_active_doc;
bool FB::Doc::m_fast_mode;

Doc   *Doc::LocateDocument(const wchar_t *id) {
  unsigned long    lv;
  if (swscanf(id,L"%lu",&lv)!=1)
    return NULL;
  return m_active_docs.Lookup((Doc*)lv);
}

// initialize a new Doc
Doc::Doc(HWND hWndFrame) :
	     m_filename(_T("Untitled.fb2")), m_namevalid(false),
//	     m_desc(hWndFrame,false),
		 m_body(hWndFrame, true),
	     m_frame(hWndFrame),
//		 m_desc_ver(-1),
		 m_body_ver(-1),
//	     m_desc_cp(-1),
		 m_body_cp(-1),
	     m_encoding(_T("utf-8")),
	     m_last_save_error(S_OK)
{
  m_body.SetDocumentFilePathSource(&m_filename, &m_namevalid);
  m_active_docs.Add(this,this);
}

// destroy a Doc
Doc::~Doc() {
  // destroy windows explicitly
//  if (m_desc.IsWindow())
//    m_desc.DestroyWindow();
  if (m_body.IsWindow())
    m_body.DestroyWindow();
  m_active_docs.Remove(this);
}

bool  Doc::GetBinary(const wchar_t *id,_variant_t& vt) {
  if (id && *id==L'#') {
	  CComDispatchDriver	    body(m_body.Script());
    _variant_t	  vid(id+1);
    body.Invoke1(L"apiGetBinary",&vid,&vt);
    return true;
  }
  return false;
}

struct ThreadArgs {
  MSXML2::IXSLProcessor	*proc;
  HANDLE		hWr;
};

static DWORD __stdcall XMLTransformThread(LPVOID varg) {
  ThreadArgs			*arg=(ThreadArgs*)varg;

  arg->proc->put_output(_variant_t(U::NewStream(arg->hWr)));
  VARIANT_BOOL	val;
  arg->proc->raw_transform(&val);
  arg->proc->Release();

  delete arg;

  return 0;
}

void Doc::TransformXML(MSXML2::IXSLTemplatePtr tp,MSXML2::IXMLDOMDocument2Ptr doc,
    CFBEView& dest)
{
  StartupTrace::Event(L"xslt", L"T400", L"XML transform for view started");
  // create processor
  MSXML2::IXSLProcessorPtr	proc(tp->createProcessor());
  proc->input=_variant_t(doc.GetInterfacePtr());

  // add parameters
  CString	  fss(_Settings.GetFont());
  if (!fss.IsEmpty())
    proc->addParameter(L"font",(const wchar_t *)fss,_bstr_t());

  DWORD		  fs = _Settings.GetFontSize();
  if (fs>1) {
    fss.Format(_T("%d"), static_cast<int>(fs));
    proc->addParameter(L"fontSize",(const wchar_t *)fss,_bstr_t());
  }

  fs = _Settings.GetColorFG();
  if (fs!=CLR_DEFAULT) {
    fss.Format(_T("rgb(%d,%d,%d)"),GetRValue(fs),GetGValue(fs),GetBValue(fs));
    proc->addParameter(L"colorFG",(const wchar_t *)fss,_bstr_t());
  }

  fs = _Settings.GetColorBG();
  if (fs!=CLR_DEFAULT) {
    fss.Format(_T("rgb(%d,%d,%d)"),GetRValue(fs),GetGValue(fs),GetBValue(fs));
    proc->addParameter(L"colorBG",(const wchar_t *)fss,_bstr_t());
  }

  fs=::GetSysColor(COLOR_BTNFACE);
  fss.Format(_T("rgb(%d,%d,%d)"),GetRValue(fs),GetGValue(fs),GetBValue(fs));
  proc->addParameter(L"dlgBG",(const wchar_t *)fss,_bstr_t());

  fs=::GetSysColor(COLOR_BTNTEXT);
  fss.Format(_T("rgb(%d,%d,%d)"),GetRValue(fs),GetGValue(fs),GetBValue(fs));
  proc->addParameter(L"dlgFG",(const wchar_t *)fss,_bstr_t());

  proc->addParameter(L"dID",(const wchar_t *)MyID(),_bstr_t());

  // add jscript paths
  proc->addParameter(L"bodyscript",(const wchar_t *)U::UrlFromPath(U::GetProgDirFile(_T("body.js"))),_bstr_t());
  proc->addParameter(L"descscript",(const wchar_t *)U::UrlFromPath(U::GetProgDirFile(_T("desc.js"))),_bstr_t());

  ThreadArgs	*arg=new ThreadArgs;

  // pass the processor to worker thread, this is a dirty hack, but we know that
  // MSXML survives it, otherwise we'd have to jump through the hoops with
  // COM MTAs, because if we just marshal the pointer to worker thread, then
  // we'll deadlock in load. To do everything cleanly, we'd need to create the
  // XSL processor and XML documents in an MTA, and run the transforms there,
  // all this is rather awkward. Another alternative is to transform all XML
  // to memory and the load from it
  arg->proc=proc.Detach();

  // setup the streams
  HANDLE	  hRd = NULL;
  if (!::CreatePipe(&hRd,&arg->hWr,NULL,0)) {
    arg->proc->Release();
    delete arg;
    throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
  }

  // start the processor
  HANDLE transformThread = ::CreateThread(NULL,0,XMLTransformThread,arg,0,NULL);
  if (!transformThread) {
    ::CloseHandle(hRd);
    ::CloseHandle(arg->hWr);
    arg->proc->Release();
    delete arg;
    throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
  }
  ::CloseHandle(transformThread);

  // now stuff the data into mshtml
  IPersistStreamInitPtr	ips(dest.Browser()->Document);
  ips->InitNew();
  ips->Load(U::NewStream(hRd));
  StartupTrace::Event(L"xslt", L"T490", L"XML transform for view completed");
}

static MSXML2::IXSLTemplatePtr	LoadXSL(const CString& path) {
  CString trace;
  trace.Format(L"XSL load; template=%s", path.CompareNoCase(L"body.xsl") == 0 ? L"body" : L"description");
  StartupTrace::Event(L"xslt", L"T300", trace);
  MSXML2::IXMLDOMDocument2Ptr	xsl(U::CreateDocument(true));
  if (!U::LoadXml(xsl,U::GetProgDirFile(path)))
  {
    StartupTrace::Error(L"xslt", L"T301", L"XSL load failed");
    throw _com_error(E_FAIL);
  }
  MSXML2::IXSLTemplatePtr	tp(U::CreateTemplate());
  tp->stylesheet=xsl;
  StartupTrace::Event(L"xslt", L"T390", L"XSL template prepared");
  return tp;
}

// loading
/*bool	Doc::LoadFromDOM(HWND hWndParent,MSXML2::IXMLDOMDocument2 *dom) {
  try {
    dom->setProperty(L"SelectionLanguage",L"XPath");
    CString   nsprop(L"xmlns:fb='");
    nsprop+=(const wchar_t *)FBNS;
    nsprop+=L"' xmlns:xlink='";
    nsprop+=(const wchar_t *)XLINKNS;
    nsprop+=L"'";
    dom->setProperty(L"SelectionNamespaces",(const TCHAR *)nsprop);

    // try to find out current encoding
    MSXML2::IXMLDOMProcessingInstructionPtr   pi(dom->firstChild);
    if (pi) {
      MSXML2::IXMLDOMNamedNodeMapPtr  attr(pi->attributes);
      if (attr) {
	MSXML2::IXMLDOMNodePtr	enc(attr->getNamedItem(L"encoding"));
	if (enc)
	  m_encoding=(const wchar_t *)enc->text;
      }
    }

    // create desc view
    m_desc.Create(hWndParent, CRect(0,0,500,500), _T("{8856F961-340A-11D0-A96B-00C04FD705A2}"));

    // navigate to blank page
    m_desc.Browser()->Navigate(L"about:blank");

    // run a message loop until it loads
    MSG	  msg;
    while (!m_desc.Loaded() && ::GetMessage(&msg,NULL,0,0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    // transform to html
    TransformXML(LoadXSL(_T("description.xsl")),dom,m_desc);

    // wait until it loads
    while (!m_desc.Loaded() && ::GetMessage(&msg,NULL,0,0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    // initialize view
    m_desc.Init();

    // store binaries
    CComDispatchDriver	  desc(m_desc.Script());
    _variant_t	    arg(dom);
    desc.Invoke1(L"PutBinaries",&arg);

    // create body view
    m_body.Create(hWndParent, CRect(0,0,500,500), _T("{8856F961-340A-11D0-A96B-00C04FD705A2}"));

    // navigate body browser
    m_body.Browser()->Navigate(L"about:blank");

    // wait until it loads
    while (!m_body.Loaded() && ::GetMessage(&msg,NULL,0,0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    // transform to html
    TransformXML(LoadXSL(_T("body.xsl")),dom,m_body);

    // wait until it loads
    while (!m_body.Loaded() && ::GetMessage(&msg,NULL,0,0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    // initialize view
    m_body.Init();

    // mark unchanged
    MarkSavePoint();

    // ok, now setup filename and return
    m_filename=_T("Untitled");
    m_namevalid=false;
  }
  catch (_com_error& e) {
    U::ReportError(e);
    return false;
  }

  return true;
}*/

static CString VariantTypeName(const VARIANT& value)
{
	const VARTYPE variantType = V_VT(&value);
	CString type;
	type.Format(L"VT_%u", static_cast<unsigned int>(variantType & VT_TYPEMASK));
	if (variantType & VT_BYREF) type += L"|VT_BYREF";
	if (variantType & VT_ARRAY) type += L"|VT_ARRAY";
	return type;
}

HRESULT Doc::InvokeFunc(LPCOLESTR FuncName, CComVariant *params, int count, CComVariant &vtResult)
{
	CString trace;
	trace.Format(L"InvokeFunc: %s; arguments=%d", FuncName, count);
	StartupTrace::Event(L"script", L"C100", trace);

	if (!m_body.Browser())
	{
		StartupTrace::HResult(L"script", L"C101", E_UNEXPECTED, L"web browser unavailable");
		return E_UNEXPECTED;
	}

	IHTMLDocument2Ptr doc = m_body.Browser()->Document;
	if (!doc)
	{
		StartupTrace::HResult(L"script", L"C102", E_NOINTERFACE, L"HTML document unavailable");
		return E_NOINTERFACE;
	}

	CComPtr<IDispatch> pScript;
	HRESULT hr = doc->get_Script(&pScript);
	StartupTrace::HResult(L"script", L"C110", hr, L"get_Script");
	if (FAILED(hr) || !pScript)
		return FAILED(hr) ? hr : E_NOINTERFACE;

	LPOLESTR szMember = const_cast<LPOLESTR>(FuncName);
	DISPID dispid = DISPID_UNKNOWN;
	hr = pScript->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_SYSTEM_DEFAULT, &dispid);
	trace.Format(L"GetIDsOfNames: name=%s; dispid=%ld", FuncName, static_cast<long>(dispid));
	StartupTrace::HResult(L"script", L"C120", hr, trace);
	if (FAILED(hr))
		return hr;

	CString argumentTypes;
	for (int index = 0; index < count; ++index)
	{
		if (!argumentTypes.IsEmpty())
			argumentTypes += L",";
		argumentTypes += VariantTypeName(params[index]);
	}
	trace.Format(L"Invoke: dispid=%ld; argument-types=[%s]", static_cast<long>(dispid),
		(const wchar_t*)argumentTypes);
	StartupTrace::Event(L"script", L"C130", trace);

	DISPPARAMS dispatchParameters = {};
	dispatchParameters.rgvarg = params;
	dispatchParameters.cArgs = count;
	EXCEPINFO exceptionInfo = {};
	UINT argumentError = UINT_MAX;
	::VariantClear(&vtResult);
	::VariantInit(&vtResult);
	hr = pScript->Invoke(dispid, IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_METHOD,
		&dispatchParameters, &vtResult, &exceptionInfo, &argumentError);
	CComPtr<IErrorInfo> errorInfo;
	if (FAILED(hr))
		::GetErrorInfo(0, &errorInfo);
	if (exceptionInfo.pfnDeferredFillIn)
		exceptionInfo.pfnDeferredFillIn(&exceptionInfo);

	CString details;
	CString argumentText;
	if(argumentError == UINT_MAX) argumentText = L"none";
	else argumentText.Format(L"%u", argumentError);
	details.Format(L"Invoke: dispid=%ld; argument-error=%s; result-type=VT_%u",
		static_cast<long>(dispid), (LPCWSTR)argumentText, static_cast<unsigned int>(V_VT(&vtResult)));
	if (exceptionInfo.bstrDescription)
		details += L"; excep.description=" + StartupTrace::SanitizeExceptionText(exceptionInfo.bstrDescription);
	if (exceptionInfo.bstrSource)
		details += L"; excep.source=" + StartupTrace::SanitizeExceptionText(exceptionInfo.bstrSource);
	details.AppendFormat(L"; excep.wCode=%u; excep.scode=0x%08lX; excep.help=%d; excep.helpContext=%lu; excep.deferred=%d",
		exceptionInfo.wCode, static_cast<unsigned long>(exceptionInfo.scode), exceptionInfo.bstrHelpFile ? 1 : 0,
		exceptionInfo.dwHelpContext, exceptionInfo.pfnDeferredFillIn ? 1 : 0);
	if (errorInfo)
	{
		BSTR source = NULL, description = NULL, help = NULL; DWORD context = 0; GUID guid = {};
		errorInfo->GetGUID(&guid); errorInfo->GetSource(&source); errorInfo->GetDescription(&description);
		errorInfo->GetHelpFile(&help); errorInfo->GetHelpContext(&context);
		details.AppendFormat(L"; errorInfo.guid=%08lX; errorInfo.source=%s; errorInfo.description=%s; errorInfo.help=%d; errorInfo.helpContext=%lu",
			guid.Data1, (LPCWSTR)StartupTrace::SanitizeExceptionText(source), (LPCWSTR)StartupTrace::SanitizeExceptionText(description), help ? 1 : 0, context);
		::SysFreeString(source); ::SysFreeString(description); ::SysFreeString(help);
	}
	if (exceptionInfo.bstrDescription) ::SysFreeString(exceptionInfo.bstrDescription);
	if (exceptionInfo.bstrSource) ::SysFreeString(exceptionInfo.bstrSource);
	if (exceptionInfo.bstrHelpFile) ::SysFreeString(exceptionInfo.bstrHelpFile);
	StartupTrace::HResult(L"script", FAILED(hr) ? L"C140" : L"C131", hr, details);
	return hr;
}

void Doc::ShowDescription(bool Show)
{
	CComVariant vtResult;
	CComVariant params;
	V_VT(&params) = VT_BOOL;
	V_BOOL(&params) = Show ? VARIANT_TRUE : VARIANT_FALSE;
	InvokeFunc(L"apiShowDesc", &params, 1, vtResult);
	return;
}

void Doc::RunScript(LPCOLESTR filePath)
{
	TraceDocumentEvent(L"D300", L"script execution started", CString(filePath));
	CComVariant vtResult;

	// Пользовательский набор может лежать вне штатного runtime. В этом случае
	// вспомогательные HTML-окна должны искаться рядом с выбранной папкой Scripts.
	CString htmlFolder = _Settings.GetScriptsFolder();
	htmlFolder.TrimRight(L"\\/");
	int separator = htmlFolder.ReverseFind(L'\\');
	const int slash = htmlFolder.ReverseFind(L'/');
	if (slash > separator)
		separator = slash;
	if (separator >= 0)
		htmlFolder = htmlFolder.Left(separator + 1) + L"HTML\\";
	else
		htmlFolder.Empty();

	const DWORD attributes = htmlFolder.IsEmpty() ? INVALID_FILE_ATTRIBUTES : ::GetFileAttributes(htmlFolder);
	if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		htmlFolder = U::GetProgDirFile(L"HTML\\");
	StartupTrace::Event(L"script", L"C200", L"script HTML folder selected");

	// InvokeN передаёт аргументы в обратном порядке.
	CComVariant params[2];
	params[1] = filePath;
	params[0] = htmlFolder;
	InvokeFunc(L"apiRunCmd", params, 2, vtResult);
	TraceDocumentEvent(L"D301", L"script execution completed", CString(filePath));
}
VARIANT_BOOL Doc::CheckScript(LPCOLESTR filePath)
{
	CComVariant vtResult;
	CComVariant params(filePath);
	InvokeFunc(L"apiCheckScript", &params, 1, vtResult);

	return vtResult.boolVal;
}

bool Doc::LoadFromHTML(HWND hWndParent,const CString& filename)
{
	TraceDocumentEvent(L"D110", L"book load started", filename);
	HRESULT	hr;
	StartupTrace::Event(L"webbrowser", L"WB100", L"m_body.Create begin");
	const CString path = U::GetProgDirFile(L"main.html");
	const HWND browserWindow = m_body.Create(hWndParent, CRect(0,0,500,500), _T("{8856F961-340A-11D0-A96B-00C04FD705A2}"));
	if (!browserWindow)
	{
		StartupTrace::HResult(L"webbrowser", L"WB101", HRESULT_FROM_WIN32(::GetLastError()), L"m_body.Create returned no HWND");
		return false;
	}
	if (!m_body.Browser())
	{
		StartupTrace::Error(L"webbrowser", L"WB102", L"m_body.Create did not provide IWebBrowser2");
		return false;
	}
	StartupTrace::Event(L"webbrowser", L"WB110", L"IWebBrowser2 available");
	m_body.BeginNavigationTrace();
	hr = m_body.Browser()->Navigate((LPCTSTR)path);
	StartupTrace::HResult(L"webbrowser", L"WB120", hr, L"Navigate main.html");
	if (FAILED(hr))
		return false;
	MSG msg;
	const ULONGLONG navigationStarted = ::GetTickCount64();
	const DWORD documentCompleteTimeoutMs = 30000;
	StartupTrace::Event(L"webbrowser", L"WB130", L"waiting for DocumentComplete");
	while (!m_body.Loaded())
	{
		const ULONGLONG elapsed = ::GetTickCount64() - navigationStarted;
		if (elapsed >= documentCompleteTimeoutMs)
		{
			CString details; details.Format(L"elapsed=%llu; document-present=%d; last-browser-event=%s; url=%s", elapsed, m_body.HasDoc() ? 1 : 0, (LPCWSTR)m_body.LastBrowserEvent(), (LPCWSTR)StartupTrace::RedactPath(m_body.NavURL()));
			StartupTrace::Warning(L"webbrowser", L"WB133", details);
			return false;
		}
		const DWORD waitResult = ::MsgWaitForMultipleObjects(0, NULL, FALSE, static_cast<DWORD>(documentCompleteTimeoutMs - elapsed), QS_ALLINPUT);
		if (waitResult == WAIT_TIMEOUT)
			continue;
		if (waitResult == WAIT_FAILED)
		{
			StartupTrace::HResult(L"webbrowser", L"WB131", HRESULT_FROM_WIN32(::GetLastError()), L"MsgWaitForMultipleObjects failed");
			return false;
		}
		const int messageResult = ::GetMessage(&msg, NULL, 0, 0);
		if (messageResult == 0)
		{
			StartupTrace::Warning(L"webbrowser", L"WB132", L"message loop ended before DocumentComplete");
			return false;
		}
		if (messageResult == -1)
		{
			StartupTrace::HResult(L"webbrowser", L"WB131", HRESULT_FROM_WIN32(::GetLastError()), L"GetMessage failed");
			return false;
		}
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	StartupTrace::Event(L"webbrowser", L"WB150", L"SetExternalDispatch #1");
	m_body.SetExternalDispatch(m_body.CreateHelper());

	if (!m_body.Init())
	{
		StartupTrace::Error(L"webbrowser", L"WB298", L"CFBEView::Init failed");
		return false;
	}
	StartupTrace::Event(L"webbrowser", L"WB199", L"browser ready");
	TraceHtmlDocumentState(m_body.Browser()->Document);
	//FastMode();

	CComVariant params[2];
	params[1] = filename;
	params[0] = _Settings.GetInterfaceLanguageName();
	CComVariant res;

	const bool diagnosticsActive = StartupTrace::Enabled();
	bool diagnosticBridgeUnavailable = false;
	if (diagnosticsActive)
	{
	CComVariant diagnosticTrace;
	V_VT(&diagnosticTrace) = VT_BOOL;
	V_BOOL(&diagnosticTrace) = StartupTrace::Enabled() ? VARIANT_TRUE : VARIANT_FALSE;
	CComVariant diagnosticResult;
	hr = InvokeFunc(L"apiSetDiagnosticTraceEnabled", &diagnosticTrace, 1, diagnosticResult);
		if (FAILED(hr))
	{
		// The diagnostic API was introduced after the original runtime. Its
		// absence (including DISP_E_UNKNOWNNAME) must never prevent a book from
		// opening with an older main.js.
		TraceOptionalDiagnosticWarning(L"J011", hr, L"apiSetDiagnosticTraceEnabled");
	}
		else
		{
			StartupTrace::HResult(L"script", L"J010", hr, L"apiSetDiagnosticTraceEnabled");
			CComVariant bridgeState;
			const HRESULT bridgeResult = InvokeFunc(L"apiGetDiagnosticTraceBridgeState", NULL, 0, bridgeState);
			if (SUCCEEDED(bridgeResult) && V_VT(&bridgeState) == VT_I4)
			{
				const long state = V_I4(&bridgeState);
				diagnosticBridgeUnavailable = state == -1;
				StartupTrace::Event(L"script", L"J012", state == 1 ? L"diagnostic trace bridge=available" :
					state == -1 ? L"diagnostic trace bridge=unavailable" : L"diagnostic trace bridge=unknown");
			}
			else
			{
				TraceOptionalDiagnosticWarning(L"J013", bridgeResult, L"apiGetDiagnosticTraceBridgeState");
			}
		}


	}
	ApplyConfChanges();
	StartupTrace::Event(L"document", L"J100", L"apiLoadFB2 begin");
	hr = InvokeFunc(L"apiLoadFB2", params, 2, res);
	StartupTrace::HResult(L"document", L"J200", hr, L"apiLoadFB2");
	if (FAILED(hr))
	{
		if (diagnosticsActive)
		{
		// Preserve the original apiLoadFB2 HRESULT. This extra query is diagnostic only.
		CComVariant lastStage;
		const HRESULT stageResult = InvokeFunc(L"apiGetDiagnosticFailureStage", NULL, 0, lastStage);
		if (SUCCEEDED(stageResult) && V_VT(&lastStage) == VT_BSTR)
			TraceScriptStageSnapshot(L"D115", L"failure-stage", V_BSTR(&lastStage));
		else
			TraceOptionalDiagnosticWarning(L"J115", stageResult, L"apiGetDiagnosticFailureStage");

		}
		TraceDocumentEvent(L"D111", L"book load JavaScript failure", filename);
		return false;
	}
	//m_body.Normalize(m_body.Document()->body);
	bool loaded = false;
	if(res.vt == VT_BOOL)
	{
		m_encoding = _Settings.GetDefaultEncoding();
		loaded = res.boolVal != VARIANT_FALSE;
	}
	else if(res.vt == VT_BSTR)
	{
		m_encoding = res.bstrVal;
		loaded = true;
	}
	else if(res.vt == VT_EMPTY)
	{
		// VT_EMPTY допустим только после успешного Invoke, что уже проверено выше.
		m_encoding = _Settings.GetDefaultEncoding();
		loaded = true;
	}

	if (!loaded)
	{
		if (diagnosticsActive)
		{
			CComVariant operationStage;
			const HRESULT stageResult = InvokeFunc(L"apiGetDiagnosticOperationStage", NULL, 0, operationStage);
			if (SUCCEEDED(stageResult) && V_VT(&operationStage) == VT_BSTR)
				TraceScriptStageSnapshot(L"D116", L"operation-stage", V_BSTR(&operationStage));
			else
				TraceOptionalDiagnosticWarning(L"J117", stageResult, L"apiGetDiagnosticOperationStage");
		}
		TraceDocumentEvent(L"D112", L"book load returned false", filename);
		return false;
	}

	if (diagnosticsActive)
	{
	CComVariant postLoadBridgeState;
		const HRESULT postLoadBridgeResult = InvokeFunc(L"apiGetDiagnosticTraceBridgeState", NULL, 0, postLoadBridgeState);
		if (SUCCEEDED(postLoadBridgeResult) && V_VT(&postLoadBridgeState) == VT_I4)
			diagnosticBridgeUnavailable = diagnosticBridgeUnavailable || V_I4(&postLoadBridgeState) == -1;
		if (diagnosticBridgeUnavailable)
		{
			CComVariant lastStage;
			const HRESULT stageResult = InvokeFunc(L"apiGetDiagnosticOperationStage", NULL, 0, lastStage);
			if (SUCCEEDED(stageResult) && V_VT(&lastStage) == VT_BSTR)
				TraceScriptStageSnapshot(L"D117", L"operation-stage", V_BSTR(&lastStage));
			else
				TraceOptionalDiagnosticWarning(L"J119", stageResult, L"apiGetDiagnosticOperationStage");
		}


	}

	// Отмечаем документ неизменённым только после подтверждённой загрузки JavaScript.
	MarkSavePoint();
	TraceDocumentEvent(L"D113", L"book load completed", filename);
	return true;
}

bool Doc::Load(HWND hWndParent,const CString& filename) {

 try {
 //   AU::CPersistentWaitCursor wc;

    // load document into DOM
    /*MSXML2::IXMLDOMDocument2Ptr	dom(U::CreateDocument(true));
    if (!U::LoadXml(dom,filename))
      return false;

    if (!LoadFromDOM(hWndParent,dom))
      return false;*/

	CWaitCursor hourglass;
	if(!LoadFromHTML(hWndParent, filename))
	{
		return false;
	}

    m_filename = filename;
    m_namevalid = true;
  }
  catch (_com_error& e) {
	TraceDocumentEvent(L"D114", L"book load COM failure", filename);
    U::ReportError(e);
    return false;
  }

  return true;
}

void  Doc::CreateBlank(HWND hWndParent) {
  try {
	TraceDocumentEvent(L"D120", L"blank book creation", L"blank.fb2");
    // load document into DOM
	  if (!LoadFromHTML(hWndParent, L"blank.fb2"))
	  StartupTrace::Error(L"document", L"D201", L"blank book was not loaded");
    //LoadFromDOM(hWndParent,U::CreateDocument(true));
  }
  catch (_com_error& e) {
    U::ReportError(e);
  }
}

// indent something
static void Indent(MSXML2::IXMLDOMNode *node, MSXML2::IXMLDOMDocument2 *xml, int len)
{
	// inefficient
	CStringW indent(L"\r\n");
	for (int i = 0; i < len; ++i)
		indent.AppendChar(L' ');
	MSXML2::IXMLDOMTextPtr text;
	if(SUCCEEDED(xml->raw_createTextNode(_bstr_t(indent), &text)))
		node->raw_appendChild(text, NULL);
}

// MSXML форматирует значение узла bin.base64 переводами строк. Для FB2 это
// допустимо, но заметно раздувает книги с большим количеством иллюстраций.
// После получения двоичных данных из редактора сохраняем тот же base64 как
// обычный текст без разделяющих пробельных символов.
static void CompactBinaryTextContent(MSXML2::IXMLDOMDocument2Ptr document)
{
	MSXML2::IXMLDOMNodeListPtr binaries = document->selectNodes(
		_bstr_t(L"/fb:FictionBook/fb:binary"));
	if (binaries == NULL)
		return;

	const long count = binaries->length;
	for (long index = 0; index < count; ++index)
	{
		MSXML2::IXMLDOMNodePtr binary = binaries->item[index];
		if (binary == NULL)
			continue;

		MSXML2::IXMLDOMElementPtr binaryElement(binary);
		if (binaryElement != NULL)
			binaryElement->PutdataType(_bstr_t(L"bin.base64"));
		_bstr_t encoded(binary->Gettext());
		CString compact((const wchar_t*)encoded);
		compact.Remove(L' ');
		compact.Remove(L'\t');
		compact.Remove(L'\r');
		compact.Remove(L'\n');

		// GetBinaries уже создаёт обычный текстовый узел Base64. Повторное
		// назначение dataType на элементе <binary> несовместимо с частью
		// версий MSXML6 и возвращает E_INVALIDARG. Заменяем дочерний текстовый
		// узел обычным способом DOM: так Base64 остаётся компактным и не
		// ломает переход редактора в режим исходного кода.
		MSXML2::IXMLDOMNodePtr child = binary->firstChild;
		while (child != NULL)
		{
			binary->removeChild(child);
			child = binary->firstChild;
		}
		binary->appendChild(document->createTextNode(
			_bstr_t((const wchar_t*)compact)));
	}
}

// set an attribute on the element
static void   SetAttr(MSXML2::IXMLDOMElement *xe,const wchar_t *name,
		      const wchar_t *ns,const _bstr_t& val,
		      MSXML2::IXMLDOMDocument2 *doc)
{
  MSXML2::IXMLDOMAttributePtr  attr(doc->createNode(2L,name,ns));
  attr->appendChild(doc->createTextNode(val));
  xe->setAttributeNode(attr);
}

// setup an ID for the element
static void   SetID(MSHTML::IHTMLElement *he,MSXML2::IXMLDOMElement *xe,
		    MSXML2::IXMLDOMDocument2 *doc) {
  _bstr_t     id(he->id);
  if (id.length()>0)
    SetAttr(xe,L"id",FBNS,id,doc);
}

// copy text
static MSXML2::IXMLDOMTextPtr MkText(MSHTML::IHTMLDOMNode *hn,MSXML2::IXMLDOMDocument2 *xml)
{
  VARIANT   vt;
  VariantInit(&vt);
  CheckError(hn->get_nodeValue(&vt));
  if (V_VT(&vt)!=VT_BSTR) {
    VariantClear(&vt);
    return xml->createTextNode(_bstr_t());
  }
  MSXML2::IXMLDOMText	    *txt;
  HRESULT   hr=xml->raw_createTextNode(V_BSTR(&vt),&txt);
  VariantClear(&vt);
  CheckError(hr);
  return MSXML2::IXMLDOMTextPtr(txt,FALSE);
}

// set an href attribute
static void SetHref(MSXML2::IXMLDOMElementPtr xe,MSXML2::IXMLDOMDocument2 *xml,
		    const _bstr_t& href)
{
  SetAttr(xe,L"l:href",XLINKNS,href,xml);
}

static void SetTitle(MSXML2::IXMLDOMElementPtr xe,MSXML2::IXMLDOMDocument2 *xml,
		    const _bstr_t& title)
{
	if(!title)
	{
		return;
	}
	SetAttr(xe,L"title",FB::FBNS,title,xml);
}

// handle inline formatting
static MSXML2::IXMLDOMNodePtr	  ProcessInline(MSHTML::IHTMLDOMNode *inl,
						MSXML2::IXMLDOMDocument2 *doc)
{
  //      Source
  _bstr_t		      name(inl->nodeName);
  MSHTML::IHTMLElementPtr     einl(inl);
  _bstr_t		      cls(einl->className);

  const wchar_t		      *xname=NULL;
  bool			      fA=false;
  bool			      fStyle=false;
  bool				  fUnk = false;
  bool				  fImg = false;

  // Modification by Pilgrim
  if (U::scmp(name,L"STRONG")==0)
    xname=L"strong";
  else if (U::scmp(name,L"EM")==0)
    xname=L"emphasis";
  else if (U::scmp(name,L"STRIKE")==0)
	  xname=L"strikethrough";
  else if (U::scmp(name,L"SUB")==0)
	  xname=L"sub";
  else if (U::scmp(name,L"SUP")==0)
	  xname=L"sup";
  else if (U::scmp(name,L"A")==0) {
      xname=L"a"; fA=true;
  } else if (U::scmp(name,L"SPAN")==0)
   {
	  if(U::scmp(cls,L"unknown_element")==0)
	  {
		  _bstr_t realClassName = einl->getAttribute(L"source_class", 2);
		  xname = realClassName;
		  fUnk = true;
	  }
	  else if (U::scmp(cls,L"image")==0)
	  {
		  fImg=true;
		  xname=L"image";
	  }
      else if (U::scmp(cls,L"code")==0)
		xname=L"code";
	  else
	  {
           xname=L"style"; fStyle=true;
	  }
  }

  MSXML2::IXMLDOMElementPtr   xinl(doc->createNode(1L,xname,FBNS));

  if (fImg)
	SetHref(xinl,doc,AU::GetAttrB(einl,L"href"));

  if (fA) {
    SetHref(xinl,doc,AU::GetAttrB(einl,L"href"));
    if (U::scmp(cls,L"note")==0)
      SetAttr(xinl,L"type",FBNS,cls,doc);
  }
  if (fStyle)
    SetAttr(xinl,L"name",FBNS,cls,doc);

  if (fUnk)
  {
	  MSHTML::IHTMLAttributeCollectionPtr col = inl->attributes;
	for(int i = 0; i < col->length; ++i)
	{
		VARIANT index;
		V_VT(&index) = VT_INT;
		index.intVal = i;
		_bstr_t attr_name = MSHTML::IHTMLDOMAttributePtr(col->item(&index))->nodeName;
		_bstr_t attr_value;
		wchar_t* real_attr_name = 0;
		const wchar_t* prefix = L"unknown_attribute_";
		if(wcsncmp(attr_name, prefix, wcslen(prefix)))
		{
			continue;
		}
		else
		{
			real_attr_name = (wchar_t*)attr_name + wcslen(prefix);
			attr_value = MSHTML::IHTMLDOMAttributePtr(col->item(&index))->nodeValue;
		}
		MSXML2::IXMLDOMAttributePtr  attr(doc->createNode(2L,real_attr_name,FBNS));
		attr->appendChild(doc->createTextNode(attr_value));
		xinl->setAttributeNode(attr);
	}
  }

  MSHTML::IHTMLDOMNodePtr     cn(inl->firstChild);

  // Modification by Pilgrim
  while ((bool)cn) {
    if (cn->nodeType==NODE_TEXT/*3*/)
      xinl->appendChild(MkText(cn,doc));
    else if ((cn->nodeType==NODE_ELEMENT/*1*/) && (!fImg)) // added by SeNS
      xinl->appendChild(ProcessInline(cn,doc));
    cn=cn->nextSibling;
  }

  return xinl;
}

// handle a paragraph element with subelements
static MSXML2::IXMLDOMNodePtr	  ProcessP(MSHTML::IHTMLElement *p,
					   MSXML2::IXMLDOMDocument2 *doc,
					   const wchar_t *baseName)
{
  _bstr_t cls(p->className);
  if (U::scmp(cls,L"text-author")==0)
    baseName=L"text-author";
  else if (U::scmp(cls,L"subtitle")==0)
    baseName=L"subtitle";
  // Modification by Pilgrim
  else if (U::scmp(cls,L"th")==0)
	baseName=L"th";
  else if (U::scmp(cls,L"td")==0)
	baseName=L"td";

  MSHTML::IHTMLDOMNodePtr   hp(p);

  // check if it is an empty-line
  if(U::scmp(cls, L"td") && U::scmp(cls, L"th"))
  {
	if	(hp->hasChildNodes()==VARIANT_FALSE ||
		(!(bool)hp->firstChild->nextSibling && hp->firstChild->nodeType==3 &&
		U::is_whitespace(hp->firstChild->nodeValue.bstrVal)))
	{
		MSHTML::IHTMLElementPtr pParent = MSHTML::IHTMLElementPtr(p)->parentElement;
		if (MSHTML::IHTMLElement3Ptr(p)->inflateBlock == VARIANT_TRUE)
		{
			if(pParent && U::scmp(pParent->className, L"stanza") == 0)
			{
				MSXML2::IXMLDOMNodePtr vNode = doc->createNode(NODE_ELEMENT, L"v", FBNS);
				vNode->appendChild(doc->createTextNode(L" "));
				return vNode;
			}
			else
				return doc->createNode(1L, L"empty-line", FBNS);
		}
		return MSXML2::IXMLDOMNodePtr();
	}
  }

  MSXML2::IXMLDOMElementPtr xp(doc->createNode(1L,baseName,FBNS));

  SetID(p,xp,doc);

  _bstr_t	style(AU::GetAttrB(p,L"fbstyle"));
  if (style.length()>0)
    SetAttr(xp,L"style",FBNS,style,doc);

  // Modification by Pilgrim
  _bstr_t	colspan(AU::GetAttrB(p,L"fbcolspan"));
  if (colspan.length()>0)
	  SetAttr(xp,L"colspan",FBNS,colspan,doc);

  _bstr_t	rowspan(AU::GetAttrB(p,L"fbrowspan"));
  if (rowspan.length()>0)
	  SetAttr(xp,L"rowspan",FBNS,rowspan,doc);

  _bstr_t	align(AU::GetAttrB(p,L"fbalign"));
  if (align.length()>0)
	  SetAttr(xp,L"align",FBNS,align,doc);

  _bstr_t	valign(AU::GetAttrB(p,L"fbvalign"));
  if (valign.length()>0)
	  SetAttr(xp,L"valign",FBNS,valign,doc);

  hp=hp->firstChild;

  // Modification by Pilgrim
  while ((bool)hp) {
    if (hp->nodeType==NODE_TEXT/*3*/) // text segment
      xp->appendChild(MkText(hp,doc));
    else if (hp->nodeType==NODE_ELEMENT/*1*/)
      xp->appendChild(ProcessInline(hp,doc));
    hp=hp->nextSibling;
  }

  _bstr_t selected	(AU::GetAttrB(p,L"fbe_selected"));
  if (selected.length()>0)
	  SetAttr(xp,L"selected",FBNS,selected,doc);

  return xp;
}

// handle a div element with subelements
static MSXML2::IXMLDOMNodePtr	  ProcessDiv(MSHTML::IHTMLElement *div,
					     MSXML2::IXMLDOMDocument2 *doc,
					     int indent)
{
  _bstr_t		    cls(div->className);

  MSXML2::IXMLDOMElementPtr xdiv(doc->createNode(1L,cls,FBNS));

  if (U::scmp(cls,L"image")==0) {
    SetID(div,xdiv,doc);
    SetHref(xdiv,doc,AU::GetAttrB(div,L"href"));
	SetTitle(xdiv,doc,AU::GetAttrB(div,L"title"));
    return xdiv;
  }

  SetID(div,xdiv,doc);

  // Modification by Pilgrim
  if (U::scmp(cls,L"table")==0) {
	  _bstr_t	style(AU::GetAttrB(div,L"fbstyle"));
	  if (style.length()>0){
		  SetAttr(xdiv,L"style",FBNS,style,doc);
	  }
  }
  if (U::scmp(cls,L"tr")==0) {
	 _bstr_t	align(AU::GetAttrB(div,L"fbalign"));
	 if (align.length()>0){
		SetAttr(xdiv,L"align",FBNS,align,doc);
	 }
  }

  MSHTML::IHTMLDOMNodePtr   ndiv(div);
  MSHTML::IHTMLDOMNodePtr   fc(ndiv->firstChild);

  const wchar_t *bn=U::scmp(cls,L"stanza")==0 ? L"v" : L"p";

  while ((bool)fc) {
    _bstr_t	name(fc->nodeName);
    MSHTML::IHTMLElementPtr efc(fc);
	// process empty lines
    MSHTML::IHTMLElement3Ptr(efc)->inflateBlock = VARIANT_TRUE;

    if (U::scmp(name,L"DIV")==0) {
      Indent(xdiv,doc,indent+1);
	  MSXML2::IXMLDOMNodePtr nnp = ProcessDiv(efc,doc,indent+1);
      xdiv->appendChild(nnp);
    } else if (U::scmp(name,L"P")==0) {
		MSXML2::IXMLDOMNodePtr  np;
		try { np = ProcessP(efc,doc,bn); } catch (...) { np = 0; }
      if (np) {
		Indent(xdiv,doc,indent+1);
		xdiv->appendChild(np);
      }
    }

    fc=fc->nextSibling;
  }

  Indent(xdiv,doc,indent);

  return xdiv;
}

// find a first named DIV
static MSXML2::IXMLDOMNodePtr  GetDiv(MSHTML::IHTMLElementPtr body,
				      MSXML2::IXMLDOMDocument2 *xml,
				      const wchar_t *name,
				      int indent)
{
  MSHTML::IHTMLElementCollectionPtr children(body->children);
  long				    c_len=children->length;

  for (long i=0;i<c_len;++i) {
    MSHTML::IHTMLElementPtr div(children->item(i));
    if (!(bool)div)
      continue;
    if (U::scmp(div->tagName,L"DIV")==0 && U::scmp(div->className,name)==0)
      return ProcessDiv(div,xml,indent);
  }

  return MSXML2::IXMLDOMNodePtr();
}

// fetch bodies
static void   GetBodies(MSHTML::IHTMLElementPtr	body,
			MSXML2::IXMLDOMDocument2 *doc)
{
  MSHTML::IHTMLElementCollectionPtr children(body->children);
  long			      c_len=children->length;

  for (long i=0;i<c_len;++i) {
    MSHTML::IHTMLElementPtr div(children->item(i));

    if (!(bool)div)
      continue;

	if (U::scmp(div->tagName,L"DIV")==0 && U::scmp(div->className,L"body")==0)
	{
      MSXML2::IXMLDOMElementPtr	xb(ProcessDiv(div,doc,1));
      _bstr_t	  bn(AU::GetAttrB(div,L"fbname"));
      if (bn.length()>0)
		SetAttr(xb,L"name",FBNS,bn,doc);
      Indent(doc->documentElement,doc,1);
      doc->documentElement->appendChild(xb);
    }
  }
}

// validator object
class SAXErrorHandler: public CComObjectRoot, public MSXML2::ISAXErrorHandler {
public:
  CString     m_msg;
  int	      m_line,m_col;

  SAXErrorHandler() : m_line(0),m_col(0) { }

  void	SetMsg(MSXML2::ISAXLocator *loc, const wchar_t *msg, HRESULT hr) {
    if (!m_msg.IsEmpty())
      return;
    m_msg=msg;
    CString   ns;
    ns.Format(_T("{%s}"),(const TCHAR *)FBNS);
    m_msg.Replace(ns,_T(""));
    ns.Format(_T("{%s}"),(const TCHAR *)XLINKNS);
    m_msg.Replace(ns,_T("xlink"));
    m_line=loc->getLineNumber();
    m_col=loc->getColumnNumber();
  }

  BEGIN_COM_MAP(SAXErrorHandler)
    COM_INTERFACE_ENTRY(MSXML2::ISAXErrorHandler)
  END_COM_MAP()

  STDMETHOD(raw_error)(MSXML2::ISAXLocator *loc, wchar_t *msg, HRESULT hr) {
    SetMsg(loc,msg,hr);
    return E_FAIL;
  }
  STDMETHOD(raw_fatalError)(MSXML2::ISAXLocator *loc, wchar_t *msg, HRESULT hr) {
    SetMsg(loc,msg,hr);
    return E_FAIL;
  }
  STDMETHOD(raw_ignorableWarning)(MSXML2::ISAXLocator *loc, wchar_t *msg, HRESULT hr) {
    SetMsg(loc,msg,hr);
    return E_FAIL;
  }
};

MSXML2::IXMLDOMDocument2Ptr Doc::CreateDOMImp(const CString& encoding, bool compactBinaries) {
  // normalize body first
  _EDMnr.CleanUpAll();
   m_body.Normalize(m_body.Document()->body);

  // create document
  MSXML2::IXMLDOMDocument2Ptr	ndoc(U::CreateDocument(false));
  ndoc->async=VARIANT_FALSE;

  // set encoding
  if (!encoding.IsEmpty())
    ndoc->appendChild(ndoc->createProcessingInstruction(L"xml",(const wchar_t *)(L"version=\"1.0\" encoding=\""+encoding+L"\"")));

  // create document element
  MSXML2::IXMLDOMElementPtr	root=ndoc->createNode(_variant_t(1L),L"FictionBook",FBNS);
  root->setAttribute(L"xmlns:l",XLINKNS);
  ndoc->documentElement=MSXML2::IXMLDOMElementPtr(root);

  // enable xpath queries
  ndoc->setProperty(L"SelectionLanguage",L"XPath");
  CString   nsprop(L"xmlns:fb='");
  nsprop+=(const wchar_t *)FBNS;
  nsprop+=L"' xmlns:xlink='";
  nsprop+=(const wchar_t *)XLINKNS;
  nsprop+=L"'";
  ndoc->setProperty(L"SelectionNamespaces",(const TCHAR *)nsprop);

  // fetch annotation

  MSHTML::IHTMLElementCollectionPtr children(m_body.Document()->body->children);
  long c_len = children->length;

  MSHTML::IHTMLElementPtr fbw_body;

  for (long i=0;i<c_len;++i) {
    MSHTML::IHTMLElementPtr div(children->item(i));

    if (!(bool)div)
      continue;

	if (U::scmp(div->tagName,L"DIV")==0 && U::scmp(div->id,L"fbw_body")==0)
	{
		 fbw_body = div;
		 break;
    }
  }

  MSXML2::IXMLDOMNodePtr  ann(GetDiv(fbw_body,ndoc,L"annotation",3));

  // fetch history
  MSXML2::IXMLDOMNodePtr  hist(GetDiv(fbw_body,ndoc,L"history",3));

  // fetch description
  CComDispatchDriver	body(m_body.Script());
  CComVariant		    args[3];
  if (hist)
    args[0]=hist.GetInterfacePtr();
  if (ann)
    args[1]=ann.GetInterfacePtr();
  args[2]=ndoc.GetInterfacePtr();

  CheckError(body.InvokeN(L"GetDesc",&args[0],3));

  // fetch body elements
  GetBodies(fbw_body,ndoc);

  // fetch binaries
  CheckError(body.Invoke1(L"GetBinaries",&args[2]));

	// Уплотнение base64 нужно только для записи файла. Переход в Source,
	// экспорт и скриптовый API должны получать DOM без этой необязательной
	// операции, чтобы вложение не могло сорвать работу редактора.
	if (compactBinaries)
		CompactBinaryTextContent(ndoc);

  Indent(root,ndoc,0);
  return ndoc;
}

MSXML2::IXMLDOMDocument2Ptr Doc::CreateDOM(const CString& encoding, bool compactBinaries)
{
	CString trace;
	trace.Format(L"X100 CreateDOM started: encoding=%s, compact-binaries=%d",
		(const wchar_t*)encoding, compactBinaries ? 1 : 0);
	StartupTrace::Event(L"xml", L"X100", trace);
	try
	{
		MSXML2::IXMLDOMDocument2Ptr result(CreateDOMImp(encoding, compactBinaries));
		StartupTrace::Event(L"xml", L"X190", L"CreateDOM completed");
		return result;
	}
	catch (_com_error& e)
	{
		StartupTrace::HResult(L"com", L"X191", e.Error(), L"CreateDOM");
		StartupTrace::Event(L"com", L"X192", trace);
		U::ReportError(e);
	}

	return NULL;
}

static void CommitRecoveryFile(const CString& temporaryFile, const CString& destinationFile)
{
	HANDLE temporaryHandle = ::CreateFile(temporaryFile, GENERIC_WRITE, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (temporaryHandle == INVALID_HANDLE_VALUE)
		throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));

	const BOOL flushed = ::FlushFileBuffers(temporaryHandle);
	const DWORD flushError = flushed ? ERROR_SUCCESS : ::GetLastError();
	::CloseHandle(temporaryHandle);
	if (!flushed)
		throw _com_error(HRESULT_FROM_WIN32(flushError));

	if (!::MoveFileEx(temporaryFile, destinationFile,
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
}
static void CommitSavedFile(const CString& temporaryFile, const CString& destinationFile, bool createBackupFile)
{
	HANDLE temporaryHandle = ::CreateFile(temporaryFile, GENERIC_WRITE, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (temporaryHandle == INVALID_HANDLE_VALUE)
		throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));

	const BOOL flushed = ::FlushFileBuffers(temporaryHandle);
	const DWORD flushError = flushed ? ERROR_SUCCESS : ::GetLastError();
	::CloseHandle(temporaryHandle);
	if (!flushed)
		throw _com_error(HRESULT_FROM_WIN32(flushError));

	const DWORD attributes = ::GetFileAttributes(destinationFile);
	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		const DWORD attributesError = ::GetLastError();
		if (attributesError != ERROR_FILE_NOT_FOUND && attributesError != ERROR_PATH_NOT_FOUND)
			throw _com_error(HRESULT_FROM_WIN32(attributesError));

		if (!::MoveFileEx(temporaryFile, destinationFile, MOVEFILE_WRITE_THROUGH))
			throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
		TraceDocumentEvent(L"D210", L"book save created new file without backup", destinationFile);
		return;
	}

	CString backupFile;
	LPCWSTR backupFilePath = NULL;
	if (createBackupFile)
	{
		backupFile = destinationFile + L".bak";
		if (!::DeleteFile(backupFile))
		{
			const DWORD backupError = ::GetLastError();
			if (backupError != ERROR_FILE_NOT_FOUND)
				throw _com_error(HRESULT_FROM_WIN32(backupError));
		}
		backupFilePath = backupFile;
	}

	if (!::ReplaceFile(destinationFile, temporaryFile, backupFilePath, 0, NULL, NULL))
		throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
	TraceDocumentEvent(L"D211", createBackupFile ? L"book save backup created" : L"book save replacing existing file",
		createBackupFile ? backupFile : destinationFile);
}

static CString CreateTemporaryFileName(const CString& directory, const wchar_t* prefix)
{
	wchar_t temporaryPath[MAX_PATH] = {};
	if (::GetTempFileName(directory, prefix, 0, temporaryPath) == 0)
		throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
	return CString(temporaryPath);
}
bool  Doc::SaveToFile(const CString& filename,bool fValidateOnly,
		      int *errline,int *errcol,bool reportAccessDenied)
{
	m_last_save_error = S_OK;
	CString trace;
	trace.Format(L"%s; file-present=%d", fValidateOnly ? L"book validation started" : L"book save started",
		filename.IsEmpty() ? 0 : 1);
	StartupTrace::Event(L"document", L"D200", trace);
  try {
    // create a schema collection
    MSXML2::IXMLDOMSchemaCollection2Ptr	scol;
    CheckError(scol.CreateInstance(L"Msxml2.XMLSchemaCache.6.0"));

    // load fictionbook schema
    scol->add(FBNS,(const wchar_t *)U::GetProgDirFile(L"FictionBook.xsd"));

    // create a SAX reader
    MSXML2::ISAXXMLReaderPtr	  rdr;
    CheckError(rdr.CreateInstance(L"Msxml2.SAXXMLReader.6.0"));

    // attach a schema
    rdr->putFeature(L"schema-validation",VARIANT_TRUE);
    rdr->putProperty(L"schemas",scol.GetInterfacePtr());
    rdr->putFeature(L"exhaustive-errors",VARIANT_TRUE);

    // create an error handler
    CComObject<SAXErrorHandler>	  *ehp;
    CheckError(CComObject<SAXErrorHandler>::CreateInstance(&ehp));
    CComPtr<CComObject<SAXErrorHandler> > eh(ehp);
    rdr->putErrorHandler(eh);

    // construct the document
	MSXML2::IXMLDOMDocument2Ptr	ndoc(CreateDOMImp(_Settings.KeepEncoding() ? m_encoding : _Settings.GetDefaultEncoding(), true));

    // reparse the document
    IStreamPtr	    isp(ndoc);
    HRESULT hr=rdr->raw_parse(_variant_t((IUnknown *)isp));
    bool bErrSave = false;
	if (FAILED(hr)) {
      if (!eh->m_msg.IsEmpty()) {
	// record error position
	if (errline)
	  *errline=eh->m_line;
	if (errcol)
	  *errcol=eh->m_col;
	if (fValidateOnly)
	  ::MessageBeep(MB_ICONERROR);
	else
	{
	  if(IDYES == U::MessageBox(MB_YESNO|MB_DEFBUTTON2|MB_ICONERROR, IDS_VALIDATION_FAIL_CPT, IDS_VALIDATION_FAIL_MSG, eh->m_msg))
	  {
			bErrSave = true;
			goto forcesave;
	  }
	}
	::SendMessage(m_frame,AU::WM_SETSTATUSTEXT,0,
	  (LPARAM)(const TCHAR *)eh->m_msg);
      } else
	U::ReportError(hr);
	  CString validationTrace;
	  validationTrace.Format(L"%s: строка=%ld, столбец=%ld",
		fValidateOnly ? L"book validation started" : L"book save started",
		eh->m_line, eh->m_col);
	  StartupTrace::Warning(L"document", L"D220", validationTrace);
      return false;
    }

	if (fValidateOnly)
	{
		wchar_t buf[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(), IDS_SB_NO_ERR, buf, MAX_LOAD_STRING);
		::SendMessage(m_frame,AU::WM_SETSTATUSTEXT, 0, (LPARAM)buf);
		::MessageBeep(MB_OK);
		StartupTrace::Event(L"document", L"D221", L"book validation completed without errors");
		return true;
    }

forcesave:
    // now save it
    // create tmp filename
    CString	path(filename);
    int		cp=path.ReverseFind(_T('\\'));
    if (cp<0)
      path=_T(".\\");
    else
      path.Delete(cp,path.GetLength()-cp);
    CString	buf(CreateTemporaryFileName(path, _T("fbe")));

	// added by SeNS: replace all nbsp - non-breaking spaces
	if (_Settings.GetNBSPChar().Compare(L"\u00A0") != 0)
	{
		MSXML2::IXMLDOMNodePtr node = ndoc->firstChild;

		while (node && node!=ndoc)
		{
			if (node->nodeType==3)
			{
				CString s = node->nodeValue;
				int n = s.Replace( _Settings.GetNBSPChar(), L"\u00A0");
				int k = s.Replace(L"<p>\u00A0</p>", L"<empty-line/>");
				if (n || k)
				{
					node->nodeValue = s.AllocSysString();
				}
			}
			if (node->firstChild)
				node=node->firstChild;
			else
			{
				while (node && node!=ndoc && node->nextSibling==NULL) node=node->parentNode;
				if (node && node!=ndoc) node=node->nextSibling;
			}
		}
	}

    // try to save file
    hr=ndoc->raw_save(_variant_t((const wchar_t *)buf));
    if (FAILED(hr)) {
      ::DeleteFile(buf);
      _com_issue_errorex(hr,ndoc,__uuidof(ndoc));
    }

	try {
		CommitSavedFile(buf, filename, _Settings.GetCreateBackupFile());
	}
	catch (...) {
		::DeleteFile(buf);
		throw;
	}

	if(bErrSave) {// Modification by Pilgrim
		CString mes("Document saved with errors: ");
		mes+=(const TCHAR *)eh->m_msg;
		::SendMessage(m_frame,AU::WM_SETSTATUSTEXT,0,
			(LPARAM)(const TCHAR *)mes);
		::MessageBeep(MB_OK);
	}
	else
	{
		wchar_t status[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(), IDS_SB_SAVED_NO_ERR, status, MAX_LOAD_STRING);
		::SendMessage(m_frame,AU::WM_SETSTATUSTEXT, 0, (LPARAM)status);
		::MessageBeep(MB_OK);
	}

	m_encoding = _Settings.KeepEncoding() ? m_encoding : _Settings.GetDefaultEncoding();
	StartupTrace::Event(L"document", L"D222", L"book save completed");
  }
  catch (_com_error& e) {
	StartupTrace::Error(L"document", L"D223", fValidateOnly ? L"book validation started" : L"book save started");
	m_last_save_error = e.Error();
	if (reportAccessDenied || (e.Error() != E_ACCESSDENIED && HRESULT_CODE(e.Error()) != ERROR_ACCESS_DENIED))
		U::ReportError(e);
    return false;
  }

  return true;
}

bool Doc::SaveRecoveryCopy(const CString& filename)
{
	CString temporaryFile;
	TraceRecoveryEvent(L"R110", L"Автосохранение начато", filename);
	try
	{
		MSXML2::IXMLDOMDocument2Ptr document(CreateDOMImp(
			_Settings.KeepEncoding() ? m_encoding : _Settings.GetDefaultEncoding(), true));

		CString directory(filename);
		const int separator = directory.ReverseFind(L'\\');
		if (separator < 0)
			directory = L".\\";
		else
			directory.Delete(separator, directory.GetLength() - separator);

		temporaryFile = CreateTemporaryFileName(directory, L"fbr");

		const HRESULT result = document->raw_save(_variant_t((const wchar_t*)temporaryFile));
		if (FAILED(result))
			_com_issue_errorex(result, document, __uuidof(document));

		CommitRecoveryFile(temporaryFile, filename);
		TraceRecoveryEvent(L"R111", L"Автосохранение завершено", filename);
		return true;
	}
	catch (...)
	{
		if (!temporaryFile.IsEmpty())
			::DeleteFile(temporaryFile);
		TraceRecoveryEvent(L"R112", L"Автосохранение завершилось ошибкой", filename);
		return false;
	}
}
bool  Doc::Save() {
  if (!m_namevalid)
    return false;
  AU::CPersistentWaitCursor wc;
  if (SaveToFile(m_filename, false, NULL, NULL, false)) {
    MarkSavePoint();
    return true;
  }
  return false;
}

bool  Doc::Save(const CString& filename) {
  AU::CPersistentWaitCursor wc;
  if (SaveToFile(filename)) {
    MarkSavePoint();
    m_filename=filename;
	U::SetCurrentDirectoryToFile(filename);
    m_namevalid=true;
    return true;
  }
  return false;
}

// IDs
static const wchar_t  *AddHash(CString& tmp,const _bstr_t& id) {
  tmp = L"#";
  tmp += (const wchar_t *)id;
  return tmp;
}

static void GrabIDs(CString& tmp,CComboBox& box,MSHTML::IHTMLDOMNode *node) {
  if (node->nodeType!=1)
    return;

  _bstr_t		  name(node->nodeName);
  if (U::scmp(name,L"P") && U::scmp(name,L"DIV") && U::scmp(name,L"BODY"))
    return;

  MSHTML::IHTMLElementPtr elem(node);
  _bstr_t		  id(elem->id);
  if (id.length()>0)
    box.AddString(AddHash(tmp,id));

  MSHTML::IHTMLDOMNodePtr cn(node->firstChild);
  while ((bool)cn) {
    GrabIDs(tmp,box,cn);
    cn=cn->nextSibling;
  }
}

void  Doc::ParaIDsToComboBox(CComboBox& box) {
  try {
    CString tmp;
    MSHTML::IHTMLDOMNodePtr body(m_body.Document()->body);
    GrabIDs(tmp,box,body);
  }
  catch (_com_error&) { }
}

void  Doc::BinIDsToComboBox(CComboBox& box) {
  try {
	  IDispatchPtr	bo(m_body.Document()->all->item(L"id"));
    if (!(bool)bo)
      return;
    CString	  tmp;
    MSHTML::IHTMLElementCollectionPtr sbo(bo);
    if ((bool)sbo) {
      long    l=sbo->length;
      for (long i=0;i<l;++i)
	  {
		MSHTML::IHTMLElementPtr elem = sbo->item(i);
		CString value = elem->getAttribute(L"value", 0);
		if (!value.IsEmpty())
		{
			box.AddString(AddHash(tmp, _bstr_t(value)));
		}
	  }
    } else {
      MSHTML::IHTMLInputTextElementPtr ebo(bo);
      if ((bool)ebo)
	box.AddString(AddHash(tmp,ebo->value));
    }
  }
  catch (_com_error&) { }
}

BSTR Doc::PrepareDefaultId(const CString& filename){

  CString _filename = U::Transliterate(filename);
  // prepare a default id
  int cp = _filename.ReverseFind(_T('\\'));
  if (cp < 0)
    cp = 0;
  else
    ++cp;
  CString   newid;
  while (cp<_filename.GetLength()) {
    TCHAR   c=_filename[cp];
    if ((c>=_T('0') && c<=_T('9')) ||
	(c>=_T('A') && c<=_T('Z')) ||
	(c>=_T('a') && c<=_T('z')) ||
	c==_T('_') || c==_T('-') || c==_T('.'))
      newid.AppendChar(c);
    ++cp;
  }
  if (!newid.IsEmpty() && !(
    (newid[0]>=_T('A') && newid[0]<=_T('Z')) ||
    (newid[0]>=_T('a') && newid[0]<=_T('z')) ||
    newid[0]==_T('_')))
    newid.Insert(0,_T('_'));
  return newid.AllocSysString();
 }

// binaries
void Doc::AddBinary(const CString& filename)
{
	_variant_t args[4];
	HRESULT hr;

	V_BSTR(&args[3]) = filename.AllocSysString();
	V_VT(&args[3]) = VT_BSTR;

	if(FAILED(hr = U::LoadFile(filename, &args[0])))
	{
		U::ReportError(hr);
		return;
	}

	V_BSTR(&args[2]) = PrepareDefaultId(filename);
	V_VT(&args[2]) = VT_BSTR;

	// Try to find out mime type
	V_BSTR(&args[1]) = U::GetMimeType(filename).AllocSysString();
	V_VT(&args[1]) = VT_BSTR;

	// Stuff the thing into JavaScript
	CComDispatchDriver body(m_body.Script());
	hr = body.InvokeN(L"apiAddBinary", args, 4);
		if(FAILED(hr))
			U::ReportError(hr);

	hr = body.Invoke0(L"FillCoverList");
	if(FAILED(hr))
		U::ReportError(hr);
}

void  Doc::ApplyConfChanges() {
  try {
    MSHTML::IHTMLStylePtr	  hs(m_body.Document()->body->style);

	CString	  fss(_Settings.GetFont());
    if (!fss.IsEmpty())
      hs->fontFamily=(const wchar_t *)fss;

	DWORD		  fs = _Settings.GetFontSize();
    if (fs>1) {
      fss.Format(_T("%dpt"), static_cast<int>(fs));
      hs->fontSize=(const wchar_t *)fss;
    }

    fs = _Settings.GetColorFG();
    if (fs==CLR_DEFAULT)
      fs=::GetSysColor(COLOR_WINDOWTEXT);
    fss.Format(_T("rgb(%d,%d,%d)"),GetRValue(fs),GetGValue(fs),GetBValue(fs));
    hs->color=(const wchar_t *)fss;

    fs = _Settings.GetColorBG();
    if (fs==CLR_DEFAULT)
      fs=::GetSysColor(COLOR_WINDOW);
    fss.Format(_T("rgb(%d,%d,%d)"),GetRValue(fs),GetGValue(fs),GetBValue(fs));
    hs->backgroundColor=(const wchar_t *)fss;

	bool mode = _Settings.FastMode();
	SetFastMode(mode);
	::SendMessage(m_frame, WM_COMMAND, MAKELONG(mode,IDN_FAST_MODE_CHANGE), (LPARAM)0);
  }
  catch (_com_error&) { }
}

static int compare_nocase(const void* v1,const void* v2)
{
	CString* s1 = (CString*)v1;
	CString* s2 = (CString*)v2;

	int cv = s1->CompareNoCase(*s2);
	if(cv != 0)
		return cv;

	return s1->Compare(*s2);
}

static int  compare_counts(const void *v1,const void *v2)
{
  const Doc::Word *w1=(const Doc::Word *)v1;
  const Doc::Word *w2=(const Doc::Word *)v2;
  int	diff=w1->count - w2->count;
  return diff ? diff : w1->word.CompareNoCase(w2->word);
}

void Doc::GetWordList(int flags, CSimpleArray<Word>& words, CString tagName)
{
	CWaitCursor hourglass;

	MSHTML::IHTMLElementPtr fbw_body = MSHTML::IHTMLDocument3Ptr(m_body.Document())->getElementById(L"fbw_body");
	MSHTML::IHTMLElementCollectionPtr paras = MSHTML::IHTMLElement2Ptr(fbw_body)->getElementsByTagName(L"P");
	if(!paras->length)
		return;

	int iNextElem = 0;

	// Construct a word list
	CSimpleArray<CString> wl;

	while(iNextElem < paras->length)
	{
		MSHTML::IHTMLElementPtr currElem(paras->item(iNextElem));
		CString innerText = currElem->innerText;

		MSHTML::IHTMLDOMNodePtr currNode(currElem);
		if(MSHTML::IHTMLElementPtr siblElem = currNode->nextSibling)
		{
			int jNextElem = iNextElem + 1;
			for(int i = jNextElem; i < paras->length; ++i)
			{
				MSHTML::IHTMLElementPtr nextElem = paras->item(i);
				if(siblElem == nextElem)
				{
					innerText += CString(L"\r\n") + siblElem->innerText.GetBSTR();
					iNextElem++;
					siblElem = MSHTML::IHTMLDOMNodePtr(nextElem)->nextSibling;
				}
				else
				{
					break;
				}
			}
		}

		_bstr_t bb(innerText.AllocSysString());

		if(bb.length() == 0)
		{
			iNextElem++;
			continue;
		}

		// iterate over bb using a primitive fsm
		wchar_t *p = bb, *e = p + bb.length() + 1; // include trailing 0!
		wchar_t *wstart,*wend;

		enum
		{
			INITIAL,
			INWORD1,
			INWORD2,
			HYPH1,
			HYPH2
		} state = INITIAL;

		while(p < e)
		{
			int letter = iswalpha(*p);
			switch(state)
			{
			case INITIAL: initial:
				if(letter)
				{
					wstart = p;
					state = INWORD1;
				}
				break;
			case INWORD1:
				if(!letter)
				{
					if(flags & GW_INCLUDE_HYPHENS)
					{
						if(iswspace(*p))
						{
							wend = p;
							state = HYPH1;
							break;
						}
						else if(*p == L'-')
						{
							wend = p;
							state = HYPH2;
							break;
						}
					}
					if(!(flags & GW_HYPHENS_ONLY))
					{
						*p = L'\0';
						wl.Add(wstart);
					}
					state = INITIAL;
				}
				break;
			case INWORD2:
				if(!letter)
				{
					if(flags & GW_INCLUDE_HYPHENS)
					{
						if(iswspace(*p))
						{
							wend = p;
							state = HYPH1;
							break;
						}
						else if(*p == L'-')
						{
							wend = p;
							state = HYPH2;
							break;
						}
					}
					*p = L'\0';
					U::RemoveSpaces(wstart);
					wl.Add(wstart);
					state = INITIAL;
				}
				break;
			case HYPH1:
				if(*p == L'-')
					state = HYPH2;
				else if(!iswspace(*p))
				{
					if(!(flags & GW_HYPHENS_ONLY))
					{
						*wend = L'\0';
						wl.Add(wstart);
					}
					state = INITIAL;
					goto initial;
				}
				break;
			case HYPH2:
				if(letter)
					state = INWORD2;
				else if (!iswspace(*p))
				{
					if(!(flags & GW_HYPHENS_ONLY))
					{
						*wend = L'\0';
						wl.Add(wstart);
					}
					state = INITIAL;
					goto initial;
				}
				break;
			}
			++p;
		}

		iNextElem++;
	}

	if(wl.GetSize() == 0)
		return;

	// now sort the list
	qsort(wl.GetData(), wl.GetSize(), sizeof(CString), compare_nocase);

	int wlSize = wl.GetSize();
	for(int i = 0; i < wlSize; ++i)
	{
		int count = 1, k = 0;
		for(int j = i + 1; j < wlSize; ++j)
		{
			if(wl[i] == wl[j])
				count++;
			else
			{
				k = --j;
				break;
			}

			k = j;
		}
		words.Add(Word(wl[i], count));
		if(k)
			i = k;
	}

	// Sort by count now
	if(flags & GW_SORT_BY_COUNT)
		qsort(words.GetData(), words.GetSize(), sizeof(Word), compare_counts);
}

/*bool  Doc::SetXML(MSXML2::IXMLDOMDocument2 *dom) {
  if (!dom)
    return false;

  try {
    // ok, it seems valid, put it into document then
    dom->setProperty(L"SelectionLanguage",L"XPath");
    CString   nsprop(L"xmlns:fb='");
    nsprop+=(const wchar_t *)FBNS;
    nsprop+=L"' xmlns:xlink='";
    nsprop+=(const wchar_t *)XLINKNS;
    nsprop+=L"'";
    dom->setProperty(L"SelectionNamespaces",(const TCHAR *)nsprop);

    // transform to html
    TransformXML(LoadXSL(_T("description.xsl")),dom,m_desc);

    // wait until it loads
    MSG msg;

    while (!m_desc.Loaded() && ::GetMessage(&msg,NULL,0,0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    m_desc.Init();

    // store binaries
    CComDispatchDriver	  desc(m_desc.Script());
    _variant_t	    arg(dom);
    desc.Invoke1(L"PutBinaries",&arg);

    // transform to html
	TransformXML(LoadXSL(_T("body.xsl")),dom,m_body);

    // wait until it loads
    while (!m_body.Loaded() && ::GetMessage(&msg,NULL,0,0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    m_body.Init();
  }
  catch (_com_error& e) {
    U::ReportError(e);
    return false;
  }

  return true;
}*/

// source editing
bool  Doc::SetXMLAndValidate(HWND sci,bool fValidateOnly,int& errline,int& errcol) {
  errline=errcol=0;

  // validate it first
  try {
    // create a schema collection
    MSXML2::IXMLDOMSchemaCollection2Ptr	scol;
    CheckError(scol.CreateInstance(L"Msxml2.XMLSchemaCache.6.0"));

    // load fictionbook schema
    scol->add(FBNS,(const wchar_t *)U::GetProgDirFile(L"FictionBook.xsd"));

    // create a SAX reader
    MSXML2::ISAXXMLReaderPtr	  rdr;
    CheckError(rdr.CreateInstance(L"Msxml2.SAXXMLReader.6.0"));

    // attach a schema
    rdr->putFeature(L"schema-validation",VARIANT_TRUE);
    rdr->putProperty(L"schemas",scol.GetInterfacePtr());
    rdr->putFeature(L"exhaustive-errors",VARIANT_TRUE);

    // create an error handler
    CComObject<SAXErrorHandler>	  *ehp;
    CheckError(CComObject<SAXErrorHandler>::CreateInstance(&ehp));
    CComPtr<CComObject<SAXErrorHandler> > eh(ehp);
    rdr->putErrorHandler(eh);

    // construct a document
    MSXML2::IXMLDOMDocument2Ptr	dom;

    if (!fValidateOnly) {
      dom=U::CreateDocument(true);

      // construct an xml writer
      MSXML2::IMXWriterPtr	wrt;
      CheckError(wrt.CreateInstance(L"Msxml2.MXXMLWriter.6.0"));

      // connect document to the writer
      wrt->output=dom.GetInterfacePtr();

      // connect the writer to the reader
      rdr->putContentHandler(MSXML2::ISAXContentHandlerPtr(wrt));
	}

    // now parse it!
    // oh well, let's waste more memory
    int	    textlen=::SendMessage(sci, SCI_GETLENGTH, 0, 0);
    std::vector<char> buffer;
    try {
      buffer.resize(textlen + 1);
    } catch (const std::bad_alloc&) {
	  wchar_t msg[MAX_LOAD_STRING + 1];
	  wchar_t cpt[MAX_LOAD_STRING + 1];
	  FbeLoadString(_Module.GetResourceInstance(), IDS_OUT_OF_MEM_MSG, msg, MAX_LOAD_STRING);
	  FbeLoadString(_Module.GetResourceInstance(), IDR_MAINFRAME, cpt, MAX_LOAD_STRING);
      ::MessageBox(::GetActiveWindow(), msg, cpt, MB_OK|MB_ICONERROR);
      return false;
    }
    ::SendMessage(sci, SCI_GETTEXT, textlen+1, (LPARAM)buffer.data());
    DWORD   ulen=::MultiByteToWideChar(CP_UTF8,0,buffer.data(),textlen,NULL,0);
    CComBSTR ustr;
    ustr.Attach(::SysAllocStringLen(NULL,ulen));
    if (!ustr) {
	  wchar_t msg[MAX_LOAD_STRING + 1];
	  wchar_t cpt[MAX_LOAD_STRING + 1];
	  FbeLoadString(_Module.GetResourceInstance(), IDS_OUT_OF_MEM_MSG, msg, MAX_LOAD_STRING);
	  FbeLoadString(_Module.GetResourceInstance(), IDR_MAINFRAME, cpt, MAX_LOAD_STRING);
      ::MessageBox(::GetActiveWindow(), msg, cpt, MB_OK|MB_ICONERROR);
      return false;
    }
    ::MultiByteToWideChar(CP_UTF8,0,buffer.data(),textlen,ustr,ulen);

    VARIANT vt;
    ::VariantInit(&vt);
    V_VT(&vt)=VT_BSTR;
    V_BSTR(&vt)=ustr.Detach();
    HRESULT hr=rdr->raw_parse(vt);
    ::VariantClear(&vt);

	if (FAILED(hr)) {
      if (!eh->m_msg.IsEmpty()) {
	// record error position
	errline=eh->m_line;
	errcol=eh->m_col;
	::MessageBeep(MB_ICONERROR);
	::SendMessage(m_frame,AU::WM_SETSTATUSTEXT,0,
	  (LPARAM)(const TCHAR *)eh->m_msg);
      } else
	U::ReportError(hr);
      return false;
    }

    if (fValidateOnly)
	{
		wchar_t buf[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(), IDS_SB_NO_ERR, buf, MAX_LOAD_STRING);
		::SendMessage(m_frame,AU::WM_SETSTATUSTEXT, 0, (LPARAM)buf);
		::MessageBeep(MB_OK);
		return true;
    }

    // ok, it seems valid, put it int6o document then
    dom->setProperty(L"SelectionLanguage",L"XPath");
    CString   nsprop(L"xmlns:fb='");
    nsprop+=(const wchar_t *)FBNS;
    nsprop+=L"' xmlns:xlink='";
    nsprop+=(const wchar_t *)XLINKNS;
    nsprop+=L"'";
    dom->setProperty(L"SelectionNamespaces",(const TCHAR *)nsprop);

    // transform to html
	CComDispatchDriver	body(m_body.Script());
	CComVariant		    args[2];
	args[1]=dom.GetInterfacePtr();
	args[0] = _Settings.GetInterfaceLanguageName();
	CheckError(body.InvokeN(L"LoadFromDOM", args, 2));
	m_body.Init();
    /*TransformXML(LoadXSL(_T("description.xsl")),dom,m_desc);

    // wait until it loads
    MSG msg;

    while (!m_desc.Loaded() && ::GetMessage(&msg,NULL,0,0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    // initialize view
    m_desc.Init();

    // store binaries
    CComDispatchDriver	  desc(m_desc.Script());
    _variant_t	    arg(dom.GetInterfacePtr());
    desc.Invoke1(L"PutBinaries",&arg);

    // transform to html
    TransformXML(LoadXSL(_T("body.xsl")),dom,m_body);


    // wait until it loads
    while (!m_body.Loaded() && ::GetMessage(&msg,NULL,0,0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    // initialize view
    m_body.Init();*/

    // mark unchanged
    MarkSavePoint();
  }
  catch (_com_error& e) {
    U::ReportError(e);
    return false;
  }

  return true;
}

void Doc::SaveSelectedPos()
{
	MSHTML::IHTMLElementPtr selected = m_body.SelectionStructCon();

	//  UUID
	UUID	      uuid;
	wchar_t *str;
	if (UuidCreate(&uuid)==RPC_S_OK && UuidToStringW(&uuid,&str)==RPC_S_OK)
	{
		m_save_marker = str;
	}
	else
	{
		return;
	}
	selected->setAttribute(L"fbe_selected", m_save_marker, 0);
	m_saved_element = selected;
}

long Doc::GetSavedPos(bstr_t &xml, bool deleteMarker)
{
	bstr_t searchString = L" selected=\"" + m_save_marker + L"\"";
	const wchar_t* wpos = wcsstr(xml.operator const wchar_t *(), searchString);
	if(!wpos)
	{
		return 0;
	}
	int pos = wpos - (wchar_t*)xml;

	if(deleteMarker)
	{
		CStringW cleaned((const wchar_t*)xml);
		cleaned.Delete(pos, searchString.length());
		xml = static_cast<const wchar_t*>(cleaned);
	}
	return pos;
}

void Doc::DeleteSaveMarker()
{
	m_saved_element->removeAttribute(L"fbe_selected", 1);
}



/*class SAXContentHandler:public CComObjectRoot, public MSXML2::ISAXContentHandler
{
private:
	MSXML2::ISAXContentHandlerPtr m_writer;

public:
	SAXContentHandler::SAXContentHandler():m_writer(0){}
	SAXContentHandler::~SAXContentHandler()
	{
	}

	BEGIN_COM_MAP(SAXContentHandler)
		COM_INTERFACE_ENTRY(MSXML2::ISAXContentHandler)
	END_COM_MAP()

	void SetWriter(MSXML2::ISAXContentHandlerPtr writer)
	{
		 m_writer = writer;
	}

	STDMETHOD(raw_characters)(wchar_t * pwchChars,int cchChars)
	{
		 return m_writer->raw_characters(pwchChars, cchChars);
	}

	STDMETHOD(raw_endDocument)()
	{
		return m_writer->raw_endDocument();
	}

	STDMETHOD(raw_startDocument)()
	{
		return m_writer->raw_startDocument();
	}

	STDMETHOD(raw_endElement)(wchar_t * pwchNamespaceUri, int cchNamespaceUri,  wchar_t * pwchLocalName, int cchLocalName, wchar_t * pwchQName, int cchQName)
	{
		return m_writer->raw_endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
	}

	STDMETHOD(raw_startElement)(wchar_t * pwchNamespaceUri, int cchNamespaceUri, wchar_t * pwchLocalName, int cchLocalName, wchar_t * pwchQName, int cchQName, MSXML2::ISAXAttributes * pAttributes)
	{
		return m_writer->raw_startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName,pAttributes);
	}
 	STDMETHOD(raw_ignorableWhitespace)(wchar_t * pwchChars, int cchChars)
	{
		return m_writer->raw_ignorableWhitespace(pwchChars, cchChars);
	}
 	STDMETHOD(raw_endPrefixMapping)(wchar_t * pwchPrefix, int cchPrefix)
	{
		return m_writer->raw_endPrefixMapping(pwchPrefix, cchPrefix);
	}
 	STDMETHOD(raw_startPrefixMapping)(wchar_t * pwchPrefix, int cchPrefix, wchar_t * pwchUri, int cchUri)
	{
		return m_writer->raw_startPrefixMapping(pwchPrefix, cchPrefix, pwchUri, cchUri);
	}
 	STDMETHOD(raw_processingInstruction)(wchar_t * pwchTarget, int cchTarget, wchar_t * pwchData, int cchData)
	{
		return m_writer->raw_processingInstruction(pwchTarget, cchTarget, pwchData, cchData);
	}
 	STDMETHOD(raw_skippedEntity)(wchar_t * pwchName, int cchName)
	{
		return m_writer->raw_skippedEntity(pwchName, cchName);
	}

	STDMETHOD(raw_putDocumentLocator)(MSXML2::ISAXLocator * pLocatore)
	{
		return m_writer->raw_putDocumentLocator(pLocatore);
	}
};*/

bool Doc::TextToXML(BSTR text, MSXML2::IXMLDOMDocument2Ptr* xml)
{
	MSXML2::IXMLDOMSchemaCollection2Ptr	scol;
    CheckError(scol.CreateInstance(L"Msxml2.XMLSchemaCache.6.0"));

    // load fictionbook schema
	scol->add(FBNS,(const wchar_t *)U::GetProgDirFile(L"FictionBook.xsd"));

    // create a SAX reader
    MSXML2::ISAXXMLReaderPtr	  rdr;
    CheckError(rdr.CreateInstance(L"Msxml2.SAXXMLReader.6.0"));

    // attach a schema
    rdr->putFeature(L"schema-validation",VARIANT_TRUE);
    rdr->putProperty(L"schemas",scol.GetInterfacePtr());
    rdr->putFeature(L"exhaustive-errors",VARIANT_TRUE);

    // create an error handler
    CComObject<SAXErrorHandler>	  *ehp;
    CheckError(CComObject<SAXErrorHandler>::CreateInstance(&ehp));
    CComPtr<CComObject<SAXErrorHandler> > eh(ehp);
    rdr->putErrorHandler(eh);

    *xml=U::CreateDocument(true);

    // construct an xml writer
    MSXML2::IMXWriterPtr	wrt;
    CheckError(wrt.CreateInstance(L"Msxml2.MXXMLWriter.6.0"));

    // connect document to the writer
    wrt->output=xml->GetInterfacePtr();

    // connect the writer to the reader
	rdr->putContentHandler(MSXML2::ISAXContentHandlerPtr(wrt));

    // now parse it!
    // oh well, let's waste more memory

    VARIANT vt;
    V_VT(&vt)=VT_BSTR;
    V_BSTR(&vt)=text;
    HRESULT hr=rdr->raw_parse(vt);
    //::VariantClear(&vt);

	bstr_t msg = eh->m_msg;
    if (FAILED(hr))
	{
      if (!eh->m_msg.IsEmpty())
	  {
		// record error position
		int errline = eh->m_line;
		int errcol = eh->m_col;
		::MessageBeep(MB_ICONERROR);
		::SendMessage(m_frame,AU::WM_SETSTATUSTEXT,0,
		(LPARAM)(const TCHAR *)eh->m_msg);
      }
	  else
	  {
		U::ReportError(hr);
	  }
      return false;
    }

    // ok, it seems valid, put it into document then
    (*xml)->setProperty(L"SelectionLanguage",L"XPath");
    CString   nsprop(L"xmlns:fb='");
    nsprop+=(const wchar_t *)FBNS;
    nsprop+=L"' xmlns:xlink='";
    nsprop+=(const wchar_t *)XLINKNS;
    nsprop+=L"'";
	(*xml)->setProperty(L"SelectionNamespaces",(const TCHAR *)nsprop);

	return true;
}

MSHTML::IHTMLDOMNodePtr Doc::MoveNode(MSHTML::IHTMLDOMNodePtr from, MSHTML::IHTMLDOMNodePtr to, MSHTML::IHTMLDOMNodePtr insertBefore)
{
	VARIANT disp;
	MSHTML::IHTMLElementPtr elem = (MSHTML::IHTMLElementPtr)to;
	bstr_t text = elem->innerHTML;

	//  title
	if((bool)insertBefore)
	{
		while(1)
		{
			MSHTML::IHTMLElementPtr elem = (MSHTML::IHTMLElementPtr)insertBefore;
			_bstr_t class_name(elem->className);
			if(
				(0 == U::scmp(class_name, L"title"))
				|| (0 == U::scmp(class_name, L"epigraph"))
				|| (0 == U::scmp(class_name, L"annotation"))
				|| (0 == U::scmp(class_name, L"image"))
				)
			{
				insertBefore = insertBefore->nextSibling;
				continue;
			}
			break;
		}
	}

	MSHTML::IHTMLDOMNodePtr ret;
	if((bool)insertBefore)
	{
		disp.pdispVal = insertBefore;
		disp.vt = VT_DISPATCH;
		ret = to->insertBefore(from, disp);
	}
	else
	{
		ret = to->appendChild(from);
	}

	return ret ;
}

void Doc::FastMode()
{
	CComDispatchDriver	body(m_body.Script());
	CComVariant		    args[1];
	args[0]=m_fast_mode;
	CheckError(body.Invoke1(L"apiSetFastMode",&args[0]));
	return;
}

void Doc::SetFastMode(bool fast)
{
	m_fast_mode = fast;
	if(m_body != 0)
		FastMode();
}

bool Doc::GetFastMode()
{
	return m_fast_mode;
}

// TODO (by SeNS): should be fixed!
int Doc::GetSelectedPos()
{
	const int delta = -100000;
	MSHTML::IHTMLTxtRangePtr rng(m_body.Document()->selection->createRange());
	if(!bool(rng))
		return 0;

	int len = 0;
	while(1)
	{
		int moved = rng->move(L"character", delta);
		len -= moved;
		if(moved != delta)
		{
			//bstr_t text(rng->text);
			return len-21;
		}
	}

	return len;
}

CString Doc::GetOpenFileName()const
{
	if(m_filename == L"Untitled.fb2")
        return L"";
	else
		return m_filename;
}

} // namespace
