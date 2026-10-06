#pragma once

#include "HotkeyGroup.h"

namespace FbeSettings { namespace Hotkeys {
struct ExportRow { CString commandName; CString shortcut; };
struct ExportGroup { CString displayName; std::vector<ExportRow> rows; };
typedef std::vector<ExportGroup> ExportData;

ExportData BuildExportData(const std::vector<CHotkeysGroup>& groups);
CString BuildTextExport(const ExportData& data);
CString BuildTextExport(const std::vector<CHotkeysGroup>& groups);
CString BuildHtmlExport(const ExportData& data, LPCWSTR localeName);
CString BuildHtmlExport(const std::vector<CHotkeysGroup>& groups, LPCWSTR localeName);
bool WriteUtf8ExportFile(const CString& path, const CString& text);
}}