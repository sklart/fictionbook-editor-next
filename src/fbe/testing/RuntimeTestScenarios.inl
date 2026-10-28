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
#include "RuntimeTestBinaryStoreC4.inl"
#include "RuntimeTestBinaryStoreC41.inl"
#include "RuntimeTestSourceMemoryC5.inl"
#include "RuntimeTestBinaryScriptingCompatibility.inl"
#include "XmlUndoLifetimeProbe.h"
#include "RuntimeTestXmlScriptBackend.inl"

// Keep new script/COM probes outside the large legacy scenario stack frame.
LRESULT CMainFrame::OnSourceMemoryBenchmark(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
#ifdef FBE_BINARY_STORE_DIAGNOSTICS
#ifdef FBE_BINARY_STORE_MEMORY_ATTRIBUTION
#ifdef FBE_SOURCE_MEMORY_BENCHMARK
    if (RuntimeTests::IsScenario(L"source-memory-c5")) { static bool active=false; if(active) return 0; active=true; CAtlFile output; if(FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS))) return 0; return RunSourceMemoryC5(output); }
#endif
    if (RuntimeTests::IsScenario(L"binary-store-c41")) { static bool active=false; if(active) return 0; active=true; CAtlFile output; if (FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS))) return 0; return RunBinaryStoreC41(output); }
#endif
    if (RuntimeTests::IsScenario(L"binary-store-c4")) { static bool active=false; if(active) return 0; active=true; CAtlFile output; if (FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS))) return 0; return RunBinaryStoreC4(output); }
#endif
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
    CAtlFile output;
    if (FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS))) return 0;
    bool handled = true;
    LRESULT result = RunRuntimeArchiveScenarios(output, handled);
    if (handled) return result;
    result = RunRuntimeUndoScenarios(output, handled);
    if (handled) return result;
    result = RunRuntimeNavigationScenarios(output, handled);
    if (handled) return result;
    return RunRuntimeEditorScenarios(output);
}

__declspec(noinline) LRESULT CMainFrame::RunRuntimeArchiveScenarios(CAtlFile& output, bool& scenarioHandled)
{
    scenarioHandled = true;
#include "RuntimeTestArchiveAndLifecycle.inl"
    scenarioHandled = false;
    return 0;
}

__declspec(noinline) LRESULT CMainFrame::RunRuntimeUndoScenarios(CAtlFile& output, bool& scenarioHandled)
{
    scenarioHandled = true;
#include "RuntimeTestUndoAndContainers.inl"
    scenarioHandled = false;
    return 0;
}

__declspec(noinline) LRESULT CMainFrame::RunRuntimeNavigationScenarios(CAtlFile& output, bool& scenarioHandled)
{
    scenarioHandled = true;
#include "RuntimeTestNavigationAndStructure.inl"
    scenarioHandled = false;
    return 0;
}

__declspec(noinline) LRESULT CMainFrame::RunRuntimeEditorScenarios(CAtlFile& output)
{
#include "RuntimeTestEditorAndExport.inl"

#undef IsFbeTestScenario
