#pragma once

#include "..\..\FictionBookFileType.h"

namespace DocumentFileDialogs
{
	struct OpenResult { bool accepted; CString path; OpenResult() : accepted(false) {} };
	struct SaveRequest { CString initialFileName; CString currentFileName; CString selectedEncoding; CString encodingList; };
	struct SaveResult { bool accepted; CString path; CString encoding; FictionBookFileType documentType; SaveResult() : accepted(false), documentType(FictionBookFileType::Unknown) {} };

	OpenResult ShowOpen(HWND owner);
	SaveResult ShowSave(HWND owner, const SaveRequest& request);
}
