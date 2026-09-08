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

Обычный Design-mode Find, Find All и regex Find используют один путь:
`SearchTextSnapshot -> LiteralSearch/RegexBackend -> SearchSession -> adapter`.
Навигация начинается от snapshot-offset текущего caret/selection и хранит
generation документа; нулевые regex-hit'ы при повторном Find Next/Previous
пропускают текущую позицию, поэтому не зацикливаются. Основной literal
алгоритм больше не вызывает `IHTMLTxtRange::findText()`; MSHTML-сопоставление
сохранено только в differential regression test.

`SearchResults` хранит чистые hit/capture/preview данные и revision. Section
label и создание невыделенного `IHTMLTxtRange` остаются editor-side задачами
`SearchDocumentAdapter`. Replace All сначала формирует и показывает этот
набор результатов, валидирует все DOM-диапазоны до изменения документа, а
затем применяет замены справа налево в одной undo-группе. Для regex
editor-side адаптер создаёт совместимый `IMatch2` только на границе старого
шаблонизатора replacement, сохраняя пустые и именованные captures.

## Последствия

Не меняются regex-семантика, PCRE2 flags и legacy public COM collection.
`test-fbe-search-boundary.ps1` закрепляет размещение и отсутствие
зависимостей Search Core на UI/document coordinators; session, PCRE2 cache,
match-loop, literal differential и adapter fixtures проверяют matching,
UTF-16 mapping, generation/revision и navigation contracts. Отдельный
`benchmark-pcre2-cache.ps1` измеряет cache на коротких и Find-All-large
сценариях без введения нестабильного time-based CI-порога.
