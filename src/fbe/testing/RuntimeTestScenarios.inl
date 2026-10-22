// Оркестрация runtime-сценариев только для тестов. Файл включается из
// mainfrm.cpp, чтобы узкая граница CMainFrame сохранила private-доступ.

#define IsFbeTestScenario RuntimeTests::IsScenario

#include "../view/VisualDomNormalizer.h"
#include "PluginOptionsDialogProbe.h"
#include "TreeMarkerProbe.h"
#include "../../common/ModernFileDialog.h"
#include "../../common/DeploymentContext.h"
#include "RuntimeTestPortableState.inl"

// The adapter stays in the editor translation unit. The shared standalone
// plug-in probe neither declares nor links ThemeManager/UiMetrics symbols.
static PluginOptionsProbe::Palette EditorPluginProbePalette(HWND window) {
    return {ThemeManager::IsDark(), ThemeManager::IsHighContrast(),
        ThemeManager::TextColor(), ThemeManager::DisabledTextColor(),
        ThemeManager::ControlColor(), UiMetrics::DpiForWindow(window)};
}

#include "RuntimeTestIssue64.inl"

LRESULT CMainFrame::OnSourceMemoryBenchmark(UINT, WPARAM, LPARAM, BOOL&)
{
    PluginOptionsProbe::paletteForWindow = EditorPluginProbePalette;
#include "RuntimeTestArchiveAndLifecycle.inl"
#include "RuntimeTestUndoAndContainers.inl"
#include "RuntimeTestNavigationAndStructure.inl"
#include "RuntimeTestEditorAndExport.inl"

#undef IsFbeTestScenario
