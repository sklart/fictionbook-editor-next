#pragma once

#include <commctrl.h>

namespace FbeScriptDiagnostics {

inline CString& ScriptPath()
{
	static CString value;
	return value;
}

inline CString& ScriptSource()
{
	static CString value;
	return value;
}

inline bool& IsLoading()
{
	static bool value = false;
	return value;
}

inline bool& ErrorReported()
{
	static bool value = false;
	return value;
}

inline void SetContext(const CString& path, const CString& source)
{
	ScriptPath() = path;
	ScriptSource() = source;
	IsLoading() = true;
	ErrorReported() = false;
}

inline void FinishLoading()
{
	IsLoading() = false;
}

inline CString FileName(const CString& path)
{
	int separator = max(path.ReverseFind(L'\\'), path.ReverseFind(L'/'));
	return separator >= 0 ? path.Mid(separator + 1) : path;
}

inline CString SourceContext(ULONG line, LONG column)
{
	const CString& source = ScriptSource();
	int start = 0;
	for (ULONG current = 0; current < line; ++current) {
		start = source.Find(L'\n', start);
		if (start < 0)
			return CString();
		++start;
	}
	int end = source.Find(L'\n', start);
	if (end < 0)
		end = source.GetLength();
	CString text = source.Mid(start, end - start);
	if (!text.IsEmpty() && text[text.GetLength() - 1] == L'\r')
		text.Truncate(text.GetLength() - 1);
	CString marker;
	for (int i = 0; i < max(0, min(static_cast<int>(column), text.GetLength())); ++i)
		marker += text[i] == L'\t' ? L"    " : L" ";
	CString result;
	result.Format(L"%d: %s\r\n    %s^", line + 1, text, marker);
	return result;
}

inline void Copy(const CString& details)
{
	if (!OpenClipboard(NULL))
		return;
	EmptyClipboard();
	const SIZE_T bytes = (static_cast<SIZE_T>(details.GetLength()) + 1) * sizeof(wchar_t);
	HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
	if (data != NULL) {
		void* target = GlobalLock(data);
		if (target != NULL) {
			memcpy(target, static_cast<LPCWSTR>(details), bytes);
			GlobalUnlock(data);
			if (SetClipboardData(CF_UNICODETEXT, data) != NULL)
				data = NULL;
		}
		if (data != NULL)
			GlobalFree(data);
	}
	CloseClipboard();
}

inline void ShowDetails(HWND owner, const CString& details)
{
	CString title = FbeLoadCString(IDS_SCRIPT_MSG_CPT);
	CString copy = FbeLoadCString(IDS_SCRIPT_COPY_DETAILS);
	CString close = FbeLoadCString(IDS_SCRIPT_CLOSE_DETAILS);
	TASKDIALOG_BUTTON buttons[] = { { 1001, copy }, { IDCANCEL, close } };
	TASKDIALOGCONFIG config = { sizeof(config) };
	config.hwndParent = owner;
	config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
	config.pszWindowTitle = title;
	config.pszMainIcon = TD_ERROR_ICON;
	config.pszContent = details;
	config.cButtons = _countof(buttons);
	config.pButtons = buttons;
	int button = IDCANCEL;
	do {
		TaskDialogIndirect(&config, &button, NULL, NULL);
		if (button == 1001)
			Copy(details);
	} while (button == 1001);
}

inline void ShowLoad(HWND owner, const CString& path, const CString& message, HRESULT code)
{
	CString details;
	details.Format(FbeLoadCString(IDS_SCRIPT_LOAD_DIAGNOSTIC_MSG),
		FileName(path), path, message, static_cast<unsigned long>(code));
	ShowDetails(owner, details);
}

inline void Show(HWND owner, const EXCEPINFO& exception, ULONG line, LONG column)
{
	ErrorReported() = true;

	CString format = FbeLoadCString(IsLoading() ? IDS_SCRIPT_PARSE_DIAGNOSTIC_MSG : IDS_SCRIPT_RUNTIME_DIAGNOSTIC_MSG);
	CString description = exception.bstrDescription != NULL ? CString(exception.bstrDescription) : FbeLoadCString(IDS_SCRIPT_MSG);
	CString details;
	details.Format(format, FileName(ScriptPath()), ScriptPath(), line + 1, column + 1,
		SourceContext(line, column), description, static_cast<unsigned long>(exception.scode));
	ShowDetails(owner, details);
}

}
