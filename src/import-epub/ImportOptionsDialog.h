#pragma once
#include "stdafx.h"
#include "EpubImport.h"

// Loads/saves persistent EPUB import settings from the active deployment profile.
// Defaults are used automatically when the stored values are absent.
void LoadImportOptions(EpubImportOptions& options);
HRESULT SaveImportOptions(const EpubImportOptions& options);

enum class ImportOptionsDialogResult { Confirmed, Cancelled, Error };

// Distinguishes user cancellation from failure to create or run the dialog.
ImportOptionsDialogResult ShowImportOptionsDialog(HWND owner, EpubImportOptions& options);
