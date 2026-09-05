#include "stdafx.h"
#include "ExportHTMLPlugin.h"

#include "utils.h"
#include "HtmlExportOptionsDialog.h"
#include "..\\common\\ModernFileDialog.h"
#include "RuntimeLocalization.h"

#include <vector>
#include <regex>

namespace {

bool LoadUtf8TextFile(const CString& filename, CString& text)
{
	text.Empty();
	if (filename.IsEmpty())
		return true;

	HANDLE file = ::CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return false;

	DWORD sizeHigh = 0;
	DWORD size = ::GetFileSize(file, &sizeHigh);
	if (size == INVALID_FILE_SIZE || sizeHigh != 0 || size > 16 * 1024 * 1024) {
		::CloseHandle(file);
		::SetLastError(ERROR_FILE_TOO_LARGE);
		return false;
	}

	std::vector<char> bytes(size);
	DWORD read = 0;
	BOOL ok = size == 0 || ::ReadFile(file, &bytes[0], size, &read, NULL);
	::CloseHandle(file);
	if (!ok || read != size)
		return false;

	DWORD offset = size >= 3 && (unsigned char)bytes[0] == 0xEF &&
		(unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF ? 3 : 0;
	int sourceLength = static_cast<int>(size - offset);
	if (sourceLength == 0)
		return true;

	UINT codePage = CP_UTF8;
	int length = ::MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS,
		&bytes[offset], sourceLength, NULL, 0);
	if (length == 0) {
		codePage = CP_ACP;
		length = ::MultiByteToWideChar(codePage, 0, &bytes[offset], sourceLength, NULL, 0);
	}
	if (length == 0)
		return false;

	wchar_t* buffer = text.GetBuffer(length);
	if (::MultiByteToWideChar(codePage, 0, &bytes[offset], sourceLength, buffer, length) == 0) {
		text.ReleaseBuffer(0);
		return false;
	}
	text.ReleaseBuffer(length);
	return true;
}

void RemoveServiceMarkers(IXMLDOMDocument2Ptr source)
{
	IXMLDOMNodeListPtr textNodes;
	CheckError(source->selectNodes(bstr_t(L"//text()[contains(., '{')]"), &textNodes));
	long length = 0;
	CheckError(textNodes->get_length(&length));
	const std::wregex marker(L"-?\\{[0-9]+\\}");
	for (long index = 0; index < length; ++index) {
		IXMLDOMNodePtr node;
		CheckError(textNodes->get_item(index, &node));
		CComBSTR value;
		CheckError(node->get_text(&value));
		std::wstring original(value, value.Length());
		std::wstring cleaned = std::regex_replace(original, marker, L"");
		if (cleaned != original)
			CheckError(node->put_text(CComBSTR(cleaned.c_str())));
	}
}

}

STDMETHODIMP CExportHTMLPlugin::GetPluginId(BSTR* value)
{
	if (!value) return E_POINTER; *value = ::SysAllocString(L"export-html"); return *value ? S_OK : E_OUTOFMEMORY;
}
STDMETHODIMP CExportHTMLPlugin::GetPluginVersion(BSTR* value)
{
	if (!value) return E_POINTER; *value = ::SysAllocString(L"3.0.8"); return *value ? S_OK : E_OUTOFMEMORY;
}
STDMETHODIMP CExportHTMLPlugin::GetApiVersion(ULONG* value) { if (!value) return E_POINTER; *value = 2; return S_OK; }
STDMETHODIMP CExportHTMLPlugin::GetCapabilities(ULONGLONG* value) { if (!value) return E_POINTER; *value = 0; return S_OK; }

STDMETHODIMP CExportHTMLPlugin::Export(IFBEPluginHost* host, BSTR filename, IFBEDocumentSnapshot* document)
{
	if (!host || !document) return E_POINTER;
	LONGLONG ownerValue = 0; HRESULT hr = host->GetOwnerWindow(&ownerValue); if (FAILED(hr)) return hr;
	BSTR hostVersion = NULL, hostLocale = NULL;
	hr = host->GetHostVersion(&hostVersion); if (SUCCEEDED(hr)) hr = host->GetUiLocale(&hostLocale);
	if (hostVersion) ::SysFreeString(hostVersion); if (hostLocale) ::SysFreeString(hostLocale);
	if (FAILED(hr)) return hr;
	CComPtr<IFBECancellationToken> cancellation; hr = host->GetCancellationToken(&cancellation); if (FAILED(hr)) return hr;
	BOOL cancelled = FALSE; hr = cancellation->IsCancellationRequested(&cancelled); if (FAILED(hr) || cancelled) return FAILED(hr) ? hr : HRESULT_FROM_WIN32(ERROR_CANCELLED);
	CComPtr<IStream> stream; hr = document->OpenXmlStream(&stream); if (FAILED(hr)) return hr;
	IXMLDOMDocument2Ptr source(U::CreateDocument(false)); VARIANT_BOOL loaded = VARIANT_FALSE;
	hr = source->load(_variant_t((IUnknown*)stream), &loaded); if (FAILED(hr) || loaded != VARIANT_TRUE) { host->ReportMessage(2, CComBSTR(L"xml-load"), CComBSTR(L"snapshot XML could not be loaded")); return FAILED(hr) ? hr : E_FAIL; }
	CComPtr<IFBEProgressSink> progress; if (SUCCEEDED(host->GetProgressSink(&progress))) progress->Report(0, 1, CComBSTR(L"export-html"));
	hr = Export(static_cast<long>(ownerValue), filename, source);
	if (progress) progress->Report(1, 1, CComBSTR(L"export-html"));
	if (FAILED(hr)) host->ReportMessage(2, CComBSTR(L"export-failed"), CComBSTR(L"ExportHTML failed"));
	return hr;
}

HRESULT	CExportHTMLPlugin::Export(long hWnd, BSTR filename, IDispatch *doc)
{
	InitExportHtmlRuntimeStrings();

	HANDLE  hOut = INVALID_HANDLE_VALUE;
	CString strMessage;

	try {
		// * construct doc pointer
		IXMLDOMDocument2Ptr	    source(doc);
		// Work on a private DOM copy: export cleanup must not modify the open book.
		// cloneNode returns a generic node wrapper.  MSXML's XSL processor can
		// then lose the document-owned key() index used by html.xsl for FB2
		// binaries.  Round-trip through a private DOM keeps the editor document
		// untouched while retaining a real document owner for the transform.
		CComBSTR sourceXml;
		CheckError(source->get_xml(&sourceXml));
		IXMLDOMDocument2Ptr sourceCopy(U::CreateDocument(false));
		VARIANT_BOOL sourceLoaded = VARIANT_FALSE;
		CheckError(sourceCopy->loadXML(sourceXml, &sourceLoaded));
		if (sourceLoaded != VARIANT_TRUE)
			return S_FALSE;
		source = sourceCopy;
		CheckError(source->setProperty(bstr_t(L"SelectionLanguage"), variant_t(L"XPath")));
		CheckError(source->setProperty(bstr_t(L"SelectionNamespaces"),
			variant_t(L"xmlns:fb='http://www.gribuser.ru/xml/fictionbook/2.0'")));
		wchar_t testDomPath[MAX_PATH] = {};
		const DWORD testDomPathLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_EXPORT_HTML_DOM_PATH", testDomPath, _countof(testDomPath));
		if (testDomPathLength > 0 && testDomPathLength < _countof(testDomPath))
			CheckError(source->save(_variant_t(testDomPath)));

		// * ask the user where he wants his html
		struct HtmlExportDialogState { struct { UINT nFilterIndex; } m_ofn; wchar_t m_szFileName[MAX_PATH]; CString m_template, m_customCss; bool m_usingCustomTemplate, m_includedesc; int m_tocdepth, m_imageMaxWidth, m_imageMaxHeight; } dlg = {};
		// The portable, self-contained document is the safest default: it cannot
		// lose its CSS or images when moved to another folder or machine.
		dlg.m_ofn.nFilterIndex = 4;
		wchar_t testModeEnabled[4] = {}, testScenario[32] = {}, testOutput[MAX_PATH] = {};
		const bool deterministicTestExport = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testModeEnabled, _countof(testModeEnabled)) == 1 &&
			testModeEnabled[0] == L'1' && ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SCENARIO", testScenario, _countof(testScenario)) == wcslen(L"export-html") &&
			wcscmp(testScenario, L"export-html") == 0;
		const DWORD testOutputLength = deterministicTestExport ? ::GetEnvironmentVariable(L"FBE_NEXT_TEST_EXPORT_HTML_PATH", testOutput, _countof(testOutput)) : 0;
		if (testOutputLength > 0 && testOutputLength < _countof(testOutput)) {
			wchar_t testMode[8] = {};
			const DWORD testModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_EXPORT_HTML_MODE", testMode, _countof(testMode));
			dlg.m_ofn.nFilterIndex = testModeLength ? max(1, min(4, _wtoi(testMode))) : 4;
			::wcsncpy_s(dlg.m_szFileName, _countof(dlg.m_szFileName), testOutput, _TRUNCATE);
			dlg.m_template = U::GetProgDirFile(L"html.xsl");
			dlg.m_usingCustomTemplate = false;
		} else {
			CHtmlExportOptionsDialog options;
			options.LoadSettings();
			std::vector<CString> filterLabels, filterPatterns;
			std::vector<COMDLG_FILTERSPEC> filters;
			BuildHtmlModernFileTypes(LoadExportHtmlString(IDS_SAVE_FILE_FILTER), filterLabels, filterPatterns, filters);
			CComObject<CHtmlFileDialogEvents>* rawEvents = nullptr;
			HRESULT eventHr = CComObject<CHtmlFileDialogEvents>::CreateInstance(&rawEvents);
			if (FAILED(eventHr) || !rawEvents) return S_FALSE;
			rawEvents->AddRef();
			rawEvents->owner = (HWND)hWnd;
			rawEvents->options = &options;
			CComPtr<IFileDialogEvents> events;
			eventHr = rawEvents->QueryInterface(IID_PPV_ARGS(&events));
			rawEvents->Release();
			if (FAILED(eventHr)) return S_FALSE;
			ModernFileDialog::Request request;
			request.save = true; request.pathMustExist = true; request.overwritePrompt = true; request.defaultExtension = L"html";
			request.initialFileName = filename ? filename : L""; request.filters = filters.data(); request.filterCount = static_cast<UINT>(filters.size()); request.filterIndex = 4;
			request.events = events;
			request.customize = [](IFileDialogCustomize* customize) {
				CString button = LoadExportHtmlString(IDS_HTML_EXPORT_OPTIONS_TITLE);
				button += L"...";
				return customize->AddPushButton(CHtmlFileDialogEvents::SettingsButtonId,
					button);
			};
			const ModernFileDialog::Result result = ModernFileDialog::Show((HWND)hWnd, request);
			if (result.outcome == ModernFileDialog::Outcome::Cancelled) return S_FALSE;
			if (result.outcome == ModernFileDialog::Outcome::Failed) {
				FbeDiagnostic::HResult(L"file-dialog", L"FD203", result.error, L"Export HTML save dialog");
				return S_FALSE;
			}
			dlg.m_template = options.m_template; dlg.m_customCss = options.m_customCss; dlg.m_usingCustomTemplate = options.m_usingCustomTemplate;
			dlg.m_includedesc = options.m_includedesc; dlg.m_tocdepth = options.m_tocdepth; dlg.m_imageMaxWidth = options.m_imageMaxWidth; dlg.m_imageMaxHeight = options.m_imageMaxHeight;
			options.Persist();
			dlg.m_ofn.nFilterIndex = result.filterIndex;
			::wcsncpy_s(dlg.m_szFileName, _countof(dlg.m_szFileName), result.paths.front().c_str(), _TRUNCATE);
		}
		bool    fMIME = dlg.m_ofn.nFilterIndex == 2;
		bool    fExternalImages = dlg.m_ofn.nFilterIndex == 1;
		bool    fEmbeddedImages = dlg.m_ofn.nFilterIndex == 4;
		bool    fImages = fExternalImages || fMIME || fEmbeddedImages;
		CString customCss;
		if (!LoadUtf8TextFile(dlg.m_customCss, customCss)) {
			strMessage = FormatExportHtmlString(IDS_ERROR_OPEN_FILE, (LPCTSTR)dlg.m_customCss,
				(LPCTSTR)U::Win32ErrMsg(::GetLastError()));
			ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage,
				(LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
			return S_FALSE;
		}

		// * load template
		// MSXML does not reliably populate XSL key() indexes for a stylesheet
		// loaded through FreeThreadedDOMDocument.  html.xsl resolves FB2 binary
		// nodes through key('binary-by-id', ...), so use the regular DOM that the
		// processor contract expects.
		IXMLDOMDocument2Ptr	    tdoc(U::CreateDocument(false));
		if (!U::LoadXml(tdoc, dlg.m_template))
			return S_FALSE;
		if (fEmbeddedImages && dlg.m_usingCustomTemplate && !SupportsEmbeddedImages(tdoc)) {
			ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML,
				LoadExportHtmlString(IDS_ERROR_EMBEDDED_IMAGES_TEMPLATE),
				NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
			return S_FALSE;
		}
		IXSLTemplatePtr	    tmpl(U::CreateTemplate());
		CheckError(tmpl->putref_stylesheet(tdoc));

		// * create processor
		IXSLProcessorPtr	    proc;
		CheckError(tmpl->createProcessor(&proc));

		// * setup input
		RemoveServiceMarkers(source);
		CheckError(proc->put_input(variant_t((IDispatch*)source)));

		// * install template parameters
		CheckError(proc->addParameter(bstr_t(L"includedesc"), variant_t(dlg.m_includedesc), _bstr_t()));
		CheckError(proc->addParameter(bstr_t(L"tocdepth"), variant_t((long)dlg.m_tocdepth), _bstr_t()));
		CheckError(proc->addParameter(bstr_t(L"imagemaxwidth"), variant_t((long)dlg.m_imageMaxWidth), _bstr_t()));
		CheckError(proc->addParameter(bstr_t(L"imagemaxheight"), variant_t((long)dlg.m_imageMaxHeight), _bstr_t()));
		if (!customCss.IsEmpty())
			CheckError(proc->addParameter(bstr_t(L"customcss"), variant_t((LPCTSTR)customCss), _bstr_t()));

		// 1 = HTML and an adjacent resource folder, 2 = MHT,
		// 3 = HTML without images, 4 = self-contained HTML with data: URIs.
		CString dfile(dlg.m_szFileName);

		// * open the file
		hOut = ::CreateFile(dlg.m_szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
		if (hOut == INVALID_HANDLE_VALUE)
		{
			CString strMessage;
			strMessage = FormatExportHtmlString(IDS_ERROR_OPEN_FILE, dlg.m_szFileName, (LPCTSTR)U::Win32ErrMsg(::GetLastError()));
			ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
			return S_FALSE;
		}

		// * construct images directory
		int	  cp = dfile.ReverseFind(_T('.'));
		if (cp >= 0)
			dfile.Delete(cp, dfile.GetLength() - cp);
		dfile += _T("_files");
		if (fExternalImages) {
			// construct a relative path
			CString	relpath(dfile);
			cp = relpath.ReverseFind(_T('\\'));
			if (cp >= 0)
				relpath.Delete(0, cp + 1);

			// see if it is ascii only
			bool fAscii = true;
			for (int i = 0; i < relpath.GetLength(); ++i)
				if (relpath[i] < 32 || relpath[i]>127) {
					fAscii = false;
					break;
				}

			if (fAscii && !fMIME) {
				relpath += _T('/');
				CheckError(proc->addParameter(bstr_t(L"imgprefix"), variant_t((const TCHAR *)relpath), _bstr_t()));

				if (!::CreateDirectory(dfile, NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS) {
					DWORD	de = ::GetLastError();
					CloseHandle(hOut);
					::DeleteFile(dlg.m_szFileName);
					strMessage = FormatExportHtmlString(IDS_ERROR_CREATE_DIRECTORY, (LPCTSTR)dfile, (LPCTSTR)U::Win32ErrMsg(de));
					ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
					return S_FALSE;
				}
			}
			else
				dfile.Delete(cp, dfile.GetLength() - cp);

		}
		if (fImages)
			CheckError(proc->addParameter(bstr_t(L"saveimages"), variant_t(true), _bstr_t()));
		if (fEmbeddedImages)
			CheckError(proc->addParameter(bstr_t(L"embedimages"), variant_t(true), _bstr_t()));

		char    boundary[256];

		// * write relevant MIME headers
		if (fMIME) {
			// format date
			char  date[256];
			tm _tm;
			time_t tt;
			
			time(&tt);
			gmtime_s(&_tm, &tt);
			strftime(date, _countof(date), "%a, %d %b %Y %H:%M:%S +0000", &_tm);

			// construct some random mime boundary
			_snprintf_s(
				boundary,
				_countof(boundary),
				"------NextPart---%016llX.%08X",
				static_cast<unsigned long long>(tt),
				static_cast<unsigned int>(rand()));

			// construct mime header
			char  mime_hdr[2048];
			_snprintf_s(mime_hdr, _countof(mime_hdr),
				"From: <Saved by Haali ExportHTML Plugin>\r\n"
				"Date: %s\r\n" // Thu, 17 Apr 2003 07:34:30 +0400
				"MIME-Version: 1.0\r\n"
				"Content-Type: multipart/related; boundary=\"%s\"; type=\"text/html\"\r\n"
				"\r\n"
				"This is a multi-part message in MIME format.\r\n"
				"\r\n"
				"%s\r\n"
				"Content-Type: text/html; charset=\"utf-8\"\r\n"
				"Content-Transfer-Encoding: 8bit\r\n"
				"\r\n",
				date, boundary + 2, boundary);

			DWORD   len = strlen(mime_hdr);
			DWORD   nw;
			BOOL    fWr = WriteFile(hOut, mime_hdr, len, &nw, NULL);
			if (!fWr || nw != len)
			{
				if (!fWr)
				{
					strMessage = FormatExportHtmlString(IDS_ERROR_WRITE_FILE, dlg.m_szFileName, (LPCTSTR)U::Win32ErrMsg(::GetLastError()));
					ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
				}
				else
				{
					strMessage = FormatExportHtmlString(IDS_ERROR_WRITE_FILE2, dlg.m_szFileName);
					ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
				}
				::CloseHandle(hOut);
				::DeleteFile(dlg.m_szFileName);
				return S_FALSE;
			}
		}

		// * transform
		CheckError(proc->put_output(variant_t((IUnknown*)U::NewStream(hOut, !fMIME))));
		VARIANT_BOOL Done = VARIANT_FALSE;
		CheckError(proc->transform(&Done));

		// * save images
		if (fExternalImages || fMIME) {
			if (dfile.IsEmpty() || dfile[dfile.GetLength() - 1] != _T('\\'))
				dfile += _T('\\');
			IXMLDOMNodeListPtr      bins;
			CheckError(source->selectNodes(bstr_t(L"/fb:FictionBook/fb:binary"), &bins));
			long listLength = 0;
			CheckError(bins->get_length(&listLength));
			for (long l = 0; l < listLength; ++l) {
				try {
					IXMLDOMNodePtr   be;
					CheckError(bins->get_item(l, &be));
					IXMLDOMElementPtr element;
					CheckError(be->QueryInterface(IID_PPV_ARGS(&element)));
					_variant_t	id;
					CheckError(element->getAttribute(bstr_t(L"id"), &id));
					_variant_t	ct;
					CheckError(element->getAttribute(bstr_t(L"content-type"), &ct));
					if (V_VT(&id) != VT_BSTR || V_VT(&ct) != VT_BSTR)
						continue;

					if (fMIME) {
						// get base64 data
						CComBSTR   data;
						CheckError(be->get_text(&data));

						// allocate buffer
						char      *buffer = (char*)malloc(data.Length() + 1024);
						if (buffer == NULL)
							continue;

						// construct a MIME header
						_snprintf_s(buffer, 1024, _TRUNCATE,
							"\r\n"
							"%s\r\n"
							"Content-Type: %S\r\n"
							"Content-Transfer-Encoding: base64\r\n"
							"Content-Location: %S\r\n"
							"\r\n",
							boundary, V_BSTR(&ct), V_BSTR(&id));
						DWORD     hlen = strlen(buffer);

						// convert data to ascii
						DWORD     mlen = WideCharToMultiByte(CP_ACP, 0,
							data, data.Length(),
							buffer + hlen, data.Length(),
							NULL, NULL);

						// write a new mime header+data
						DWORD   nw;
						BOOL    fWr = WriteFile(hOut, buffer, hlen + mlen, &nw, NULL);
						DWORD   de = ::GetLastError();
						free(buffer);

						if (!fWr || nw != hlen + mlen)
						{
							if (!fWr)
							{
								strMessage = FormatExportHtmlString(IDS_ERROR_WRITE_FILE, dlg.m_szFileName, (LPCTSTR)U::Win32ErrMsg(de));
								ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
							}
							else
							{
								strMessage = FormatExportHtmlString(IDS_ERROR_WRITE_FILE2, dlg.m_szFileName);
								ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
							}
							::CloseHandle(hOut);
							::DeleteFile(dlg.m_szFileName);
							return S_FALSE;
						}
					}
					else
					{
						CheckError(be->put_dataType(bstr_t(L"bin.base64")));
						_variant_t	data;
						CheckError(be->get_nodeTypedValue(&data));
						if (V_VT(&data) != (VT_ARRAY | VT_UI1) || ::SafeArrayGetDim(V_ARRAY(&data)) != 1)
							continue;
						DWORD len = V_ARRAY(&data)->rgsabound[0].cElements;
						void	*buffer;
						::SafeArrayAccessData(V_ARRAY(&data), &buffer);
						CString fname(dfile);
						fname += V_BSTR(&id);
						HANDLE hFile = ::CreateFile(fname, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
						if (hFile == INVALID_HANDLE_VALUE && ::GetLastError() == ERROR_FILE_EXISTS)
						{
							strMessage = FormatExportHtmlString(IDS_WARNING_FILE_ALREADY_EXISTS, (LPCTSTR)fname);
							if (ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_WARNING_ICON) != IDYES)
								goto skip;
							hFile = ::CreateFile(fname, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
						}
						if (hFile != INVALID_HANDLE_VALUE)
						{
							DWORD wr;
							BOOL fWr = ::WriteFile(hFile, buffer, len, &wr, NULL);
							DWORD de = ::GetLastError();
							::CloseHandle(hFile);
							if (!fWr || wr != len)
							{
								if (!fWr)
								{
									strMessage = FormatExportHtmlString(IDS_ERROR_WRITE_FILE, (LPCTSTR)fname, (LPCTSTR)U::Win32ErrMsg(de));
									ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
								}
								else
								{
									strMessage = FormatExportHtmlString(IDS_ERROR_WRITE_FILE2, (LPCTSTR)fname);
									ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
								}
								::DeleteFile(fname);
							}
						}
						else
						{
							strMessage = FormatExportHtmlString(IDS_ERROR_OPEN_FILE, (LPCTSTR)fname, (LPCTSTR)U::Win32ErrMsg(::GetLastError()));
							ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
						}
					skip:
						::SafeArrayUnaccessData(V_ARRAY(&data));
					}
				}
				catch (const _com_error&)
				{
					// Ошибка отдельного изображения не должна прерывать экспорт остальных.
					continue;
				}
			}
		}

		// * write a final mime boundary
		if (fMIME) {
			char    mime_tmp[256];
			_snprintf_s(mime_tmp, sizeof(mime_tmp), "\r\n%s\r\n", boundary);
			DWORD   len = strlen(mime_tmp);
			DWORD   nw;
			BOOL    fWr = WriteFile(hOut, mime_tmp, len, &nw, NULL);
			if (!fWr || nw != len)
			{
				if (!fWr)
				{
					strMessage = FormatExportHtmlString(IDS_ERROR_WRITE_FILE, dlg.m_szFileName, (LPCTSTR)U::Win32ErrMsg(::GetLastError()));
					ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
				}
				else
				{
					strMessage = FormatExportHtmlString(IDS_ERROR_WRITE_FILE2, dlg.m_szFileName);
					ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage, (LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
				}
				::CloseHandle(hOut);
				::DeleteFile(dlg.m_szFileName);
				return S_FALSE;
			}
			::CloseHandle(hOut);
		}
	}
	catch (_com_error& e)
	{
		if (hOut != INVALID_HANDLE_VALUE)
			CloseHandle(hOut);
		U::ReportError(e);
		return S_FALSE;
	}
	return S_OK;
}
