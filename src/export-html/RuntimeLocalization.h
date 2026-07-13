#pragma once

CString LoadExportHtmlString(UINT id);
CString FormatExportHtmlString(UINT id, ...);
int ShowExportHtmlTaskDialog(HWND owner, UINT titleId, LPCTSTR instruction, LPCTSTR content, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, PCWSTR icon);
void InitExportHtmlRuntimeStrings();
