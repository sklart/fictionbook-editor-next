# Issue 71: изображения меню

Основной WTL command bar получает сопоставление из `IDR_MAINFRAME_SMALL` и своего `m_arrCommand`. Штатное тёмное меню получает только отдельно зарегистрированные bitmap; для перечисленных ниже 35 команд отдельной регистрации не было. Названия — исходные подписи ресурса на английском, локализованный runtime использует те же Command ID. Таблица описывает цепочку ресурсов, а не самостоятельную визуальную проверку пикселей.

| Command ID | Пункт | Светлая тема / источник | Тёмная тема до | Тёмная тема после |
|---|---|---|---|---|
| `ID_FILE_NEW` | New | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_FILE_OPEN` | Open... | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_FILE_SAVE` | Save | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_FILE_VALIDATE` | Validate | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_VIEW_DESC` | Description | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_VIEW_BODY` | Body | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_VIEW_SOURCE` | Source | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_VIEW_OPTIONS` | Settings... | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_ADDBINARY` | Add Binary Object... | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_FIND` | Find... | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_FINDNEXT` | Find Next | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_REPLACE` | Replace... | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_CUT` | Cut | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_COPY` | Copy | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_PASTE` | Paste | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_UNDO` | Undo | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_REDO` | Redo | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_INS_IMAGE` | Image | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_INS_INLINEIMAGE` | Inline image | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_ADD_IMAGE` | Section image | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_STYLE_LINK` | Link... | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_STYLE_NOTE` | Footnote... | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_STYLE_NOLINK` | Remove link | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_STYLE_NORMAL` | Normal | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_STYLE_SUBTITLE` | Subtitle | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_STYLE_TEXTAUTHOR` | Text Author | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_GOTO_FOOTNOTE` | Go to footnote or reference | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_ADD_BODY` | Body | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_ADD_TITLE` | Title | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_ADD_EPIGRAPH` | Epigraph | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_ADD_ANN` | Annotation | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_INS_POEM` | Poem | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_EDIT_INS_CITE` | Cite | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_INSERT_TABLE` | Table | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |
| `ID_TOOLS_SPELLCHECK` | Spell check... | Toolbar_small / WTL | нет регистрации | WTL → native bitmap |

| Категория | Источник и состояние до | Состояние после |
|---|---|---|
| Проверка качества FB2 | `IDI_FB2_QUALITY`, WTL `AddIcon`; native регистрации не было | общий мост из WTL image list |
| Вставка текста, обновление скриптов, действия таблиц | Отдельные BMP через WTL и native-регистрации | существующая регистрация сохранена |
| Плагины | WTL поддерживал `AddIcon`, но optional `icon` манифеста не читался; текущий штатный манифест значки не задаёт | optional `icon` читается, явно заданный значок проходит через общий мост |
| Скрипты | Отдельная native-регистрация; BMP без alpha-маски | bitmap преобразуется с маской WTL |
| Папки скриптов | Native-регистрация существовала, но popup ID читался как `-1` | применение ищет `MIIM_ID` |

## Автоматические проверки

- Release Win32, v143, строгие предупреждения: сборка FBE прошла.
- Каталог меню и контракт issue #64 прошли. Runtime-прогон Dark/ru-RU с 12 циклами и отдельный запуск Light/en-US прошли; тестовые манифесты явно задают две иконки плагинов, штатный манифест их не содержит.
- Проверены 51 сопоставление, прозрачность и отсутствие розового фона, преобразование для 96/144/192 DPI, папки скриптов и восстановление изменённого меню. После 12 пересозданий GDI: 226 → 226; отдельные проверки замены и времени жизни также сбалансированы.
- В одном более раннем прогоне ожидание открытия меню истекло; последующие прогоны прошли. Это ограничение стабильности автоматизации остаётся отмеченным.
- `verify-release.ps1`: INCONCLUSIVE — в изолированной сборке отсутствует `out/build-provenance/CommonCore-Release.json`, создаваемый полным авторитетным конвейером сборки.

Проверка DPI преобразования bitmap не равнозначна реальному визуальному тесту масштабирования Windows. Проверка в Windows 10/11, экранных режимов 100/150/200%, высокой контрастности и ru-RU/en-US остаётся отдельным ручным gate.

## Ручной smoke Windows 10 / 11

1. Открыть книгу, сравнить «Файл», «Правка», «Вставка», «Стиль», «Таблица», «Сервис» и вложенные меню скриптов/плагинов в Light и Dark; проверить отсутствие розовых полей и читаемость активных и недоступных значков.
2. Переключить Light → Dark → Light при открытой книге; открыть вложенные меню мышью и клавиатурой, выбрать несколько безопасных команд.
3. Дважды обновить скрипты, снова раскрыть их папки и убедиться, что названия, иконки и команды сохранились; повторить с открытым меню по возможности.
4. Повторить репрезентативные проверки в ru-RU и en-US, с высокой контрастностью и при масштабировании 100%, 150%, 200%. Проверить холодный запуск в Dark и автоматическую тему при переключении системной темы.
