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

## Последствия

Не меняются regex-семантика, cache, PCRE2 flags, COM match collection или
поведение поиска/замены. `test-fbe-search-boundary.ps1` закрепляет размещение
и отсутствие зависимостей на UI/document coordinators; существующие PCRE2
cache, match-loop и wrapper fixtures проверяют поведение.
