# FBE Next 3.0.10: проверка замечаний сообщества

Дата: 28.10.2026. Статус: **PARTIAL / PENDING MANUAL**.

База: `b8c0611e8947bd1c68beb5f983ef156375ad095c`, на начало работы совпадала с `origin/master`.
Изолированная ветка: `codex/community-defects-3010`.
Изменения оформлены локально; push и закрытие Issues не выполнялись.

| Дефект | Установленная причина | Исправлен | Runtime | Ручной тест |
|---|---|---|---|---|
| Copy/Paste | Отложенное сообщение могло исполняться внутри следующего onpaste, до закрытия native Undo parent; заимствованная ссылка на очередь становилась небезопасной | Да, для подтверждённого повторного входа; исходный периодический сбой пользователя — INCONCLUSIVE | PASS, контролируемый RED/GREEN, реальные MSHTML/OLE | PENDING MANUAL |
| Обычный текст / Undo | Прямой IDM_PASTE при существующем Redo менял DOM без ожидаемой Undo-записи | Да, общий записываемый native text путь | PASS, исходная сборка FAIL / исправленная PASS | PENDING MANUAL |
| Поле «Ссылка» | Повторный SetWindowText и обновление каталога сбрасывали каретку; изменение href не было сгруппировано для Undo | Да | PASS, реальный edit HWND, уведомления, href, Undo/Redo, Save/Reopen | PENDING MANUAL |
| Тело → Код | Относительный HTML-путь применялся к корню XML и мог возвращать ложный успешный ноль | Да, структурное сопоставление | PASS, каретка и диапазоны | PENDING MANUAL |
| Код → Тело | Неверная корневая область, неоднозначность границ TextRange; после редактирования очищалось уже восстановленное выделение | Да | PASS, включая повторный цикл после правки и сохранение диапазона | PENDING MANUAL |
| Дерево документа | После восстановления позиции не обновлялся соответствующий элемент дерева | Да | PASS, проверяется ближайший соответствующий элемент | PENDING MANUAL |
| Импорт EPUB | Пользовательский исходный файл отсутствует | Нет подтверждённого дефекта для исправления; подготовлен аудит пары | Существующие тесты PASS; конкретный файл PENDING | PENDING FILE |
| Диагностика | Подтверждённой ошибки штатного механизма не обнаружено; для вставки не хватало безопасных подробностей сбоя | Добавлены HRESULT/этап/маршрут и runtime-проверки | PASS, перезапуски и реальное аварийное завершение тестового процесса | PENDING MANUAL |

## Подтверждённые воспроизведения и исправления

### Copy/Paste

Контролируемый сценарий исполняет уже опубликованный `WM_FINALIZE_HTML_PASTE` внутри реального захвата следующей вставки. На старом обработчике зарегистрирован **фактический `0x8000FFFF`** из проверки открытого OLE Undo parent, потеря Queue1/Queue2 и нарушение очереди. Код ошибки не выводился из текста «Разрушительный сбой».

Исправление удерживает очередь через owning shared_ptr, считает глубину onpaste и откладывает replay до возврата callback. Если повторная публикация сообщения невозможна, принятые ранее запросы исполняются из штатного OnIdle. Новые COM-ошибки продолжают сообщаться; исключения не подавлены.

Отдельно существующий `queue-plain-two` воспроизвёл ошибку прямой вставки обычного текста на замороженной исходной сборке и на кандидате до исправления. Прямая текстовая вставка теперь использует тот же native TextRange в транзакции с rollback, что и отложенная. Bitmap/DIB остаются на прежнем пути. Тест в скрытом окне явно возвращает фокус и активирует текущий TextRange перед настоящим Paste после Undo; сравнения DOM, каретки, точных Undo/Redo-объектов и признака изменений сохранены.

Исходный случай из пользовательской книги без управляемого повторного входа не воспроизведён: **INCONCLUSIVE**. При раннем полном прогоне был единичный сбой queue-three; последующие повторы на базе и кандидате проходили. Это стало поводом добавить детерминированный сценарий повторного входа, а не основанием считать случайный успех доказательством исправления.

Реальные Copy/Cut тесты не подставляют HTML вместо внутреннего Copy. Проверены символ, слово, абзац, переносы внутри/между абзацами, разделы, два одновременно открытых документа, форматирование/сноска, кириллица/NBSP и повторные Undo/Redo, Save/Reopen. Для полного абзаца тест удаляет его ID перед Copy: действующая защита от дублирования ID намеренно отказывает в небезопасном фрагменте. Копирование целого абзаца с конфликтующим ID не объявляется исправленным. Между процессами проверен текстовый фрагмент; источник clipboard остаётся жив.

Одна промежуточная проверка очистки теста не восстановила исходный clipboard при успешных основных проверках. Добавлены ограниченные повторы OLE restore/flush с выдачей HRESULT при отказе; пустой исходный clipboard не требует Flush. Утверждение `clipboard_restored` осталось обязательным.

### Ссылка и переходы

Для href неизменившийся текст не перезаписывается; выделение edit-контрола сохраняется при обновлении каталога. Изменения реального href имеют Undo-группу. Проверены #id, HTTP/HTTPS и сноска, начало/середина/конец, удаление, Idle, Undo/Redo и сохранение изменённой внешней ссылки.

Body/Source использует временный индекс элементов существующего XML и явную область fbw_body/body/annotation/history. Учитываются notes/comments, одинаковые фразы, вложенность, inline, UTF-8/UTF-16, entities, NBSP и emoji, служебный HTML TBODY. Постоянная копия текста документа не сохраняется.

Сохраняется пустая каретка, а для нулевой границы TextRange используется уже принятый редактором приём +1/-1. Очистка старых HTML-диапазонов перенесена до отображения нового DOM; после mapping новый диапазон не очищается. При недоступных метаданных остаётся живое выделение Body и регистрируется BS202, а позиция внутри XML-разметки обозначается как приблизительная (BS203), без ложного точного успеха.

## Проверки

| Проверка | Результат |
|---|---|
| Release Win32, StrictFirstPartyWarnings, v143 / VC Tools 14.44.35207 | PASS |
| Native Copy/Cut → Paste, 9 сценариев / 10 процессов, включая два документа | PASS |
| test-fbe-html-paste-runtime.ps1 -RealPaste, полный набор 21 | PASS |
| Новые queue-reentrant и queue-reentrant-post-failure | PASS; проверены HRESULT на RED и сохранность принятых запросов на GREEN |
| test-fbe-clipboard-paste-runtime.ps1 | PASS, точный DOM/NBSP/границы области |
| test-fbe-clipboard-paste-boundary.ps1; test-fbe-html-paste-sanitizer.ps1 | PASS |
| Native Body/Source: 180 случаев плюс независимая каретка Source, несколько абзацев, fallback, правка Source | PASS, также диапазон/позиция/следующий цикл после правки, Undo/Redo, Save/Reopen |
| Selection transfer C++ behavior и affected boundary-контракты | PASS |
| Native href и context-attribute-bars runtime/boundary | PASS |
| Body/Source transition runtime; malformed source fallback и correction | PASS |
| Table production round-trip | PASS, 11 снимков, Save → reopen → Save и FB2 XSD |
| test-import-epub-fidelity.ps1 | PASS, 55 Python tests |
| test-compare-epub-fb2.py | PASS, 6 Python tests; всего 61 |
| test-import-epub-v2-runtime.ps1, абсолютный RuntimeDirectory | PASS, реальный COM импортёр |
| Diagnostic dialog runtime | PASS, DPI 96/144/192, 12 языков, light/dark, worker/cancel/review |
| Diagnostic sessions, crash maintenance, operation runtime, privacy native/contract | PASS |
| Сохранение настройки трассы через три процесса normal → extended → normal | PASS, реальная кнопка и подпись режима |
| Реальный crash test с trace=0 и trace=1 | PASS, непустой DMP, TXT, выбранный аварийный сеанс и пакет |
| git diff --check по всей серии и рабочему дереву | PASS |
| verify-release.ps1 -FullValidation | **NOT RUN**, полного release-доказательства нет |
| Ручной desktop smoke Windows 10/11 | **PENDING MANUAL** |

Управляемый crash использует тестовый unhandled EXCEPTION_ACCESS_VIOLATION; это проверка настоящего production crash handler, а не обнаруженный случайный access violation редактора. В обоих режимах сохранены session_id, crash outcome, исходный crash TXT, exception и безопасные HRESULT/этап/маршрут. Без трассы пакет содержит trace/absent.txt **и** минимальные error-events; отсутствие трассы не выдаётся за отсутствие данных. Приватность пакета не ослаблялась.

## Диагностические коды

CP200 фиксирует исходную ошибку до очистки; CP201 — внешний OnPaste; CP202 — deferred replay; CP203/204 — закрытие parent/rollback; CP205 — фактическое состояние OLE parent. Этапы CP301–314: flush, capture, selection, clipboard, normalization scope, open parent, rollback preparation, markup begin, insertion, markup end, normalization, selection finish, parent close, deferred finish/replay. Маршруты CP401/402/403: native/other, internal HTML, external HTML/text. Передаются типизированные коды и HRESULT без текста книги, clipboard и приватных путей. CP297/298/299 используются только guarded runtime-тестами (pre-crash/reentry/injected error).

## Локальные коммиты

| SHA | Содержание |
|---|---|
| 40cc64540c1c77bbf056188245b3fa1d2cdac2dc | Native clipboard tests и диагностика ошибок |
| fae611214ee658a3e95170d64b280f0dd574f68a | Read-only аудит EPUB → FB2 и его тесты |
| f08238c29250fe6235a2603cbfff45140a1c17c6 | Каретка href и Undo |
| 0b62340ba3be51df2525769c5f539ecac6b4a279 | Структурный перенос каретки/диапазонов и дерево |
| 44f0ee8f10baec2f0dbe220e1c0a50ec0f20d3d8 | Проверки настройки трассы и настоящего crash/package |
| e7b0f4240f8834ca026b9429485e1391ac0a7b24 | Защита очереди вставки от повторного входа |
| 4a0826fa3edb000bf060923bbac5002fa61bb0d9 | Undo/rollback прямой вставки обычного текста |

## Изменённые файлы

Production:

- src/fbe/FBEview.cpp, FBEview.h, mainfrm.cpp;
- src/fbe/ui/ContextAttributeBars.cpp, ContextAttributeBars.h;
- src/fbe/source/BodySourceSelectionCoordinator.cpp, BodySourceSelectionState.h, SourceViewSession.cpp, XmlSelectionPosition.h.

Встроенные native тесты:

- src/fbe/testing/RuntimeTestCommunityClipboard.inl, RuntimeTestCommunityHref.inl, RuntimeTestCommunitySelection.inl;
- src/fbe/testing/RuntimeTestEditorAndExport.inl, RuntimeTestNavigationAndStructure.inl;
- src/fbe/testing/DiagnosticDialogTest.inl, RuntimeTestArchiveAndLifecycle.inl.

Скрипты и контракты:

- tools/tests/body-source-selection-transfer-test.cpp;
- tools/tests/test-fbe-body-source-selection-boundary.ps1, test-fbe-body-source-selection-transfer.ps1;
- tools/tests/test-fbe-community-selection-runtime.ps1, test-fbe-href-edit-runtime.ps1;
- tools/tests/test-fbe-context-attribute-bars-runtime.ps1, test-fbe-html-paste-runtime.ps1, test-fbe-native-copy-paste-runtime.ps1;
- tools/tests/test-diagnostic-real-crash-runtime.ps1, test-diagnostic-trace-preference-runtime.ps1;
- tools/tests/compare-epub-fb2.py, test-compare-epub-fb2.py;
- docs/epub-import-pair-audit.md и этот отчёт.

Vendor, версии библиотек, COM IDL, CI, runtime Scintilla/Lexilla, исходные пользовательские книги не изменялись. Полный набор Scripts не загружался в тестах: использован приватный пустой каталог скриптов. Проверка полного пользовательского набора макросов не заявляется.

## Локальные артефакты

Рабочее дерево: `D:\Download\FBeditor\build\source-find-investigation`.
Сборка: `D:\Download\FBeditor\build\community-plain-focus-build.log`.
Финальный целевой лог: `D:\Download\FBeditor\build\community-final-targeted-result.log`.
Финальный native лог: `D:\Download\FBeditor\build\community-final-native-result.log`.
Финальные native отчёты в рабочем дереве: build/community-selection-479ca875ef4c4c4e85202454e9f4d2a0 (cases=180), build/community-href-48a4b6f97c2242adbd62bc83eca291ce, build/community-native-copy-cd3ad4fff81a4354a0065bd91656c436.
Контролируемый RED: `D:\Download\FBeditor\build\community-reentrant-red-result.txt`.
Артефакты диагностики: build/trace-preference-adc0aabb030a4fb1acbace2226b9e848 и build/real-crash-retention-c3e291fe92d74392868d7088ce0f9aa0 в рабочем дереве.

Portable-кандидат: `D:\Download\FBeditor\build\community-3010\runtime\FBE.exe`.
SHA256: `D83EF651EECC9CF9DFF6F507010A876BCFEB57B81EC24D468E1F1A08E6AB3CFC`.
Бинарник собран до оформления последних коммитов; идентичность проверяется этим хешем, встроенный build stamp не объявляется чистым финальным SHA.
Книга: `D:\Download\FBeditor\build\community-3010\COMMUNITY_SMOKE_3010.fb2`.
Синтетическая книга для ручного smoke отдельно проверена MSXML по FictionBook.xsd: PASS.

## Ручная проверка Windows 10/11

Автоматизация desktop оказалась недоступна: `foreground window did not report a process id`, включая одну повторную попытку. По guidance компьютерного инструмента после неудачной recovery работа с GUI остановлена. Native C++ проверки выше не выдаются за ручной smoke. FullValidation не запускался при незавершённом desktop-контуре; никакие проверки release gate не отключались. Windows 7 в этой среде не проверялась.

Применён skill `computer-use` (версия 26.1021.31345); его обязательный `guidance.md` предписывает: “If state capture or window activation fails, stop using prior coordinates or element indexes. Refresh the app/window selection and retry once; report the exact error if recovery fails.” Это ограничение относится к desktop-автоматизации; оно не запрещает native runtime-тесты и само по себе не требует пользовательского разрешения на их запуск.

1. Запустить указанный portable FBE.exe и открыть синтетическую книгу. Для личных книг использовать копии.
2. В Body копировать символ, часть слова, абзац без ID и форматированную ссылку. Вставлять внутри/между абзацами, в соседний раздел, затем во второй открытый документ. Повторить Ctrl+C/V и Ctrl+Z/Y не менее 20 раз. Проверить внешний текст с кириллицей, NBSP, emoji и пустыми строками. После каждой серии Undo должен возвращать точный исходный текст и признак изменений, Redo — результат.
3. Установить каретку в начале/середине/конце двух одинаковых фраз, заголовка, вложенного раздела, аннотации, notes/comments и форматированного текста. Повторить Тело → Код → Тело трижды без выделения и с выделением, включая два абзаца. Убедиться в позиции, выделении и соответствующем узле дерева; простое переключение не должно менять книгу.
4. Изменить текст сначала в Body, затем в Source. Повторить переходы, Undo/Redo; проверить позицию и выделение после возврата и следующего цикла.
5. Выбрать ссылку, редактировать поле «Ссылка»: #note, HTTP/HTTPS; ввод/вставка/удаление в начале, середине, конце, выделение. Проверить отсутствие прыжка после Idle, реальный переход по ссылке и Undo/Redo изменения атрибута.
6. Сохранить под новым именем, закрыть и открыть. Проверить текст, форматирование, сноски, ссылки, структуру и валидацию FB2.
7. Справка → Диагностика: проверить «Обычный», переключить трассу, перезапустить; проверить «Расширенный», вернуть настройку и снова перезапустить. Сформировать пакет нужного сеанса. При сбое вставки сохранить пакет сразу и после перезапуска; записать точные действия. Самостоятельно аварийно завершать рабочую книгу не требуется.
8. Для EPUB передать исходный проблемный EPUB и результат FB2; выполнить процедуру docs/epub-import-pair-audit.md. До этого пользовательский дефект импорта остаётся PENDING FILE.
