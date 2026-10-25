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

#include "RuntimeTestBinaryStore.inl"
#include "RuntimeTestBinaryScriptingCompatibility.inl"
#include "XmlUndoLifetimeProbe.h"
#include "RuntimeTestXmlScriptBackend.inl"

// Keep new script/COM probes outside the large legacy scenario stack frame.
LRESULT CMainFrame::OnSourceMemoryBenchmark(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    if (RuntimeTests::IsScenario(L"binary-store") || RuntimeTests::IsScenario(L"binary-scripting-compatibility") || RuntimeTests::IsScenario(L"xml-script-backend-runtime")) {
        CAtlFile output;
        if (FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS))) return 0;
        if (RuntimeTests::IsScenario(L"xml-script-backend-runtime")) return RunXmlScriptBackendRuntime(output);
        return RuntimeTests::IsScenario(L"binary-store") ? RunBinaryStoreRuntime(output) : RunBinaryScriptingCompatibilityRuntime(output);
    }
    return RunLegacyRuntimeScenarios(message, wparam, lparam, handled);
}

__declspec(noinline) LRESULT CMainFrame::RunLegacyRuntimeScenarios(UINT, WPARAM, LPARAM, BOOL&)
{
    PluginOptionsProbe::paletteForWindow = EditorPluginProbePalette;
#include "RuntimeTestArchiveAndLifecycle.inl"
#include "RuntimeTestUndoAndContainers.inl"
#include "RuntimeTestNavigationAndStructure.inl"
#include "RuntimeTestEditorAndExport.inl"

#undef IsFbeTestScenario
