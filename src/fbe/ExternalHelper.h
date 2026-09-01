#ifndef EXTERNALHELPER_H
#define EXTERNALHELPER_H


#include <fcntl.h>
#include "Settings.h"
#include "utils.h"
#include "StartupTrace.h"
#include "BinaryFileSave.h"
#include "BinarySaveNotification.h"
#include "RuntimeLocalization.h"
#include "..\\common\\ModernFileDialog.h"

inline bool IsDiagnosticFaultInjectionEnabled(const wchar_t* point)
{
	wchar_t testMode[4] = {};
	const DWORD testModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode));
	if (!point || !*point || !StartupTrace::Enabled() || testModeLength != 1 || testMode[0] != L'1')
		return false;
	wchar_t value[64] = {};
	const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", value, _countof(value));
	return length && length < _countof(value) && _wcsicmp(value, point) == 0;
}

extern CSettings _Settings;

static int modalResultCode;

extern "C"
{
	extern const char* build_timestamp;
	extern const char* build_name;
	extern const char* build_commit;
	extern const char* build_configuration;
};

class ExternalHelper :
  public CComObjectRoot,
  public IDispatchImpl<IExternalHelper, &IID_IExternalHelper>
{
  const CString* m_document_filename;
  const bool* m_document_namevalid;
  static __declspec(thread) bool s_traceScriptActive;
  static CComAutoCriticalSection s_embeddedTypeInfoLock;
  static CComPtr<ITypeInfo> s_embeddedTypeInfo;
  static HRESULT GetEmbeddedTypeInfo(ITypeInfo** resultTypeInfo);

public:
  static void FlushTraceSummary();
  ExternalHelper() : m_document_filename(NULL), m_document_namevalid(NULL) {}

  DECLARE_NO_REGISTRY()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  BEGIN_COM_MAP(ExternalHelper)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IExternalHelper)
  END_COM_MAP()

  // IExternalHelper
  void SetDocumentFilePathSource(const CString* filename, const bool* namevalid)
  {
    m_document_filename = filename;
    m_document_namevalid = namevalid;
  }
  STDMETHOD(GetTypeInfoCount)(UINT* typeInfoCount);
  STDMETHOD(GetTypeInfo)(UINT typeInfo, LCID lcid, ITypeInfo** resultTypeInfo);
  STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR* names, UINT nameCount, LCID lcid, DISPID* dispids);
  STDMETHOD(Invoke)(DISPID dispid, REFIID riid, LCID lcid, WORD flags,
    DISPPARAMS* parameters, VARIANT* result, EXCEPINFO* exceptionInfo, UINT* argumentError);
  STDMETHOD(GetDocumentFilePath)(BSTR* path);
  STDMETHOD(GetDocumentFileName)(BSTR* name);
  STDMETHOD(GetDocumentDirectory)(BSTR* directory);

  STDMETHOD(IsDiagnosticTraceEnabled)(BOOL* enabled)
  {
    if (!enabled)
      return E_POINTER;
    *enabled = StartupTrace::Enabled() ? TRUE : FALSE;
    return S_OK;
  }

  STDMETHOD(TraceScript)(BSTR code, BSTR message)
  {
    if(s_traceScriptActive)
      return S_OK;
    struct TraceScriptGuard
    {
      bool& active;
      TraceScriptGuard(bool& value) : active(value) { active = true; }
      ~TraceScriptGuard() { active = false; }
    } guard(s_traceScriptActive);
    CString safeCode(code ? code : L"");
    safeCode = safeCode.Left(32);
    CString safeMessage = StartupTrace::SanitizeLogText(message ? message : L"", 512);
    StartupTrace::ScriptEvent(safeCode, safeMessage);
    return S_OK;
  }
  STDMETHOD(BeginUndoUnit)(IDispatch *obj,BSTR name) {
    MSHTML::IMarkupServices   *srv;
    HRESULT hr=obj->QueryInterface(&srv);
    if (FAILED(hr))
      return hr;
    hr=srv->raw_BeginUndoUnit(name);
    srv->Release();
    return hr;
  }
  STDMETHOD(EndUndoUnit)(IDispatch *obj) {
    MSHTML::IMarkupServices   *srv;
    HRESULT hr=obj->QueryInterface(&srv);
    if (FAILED(hr))
      return hr;
    hr=srv->raw_EndUndoUnit();
    srv->Release();
    return hr;
  }
  STDMETHOD(get_inflateBlock)(IDispatch *obj,BOOL *ifb) {
    MSHTML::IHTMLElement3 *elem;
    HRESULT hr=obj->QueryInterface(&elem);
    if (FAILED(hr))
      return hr;
    VARIANT_BOOL vb;
    hr=elem->get_inflateBlock(&vb);
    *ifb=SUCCEEDED(hr) && vb==VARIANT_TRUE ? TRUE : FALSE;
    elem->Release();
    return hr;
  }
  STDMETHOD(put_inflateBlock)(IDispatch *obj,BOOL ifb) {
    MSHTML::IHTMLElement3 *elem;
    HRESULT hr=obj->QueryInterface(&elem);
    if (FAILED(hr))
      return hr;
    hr=elem->put_inflateBlock(ifb ? VARIANT_TRUE : VARIANT_FALSE);
    elem->Release();
    return hr;
  }
  STDMETHOD(GenrePopup)(IDispatch *obj,LONG x,LONG y,BSTR *name);
  STDMETHOD(DescShowMenu)(IDispatch *obj,LONG x,LONG y,BSTR *element_id);
  // Modification by Pilgrim
  /*STDMETHOD(LangPopup)(IDispatch *obj,LONG x,LONG y,BSTR *name);
  STDMETHOD(SrcLangPopup)(IDispatch *obj,LONG x,LONG y,BSTR *name);
  STDMETHOD(STILangPopup)(IDispatch *obj,LONG x,LONG y,BSTR *name);

  STDMETHOD(STISrcLangPopup)(IDispatch *obj,LONG x,LONG y,BSTR *name);*/


	STDMETHOD(GetStylePath)(BSTR *name)
	{
		CString path = U::GetProgDir();
		path.TrimRight(L"\\");
		*name = path.AllocSysString();
		
		return S_OK;
	}

	STDMETHOD(GetBinarySize)(BSTR data, int *length)
	{
		*length = SysStringByteLen(data);
		return S_OK;
	}  
	STDMETHOD(InflateParagraphs)(IDispatch *elem)
	{
		if (IsDiagnosticFaultInjectionEnabled(L"inflate-paragraphs"))
		{
			StartupTrace::HResult(L"fault", L"FI595", E_FAIL, L"InflateParagraphs injected failure");
			return E_FAIL;
		}
		if (!elem)
			return E_POINTER;
		MSHTML::IHTMLElement2Ptr element;
		HRESULT result = elem->QueryInterface(IID_IHTMLElement2, reinterpret_cast<void**>(&element));
		if (FAILED(result) || !element)
			return FAILED(result) ? result : E_NOINTERFACE;
		MSHTML::IHTMLElementCollectionPtr paragraphs(element->getElementsByTagName(L"P"));
		if (!paragraphs)
			return E_NOINTERFACE;
		for (long index = 0; index < paragraphs->length; ++index)
		{
			MSHTML::IHTMLElement3Ptr paragraph(paragraphs->item(index));
			if (!paragraph)
				return E_NOINTERFACE;
			result = paragraph->put_inflateBlock(VARIANT_TRUE);
			if (FAILED(result))
				return result;
		}
		return S_OK;
	}

	STDMETHOD(GetUUID)(BSTR *uid)
	{
		UUID	      uuid;
		unsigned char *str;
		if (UuidCreate(&uuid)==RPC_S_OK && UuidToStringA(&uuid,&str)==RPC_S_OK) 
		{
			CString     us(str);
			RpcStringFreeA(&str);
			us.MakeUpper();
			*uid = us.AllocSysString();
			return S_OK;
		}
		return S_FALSE;
	}


	STDMETHOD(GetNBSP)(BSTR *nbsp)
	{
		CString s_nbsp = _Settings.GetNBSPChar();
		*nbsp = s_nbsp.AllocSysString();
		return S_OK;
	}

	STDMETHOD(MsgBox)(BSTR message)
	{
		wchar_t cpt[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(), IDS_SCRIPT_MSG_CPT, cpt, MAX_LOAD_STRING);
		MessageBoxW(GetActiveWindow(), message, cpt, MB_ICONINFORMATION|MB_OK);
		return S_OK;
	}

	STDMETHOD(AskYesNo)(BSTR message, BOOL *pVal)
	{
		wchar_t cpt[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(), IDS_SCRIPT_MSG_CPT, cpt, MAX_LOAD_STRING);
		if (IDYES == MessageBoxW(GetActiveWindow(), message, cpt, MB_ICONQUESTION|MB_YESNO))
		{
			*pVal = true;
		}
		else
		{
			*pVal = false;
		}
		return S_OK;
	}

	STDMETHOD(SaveBinary)(BSTR path, BSTR data, BOOL prompt, BOOL* ret)
	{
		if (!ret)
			return E_POINTER;
		INT_PTR modalResult = IDOK;
		*ret = false;
		CString file_name = CString(path);

		if (prompt)
		{
			CString fname = ATLPath::FindFileName(file_name );
			CString fpath(file_name);
			fpath = fpath.Left(file_name.GetLength()-fname.GetLength());
			const COMDLG_FILTERSPEC filters[] = { { L"JPEG files (*.jpg)", L"*.jpg" }, { L"PNG files (*.png)", L"*.png" }, { L"All files (*.*)", L"*.*" } };
			ModernFileDialog::Request request;
			request.save = true; request.pathMustExist = true; request.overwritePrompt = true;
			request.defaultExtension = L"jpg"; request.initialFileName = fname.GetString(); request.initialFolder = fpath.GetString();
			request.filters = filters; request.filterCount = _countof(filters); request.filterIndex = 1;
			const ModernFileDialog::Result dialogResult = ModernFileDialog::Show(NULL, request);
			if (dialogResult.outcome == ModernFileDialog::Outcome::Failed)
				StartupTrace::HResult(L"file-dialog", L"FD106", dialogResult.error, L"SaveBinary dialog");
			modalResult = dialogResult.outcome == ModernFileDialog::Outcome::Accepted ? IDOK : IDCANCEL;
			if (modalResult == IDOK) file_name = dialogResult.paths.front().c_str();
		}

		if (modalResult == IDOK)
		{
			const DWORD byteCount = SysStringByteLen(data);
			DWORD error = ERROR_SUCCESS;
			const BinaryFileSave::ExistingFilePolicy existingFilePolicy = prompt
				? BinaryFileSave::ExistingFilePolicy::ReplaceExisting
				: BinaryFileSave::ExistingFilePolicy::FailIfExists;
			if (BinaryFileSave::WriteAtomically(file_name, data, byteCount, existingFilePolicy, &error))
				*ret = true;
			else
			{
				const bool expectedExists = !prompt &&
					(error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS);
				if (!expectedExists)
				{
					CString message;
					message.Format(L"SaveBinary failed (error %lu)", error);
					StartupTrace::Error(L"binary-save", L"B510", message);
					if (prompt)
						ShowBinarySaveFailure(GetActiveWindow(), file_name, error);
				}
			}
		}
		return S_OK;
	}

	STDMETHOD(GetExtendedStyle)(BSTR elem, BOOL* ext)
	{
		if (!ext)
			return E_POINTER;
		if (IsDiagnosticFaultInjectionEnabled(L"get-extended-style"))
		{
			StartupTrace::HResult(L"fault", L"FI820", E_FAIL, L"GetExtendedStyle injected failure");
			return E_FAIL;
		}
		*ext = _Settings.GetExtElementStyle(elem);
		return S_OK;
	}

  STDMETHOD(IsFastMode)(BOOL* ext)
  {
    *ext = _Settings.FastMode();
    return S_OK;
  }

	STDMETHOD(DescShowElement)(BSTR elem, BOOL show)
	{
		_Settings.SetExtElementStyle(elem, show != 0);
		return S_OK;
	}

	STDMETHOD(SetStyleEx)(IDispatch* doc, IDispatch* elem, BSTR style)
	{
		MSHTML::IHTMLElementPtr el = elem;
		U::ChangeAttribute(el, L"class", style);
		return S_OK;
	}

	STDMETHOD(GetImageDimsByPath)(BSTR path, BSTR* dims)
	{
		int nWidth, nHeight;

		if(U::GetImageDimsByPath(path, &nWidth, &nHeight))
		{
			CString format;
			format.Format(L"%dx%d", nWidth, nHeight);
			*dims = format.AllocSysString();
		}
		else *dims = ::SysAllocString(L"");

		return S_OK;
	}

	STDMETHOD(GetImageDimsByData)(VARIANT* data, BSTR* dims)
	{
		int nWidth, nHeight;

		SAFEARRAY* psa = data->parray;
		long lUbound;

		if(SafeArrayGetUBound(psa, 1, &lUbound) == S_OK && U::GetImageDimsByData(psa, lUbound, &nWidth, &nHeight))
		{
			CString format;
			format.Format(L"%dx%d", nWidth, nHeight);
			*dims = format.AllocSysString();
		}
		else *dims = ::SysAllocString(L"");

		return S_OK;
	}

	STDMETHOD(GetViewWidth)(int* width)
	{
		*width = _Settings.GetViewWidth();
		return S_OK;
	}

	STDMETHOD(GetViewHeight)(int* height)
	{
		*height = _Settings.GetViewHeight();
		return S_OK;
	}

	STDMETHOD(GetProgramVersion)(BSTR* ver)
	{
		CString version(build_name);
		*ver = version.AllocSysString();
		return S_OK;
	}

	STDMETHOD(InputBox)(BSTR prompt, BSTR title, BSTR value, BSTR* input)
	{
		CString sPrompt(prompt);
		CString sTitle(title);
		CString sInput(value);

		modalResultCode = AU::InputBox (sInput, sTitle, sPrompt);

		if (modalResultCode != IDYES) sInput.SetString(L"");
		*input = sInput.AllocSysString();
		return S_OK;
	}

	STDMETHOD(GetModalResult)(int* modalResult)
	{
		*modalResult = modalResultCode;
		return S_OK;
	}

	STDMETHOD(SetStatusBarText)(BSTR text)
	{
		CString sbtext(text);
		::SendMessage(_Settings.GetMainWindow(), AU::WM_SETSTATUSTEXT, 0, (LPARAM) (LPCTSTR) sbtext.GetBuffer());
		return S_OK;
	}

	STDMETHOD(GetLocalizedString)(BSTR key, BSTR* text)
	{
		if (!text) return E_POINTER;
		const CString localized = FbeLoadRuntimeStringByKey(key, L"");
		*text = localized.AllocSysString();
		return S_OK;
	}
};

#endif
