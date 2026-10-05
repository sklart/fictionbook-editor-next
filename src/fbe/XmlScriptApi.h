#pragma once

#include <atlstr.h>

// Synchronous request passed from window.external to the editor-owned XML
// backend.  It deliberately contains no parsing or document logic.
enum class XmlScriptApiOperation
{
	GetSourceText,
	ValidateSourceText,
	ApplySourceText
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

	explicit XmlScriptApiRequest(XmlScriptApiOperation requestedOperation = XmlScriptApiOperation::GetSourceText) :
		operation(requestedOperation), text(NULL), action(NULL), succeeded(false),
		valid(false), line(0), column(0) {}
};
