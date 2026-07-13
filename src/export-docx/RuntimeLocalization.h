#pragma once

CStringW LoadExportDocxString(UINT stringId, LPCWSTR fallback = nullptr);
CStringW LoadExportDocxStringByKey(LPCWSTR key, LPCWSTR fallback = nullptr);
void InitExportDocxRuntimeStrings();
