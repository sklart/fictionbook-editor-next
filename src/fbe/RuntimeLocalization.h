#pragma once

// Runtime-слой локализации FBE поверх встроенных Win32-ресурсов.
// Если рядом с программой нет Lang/<locale>/fbe.json или в нём нет нужного ключа,
// приложение продолжает использовать встроенный английский ресурс FBE.exe.

int FbeLoadRuntimeString(UINT id, wchar_t* buffer, int bufferChars);
CString FbeLoadRuntimeString(UINT id, LPCWSTR fallback = NULL);
CString FbeLoadRuntimeStringByKey(LPCWSTR key, LPCWSTR fallback = NULL);
// Applies catalog-backed text to controls created from the English FBE.exe
// dialog template.  This is deliberately a small binding layer: dialogs that
// already localize dynamic controls themselves keep doing so.
void FbeApplyRuntimeDialogLocalization(HWND dialog, UINT dialogId);
bool FbeIsRuntimeLocaleInstalled(LPCWSTR localeName);
void FbePublishRuntimeLocaleName(LPCWSTR localeName);
void FbeResetRuntimeLocalization();
