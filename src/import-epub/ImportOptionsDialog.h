#pragma once
#include "stdafx.h"
#include "EpubImport.h"

// Loads/saves persistent EPUB import settings from HKCU.
// Defaults are used automatically when the registry values are absent.
void LoadImportOptions(EpubImportOptions& options);
void SaveImportOptions(const EpubImportOptions& options);

// Shows the EPUB import settings dialog.
// Returns true when the user clicked OK and false on cancel/close.
bool ShowImportOptionsDialog(HWND owner, EpubImportOptions& options);
