#pragma once
#include "stdafx.h"
#include "EpubImport.h"

// Loads/saves persistent EPUB import settings from HKCU.
// Defaults are used automatically when the registry values are absent.
void LoadImportOptions(EpubImportOptions& options);
void SaveImportOptions(const EpubImportOptions& options);

enum class ImportOptionsDialogResult { Confirmed, Cancelled, Error };

// Distinguishes user cancellation from failure to create or run the dialog.
ImportOptionsDialogResult ShowImportOptionsDialog(HWND owner, EpubImportOptions& options);
