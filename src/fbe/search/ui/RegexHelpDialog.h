#pragma once

#include "..\\SearchPreset.h"

// Modal, native Win32 help: no browser/WebView dependency and safe on Win7.
void ShowRegexHelpDialog(HWND owner, FbeSearchPresets::SearchUiContext context);

// Runtime coverage for the geometry policy used by the native Full Help dialog.
// It does not create or persist a dialog placement.
bool RunRegexHelpPlacementRuntimeSmoke(HWND owner, CStringA& report);
// Test-mode screenshot coverage for the dialog itself; never persists the temporary placement.
bool RunRegexHelpVisualCapture(HWND owner, LPCWSTR artifactDirectory, CStringA& report);