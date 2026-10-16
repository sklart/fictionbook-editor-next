# Проверка остаточных дефектов legacy TODO

Дата: 2026-10-16. Изолированная ветка `codex/legacy-todo-residual`,
исходный код `ca4a7770` (содержит заданную базу `63512a33`).
Посторонние изменения основного checkout не включались.

## PASS

- FBE Release Win32, v143 / VC Tools 14.44.35207, `FbeStrictWarnings=true`.
- Настоящий перенос мышью между секциями (`drop-native-manual`, действие
  пользователя в изолированном окне): `moved=1 scoped=1 dirty=1 undo=1
  previous=1 redo-previous=1 redo=1`. Перед Undo нет открытого parent
  (`GetOpenParentState=S_FALSE`), команда Undo возвращает `S_OK`.
  Соседняя секция сохраняет BR, ID, ссылку и форматирование.
  Ручная проверка выявила, что группа MSHTML уже открыта к DragStart:
  внешняя группа теперь создаётся до обработки нажатия мыши браузером,
  а завершение внутреннего переноса откладывается за DragEnd.
  Обычный щелчок отбрасывает пустую группу; внешнее наведение открывает
  группу заранее и отменяется без записи в историю.
- `test-fbe-legacy-todo-runtime.ps1`, `drop-normalization-runtime`:
  локальный и межсекционный перенос, внешний текст, отмена, отсутствие
  изменений, отсоединённый источник; неизменяемая третья секция с BR,
  выделение, предыдущая история и два Undo/Redo; контракт OLE parent.
- Тот же тест, `drop-files-runtime`: порядок двух изображений, полный
  состав binary и байты после Undo/Redo, dirty/savepoint, закрытие групп,
  четыре отказа InsImage до/после изменения DOM, ошибка в середине пакета,
  отсутствие осиротевших binary, Validate / Save / Reopen.
- `test-fbe-visual-dom-normalizer-runtime.ps1`: девять структурных случаев,
  связанная вставка и Save / Reopen.
- `test-fbe-binary-rename-runtime.ps1`: переименование ссылок и binary,
  Save / Reopen.
- `test-fbe-image-import-generated-id-production.ps1`, `Inline=0` и `1`:
  исходные JPEG-байты, ID, MIME, структура, XSD, Save / Reopen / Save.
- Контракты `test-fbe-image-document-inserter-contract.ps1`,
  `test-fbe-image-document-inserter-boundary.ps1`,
  `test-fbe-visual-dom-normalizer-boundary.ps1`,
  `test-fbe-main-js-binary-behavior.ps1`; `git diff --check`.
- Дополнительный короткий `test-fbe-settings-dialog-runtime.ps1 -LayoutOnly`:
  настройки сохранены и прочитаны после перезапуска. Первый запуск с
  таймаутом 30 секунд не завершился; повторный с 90 секундами прошёл.
  Источник всплывшего пользовательского окна ошибки записи не установлен.

## FAIL и ограничения

- `test-fbe-html-paste-runtime.ps1`: `internal-history` и `plain-middle`
  не проходят проверки Undo/dirty. Оба результата воспроизведены также
  на предыдущем FBE.exe из `legacy-todo-fixes` (2026-10-16 00:49:57).
  Это существующий дефект обычной вставки; он не исправлялся в этой задаче.
- Предварительные ручные прогоны FAIL сохранены в генерируемых отчётах.
  Их причины: открытая нативная группа, вмешательство таймера восстановления
  и диагностические изменения DOM. Финальный наблюдатель использует только
  переменные JavaScript; таймер восстановления отключён только в этом тесте.
  Финальный настоящий перенос и полная проверка истории — PASS.
- NOT RUN: Windows 7, FBE 2.8.5, полноценная ручная матрица реальных книг.
- NOT RUN по ТЗ: `verify-release.ps1`, полные E2E, стресс и все скрипты.

Отчёты текущего checkout: `out/tests/legacy-residual/` (генерируемые файлы).
SHA-256 проверенного FBE.exe:
`633588D1734C5B46793B765B4B16917ECAFC19E66726FD9A209067AD8DAE1A7E`.
