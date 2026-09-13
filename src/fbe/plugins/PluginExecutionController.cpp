#include "stdafx.h"
#include "PluginExecutionController.h"
#include "PluginApiV2.h"
#include "../StartupTrace.h"

namespace
{
void TracePluginExecution(const wchar_t* type, const CLSID& clsid,
	const wchar_t* operation, HRESULT result, int domReturned)
{
	wchar_t clsidText[64] = {};
	::StringFromGUID2(clsid, clsidText, _countof(clsidText));
	CString details;
	details.Format(L"type=%s; clsid=%s; operation=%s; dom-returned=%d",
		type, clsidText, operation, domReturned);
	StartupTrace::Event(L"plugin", SUCCEEDED(result) ? L"P200" : L"P201", details);
}

PluginExecutionResult Failure(const CLSID& clsid, PluginExecutionFailure failure, HRESULT hr)
{
	PluginExecutionResult result;
	result.clsid = clsid; result.failure = failure; result.hr = hr;
	return result;
}
}

PluginImportResult PluginExecutionController::Import(PluginManager& manager,
	const CLSID& clsid, HWND owner, const CString& interfaceLanguage) const
{
	PluginImportResult result; result.clsid = clsid;
	TracePluginExecution(L"Import", clsid, L"begin", S_OK, 0);
	IUnknownPtr instance;
	HRESULT hr = manager.CreateInstance(clsid, instance);
	TracePluginExecution(L"Import", clsid, L"CreateInstance", hr, 0);
	if (FAILED(hr)) { result.failure = PluginExecutionFailure::CreateInstance; result.hr = hr; return result; }
	hr = manager.NegotiateApi(clsid, instance);
	TracePluginExecution(L"Import", clsid, L"NegotiateApiV2", hr, SUCCEEDED(hr) ? 1 : 0);
	if (FAILED(hr)) { result.failure = PluginExecutionFailure::ApiNegotiation; result.hr = hr; return result; }
	CComQIPtr<IFBEImportPlugin2> plugin(instance);
	if (!plugin) { result.failure = PluginExecutionFailure::InterfaceUnavailable; result.hr = E_NOINTERFACE; TracePluginExecution(L"Import", clsid, L"QueryInterfaceV2", result.hr, 0); return result; }
	CComPtr<IFBEPluginHost> host;
	hr = FbePluginApiV2::CreateHost(owner, interfaceLanguage, &host);
	if (FAILED(hr)) { TracePluginExecution(L"Import", clsid, L"CreateHost", hr, 0); result.failure = PluginExecutionFailure::HostCreation; result.hr = hr; return result; }
	CComBSTR suggestedFilename; CComPtr<IStream> stream;
	hr = plugin->Import(host, &suggestedFilename, &stream);
	TracePluginExecution(L"Import", clsid, L"ImportV2", hr, stream ? 1 : 0);
	if (FAILED(hr)) { result.failure = PluginExecutionFailure::PluginCall; result.hr = hr; return result; }
	if (!stream) { result.failure = PluginExecutionFailure::ResultStream; result.hr = E_FAIL; TracePluginExecution(L"Import", clsid, L"ImportV2Stream", result.hr, 0); return result; }
	CComPtr<MSXML2::IXMLDOMDocument2> document;
	hr = document.CoCreateInstance(L"Msxml2.DOMDocument.6.0");
	if (FAILED(hr)) { TracePluginExecution(L"Import", clsid, L"DOM result", hr, 0); result.failure = PluginExecutionFailure::ResultStream; result.hr = hr; return result; }
	CComQIPtr<IPersistStreamInit> loader(document);
	if (!loader) { result.failure = PluginExecutionFailure::ResultStream; result.hr = E_NOINTERFACE; TracePluginExecution(L"Import", clsid, L"ImportV2StreamLoader", result.hr, 0); return result; }
	hr = loader->Load(stream);
	if (FAILED(hr)) { TracePluginExecution(L"Import", clsid, L"ImportV2StreamLoader", hr, 0); result.failure = PluginExecutionFailure::ResultStream; result.hr = hr; return result; }
	result.suggestedFilename = static_cast<LPCWSTR>(suggestedFilename);
	result.document = document.p; result.status = PluginExecutionStatus::Success; result.failure = PluginExecutionFailure::None; result.hr = S_OK;
	TracePluginExecution(L"Import", clsid, L"DOM result", S_OK, 1);
	return result;
}

PluginExecutionResult PluginExecutionController::Export(PluginManager& manager,
	const PluginExportRequest& request) const
{
	PluginExecutionResult result; result.clsid = request.clsid;
	TracePluginExecution(L"Export", request.clsid, L"begin", S_OK, 0);
	IUnknownPtr instance; HRESULT hr = manager.CreateInstance(request.clsid, instance);
	TracePluginExecution(L"Export", request.clsid, L"CreateInstance", hr, 0);
	if (FAILED(hr)) return Failure(request.clsid, PluginExecutionFailure::CreateInstance, hr);
	hr = manager.NegotiateApi(request.clsid, instance);
	TracePluginExecution(L"Export", request.clsid, L"NegotiateApiV2", hr, SUCCEEDED(hr) ? 1 : 0);
	if (FAILED(hr)) return Failure(request.clsid, PluginExecutionFailure::ApiNegotiation, hr);
	CComQIPtr<IFBEExportPlugin2> plugin(instance);
	if (!plugin) { TracePluginExecution(L"Export", request.clsid, L"QueryInterfaceV2", E_NOINTERFACE, 0); return Failure(request.clsid, PluginExecutionFailure::InterfaceUnavailable, E_NOINTERFACE); }
	CComPtr<IFBEPluginHost> host; hr = FbePluginApiV2::CreateHost(request.owner, request.interfaceLanguage, &host);
	if (FAILED(hr)) { TracePluginExecution(L"Export", request.clsid, L"CreateHost", hr, 0); return Failure(request.clsid, PluginExecutionFailure::HostCreation, hr); }
	CComPtr<IFBEDocumentSnapshot> snapshot;
	hr = FbePluginApiV2::CreateSnapshot(request.document, request.sourceFilename,
		request.documentEncoding, &snapshot);
	if (FAILED(hr)) { TracePluginExecution(L"Export", request.clsid, L"CreateSnapshot", hr, 0); return Failure(request.clsid, PluginExecutionFailure::SnapshotCreation, hr); }
	hr = plugin->Export(host, _bstr_t(static_cast<LPCWSTR>(request.sourceFilename)), snapshot);
	TracePluginExecution(L"Export", request.clsid, L"ExportV2", hr, 0);
	if (FAILED(hr)) return Failure(request.clsid, PluginExecutionFailure::PluginCall, hr);
	result.status = PluginExecutionStatus::Success; result.failure = PluginExecutionFailure::None; result.hr = S_OK;
	TracePluginExecution(L"Export", request.clsid, L"completed", S_OK, 0);
	return result;
}
