#include "stdafx.h"
#include "ExportHTMLPlugin.h"

#include "utils.h"
#include "HtmlExportOptionsDialog.h"
#include "HtmlExportResourceAudit.h"
#include "HtmlExportXslParameters.h"
#include "HtmlExportWriterHelpers.h"
#include "HtmlExportWriter.h"
#include "HtmlSplitExport.h"
#include "..\\common\\ModernFileDialog.h"
#include "RuntimeLocalization.h"
#include "..\\version.h"

#include <vector>

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

std::wstring SplitIndexPath(const std::wstring& selectedPath)
{
    const std::wstring::size_type slash = selectedPath.find_last_of(L"\\/");
    return (slash == std::wstring::npos ? std::wstring() : selectedPath.substr(0, slash + 1)) + L"index.html";
}

HRESULT WriteUtf8Stream(IStream* stream, const std::wstring& text)
{
    if (!stream) return E_POINTER;
    const int length = text.empty() ? 0 : ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
    if (!text.empty() && length == 0) return HRESULT_FROM_WIN32(::GetLastError());
    std::vector<char> bytes(static_cast<size_t>(length));
    if (length > 0 && ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), bytes.data(), length, NULL, NULL) == 0) return HRESULT_FROM_WIN32(::GetLastError());
    ULONG written = 0;
    const HRESULT hr = bytes.empty() ? S_OK : stream->Write(bytes.data(), static_cast<ULONG>(bytes.size()), &written);
    return SUCCEEDED(hr) && written == bytes.size() ? S_OK : FAILED(hr) ? hr : STG_E_WRITEFAULT;
}
std::wstring ReadUtf8Stream(IStream* stream)
{
	STATSTG stat = {};
	CheckError(stream->Stat(&stat, STATFLAG_NONAME));
	LARGE_INTEGER start = {};
	CheckError(stream->Seek(start, STREAM_SEEK_SET, NULL));
	std::vector<char> bytes(static_cast<size_t>(stat.cbSize.QuadPart));
	ULONG read = 0;
	if (!bytes.empty()) CheckError(stream->Read(&bytes[0], static_cast<ULONG>(bytes.size()), &read));
	if (read != bytes.size()) throw _com_error(E_FAIL);
	const int length = bytes.empty() ? 0 : ::MultiByteToWideChar(CP_UTF8, 0,
		&bytes[0], static_cast<int>(bytes.size()), NULL, 0);
	if (!bytes.empty() && length == 0) throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
	std::wstring text(static_cast<size_t>(length), L'\0');
	if (length > 0) ::MultiByteToWideChar(CP_UTF8, 0, &bytes[0],
		static_cast<int>(bytes.size()), &text[0], length);
	return text;
}

}

STDMETHODIMP CExportHTMLPlugin::GetPluginId(BSTR* value)
{
	if (!value) return E_POINTER; *value = ::SysAllocString(L"export-html"); return *value ? S_OK : E_OUTOFMEMORY;
}
STDMETHODIMP CExportHTMLPlugin::GetPluginVersion(BSTR* value)
{
	if (!value) return E_POINTER; *value = ::SysAllocString(FBE_VERSION_WSTRING); return *value ? S_OK : E_OUTOFMEMORY;
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
	hr = ExportCore(static_cast<long>(ownerValue), filename, source);
	if (SUCCEEDED(hr) && progress) progress->Report(1, 1, CComBSTR(L"export-html"));
	if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_CANCELLED))
		host->ReportMessage(2, CComBSTR(L"export-failed"), CComBSTR(L"ExportHTML failed"));
	return hr;
}

HRESULT CExportHTMLPlugin::Export(long hWnd, BSTR filename, IDispatch* doc)
{
	// v1 historically treats both a dialog cancellation and an export error as
	// a non-exceptional, non-success result.  Keep that observable contract.
	const HRESULT result = ExportCore(hWnd, filename, doc);
	return FAILED(result) ? S_FALSE : result;
}

HRESULT CExportHTMLPlugin::ExportCore(long hWnd, BSTR filename, IDispatch *doc)
{
	wchar_t testModeValue[4] = {}, testCancel[4] = {}, testFail[4] = {}, testScenarioValue[32] = {};
	const bool exportHtmlTest = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testModeValue, _countof(testModeValue)) == 1 && testModeValue[0] == L'1' &&
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_SCENARIO", testScenarioValue, _countof(testScenarioValue)) == wcslen(L"export-html") && wcscmp(testScenarioValue, L"export-html") == 0;
	if (exportHtmlTest && ::GetEnvironmentVariable(L"FBE_NEXT_TEST_EXPORT_HTML_FAIL", testFail, _countof(testFail)) == 1 && testFail[0] == L'1')
		return E_FAIL;
	if (exportHtmlTest &&
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_EXPORT_HTML_CANCEL", testCancel, _countof(testCancel)) == 1 && testCancel[0] == L'1')
		return HRESULT_FROM_WIN32(ERROR_CANCELLED);
	InitExportHtmlRuntimeStrings();

	CString strMessage;
	HtmlExportSettings exportSettings;

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
			return E_FAIL;
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
		dlg.m_ofn.nFilterIndex = static_cast<UINT>(ExportHtmlFormatMode::Standalone);
		wchar_t testModeEnabled[4] = {}, dialogTestScenario[32] = {}, testOutput[MAX_PATH] = {};
		const bool deterministicTestExport = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testModeEnabled, _countof(testModeEnabled)) == 1 &&
			testModeEnabled[0] == L'1' && ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SCENARIO", dialogTestScenario, _countof(dialogTestScenario)) == wcslen(L"export-html") &&
			wcscmp(dialogTestScenario, L"export-html") == 0;
		const DWORD testOutputLength = deterministicTestExport ? ::GetEnvironmentVariable(L"FBE_NEXT_TEST_EXPORT_HTML_PATH", testOutput, _countof(testOutput)) : 0;
		if (testOutputLength > 0 && testOutputLength < _countof(testOutput)) {
			wchar_t testExportMode[8] = {};
			const DWORD testModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_EXPORT_HTML_MODE", testExportMode, _countof(testExportMode));
			dlg.m_ofn.nFilterIndex = static_cast<UINT>(testModeLength
				? ExportHtmlFormatModeFromFilterIndex(static_cast<UINT>(max(0, _wtoi(testExportMode))))
				: ExportHtmlFormatMode::Standalone);
			::wcsncpy_s(dlg.m_szFileName, _countof(dlg.m_szFileName), testOutput, _TRUNCATE);
			dlg.m_template = U::GetProgDirFile(L"html.xsl");
			dlg.m_usingCustomTemplate = false;
			exportSettings.templatePath = dlg.m_template;
            wchar_t testSplit[4] = {};
            if (::GetEnvironmentVariable(L"FBE_NEXT_TEST_EXPORT_HTML_SPLIT", testSplit, _countof(testSplit)) == 1 && testSplit[0] == L'1')
                exportSettings.documentStructure = 1;
            wchar_t testNotePlacement[8] = {};
            if (::GetEnvironmentVariable(L"FBE_NEXT_TEST_EXPORT_HTML_NOTE_PLACEMENT", testNotePlacement, _countof(testNotePlacement)) > 0)
                exportSettings.notePlacement = max(0, min(2, _wtoi(testNotePlacement)));
		} else {
			CHtmlExportOptionsDialog options;
			options.LoadSettings();
			std::vector<CString> filterLabels, filterPatterns;
			std::vector<COMDLG_FILTERSPEC> filters;
			BuildHtmlModernFileTypes(LoadExportHtmlString(IDS_SAVE_FILE_FILTER), filterLabels, filterPatterns, filters);
			CComObject<CHtmlFileDialogEvents>* rawEvents = nullptr;
			HRESULT eventHr = CComObject<CHtmlFileDialogEvents>::CreateInstance(&rawEvents);
			if (FAILED(eventHr) || !rawEvents) return FAILED(eventHr) ? eventHr : E_FAIL;
			rawEvents->AddRef();
			rawEvents->owner = (HWND)hWnd;
			rawEvents->options = &options;
			CComPtr<IFileDialogEvents> events;
			eventHr = rawEvents->QueryInterface(IID_PPV_ARGS(&events));
			rawEvents->Release();
			if (FAILED(eventHr)) return eventHr;
			ModernFileDialog::Request request;
			request.save = true; request.pathMustExist = true; request.overwritePrompt = true; request.defaultExtension = L"html";
			request.okButtonLabel = LoadExportHtmlString(IDS_SAVE_BUTTON).GetString();
			request.initialFileName = filename ? filename : L""; request.filters = filters.data(); request.filterCount = static_cast<UINT>(filters.size()); request.filterIndex = static_cast<UINT>(ExportHtmlFormatMode::Standalone);
			request.events = events;
            options.SetSplitSupported(ExportHtmlFormatSupportsSplit(ExportHtmlFormatModeFromFilterIndex(request.filterIndex)));
			request.customize = [](IFileDialogCustomize* customize) {
				CString button = LoadExportHtmlString(IDS_HTML_EXPORT_OPTIONS_TITLE);
				button += L"...";
				return customize->AddPushButton(CHtmlFileDialogEvents::SettingsButtonId,
					button);
			};
			const ModernFileDialog::Result result = ModernFileDialog::Show((HWND)hWnd, request);
			if (result.outcome == ModernFileDialog::Outcome::Cancelled) return HRESULT_FROM_WIN32(ERROR_CANCELLED);
			if (result.outcome == ModernFileDialog::Outcome::Failed) {
				FbeDiagnostic::HResult(L"file-dialog", L"FD203", result.error, L"Export HTML save dialog");
				return FAILED(result.error) ? result.error : E_FAIL;
			}
			exportSettings = options.m_settings;
			dlg.m_template = exportSettings.templatePath; dlg.m_customCss = exportSettings.customCss; dlg.m_usingCustomTemplate = exportSettings.usingCustomTemplate;
			dlg.m_includedesc = exportSettings.includeDescription; dlg.m_tocdepth = exportSettings.tocDepth; dlg.m_imageMaxWidth = exportSettings.imageMaxWidth; dlg.m_imageMaxHeight = exportSettings.imageMaxHeight;
			options.Persist();
			dlg.m_ofn.nFilterIndex = static_cast<UINT>(ExportHtmlFormatModeFromFilterIndex(result.filterIndex));
			::wcsncpy_s(dlg.m_szFileName, _countof(dlg.m_szFileName), result.paths.front().c_str(), _TRUNCATE);
		}
		const ExportHtmlFormatMode formatMode = ExportHtmlFormatModeFromFilterIndex(dlg.m_ofn.nFilterIndex);
		bool    fMIME = formatMode == ExportHtmlFormatMode::MHT;
		bool    fExternalImages = formatMode == ExportHtmlFormatMode::ExternalImages;
		bool    fEmbeddedImages = formatMode == ExportHtmlFormatMode::Standalone;
		bool    fImages = fExternalImages || fMIME || fEmbeddedImages;
		// Split is intentionally limited to the bundled XSL and external-images/HTML-only modes. MHT and
		// standalone retain their byte-for-byte single-file writer paths.
		const bool fSplit = exportSettings.documentStructure == 1 && !dlg.m_usingCustomTemplate && ExportHtmlFormatSupportsSplit(formatMode);
		CString customCss;
		if (!LoadUtf8TextFile(dlg.m_customCss, customCss)) {
			strMessage = FormatExportHtmlString(IDS_ERROR_OPEN_FILE, (LPCTSTR)dlg.m_customCss,
				(LPCTSTR)U::Win32ErrMsg(::GetLastError()));
			ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, (LPCTSTR)strMessage,
				(LPCTSTR)NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
			return E_FAIL;
		}

		// * load template
		// MSXML does not reliably populate XSL key() indexes for a stylesheet
		// loaded through FreeThreadedDOMDocument.  html.xsl resolves FB2 binary
		// nodes through key('binary-by-id', ...), so use the regular DOM that the
		// processor contract expects.
		IXMLDOMDocument2Ptr	    tdoc(U::CreateDocument(false));
		if (!U::LoadXml(tdoc, dlg.m_template))
			return E_FAIL;
		if (fEmbeddedImages && dlg.m_usingCustomTemplate && !SupportsEmbeddedImages(tdoc)) {
			ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML,
				LoadExportHtmlString(IDS_ERROR_EMBEDDED_IMAGES_TEMPLATE),
				NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
			return E_FAIL;
		}
		IXSLTemplatePtr	    tmpl(U::CreateTemplate());
		CheckError(tmpl->putref_stylesheet(tdoc));

		if ((fEmbeddedImages || fMIME) && FAILED(HtmlExportWriter::NormalizeBinaryMimeTypes(source))) return E_FAIL;

		// * create processor
		IXSLProcessorPtr	    proc;
		CheckError(tmpl->createProcessor(&proc));

		// * setup input
		// ExportHTML cannot reliably distinguish editor markers from legitimate
		// book text such as "{2026}", so it never strips brace-delimited text.
		CheckError(proc->put_input(variant_t((IDispatch*)source)));

		// Keep XSL parameters behind a value-model adapter; ExportCore remains a writer.
		HtmlExportXslParameters::Apply(proc, exportSettings, customCss);

		// Standalone = 1, ExternalImages = 2, MHT = 3, HtmlOnly = 4.
		HtmlExportWriter::Options writerOptions;
		writerOptions.targetPath = fSplit ? SplitIndexPath(dlg.m_szFileName) : std::wstring(dlg.m_szFileName);
		writerOptions.mime = fMIME;
		writerOptions.externalImages = fExternalImages;
		writerOptions.standalone = fEmbeddedImages;
		writerOptions.split = fSplit;
		writerOptions.externalImagesFolderMode = fExternalImages ? exportSettings.externalImagesFolderMode : 0;
		writerOptions.externalImagesFolderName = fExternalImages ? std::wstring((LPCWSTR)exportSettings.externalImagesFolderName) : std::wstring();
		HtmlExportWriter::Callbacks writerCallbacks;
		writerCallbacks.reportFailure = [](HtmlExportWriter::Failure failure, const std::wstring& path, DWORD error) {
			CString message;
			switch (failure) {
			case HtmlExportWriter::Failure::CreateImagesDirectory:
				message = FormatExportHtmlString(IDS_ERROR_CREATE_DIRECTORY, path.c_str(), (LPCTSTR)U::Win32ErrMsg(error));
				break;
			case HtmlExportWriter::Failure::ShortWrite:
			case HtmlExportWriter::Failure::ShortImageWrite:
				message = FormatExportHtmlString(IDS_ERROR_WRITE_FILE2, path.c_str());
				break;
			case HtmlExportWriter::Failure::OpenTarget:
			case HtmlExportWriter::Failure::OpenImage:
				message = FormatExportHtmlString(IDS_ERROR_OPEN_FILE, path.c_str(), (LPCTSTR)U::Win32ErrMsg(error));
				break;
			default:
				message = FormatExportHtmlString(IDS_ERROR_WRITE_FILE, path.c_str(), (LPCTSTR)U::Win32ErrMsg(error));
				break;
			}
			ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, message, NULL, TDCBF_OK_BUTTON, TD_ERROR_ICON);
		};
        writerCallbacks.confirmImageOverwrite = [](const std::wstring& path) {
            wchar_t testConfirm[4] = {};
            if (::GetEnvironmentVariable(L"FBE_NEXT_TEST_EXPORT_HTML_CONFIRM_OVERWRITE", testConfirm, _countof(testConfirm)) == 1)
                return testConfirm[0] == L'1';
            const CString message = FormatExportHtmlString(IDS_WARNING_FILE_ALREADY_EXISTS, path.c_str());
            return ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML, message, NULL,
                TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_WARNING_ICON) == IDYES;
        };
		HtmlExportWriter::Writer writer(writerOptions, writerCallbacks);
		HRESULT writerResult = writer.Prepare();
		if (FAILED(writerResult)) return writerResult;

		CString imagePrefix;
		if (fExternalImages) imagePrefix = writer.ImagePaths().imgPrefix.c_str();
		HtmlExportXslParameters::ApplyImageMode(proc, fImages, fEmbeddedImages, imagePrefix);

		CComPtr<IStream> transformOutput;
		if (fSplit)
			CheckError(::CreateStreamOnHGlobal(NULL, TRUE, &transformOutput));
		else
			CheckError(writer.GetTransformOutput(&transformOutput));
		CheckError(proc->put_output(variant_t((IUnknown*)transformOutput)));
		VARIANT_BOOL Done = VARIANT_FALSE;
		CheckError(proc->transform(&Done));
		if (fSplit) {
			HtmlSplitExport::Plan splitPlan;
			if (!HtmlSplitExport::BuildPlan(ReadUtf8Stream(transformOutput), splitPlan)) return E_FAIL;
            writerResult = writer.WriteSplitDocument(L"index.html", splitPlan.indexHtml);
            if (writerResult == S_FALSE) return HRESULT_FROM_WIN32(ERROR_CANCELLED);
            if (FAILED(writerResult)) return writerResult;
			for (const HtmlSplitExport::SectionDocument& section : splitPlan.sections) {
				writerResult = writer.WriteSplitDocument(section.fileName, section.html);
				if (writerResult == S_FALSE) return HRESULT_FROM_WIN32(ERROR_CANCELLED);
                if (FAILED(writerResult)) return writerResult;
			}
		}
		if (fEmbeddedImages) {
			IStream* standaloneOutput = writer.StandaloneOutput();
			if (standaloneOutput == NULL) return E_UNEXPECTED;
			STATSTG standaloneStat = {};
			CheckError(standaloneOutput->Stat(&standaloneStat, STATFLAG_NONAME));
			if (HtmlExportWriterHelpers::IsStandaloneWarningRequired(
				static_cast<unsigned long long>(standaloneStat.cbSize.QuadPart),
				static_cast<unsigned long long>(exportSettings.standaloneWarningMiB)) &&
				ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML,
					FormatExportHtmlString(IDS_WARNING_STANDALONE_SIZE, exportSettings.standaloneWarningMiB), NULL,
					TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_WARNING_ICON) != IDYES)
				return HRESULT_FROM_WIN32(ERROR_CANCELLED);
			const std::vector<std::wstring> dependencies = HtmlExportResourceAudit::FindExternalDependencies(ReadUtf8Stream(standaloneOutput));
			if (!dependencies.empty() && ShowExportHtmlTaskDialog(::GetActiveWindow(), IDR_EXPORTHTML,
				FormatExportHtmlString(IDS_WARNING_EXTERNAL_RESOURCES, static_cast<int>(dependencies.size())), NULL,
				TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, TD_WARNING_ICON) != IDYES)
				return HRESULT_FROM_WIN32(ERROR_CANCELLED);
			writerResult = writer.WriteStandaloneToTarget();
			if (FAILED(writerResult)) return writerResult;
		}

		writerResult = writer.WriteImages(source);
		if (FAILED(writerResult)) return writerResult;
		writerResult = writer.Finalize();
		if (FAILED(writerResult)) return writerResult;
		writer.Commit();
	}
	catch (_com_error& e)
	{
		U::ReportError(e);
		return e.Error();
	}
	return S_OK;
}
