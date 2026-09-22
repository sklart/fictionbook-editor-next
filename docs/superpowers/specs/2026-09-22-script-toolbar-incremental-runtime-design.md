# Инкрементальное применение пользовательских панелей скриптов

## Цель

Убрать полный rebuild подсистемы скриптов из штатного пути управления
пользовательскими панелями. Операции создания, переименования, показа,
скрытия, перемещения и удаления панели должны менять только затронутый
runtime/UI, не выполняя discovery каталога `Scripts`, `ScriptLoad`, поиск
`Run`, повторную загрузку изображений, перестроение меню Scripts или
регистрацию hotkeys.

## Неизменяемые границы

- `Toolbars.xml v2`, Script UID, command IDs, hotkeys и правила discovery не
  меняются.
- `scripts-main` сохраняет свой отдельный lifecycle: его HWND создаётся при
  создании главного окна и не проходит через generic create/destroy custom
  toolbar helpers.
- Скрытая custom-панель имеет definition, но не имеет HWND и rebar band.
- Полный `InitializeScripts()` сохраняется для startup/reload и как аварийный
  fallback восстановления runtime.

## Runtime primitives

`CMainFrame` получает узкие helpers для custom-панели по стабильному `id`:

1. создать HWND и rebar band;
2. удалить band и HWND;
3. заполнить toolbar из уже имеющегося каталога command IDs и ImageList
   основной панели;
4. применить items definition;
5. привести порядок существующих visible bands к порядку definitions через
   `CReBarCtrl::MoveBand`.

Полная инициализация использует эти же primitives после единственного
`m_scripts.Initialize()`. Создание custom toolbar не вызывает `ScriptLoad`,
`Catalog::Discover`, повторную регистрацию hotkeys и не читает изображения
скриптов с диска.

## Delta apply

`ApplyScriptToolbarDefinitions(previous, current)` работает в следующем
порядке:

1. захватывает точный persistent snapshot;
2. сохраняет `current` через `PortableToolbarStore`;
3. сопоставляет definitions по `id`;
4. создаёт только новые visible custom-панели;
5. удаляет только отсутствующие custom-панели;
6. show/hide изменяет только соответствующий HWND/band;
7. rename обновляет definition и View menu без изменения HWND;
8. reorder перемещает существующие visible bands;
9. обновляет View → Script toolbars и layout rebar.

`scripts-main` может обновить metadata/visibility, но никогда не создаётся и
не уничтожается как custom toolbar в этом пути.

## Ошибки и rollback

После ошибки runtime delta `PortableToolbarStore::RestoreSnapshot(snapshot)`
вызывается безусловно. Затем применяется обратная delta к `previous`. Если
обратная delta не удалась, вызывается аварийный
`InitializeScriptsFromDefinitions(previous, hadPersistedMainDefinition)`.
Таким образом persistent state восстанавливается точно даже при ошибке
runtime rollback, а полный rebuild не является штатным механизмом.

## Наблюдаемость и тестирование

Добавляется test-only счётчик полных script initialization/discovery и trace
для времени delta apply. Runtime regression после первичного startup выполняет
`create → rename → hide → show → move → delete` и проверяет:

- счётчик полной инициализации/discovery не изменился;
- command IDs и hotkeys не изменились;
- HWND `scripts-main` и незатронутых custom-панелей не изменились;
- только затронутая панель получает/теряет HWND и band;
- reload возвращает тот же persisted state;
- fault injection восстанавливает persistent snapshot и прежний runtime.

Покрытие запускается в portable и isolated-installed lifecycle contours.
