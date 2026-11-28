#pragma once

#include "PluginManager.h"

enum class PluginExecutionStatus
{
	Success,
	Cancelled,
	Failed
};

enum class PluginExecutionFailure
{
	None,
	CreateInstance,
	ApiNegotiation,
	InterfaceUnavailable,
	HostCreation,
	PluginCall,
	ResultStream,
	SnapshotCreation
};

struct PluginImportResult
{
	PluginExecutionStatus status = PluginExecutionStatus::Failed;
	PluginExecutionFailure failure = PluginExecutionFailure::None;
	HRESULT hr = E_FAIL;
	CLSID clsid = CLSID_NULL;
	CString suggestedFilename;
	CString warning, error;
	MSXML2::IXMLDOMDocument2Ptr document;
	bool Succeeded() const { return status == PluginExecutionStatus::Success; }
	bool Cancelled() const { return status == PluginExecutionStatus::Cancelled; }
};

struct PluginExportRequest
{
	CLSID clsid = CLSID_NULL;
	HWND owner = NULL;
	CString interfaceLanguage;
	MSXML2::IXMLDOMDocument2Ptr document;
	CString sourceFilename;
	CString documentEncoding;
};

struct PluginExecutionResult
{
	PluginExecutionStatus status = PluginExecutionStatus::Failed;
	PluginExecutionFailure failure = PluginExecutionFailure::None;
	HRESULT hr = E_FAIL;
	CLSID clsid = CLSID_NULL;
	CString warning, error;
	bool Succeeded() const { return status == PluginExecutionStatus::Success; }
	bool Cancelled() const { return status == PluginExecutionStatus::Cancelled; }
};

class PluginExecutionController
{
public:
	PluginImportResult Import(PluginManager& manager, const CLSID& clsid,
		HWND owner, const CString& interfaceLanguage) const;
	PluginExecutionResult Export(PluginManager& manager,
		const PluginExportRequest& request) const;
};
