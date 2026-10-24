# UndefinedBehaviorSanitizer: отдельный clang-cl контур

Production и [MSVC AddressSanitizer](asan-diagnostics.md) сохраняют свои
компилятор, каталоги и gates. Undefined — режим существующей Release Win32,
а не новая MSBuild Configuration. Он предназначен для диагностики на Windows
10/11; это не изменение production-политики Windows 7 SP1+.

## Toolchain и запуск

```powershell
./tools/build/prepare-ubsan-llvm.ps1
./tools/tests/test-ubsan-infrastructure.ps1
./tools/build/build.ps1 -Configuration Release -Platform Win32 -Sanitizer Undefined
./tools/tests/test-ubsan-build-policy.ps1
./tools/tests/run-sanitizers.ps1 -Sanitizer Undefined -Contour Fast
./tools/tests/run-sanitizers.ps1 -Sanitizer Undefined -Contour Full
```

Resolver проверяет Visual Studio LLVM, системный LLVM, PATH и локальный cache.
Для явного выбора доступны `-LlvmRoot` у build/self-test и `FBE_LLVM_ROOT`.
Обязательны native clang-cl, lld-link, llvm-symbolizer и i386 compiler-rt одной
версии. При отсутствии
подходящего toolchain выдаётся `UBSan build = BLOCKED`; fallback на MSVC нет.
Версия, executable, target, архитектура, resource directory и runtime записываются
в build log и диагностический manifest.

Подготовка использует официальный
[LLVM 21.1.8](https://github.com/llvm/llvm-project/releases/tag/llvmorg-21.1.8),
`LLVM-21.1.8-win32.exe`, SHA-256
`0492c5c292e515ebbbc2533671e54c5895fe209e534812313f64257407233181`.
Installer только распаковывается в `out/tools`; системная установка не меняется.
Официальный x64 package этой версии не содержит i386 UBSan runtime.
Минимальная поддерживаемая версия — 21.1.8.

Эффективные флаги:

```text
--target=i686-pc-windows-msvc
-fsanitize=undefined
-fno-sanitize-recover=undefined
/Zi
/clang:-fno-omit-frame-pointer
/clang:-fno-sanitize-merge
```

Используются обычная оптимизация Release и C++17, без MSVC /GL/LTCG для
first-party clang объектов. MSBuild MultiToolTask ограничивает параллельные
clang-компиляции четырьмя процессами по умолчанию (`CL_MPCount` можно задать).
Общие build fingerprint/provenance учитывают compiler, его версию, sanitizer,
configuration и platform. Production остаётся MSVC v143 / VC Tools 14.44.

Только Undefined использует LLD. Минимальный probe LLVM 21.1.8 выявил, что
MSVC LINK записывает адрес префикса UBSan function type metadata вместо тела
функции в CFG table: корректный indirect stdcall callback завершался
`FAST_FAIL_GUARD_ICALL_CHECK_FAILURE`. LLD сохраняет правильный адрес.
Self-test проверяет корректный callback с CFG и отдельно ошибочный function
type call, который обязан дать UBSan diagnostic. CFG и function check не
отключаются. Helpers используют `-fuse-ld=lld`.

Hunspell для LLD отдельно собирается MSVC без `/GL`/LTCG в
`build/ubsan/dependencies/hunspell`: LLD не читает MSVC LTCG objects.
Обычный `build/hunspell` и pinned vendor source остаются прежними.

## Совместимость и покрытие

Диагностические outputs: `out/ubsan/Release`, `build/ubsan`,
`out/tests/ubsan`. С ordinary `out/Release` и `out/asan` они не смешиваются.
Инструментируются FBE, FBV, FBShell, ExportHTML, ExportDOCX, ExportEPUB,
ImportEPUB, ImportEPUBLunaSVG и три batch-конвертера. Завершающий build verifier
проверяет все модули, PDB, фактические compile command logs и SHA-256.

Vendor archives (PCRE2, Scintilla, Lexilla, Hunspell, libarchive, image stack,
lunasvg/plutovg) остаются обычными MSVC dependencies. Inline-код vendor headers
может инструментироваться вместе с вызывающим first-party TU; происхождение
каждого такого finding нужно устанавливать по стеку и аргументам вызова.
Windows/COM/MSHTML не инструментируются.

Clang не поддерживает MSVC `#import`. Подготовка генерирует те же TLB declarations
штатным MSVC в `build/ubsan/generated/type-libraries`. Вспомогательные import.obj
никогда не линкуются в продукт. MSVC-only implementation mapping заменяется
включением тех же сгенерированных inline definitions. Интерфейсы, GUID, порядок
vtable и публичный `fbe.idl` не меняются; все product C/C++ TU компилирует clang-cl.
Обычные MSVC/ASan пути продолжают использовать исходные `#import`.

Официальный i386 UBSan runtime статический и использует /MT. Диагностический FBV
использует /MT вместо своего ordinary /MD; остальные first-party Win32 Release
проекты уже используют /MT. Product production CRT selection не меняется.
Standalone helpers существующих тестов запускают тот же clang-cl; их /MD
заменяется на /MT, а MSVC /GL исключается только в режиме Undefined.

## Наблюдение и self-test

Используются общий каталог, Worker, Job process monitor и summary protocol
существующего ASan runner. Политики наблюдения разделены явно:

- Address: ASAN_OPTIONS, ASan runtime import, ASan diagnostics/dumps;
- Undefined: UBSAN_OPTIONS, статические PE exports `__ubsan_handle_*`, UBSan logs.

UBSan использует `print_stacktrace=1:halt_on_error=1:symbolize=1`, собственный
llvm-symbolizer и `ubsan.*` в каталоге команды. Обход inline-symbolization MSVC
ASan на него не переносится. Exit code — лишь один источник FAIL: diagnostic
`runtime error:`, `UndefinedBehaviorSanitizer` или UBSan-prefixed report также
отвергается независимо от кода worker/child. ASan dump от UBSan не требуется.

Self-test свежим clang-cl собирает реальный инструментированный Win32 probe
внутри наблюдаемого job. Clean branch обязан дать PASS. Signed overflow, invalid
shift, misaligned load и null load обязаны дать FAIL со строкой probe.cpp и
именем функции. Проверяется также incorrect function type при включённом CFG.
Отдельный recovering probe доказывает FAIL при diagnostic,
даже если и native child, и worker завершились с exit 0.

Начальный product baseline выполняется без first-party suppressions. Для каждого
finding нужны test, UB type, source/line, stack, происхождение, воспроизводимость,
исправление и регрессия. Compile compatibility errors и намеренные self-test UB
не считаются product findings.

## Production и CI

Package guard для Core/portable/Integration/InstallerInput отвергает sanitizer
marker, UBSan runtime и статически инструментированный PE по runtime exports
или signature. Negative policy копирует настоящий UBSan FBE.exe без marker и
runtime DLL. ASan-защита сохраняется. UBSan outputs не используются для
installer, portable release, update package или GitHub Release.

Отдельный `ubsan.yml` содержит обязательные toolchain probe, build, self-test,
build/package policy и FAST. FULL запускается вручную или для tag. Загружаются
только диагностические evidence artifacts; production workflows остаются MSVC.
Удалённый CI до отдельного реального запуска имеет статус NOT RUN.

В этот этап не входят integer/implicit-conversion/unsigned-overflow группы,
vptr, TSan/MSan, fuzzing и C++20. FULL проверяет корректность; performance
thresholds исключены метаданными общего каталога.

## Локальная валидация

Первый полный FAST baseline (`20261024-181823-3c5bff43098e4a92abc1d6e77ab0c32a`):
84 сценария, 16 PASS / 68 FAIL. За 67 sanitizer reports стояли три причины:

| Finding | Происхождение и воспроизведение | Исправление / регрессия |
| --- | --- | --- |
| Signed overflow `0 - LONG_MIN`, `third_party/wtl/atlframe.h:320` (64 reports) | WTL inline, `Run → CMainFrame::CreateEx`, воспроизводится при запуске FBE | Собственный `CMainFrame::Create` сохраняет thunk/registration/Win32 arguments, но выражает 32-bit span через безопасную 64-bit арифметику. `window-rectangle-test.cpp`: 11 edge cases; CLI/runtime startup. |
| Incorrect function type, `EpubImport.cpp:2212`, adapter definition `ImportEPUBLunaSVG.cpp:65` (2 reports одного fidelity-сценария) | First-party: importer использует `/Zc:wchar_t-`, adapter прежде использовал built-in wchar_t | У adapter установлен тот же `TreatWChar_tAsBuiltInType=false`. C export name, stdcall, 16-bit pointer arguments и DLL ABI сохраняются. UBSan policy фиксирует согласованность трёх проектов; существующий SVG fidelity runtime воспроизводит исходный вызов. |
| Misaligned `std::function<void()>`, VC 14.44 `ppltasks.h:3548` (1 report) | MSVC PPL/clang-cl Win32, только delayed-entry fault fixture в EPUB importer. Отдельный `<future>`/`std::async` probe без FBE воспроизвёл тот же finding | Test-only worker использует `std::thread` с RAII join и прежним непробрасыванием неполученных worker exceptions. `prerelease-epub-import` проверяет фактическую задержку и завершённую распаковку. Production Shell extraction алгоритм прежний. |

Один baseline FAIL не был UB: image inserter contract брал `.Source` у compiler
function, теряя маршрут clang. Вызов через `CommandInfo` сохраняет function и
application routes. ASCII fixture теперь использует UTF-8 source charset,
совместимый с обоими компиляторами и основным C++ build.

После исправления top-level окна открылся тот же sentinel overflow в дочерних
ATL controls (`atlwin.h:3640`, первоначальный stack: command bar, `OnCreate`).
В editor call sites передаётся фактическая нулевая child geometry вместо
игнорируемого Win32 `CW_USEDEFAULT`. Для title-less pane вызывается тот же ATL
base с явным RECT. Последующая раскладка и HWND ownership остаются прежними.
Та же причина исправлена для tooltip popup controls (`atlwin.h:844`).
Нулевые значения для child/popup соответствуют
[контракту CreateWindowExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw).
Vendor source и first-party suppressions не добавлялись.

### Единственное исключение SDK

После исправления собственных call sites осталась цепочка
`CFBEView::OnCreate → CAxHostWindow::ActivateAx → CAxFrameWindow::GetWindow → CWindowImplBaseT::Create`
(`atlwin.h:3640`). `CAxFrameWindow::GetWindow` внутри SDK самостоятельно создаёт
служебное overlapped окно с `CW_USEDEFAULT`; first-party код не задаёт эти RECT
arguments. Сохранённый минимальный probe без FBE воспроизводит overflow.

`tools/msbuild/ubsan.ignorelist` ограничен секцией `[signed-integer-overflow]`
и единственным полным symbol (в файле `?` экранированы как литералы):

```text
?Create@?$CWindowImplBaseT@VCWindow@ATL@@V?$CWinTraits@$0FGAAAAAA@$0A@@2@@ATL@@QAEPAUHWND__@@PAU3@V_U_RECT@2@PBGKKV_U_MENUorID@2@GPAX@Z
```

Это SDK Unicode `/Zc:wchar_t-` instantiation; другие функции и checks не
исключаются. Product fingerprint и sanitizer manifest содержат SHA-256 policy;
runner отвергает несовпадение. Build-policy запрещает расширять этот список.
Self-test собирает настоящий `CAxFrameWindow`, требует успешного создания окна,
а затем в том же бинарнике вызывает собственный `OwnOverflow(INT_MAX)`:
diagnostic и FAIL обязательны. Все пять общих negative checks выполняются с той
же policy. Минимальный probe: baseline exit 1, scoped clean exit 0, own overflow
exit 1. Это третьестороннее исключение не является исправлением first-party UB.

После устранения ранних startup diagnostics pilot выявил first-party
`applying non-zero offset ... to null pointer` в
`source/SourceViewDiagnostics.cpp:55`: `GetProcessMemorySnapshot` начинал обход
`VirtualQuery` с нулевого адреса и прибавлял `RegionSize` как C++ pointer offset.
Теперь виртуальные адреса представлены `uintptr_t`, а следующий регион
проверяется на переполнение и отсутствие продвижения. Регрессия
`virtual-address-range-test.cpp` проверяет 8 случаев, включая нулевой base,
верхнюю границу адресного пространства и неизменность результата при отказе.
Она выполняется инструментированной внутри UBSan self-test. Полный исходный
stack и повтор сценария сохраняются в `out/tests/ubsan`.

Следующий first-party finding в том же image-inserter сценарии:
`ExternalHelper.h:363`, member access внутри `VARIANT` с фактическим выравниванием
4 байта вместо 8. Стек проходит через `ITypeInfo::Invoke`/OLEAUT32 из MSHTML.
`GetImageDimsByData` теперь читает представление через `ReadBorrowedVariant`
(`const void*`/`memcpy`) в выровненный локальный `VARIANT`. Исходный SAFEARRAY
заимствуется без копирования, освобождения или смены владельца; COM/IDL ABI
не меняется. `borrowed-variant-test.cpp` намеренно смещает вход на 4 байта,
проверяет реальную невыровненность, идентичность SAFEARRAY и исходных байтов,
сохраняет освобождение за оригиналом и проверяет null input. Положительный
checked-arithmetic control гарантирует runtime evidence, даже если оптимизатор
устраняет все потенциально опасные операции безопасного byte-copy helper.
Общие self-test negative controls по-прежнему обнаруживают alignment/overflow.

Другой последующий third-party diagnostic был внутри
`WTL::CPaneContainer::CreateCloseButton → CToolBarCtrl::Create → ATL::CWindow::Create`
(`atlwin.h:844`). Узкое исправление `third_party/wtl/atlctrlx.h` заменяет только
default RECT этой дочерней кнопки на нулевой RECT; версия и пути WTL не менялись.
Исключение SDK не расширено. `pane-close-button-test.cpp` создаёт настоящую
панель и проверяет существование, единственность, command ID и enabled state
её close button под UBSan; исходный sentinel overflow прерывал бы этот тест.

Этап продолжается из `d981ceed4fd094ebb092d8fb92ad575f83814f9f`.
Завершено продолжение только до чистого UBSan FAST:

| Проверка | Результат / evidence в `out/tests/ubsan` |
| --- | --- |
| Product build, 11 modules / 219 TU | PASS, `build-borrowed-variant-byte-copy.log` |
| Infrastructure self-test и граница единственного SDK exception | PASS, встроен в полный FAST; отдельный `self-test-4729c0e64ff34191ac3dfcc38f2b2773` |
| Product pilot: CLI, image inserter, SVG fidelity, delayed EPUB extraction | PASS 4/4, `20261024-194612-ea90fd0b9c6b40d7814dcaa93ea6fbde/summary.json` |
| Полный FAST attempt | 83 PASS / 1 FAIL, `20261024-194733-4b45143609aa41c8b02e0f0a98205463/summary.json` |
| Повтор всего Search Core suite после fixture fix | PASS, `20261024-202231-45060a58699545499ebd9f64d281dbe0/summary.json` |
| Аудированное текущее покрытие FAST | PASS 84/84, `fast-evidence-244ac6c63b1e40a78c370ba9d5fb480a/summary.json` |
| Build/package rejection policy | PASS, `build-policy-fast-stage.log` |
| Source safety / diff check | PASS, `source-safety-final-fast-stage.log`, `diff-check-fast-stage.log` |

Единственный FAIL полного FAST attempt был **toolchain incompatibility тестового
fixture**, не runtime/UB diagnostic: Clang неоднозначно преобразовывал два COM
smart pointer типа в ternary `search-document-adapter-mshtml.cpp:445`. Явный
`IHTMLElementPtr` сохраняет прежние assertions и семантику. Повторена вся
Search Core suite. Остальные 83 длительных проверки не запускались повторно:
аудит подтвердил идентичность SHA-256 всех 11 текущих модулей между полным
attempt, повтором и pilot, единственность каждого case и точное покрытие
актуального машинного каталога. Это объединённое evidence, **не второй свежий
полный прогон**; исходный FAIL report не переписан. Скрипт аудита и подробная
классификация: `assemble-fast-stage-evidence.ps1`, `fast-stage-findings.json`.

В каталоге 80 сценариев наблюдают инструментированный native process; ещё два
реально вызывают инструментированный ExportEPUB.dll из системного x86
PowerShell. Два сценария проверяют assets/layout без native запуска; им
не приписывается runtime UB coverage.

Открытых first-party UBSan findings в этом FAST покрытии нет. Весь UBSan этап
ещё не завершён: остаются FULL, финальная ASan regression (включая FAST),
строгий MSVC Release Win32, обычный `verify-release.ps1 -Configuration Release`,
свежие production/package stages и scoped commit. По границе этого продолжения
эти финальные gates не запускались. Удалённый CI по-прежнему NOT RUN.
