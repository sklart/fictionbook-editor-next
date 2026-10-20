// Оркестрация runtime-сценариев только для тестов. Файл включается из
// mainfrm.cpp, чтобы узкая граница CMainFrame сохранила private-доступ.

#define IsFbeTestScenario RuntimeTests::IsScenario

#include "../view/VisualDomNormalizer.h"
#include "PluginOptionsDialogProbe.h"
#include "../../common/DeploymentContext.h"
#include "RuntimeTestPortableState.inl"

LRESULT CMainFrame::OnSourceMemoryBenchmark(UINT, WPARAM, LPARAM, BOOL&)
{
#include "RuntimeTestArchiveAndLifecycle.inl"
#include "RuntimeTestUndoAndContainers.inl"
#include "RuntimeTestNavigationAndStructure.inl"
#include "RuntimeTestEditorAndExport.inl"

#undef IsFbeTestScenario
