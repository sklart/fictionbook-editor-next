#pragma once

#include <atlstr.h>
#include <atlcomcli.h>
#include <vector>

// Synchronous request passed from window.external to the editor-owned XML
// backend.  It deliberately contains no parsing or document logic.
enum class XmlScriptApiOperation
{
	GetSourceText,
	ValidateSourceText,
	ApplySourceText,
	GetScriptingEnvironment,
	ScriptingDocumentOperation,
	ValidateCurrentDocument
};

struct XmlScriptApiRequest
{
	XmlScriptApiOperation operation;
	const CString* text;
	const CString* action;
	CString resultText;
	bool succeeded;
	bool valid;
	int line;
	int column;
	CString message;
	CString scriptPath, scriptUid, viewMode;
	bool documentModified = false;
	DISPID scriptingMethod = 0;
    GUID binaryDocumentIdentity = GUID_NULL;
	std::vector<CComVariant> arguments;
	CComVariant returnValue;
	HRESULT error = S_OK;

	explicit XmlScriptApiRequest(XmlScriptApiOperation requestedOperation = XmlScriptApiOperation::GetSourceText) :
		operation(requestedOperation), text(NULL), action(NULL), succeeded(false),
		valid(false), line(0), column(0) {}
};
