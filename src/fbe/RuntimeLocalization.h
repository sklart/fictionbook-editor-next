#pragma once

// Runtime-слой локализации FBE поверх встроенных Win32-ресурсов.
// Если рядом с программой нет Lang/<locale>/fbe.json или в нём нет нужного ключа,
// приложение продолжает использовать встроенные ресурсы из res_*.dll.

int FbeLoadRuntimeString(UINT id, wchar_t* buffer, int bufferChars);
CString FbeLoadRuntimeString(UINT id, LPCWSTR fallback = NULL);
CString FbeLoadRuntimeStringByKey(LPCWSTR key, LPCWSTR fallback = NULL);
bool FbeIsRuntimeLocaleInstalled(LPCWSTR localeName);
void FbePublishRuntimeLocaleName(LPCWSTR localeName);
void FbeResetRuntimeLocalization();
