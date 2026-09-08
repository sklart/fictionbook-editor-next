# 0006. Regex backend и PCRE2 helper-ы образуют подсистему поиска

## Контекст

`RegexBackend`, PCRE2 implementation, compiled-code cache и match-loop
составляют один внутренний контур поиска. Ранее файлы лежали в корне
`src/fbe`, хотя не владеют главным окном, документом или SettingsDlg и имеют
отдельные fixture-проверки PCRE2.

## Решение

Группа расположена в `src/fbe/search` и по-прежнему компилируется прямо в
FBE. Она остаётся editor-only: адаптер `RegexBackend.cpp` использует FBE
`apputils`, но не становится общим компонентом или replacement публичного API.

`SearchTypes`, `SearchTextSnapshot`, `LiteralSearch`, `SearchSession` и
`SearchResults` образуют рядом чистый Search Core. Он работает только с
UTF-16 offsets, query/hit/capture-моделью и generation документа, не хранит
DOM pointer, HWND или модельный диалог. `SearchDocumentAdapter` и
`DocumentSearchCoordinator` находятся с editor-side: только они строят
snapshot из MSHTML и применяют найденный offset как selection.

Обычный Design-mode Find уже использует этот путь для literal search:
`SearchTextSnapshot -> LiteralSearch -> SearchSession -> adapter`. Основной
алгоритм больше не вызывает `IHTMLTxtRange::findText()`. Legacy MSHTML
сопоставление сохранено только в differential regression test, а legacy
regex COM API (`IRegExp2`) и Replace остаются compatibility boundaries до
их отдельной миграции на result/capture model.

## Последствия

Не меняются regex-семантика, cache, PCRE2 flags, COM match collection или
поведение legacy Replace. `test-fbe-search-boundary.ps1` закрепляет
размещение и отсутствие зависимостей Search Core на UI/document
coordinators; PCRE2 cache, match-loop, literal differential и adapter
fixtures проверяют matching, UTF-16 mapping и navigation contracts.
