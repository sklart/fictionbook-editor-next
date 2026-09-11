#include "stdafx.h"
#include "DocumentFileDialogs.h"
#include "..\..\..\common\ModernFileDialog.h"
#include "..\..\StartupTrace.h"
#include "..\..\RuntimeLocalization.h"

namespace DocumentFileDialogs
{
	OpenResult ShowOpen(HWND owner)
	{
		const COMDLG_FILTERSPEC filters[] = {{L"FictionBook files (*.fb2;*.fbd;*.zip;*.rar)", L"*.fb2;*.fbd;*.zip;*.rar"}, {L"FictionBook (*.fb2;*.fbd)", L"*.fb2;*.fbd"}, {L"Archives (*.zip;*.rar)", L"*.zip;*.rar"}, {L"All files (*.*)", L"*.*"}};
		ModernFileDialog::Request request; request.fileMustExist = true; request.pathMustExist = true; request.defaultExtension = L"fb2"; request.filters = filters; request.filterCount = _countof(filters); request.filterIndex = 1;
		const ModernFileDialog::Result result = ModernFileDialog::Show(owner, request);
		if (result.outcome == ModernFileDialog::Outcome::Failed) StartupTrace::HResult(L"file-dialog", L"FD101", result.error, L"Open FictionBook dialog");
		OpenResult open; if (result.outcome == ModernFileDialog::Outcome::Accepted) { open.accepted = true; open.path = result.paths.front().c_str(); } return open;
	}

	SaveResult ShowSave(HWND owner, const SaveRequest& input)
	{
		const COMDLG_FILTERSPEC filters[] = {{L"FictionBook (*.fb2)", L"*.fb2"}, {L"FictionBook Description (*.fbd)", L"*.fbd"}, {L"All files (*.*)", L"*.*"}};
		CString selectedEncoding(input.selectedEncoding);
		ModernFileDialog::Request request; request.save = true; request.pathMustExist = true; request.overwritePrompt = true; request.defaultExtension = L"fb2"; request.initialFileName = input.initialFileName; request.filters = filters; request.filterCount = _countof(filters); request.filterIndex = IsFbdFile(input.initialFileName) ? 2 : 1;
		request.customize = [&input, &selectedEncoding](IFileDialogCustomize* customize) -> HRESULT { const DWORD labelId = 1000, controlId = 1001; HRESULT hr = customize->StartVisualGroup(labelId, FbeLoadRuntimeStringByKey(L"fbe.save_as.encoding", L"Encoding:").GetString()); if (FAILED(hr)) return hr; hr = customize->AddComboBox(controlId); if (FAILED(hr)) return hr; int index = 0, selectedIndex = 0; CString remaining(input.encodingList); while (!remaining.IsEmpty()) { const int comma = remaining.Find(L','); const CString item = comma >= 0 ? remaining.Left(comma) : remaining; remaining = comma >= 0 ? remaining.Mid(comma + 1) : CString(); if (!item.IsEmpty()) { customize->AddControlItem(controlId, ++index, item); if (item == selectedEncoding) selectedIndex = index; } } hr = customize->SetSelectedControlItem(controlId, selectedIndex ? selectedIndex : 1); return FAILED(hr) ? hr : customize->EndVisualGroup(); };
		request.readCustomization = [&input, &selectedEncoding](IFileDialogCustomize* customize) { DWORD selected = 0; if (!customize || FAILED(customize->GetSelectedControlItem(1001, &selected))) return; int index = 0; CString remaining(input.encodingList); while (!remaining.IsEmpty()) { const int comma = remaining.Find(L','); const CString item = comma >= 0 ? remaining.Left(comma) : remaining; remaining = comma >= 0 ? remaining.Mid(comma + 1) : CString(); if (!item.IsEmpty() && ++index == static_cast<int>(selected)) { selectedEncoding = item; return; } } };
		const ModernFileDialog::Result dialogResult = ModernFileDialog::Show(owner, request);
		if (dialogResult.outcome == ModernFileDialog::Outcome::Failed) StartupTrace::HResult(L"file-dialog", L"FD102", dialogResult.error, L"Save FictionBook dialog");
		SaveResult result;
		if (dialogResult.outcome == ModernFileDialog::Outcome::Accepted) { result.accepted = true; result.encoding = selectedEncoding; result.documentType = dialogResult.filterIndex == 2 ? FictionBookFileType::Fbd : dialogResult.filterIndex == 1 ? FictionBookFileType::Fb2 : ResolveFictionBookTargetType(CString(), input.currentFileName); result.path = AddFictionBookExtensionIfMissing(CString(dialogResult.paths.front().c_str()), result.documentType); }
		return result;
	}
}
