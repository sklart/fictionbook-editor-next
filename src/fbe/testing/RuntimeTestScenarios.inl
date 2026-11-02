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

__declspec(noinline) LRESULT CMainFrame::RunUiDefectsRuntime(CAtlFile& output)
{
#include "RuntimeTestUiDefects.inl"
    return 0;
}

__declspec(noinline) LRESULT CMainFrame::RunSettingsDialogRuntime(CAtlFile& output)
{
	if (IsFbeTestScenario(L"settings-dialog-runtime") || IsFbeTestScenario(L"settings-dialog-runtime-verify"))
	{
		const bool verifyOnly = IsFbeTestScenario(L"settings-dialog-runtime-verify");
		auto sourceText = [&]() {
			const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> text(static_cast<size_t>(length) + 1);
			m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(text.data()));
			return text;
		};
		const std::vector<char> originalText = sourceText();
		const sptr_t originalPosition = m_source.SendMessage(SCI_GETCURRENTPOS);
		const sptr_t originalAnchor = m_source.SendMessage(SCI_GETANCHOR);
		const sptr_t originalUndo = m_source.SendMessage(SCI_CANUNDO);
		if (!verifyOnly)
			SendMessage(WM_COMMAND, MAKEWPARAM(ID_TOOLS_OPTIONS, 0), 0);
		const bool sourceUnchanged = originalText == sourceText() &&
			originalPosition == m_source.SendMessage(SCI_GETCURRENTPOS) &&
			originalAnchor == m_source.SendMessage(SCI_GETANCHOR) &&
			originalUndo == m_source.SendMessage(SCI_CANUNDO);
		const bool layoutPassed = verifyOnly ||
			::GetPropW(m_hWnd, L"FBE_SETTINGS_LAYOUT_PASSED") == reinterpret_cast<HANDLE>(1);
		::RemovePropW(m_hWnd, L"FBE_SETTINGS_LAYOUT_PASSED");
		const bool initialSystem = ::GetPropW(m_hWnd, L"FBE_CARET_INITIAL_SYSTEM") == reinterpret_cast<HANDLE>(1);
		::RemovePropW(m_hWnd, L"FBE_CARET_INITIAL_SYSTEM");
		const bool followupPassed = verifyOnly || ::GetPropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_PASSED") == reinterpret_cast<HANDLE>(1);
		::RemovePropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_PASSED");
		const bool backupPersisted = _Settings.GetCreateBackupFile();
		const bool nbspPersisted = _Settings.GetNBSPChar() == L"\x25AB";
		const bool wrapPersisted = _Settings.XmlSrcWrap();
		const bool yoPersisted = _Settings.GetCheckRussianYoUsage();
		const bool persisted = backupPersisted && nbspPersisted && wrapPersisted && yoPersisted;
		const bool applied = verifyOnly || (m_source.SendMessage(SCI_GETWRAPMODE) != SC_WRAP_NONE);
		const CaretPreferences caret = _Settings.GetCaretPreferences();
		const bool caretPersisted = caret.mode == TextCaretMode::Custom && caret.width == 4 &&
			caret.color == TextCaretColor::White && caret.blink == TextCaretBlink::Solid;
		const bool caretApplied = m_source.SendMessage(SCI_GETCARETWIDTH) ==
			UiMetrics::ScaleForDpi(4, UiMetrics::DpiForWindow(m_source.m_hWnd)) &&
			m_source.SendMessage(SCI_GETCARETPERIOD) == 0 &&
			m_source.SendMessage(SCI_GETCARETFORE) == static_cast<sptr_t>(ThemeManager::IsHighContrast() ?
				::GetSysColor(COLOR_WINDOWTEXT) : RGB(255, 255, 255));
		CStringA report;
		report.Format("followup=%d\nsettings_dialog=%d\nlayout=%d\npersisted=%d\nbackup=%d\nnbsp=%d\nwrap=%d\nyo_usage=%d\napplied=%d\ncaret_persisted=%d\ncaret_applied=%d\ncaret_initial_system=%d\nsource_unchanged=%d\n", followupPassed, !verifyOnly, layoutPassed, persisted, backupPersisted, nbspPersisted, wrapPersisted, yoPersisted, applied, caretPersisted, caretApplied, initialSystem, sourceUnchanged);
        const UINT_PTR groupPaint = reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd, L"FBE_SETTINGS_GROUP_PAINT_PASSED"));
        report.AppendFormat("group-paint=%Iu\n", groupPaint);
        report.AppendFormat("custom-active=%d\ncontrol-color=%lu\nhover-color=%lu\npressed-color=%lu\n", ThemeManager::IsCustomPaletteActive() ? 1 : 0,
            static_cast<unsigned long>(ThemeManager::ControlColor()), static_cast<unsigned long>(ThemeManager::HoverColor()), static_cast<unsigned long>(ThemeManager::PressedColor()));
        report.AppendFormat("button-states=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_BUTTON_PAINT")));
        report.AppendFormat("combo-states=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_COMBO_PAINT")));
        report.AppendFormat("editable-combo-states=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_EDITABLE_COMBO_PAINT")));
        report.AppendFormat("grouped-combo-states=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_GROUPED_COMBO_PAINT")));
        report.AppendFormat("disabled-label-paint=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_DISABLED_LABEL_PAINT")));
        report.AppendFormat("toggle-states=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_TOGGLE_PAINT")));
        report.AppendFormat("spin-states=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_SPIN_PAINT")));
        ::RemovePropW(m_hWnd,L"FBE_SETTINGS_COMBO_PAINT");
        ::RemovePropW(m_hWnd,L"FBE_SETTINGS_BUTTON_PAINT");
        report.AppendFormat("disabled-text=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_DISABLED_TEXT_PASSED")));
        report.AppendFormat("hover-text=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_HOVER_TEXT_PASSED")));
        report.AppendFormat("checkbox-caption=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_CHECKBOX_CAPTION_PASSED")));
        ::RemovePropW(m_hWnd,L"FBE_SETTINGS_CHECKBOX_CAPTION_PASSED");
        ::RemovePropW(m_hWnd,L"FBE_SETTINGS_DISABLED_TEXT_PASSED");
        ::RemovePropW(m_hWnd,L"FBE_SETTINGS_HOVER_TEXT_PASSED");
        report.AppendFormat("group-areas=%Iu group-failures=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_PAINT_AREAS")), reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_PAINT_FAILURES")));
        report.AppendFormat("repaint-pages=%Iu repaint-failures=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_REPAINT_PAGES")), reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_REPAINT_FAILURES")));
        report.AppendFormat("hover-pages=%Iu hover-failures=%Iu\n", reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_HOVER_PAGES")), reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd,L"FBE_SETTINGS_HOVER_FAILURES")));
        ::RemovePropW(m_hWnd,L"FBE_SETTINGS_REPAINT_PAGES"); ::RemovePropW(m_hWnd,L"FBE_SETTINGS_REPAINT_FAILURES");
        ::RemovePropW(m_hWnd,L"FBE_SETTINGS_HOVER_PAGES"); ::RemovePropW(m_hWnd,L"FBE_SETTINGS_HOVER_FAILURES");
        ::RemovePropW(m_hWnd,L"FBE_SETTINGS_PAINT_AREAS"); ::RemovePropW(m_hWnd,L"FBE_SETTINGS_PAINT_FAILURES");
        ::RemovePropW(m_hWnd, L"FBE_SETTINGS_GROUP_PAINT_PASSED");
        report.AppendFormat("interface-language=%lu\n", static_cast<unsigned long>(_Settings.GetEffectiveInterfaceLanguageID()));
        report.AppendFormat("appearance_screen_width=%Iu\nappearance_screen_height=%Iu\n",
            reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd, L"FBE_SETTINGS_SCREEN_WIDTH")),
            reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd, L"FBE_SETTINGS_SCREEN_HEIGHT")));
        ::RemovePropW(m_hWnd, L"FBE_SETTINGS_SCREEN_WIDTH");
        ::RemovePropW(m_hWnd, L"FBE_SETTINGS_SCREEN_HEIGHT");
        report.AppendFormat("followup_phase=%u\n",
            static_cast<unsigned int>(reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_PHASE"))));
        ::RemovePropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_PHASE");
        report.AppendFormat("followup_theme_steps=%u\nfollowup_theme_error=%lu\n",
            static_cast<unsigned int>(reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_THEME_STEPS"))),
            static_cast<unsigned long>(reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_THEME_ERROR"))));
        ::RemovePropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_THEME_STEPS");
        ::RemovePropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_THEME_ERROR");
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		wchar_t paintOnly[2] = {};
		const bool paintProbe = !verifyOnly && ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SETTINGS_PAINT_ONLY", paintOnly, _countof(paintOnly)) == 1 && paintOnly[0] == L'1';
		const bool passed = paintProbe ? (layoutPassed && (!ThemeManager::UsesPaletteSurface() || groupPaint == 1) && sourceUnchanged) :
			(followupPassed && persisted && applied && layoutPassed && caretPersisted && caretApplied && sourceUnchanged);
		::PostQuitMessage(passed ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"backup-settings-save-runtime") || IsFbeTestScenario(L"backup-settings-save-runtime-verify"))
	{
		const bool verifyOnly = IsFbeTestScenario(L"backup-settings-save-runtime-verify");
		if (!verifyOnly)
			SendMessage(WM_COMMAND, MAKEWPARAM(ID_TOOLS_OPTIONS, 0), 0);
		const bool backupDisabled = !_Settings.GetCreateBackupFile();
		bool sourceEdited = false;
		bool sourceApplied = false;
		FILE_OP_STATUS saveStatus = FAIL;
		int saveAttempts = 0;
		bool saved = false;
		if (!verifyOnly && backupDisabled)
		{
			ShowView(SOURCE);
			const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> source(static_cast<size_t>(length) + 1);
			m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
			const char* markerText = "BACKUP_RUNTIME_BEFORE";
			const char* replacementText = "BACKUP_RUNTIME_AFTER";
			const char* marker = strstr(source.data(), markerText);
			if (!marker)
			{
				markerText = "BACKUP_RUNTIME_AFTER";
				replacementText = "BACKUP_RUNTIME_FINAL";
				marker = strstr(source.data(), markerText);
			}
			if (marker)
			{
				const sptr_t start = static_cast<sptr_t>(marker - source.data());
				m_source.SendMessage(SCI_SETSEL, start, start + strlen(markerText));
				m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(replacementText));
				sourceEdited = true;
			}
			ShowView(BODY);
			sourceApplied = sourceEdited && !IsSourceActive();
			if (sourceApplied)
			{
				for (; saveAttempts < 30;)
				{
					++saveAttempts;
					saveStatus = SaveFile(false);
					if (saveStatus == OK || m_doc->GetLastSaveError() != HRESULT_FROM_WIN32(ERROR_UNABLE_TO_REMOVE_REPLACED)) break;
					::Sleep(10);
				}
			}
			saved = saveStatus == OK;
		}
		CStringA report;
		report.Format("settings_dialog=%d\nbackup_disabled=%d\nsource_edited=%d\nsource_applied=%d\nsaved=%d\nsave_attempts=%d\nsave_status=%d\nsave_error=0x%08lX\n", !verifyOnly, backupDisabled, sourceEdited, sourceApplied, saved, saveAttempts, saveStatus, static_cast<unsigned long>(m_doc->GetLastSaveError()));
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(backupDisabled && (verifyOnly || (sourceEdited && sourceApplied && saved)) ? 0 : 1);
		return 0;
	}
	return 0;
}

// Keep script/COM probes outside the large legacy scenario stack frame.
__declspec(noinline) LRESULT CMainFrame::RunScriptingStage2Runtime(CAtlFile& output)
{
#include "RuntimeTestScriptingStage2.inl"
    return 0;
}

LRESULT CMainFrame::OnSourceMemoryBenchmark(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    if (RuntimeTests::IsScenario(L"custom-palette-manage")) { RunPortableStateTestScenario(); return 0; }
    if (RuntimeTests::IsScenario(L"scripting-stage2")) {
        CAtlFile output;
        if (FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS))) return 0;
        return RunScriptingStage2Runtime(output);
    }
    if (RuntimeTests::IsScenario(L"settings-dialog-runtime") || RuntimeTests::IsScenario(L"settings-dialog-runtime-verify") ||
        RuntimeTests::IsScenario(L"backup-settings-save-runtime") || RuntimeTests::IsScenario(L"backup-settings-save-runtime-verify")) {
        PluginOptionsProbe::paletteForWindow = EditorPluginProbePalette;
        CAtlFile output;
        if (FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS))) return 0;
        return RunSettingsDialogRuntime(output);
    }
    if (RuntimeTests::IsScenario(L"ui-defects-runtime")) {
        PluginOptionsProbe::paletteForWindow = EditorPluginProbePalette;
        CAtlFile output;
        if (FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS))) return 0;
        return RunUiDefectsRuntime(output);
    }
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
#include "RuntimeTestArchiveAndLifecycle.inl"
#include "RuntimeTestUndoAndContainers.inl"
#include "RuntimeTestNavigationAndStructure.inl"
#include "RuntimeTestEditorAndExport.inl"

#undef IsFbeTestScenario
