# Инкрементальный runtime пользовательских панелей скриптов Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Сделать create, rename, show, hide, move и delete пользовательских script toolbar мгновенными: без полного discovery, reload и повторной регистрации hotkeys.

**Architecture:** ApplyScriptToolbarDefinitions остаётся persistence-транзакцией: snapshot -> Save(v2) -> runtime delta. Новый ApplyScriptToolbarRuntimeDelta(previous, current) не читает и не пишет XML; меняет только definitions, затронутые HWND и rebar. InitializeScriptsFromDefinitions остаётся startup/reload и аварийным fallback, но после единственного m_scripts.Initialize() использует те же атомарные helpers.

**Tech Stack:** C++17, Win32/WTL toolbar/rebar, Toolbars.xml v2, PowerShell contract/runtime regression.

**Spec:** docs/superpowers/specs/2026-09-22-script-toolbar-incremental-runtime-design.md

## Global Constraints

- Не менять Toolbars.xml v2, Script UID, command IDs, hotkeys, discovery и ScriptToolbarDefinition.
- scripts-main не проходит generic create/destroy; его visibility меняется только m_rebar.ShowBand.
- Скрытая custom panel имеет definition без HWND/rebar band.
- Runtime delta не вызывает PortableToolbarStore, InitializeScripts, ScriptLoad, Catalog::Discover, загрузку icon-файлов или регистрацию hotkeys.
- Create helper failure-atomic: при ошибке удалить свой HWND/band и обнулить window и rebarBandId.
- Не шарить HIMAGELIST между toolbars; наполнять новый toolbar уже загруженными VisualResource::icon в его собственный ImageList.
- Использовать только PortableToolbarStore::CaptureSnapshot, Save и RestoreSnapshot для persistence.
- Не менять third_party; build не должен оставить изменения runtime\Scintilla.dll или runtime\Lexilla.dll.
- Full rebuild допустим только для startup/reload и если обратная runtime delta не удалась.

## Review Focus

- Failure после CreateWindow или AddSimpleReBarBand не оставляет HWND/band — Task 2.
- Rename/move не меняет HWND/band незатронутых и существующих панелей — Task 4.
- Отсутствующий persisted scripts-main не превращается в пустой элемент после rollback — Task 5.
- Hidden custom toolbar сохраняет edited items и получает иконки только после show — Task 4.
- Ошибка reverse-delta сначала восстанавливает persistent snapshot, затем запускает fallback — Task 5.

---

## File Structure

- src/fbe/scripts/ScriptUiController.h/.cpp: счётчики actual init/discovery.
- src/fbe/toolbars/ScriptToolbarRuntime.h/.cpp: удаление и stable-ID reorder runtime entries.
- src/fbe/mainfrm.h/.cpp: custom primitives, delta, transaction и fallback.
- src/fbe/testing/RuntimeTestPortableState.inl: live scenarios, HWND/band/snapshot invariants.
- tools/tests/test-script-toolbar-management-contract.ps1 и test-script-toolbar-lifecycle-contract.ps1: static guards.
- tools/tests/test-script-toolbar-lifecycle-runtime.ps1: portable/isolated-installed launcher.
- CHANGELOG.md, docs/todo.md, docs/architecture/verification-status.md: evidence and manual-smoke status.

### Task 1: Добавить измеримые счётчики полного init/discovery

**Files:**
- Modify: src/fbe/scripts/ScriptUiController.h:11-33
- Modify: src/fbe/scripts/ScriptUiController.cpp:28-56
- Modify: src/fbe/mainfrm.h:234-256
- Test: tools/tests/test-script-toolbar-lifecycle-contract.ps1

**Interfaces:**
- Produces: UINT UiController::InitializeCount() const и UINT UiController::DiscoveryCount() const.
- Produces: frame-owned test-only bool ShouldFailScriptToolbarRuntimeDeltaForTest().
- Consumes: existing UiController::Initialize(...) and Catalog::Discover(...).

- [ ] **Step 1: Write the failing source contract.**

~~~powershell
Must $controller 'UINT InitializeCount\(\) const' 'UiController exposes init counter'
Must $controller 'UINT DiscoveryCount\(\) const' 'UiController exposes discovery counter'
Must $controllerCpp '\+\+m_discoveryCount;[\s\S]{0,160}catalog\.Discover' 'Counter is adjacent to discovery'
Must $frame 'ShouldFailScriptToolbarRuntimeDeltaForTest' 'Delta owns its fault point'
~~~

- [ ] **Step 2: Run it before code.**

Run: pwsh -NoProfile -File .\tools\tests\test-script-toolbar-lifecycle-contract.ps1

Expected: FAIL because neither counters nor the delta fault point exist.

- [ ] **Step 3: Implement the smallest observability surface.**

~~~cpp
UINT InitializeCount() const { return m_initializeCount; }
UINT DiscoveryCount() const { return m_discoveryCount; }

// UiController::Initialize
++m_initializeCount;
ScriptRegistry registry; if(!registry.Load()) return false;
Catalog catalog;
++m_discoveryCount;
if(!catalog.Discover(folder, L"*.js", &registry)) return false;
~~~

Initialize members to zero in the constructor. The discovery counter is immediately before Catalog::Discover, not outside the frame. Retain existing full-init fault injection and add a distinct delta fault control in CMainFrame.

- [ ] **Step 4: Re-run the focused contract.**

Run: pwsh -NoProfile -File .\tools\tests\test-script-toolbar-lifecycle-contract.ps1

Expected: PASS.

- [ ] **Step 5: Commit and push.**

~~~powershell
git add src/fbe/scripts/ScriptUiController.h src/fbe/scripts/ScriptUiController.cpp src/fbe/mainfrm.h tools/tests/test-script-toolbar-lifecycle-contract.ps1
git commit -m "test: expose script discovery counters"
git push origin master
~~~

### Task 2: Создать failure-atomic custom-toolbar primitives

**Files:**
- Modify: src/fbe/mainfrm.h:238-245
- Modify: src/fbe/mainfrm.cpp:2433-2537
- Modify: src/fbe/toolbars/ScriptToolbarRuntime.h:14-25
- Modify: src/fbe/toolbars/ScriptToolbarRuntime.cpp:4-6
- Test: src/fbe/testing/RuntimeTestPortableState.inl:67-151

**Interfaces:**
- Produces: bool CreateScriptToolbarRuntime(ScriptToolbarRuntime&), void DestroyScriptToolbarRuntime(ScriptToolbarRuntime&), bool PopulateScriptToolbarRuntime(ScriptToolbarRuntime&), bool SetScriptToolbarRuntimeVisible(ScriptToolbarRuntime&, bool), bool ReorderScriptToolbarRuntimeBands(const std::vector<ScriptToolbarDefinition>&).
- Produces: bool ScriptToolbarRuntimeCollection::Remove(const CString&) и void ScriptToolbarRuntimeCollection::Reorder(const std::vector<ScriptToolbarDefinition>&).
- Consumes: m_scripts.Menu(), existing visual resources, AddTbButton, AddSimpleReBarBand, CReBarCtrl::MoveBand.

- [ ] **Step 1: Add failing live assertions for both partial creation points.**

~~~cpp
const bool noLeaks = liveCount() == expectedHwnds &&
    validBandCount() == expectedBands && m_rebar.GetBandCount() == bandsBefore;
const bool partialFailureClean = !ApplyScriptToolbarDefinitions(previous, candidate) && noLeaks;
~~~

Inject once after toolbar HWND creation and once after rebar-band insertion; report window-clean and band-clean.

- [ ] **Step 2: Run the partial lifecycle scenario before implementation.**

Run: pwsh -NoProfile -File .\tools\tests\test-script-toolbar-lifecycle-runtime.ps1 -FbeExe .\out\Release\FBE.exe

Expected: the new assertion fails.

- [ ] **Step 3: Implement atomic creation/destruction.**

~~~cpp
bool CMainFrame::CreateScriptToolbarRuntime(ScriptToolbarRuntime& runtime)
{
    if(runtime.definition.id == L"scripts-main" || !runtime.definition.visible)
        return runtime.window != NULL;
    runtime.window = CreateSimpleToolBarCtrl(m_hWnd, IDR_SCRIPTS, FALSE,
        ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST | CCS_ADJUSTABLE);
    if(runtime.window == NULL) return false;
    if(!ConfigureAndPopulate(runtime) || ShouldFailScriptToolbarRuntimeDeltaForTest() ||
       !AddSimpleReBarBand(runtime.window, 0, TRUE, 0, FALSE) || !ReadRuntimeBandId(runtime)) {
        DestroyScriptToolbarRuntime(runtime);
        return false;
    }
    return true;
}
~~~

Destroy by stable band ID when present, then only destroy this custom HWND and clear both handles. Populate from already assigned descriptors and VisualResource::icon; use each toolbar’s own image list via existing button plumbing, never assign main’s HIMAGELIST. Preserve separators/missing UIDs.

- [ ] **Step 4: Preserve native handles across definition reorder.**

~~~cpp
void ScriptToolbarRuntimeCollection::Reorder(const std::vector<ScriptToolbarDefinition>& definitions)
{
    std::vector<ScriptToolbarRuntime> reordered;
    for(const auto& definition : definitions) {
        ScriptToolbarRuntime* old = Find(definition.id);
        if(old != NULL) { old->definition = definition; reordered.push_back(std::move(*old)); }
        else { ScriptToolbarRuntime fresh; fresh.definition = definition; reordered.push_back(std::move(fresh)); }
    }
    m_items.swap(reordered);
}
~~~

Confirm a moved-from element does not own/destruct HWND. ReorderScriptToolbarRuntimeBands calls MoveBand on existing visible custom band IDs and skips creation/destruction of scripts-main.

- [ ] **Step 5: Re-run live cleanup test.**

Expected report: window-clean=1, band-clean=1, result=pass.

- [ ] **Step 6: Commit and push.**

~~~powershell
git add src/fbe/mainfrm.h src/fbe/mainfrm.cpp src/fbe/toolbars/ScriptToolbarRuntime.h src/fbe/toolbars/ScriptToolbarRuntime.cpp src/fbe/testing/RuntimeTestPortableState.inl
git commit -m "feat: add atomic script toolbar runtime primitives"
git push origin master
~~~

### Task 3: Переиспользовать primitives при полном startup/reload

**Files:**
- Modify: src/fbe/mainfrm.cpp:2468-2537
- Test: tools/tests/test-script-toolbar-management-contract.ps1
- Test: tools/tests/test-script-toolbars-v2-contract.ps1

**Interfaces:**
- Consumes: Task 2 primitives and one m_scripts.Initialize(...).
- Produces: full init uses no duplicated direct custom-toolbar creation loop.

- [ ] **Step 1: Write failing guard for canonical creation.**

~~~powershell
Must $frame 'InitializeScriptsFromDefinitions[\s\S]*CreateScriptToolbarRuntime\(runtime\)' 'Startup uses canonical creator'
MustNot $frame 'InitializeScriptsFromDefinitions[\s\S]*CreateSimpleToolBarCtrl\(m_hWnd, IDR_SCRIPTS' 'Startup has no duplicate custom HWND creator'
~~~

- [ ] **Step 2: Run management contract.**

Expected: FAIL while the existing full-init loop directly creates each custom toolbar.

- [ ] **Step 3: Replace only that duplicate loop.**

~~~cpp
// m_scripts.Initialize has assigned menu descriptors, command IDs and visuals.
for(auto& runtime : m_scriptToolbars.Items())
    if(runtime.definition.id != L"scripts-main" && runtime.definition.visible &&
       !CreateScriptToolbarRuntime(runtime)) {
        DestroyScriptToolbarRuntimeControls();
        return false;
    }
if(!ReorderScriptToolbarRuntimeBands(definitions)) {
    DestroyScriptToolbarRuntimeControls();
    return false;
}
~~~

Keep release/discovery/menu/hotkey registration and main-toolbar default/persisted handling in InitializeScriptsFromDefinitions; only the custom HWND/band creation moves to the shared helper.

- [ ] **Step 4: Run v2 and management contracts.**

Run: pwsh -NoProfile -File .\tools\tests\test-script-toolbars-v2-contract.ps1; pwsh -NoProfile -File .\tools\tests\test-script-toolbar-management-contract.ps1

Expected: PASS.

- [ ] **Step 5: Commit and push.**

~~~powershell
git add src/fbe/mainfrm.cpp tools/tests/test-script-toolbar-management-contract.ps1
git commit -m "refactor: share script toolbar runtime creation"
git push origin master
~~~

### Task 4: Ввести чистую runtime delta и транзакционный wrapper

**Files:**
- Modify: src/fbe/mainfrm.h:241-245
- Modify: src/fbe/mainfrm.cpp:2175-2250
- Modify: src/fbe/testing/RuntimeTestPortableState.inl:154-220
- Test: tools/tests/test-script-toolbar-management-contract.ps1
- Test: tools/tests/test-script-toolbar-lifecycle-contract.ps1

**Interfaces:**
- Produces: bool ApplyScriptToolbarRuntimeDelta(const std::vector<ScriptToolbarDefinition>& previous, const std::vector<ScriptToolbarDefinition>& current).
- Produces: ApplyScriptToolbarDefinitions as snapshot/save -> forward delta -> unconditional restore/reverse delta/fallback.

- [ ] **Step 1: Add failing static and live invariants.**

~~~powershell
Must $frame 'bool CMainFrame::ApplyScriptToolbarRuntimeDelta\(' 'Runtime delta is separate'
MustNot $deltaBody 'PortableToolbarStore::(Load|Save|CaptureSnapshot|RestoreSnapshot)' 'Delta has no persistence I/O'
MustNot $applyBody 'if\(InitializeScripts\(\)\) return true' 'Normal management has no full init'
~~~

Live report records init-before/init-after, discovery-before/discovery-after, main HWND/band, untouched custom HWND/band and only-target change.

- [ ] **Step 2: Run contracts before code.**

Expected: FAIL because current wrapper calls InitializeScripts().

- [ ] **Step 3: Implement forward delta by stable definition ID.**

~~~cpp
bool CMainFrame::ApplyScriptToolbarRuntimeDelta(const std::vector<ScriptToolbarDefinition>& previous,
    const std::vector<ScriptToolbarDefinition>& current)
{
    // delete only absent non-main custom runtimes
    // update metadata by ID; create only new visible customs
    // visible custom items: repopulate existing HWND; hidden items: definition only
    // custom hide destroys only that runtime; show creates it
    // scripts-main: existing band ShowBand only
    // reorder existing band IDs, refresh View menu, autosize rebar
    return true;
}
~~~

Rename is metadata-only. Reorder must not replace runtime collection entries with new handles. Hidden edit must not allocate runtime. No XML call is permitted in this method.

- [ ] **Step 4: Implement persistence rollback ordering and trace.**

~~~cpp
if(!PortableToolbarStore::Save(layout)) return false;
const ULONGLONG started = ::GetTickCount64();
if(ApplyScriptToolbarRuntimeDelta(previous, current)) {
    TraceScriptToolbarDelta(started, previous, current, true);
    return true;
}
const bool restored = PortableToolbarStore::RestoreSnapshot(snapshot);
const bool reversed = ApplyScriptToolbarRuntimeDelta(current, previous);
if(!reversed) InitializeScriptsFromDefinitions(previous, hadPersistedMainDefinition);
TraceScriptToolbarDelta(started, previous, current, false);
return false;
~~~

RestoreSnapshot is unconditional. Never synthesize before.scriptToolbars or force scriptsToolbarPresent; fallback receives original hadPersistedMainDefinition. Trace milliseconds as diagnostic evidence only.

- [ ] **Step 5: Run focused contracts and portable lifecycle.**

Run: pwsh -NoProfile -File .\tools\tests\test-script-toolbar-management-contract.ps1; pwsh -NoProfile -File .\tools\tests\test-script-toolbar-lifecycle-contract.ps1; pwsh -NoProfile -File .\tools\tests\test-script-toolbar-lifecycle-runtime.ps1 -FbeExe .\out\Release\FBE.exe

Expected: PASS; full-init/discovery counters remain unchanged for create -> rename -> hide -> show -> move -> delete.

- [ ] **Step 6: Commit and push.**

~~~powershell
git add src/fbe/mainfrm.h src/fbe/mainfrm.cpp src/fbe/testing/RuntimeTestPortableState.inl tools/tests/test-script-toolbar-management-contract.ps1 tools/tests/test-script-toolbar-lifecycle-contract.ps1
git commit -m "perf: apply script toolbar changes incrementally"
git push origin master
~~~

### Task 5: Расширить rollback и portable/isolated-installed runtime regression

**Files:**
- Modify: src/fbe/testing/RuntimeTestPortableState.inl:67-220
- Modify: tools/tests/test-script-toolbar-lifecycle-runtime.ps1:1-76
- Modify: tools/tests/test-script-toolbar-lifecycle-contract.ps1

**Interfaces:**
- Consumes: Task 4 delta and dedicated fault point.
- Produces: exact reports for lifecycle, no-main rollback, persisted rollback and partial rollback.

- [ ] **Step 1: Add failing runtime assertions.**

~~~cpp
const bool unchangedCounters = initAfter == initBefore && discoveryAfter == discoveryBefore;
const bool stableMain = main.window == mainHwndBefore && main.rebarBandId == mainBandBefore;
const bool restored = runtime.window != NULL && runtime.rebarBandId != 0 &&
    m_rebar.IdToIndex(runtime.rebarBandId) >= 0;
~~~

For partial rollback require each restored custom HWND/band, valid-band count, live-HWND count and total GetBandCount() to equal the initial state. Add reverse-delta failure injection; test proves RestoreSnapshot precedes emergency full fallback.

- [ ] **Step 2: Run current runtime test before adding scenario mechanics.**

Expected: FAIL until reports cover all above values.

- [ ] **Step 3: Make persistence-origin scenarios self-contained.**

~~~powershell
Reset-PortableLifecycleState
Invoke-Lifecycle '--portable' 'script-toolbar-rollback-no-main-runtime' (Join-Path $portableData 'Diagnostics')
Reset-PortableLifecycleState
Invoke-Lifecycle '--portable' 'script-toolbar-rollback-persisted-runtime' (Join-Path $portableData 'Diagnostics')
Reset-PortableLifecycleState
Invoke-Lifecycle '--portable' 'script-toolbar-rollback-partial-runtime' (Join-Path $portableData 'Diagnostics')
~~~

No-main keeps absent source file and verifies default ID_LAST_SCRIPT after restart. Persisted scenario byte-compares initial snapshot plus panel order, LastScript, command toolbar state, main visibility/items and custom contents. Partial rollback runs both no-file and existing-file forms.

- [ ] **Step 4: Keep installed execution isolated and run the full matrix.**

~~~powershell
if($IncludeInstalled -and $env:FBE_CI_ISOLATED_PROFILE -ne '1') { throw 'Installed lifecycle test requires FBE_CI_ISOLATED_PROFILE=1 and must not run against a developer profile.' }
$env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $installedProfile
Invoke-Lifecycle '--installed' 'script-toolbar-rollback-no-main-runtime' $installedDiagnostics
Invoke-Lifecycle '--installed' 'script-toolbar-rollback-persisted-runtime' $installedDiagnostics
Invoke-Lifecycle '--installed' 'script-toolbar-rollback-partial-runtime' $installedDiagnostics
Invoke-Lifecycle '--installed' 'script-toolbar-lifecycle-runtime' $installedDiagnostics
Invoke-Lifecycle '--installed' 'script-toolbar-lifecycle-reload-runtime' $installedDiagnostics
~~~

Retain direct-child validation before portable deletion, always restore environment/portable.ini and delete disposable profile in finally.

- [ ] **Step 5: Run live runtime tests twice.**

Run: pwsh -NoProfile -File .\tools\tests\test-script-toolbar-lifecycle-runtime.ps1 -FbeExe .\out\Release\FBE.exe

Expected: portable PASS twice with no duplicate runtime-toolbar-*.

Run: $env:FBE_CI_ISOLATED_PROFILE='1'; pwsh -NoProfile -File .\tools\tests\test-script-toolbar-lifecycle-runtime.ps1 -FbeExe .\out\Release\FBE.exe -IncludeInstalled

Expected: portable + isolated-installed PASS without touching the developer profile.

- [ ] **Step 6: Commit and push.**

~~~powershell
git add src/fbe/testing/RuntimeTestPortableState.inl tools/tests/test-script-toolbar-lifecycle-runtime.ps1 tools/tests/test-script-toolbar-lifecycle-contract.ps1
git commit -m "test: cover incremental script toolbar rollback"
git push origin master
~~~

### Task 6: Документация, build и release verification

**Files:**
- Modify: CHANGELOG.md
- Modify: docs/todo.md
- Modify: docs/architecture/verification-status.md
- Test: all script/menu/toolbar contracts and lifecycle runner listed below.

**Interfaces:**
- Consumes: completed Tasks 1–5.
- Produces: evidence-separated documentation; no unverified manual claim.

- [ ] **Step 1: Record only confirmed shipped behavior.**

~~~markdown
- Normal script-toolbar management applies an in-memory runtime delta and does not rediscover scripts.
- InitializeScripts remains startup/reload and emergency rollback fallback.
- Automated coverage includes contracts plus portable and isolated-installed HWND/rebar lifecycle tests.
- Manual visible UI smoke is listed separately until performed.
~~~

- [ ] **Step 2: Run affected contracts.**

Run: pwsh -NoProfile -File .\tools\tests\test-script-identity-contract.ps1; pwsh -NoProfile -File .\tools\tests\test-script-toolbars-v2-contract.ps1; pwsh -NoProfile -File .\tools\tests\test-portable-scripts-infrastructure.ps1; pwsh -NoProfile -File .\tools\tests\test-customizable-toolbar-contract.ps1; pwsh -NoProfile -File .\tools\tests\test-scripts-toolbar-customize-behavior.ps1; pwsh -NoProfile -File .\tools\tests\test-fbe-script-command-ids.ps1; pwsh -NoProfile -File .\tools\tests\test-fbe-main-menu-catalog.ps1; pwsh -NoProfile -File .\tools\tests\test-script-toolbar-management-contract.ps1; pwsh -NoProfile -File .\tools\tests\test-script-toolbar-lifecycle-contract.ps1

Expected: every command PASS.

- [ ] **Step 3: Build and run live checks.**

Run: pwsh -NoProfile -File .\tools\build\build.ps1 -Configuration Debug -Platform Win32

Expected: Debug Win32 PASS.

Run: pwsh -NoProfile -File .\tools\build\build.ps1 -Configuration Release -Platform Win32

Expected: Release Win32 PASS.

Run portable twice, then isolated-installed once using Task 5 commands.

- [ ] **Step 4: Run requested release gate and hygiene checks.**

Run: pwsh -NoProfile -File .\tools\build\verify-release.ps1 -Configuration Release; git diff --check; git status --short

Expected: release gate PASS; no whitespace errors or tracked dependency-binary modifications.

- [ ] **Step 5: Perform manual smoke before claiming it is passed.**

Create Чистка, Типографика, Примечания; distribute scripts; edit a hidden toolbar; hide, rename and reorder; restart FBE. Open Скрипты, Сервис, Справка, Найти, Заменить and run an ordinary script.

Expected: no multi-second freeze; menus/dialogs work; items, icons, names, order, visibility and hotkeys restore. If desktop smoke cannot run here, document it as pending.

- [ ] **Step 6: Commit and push documentation only after automated checks pass.**

~~~powershell
git add CHANGELOG.md docs/todo.md docs/architecture/verification-status.md
git commit -m "docs: record incremental script toolbar verification"
git push origin master
~~~

## Self-Review

1. **Spec coverage:** Tasks 2–4 supply atomic controls, no-I/O delta and exact rollback; Task 5 covers absent/existing persistence, partial failure, reverse failure and installed isolation; Task 6 supplies every requested contract/build/release/manual check.
2. **Placeholder scan:** No TBD/TODO/implement later or generic test instructions exist; each task states files, interfaces, exact commands and expected result.
3. **Type consistency:** Task 2 defines every primitive Task 3–5 consume; Task 4 is the sole owner of ApplyScriptToolbarRuntimeDelta; counter names from Task 1 are used unchanged in Task 5.
4. **Review focus:** All five listed risks have a named owning Task and concrete scenario.
