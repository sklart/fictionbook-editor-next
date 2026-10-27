# Проверка Hunspell-словарей

`spellcheck-corpus` — regression и coverage harness для Hunspell-словарей FBE
Next. Он сравнивает словари на подготовленных текстовых корпусах, размеченных
наборах и benchmark опечаток, а также собирает native `hunspell-probe` с той же
библиотекой Hunspell, что и проект.

Фиксированные лицензируемые FB2-fixtures для русского, украинского и
английского языков вместе с quality/performance baseline находятся в
`fixtures\fb2`. Они обязательны: `test-spellcheck-corpus-regression.ps1`
выполняется из `tools\build\verify-release.ps1`. Корпуса, кэши загрузок,
подготовленные JSONL, результаты сравнений, FB2-review и локальное виртуальное
окружение намеренно игнорируются Git.

## Быстрые проверки

Из корня репозитория запустите unit-тесты (они используют только стандартную
библиотеку Python и не скачивают корпуса):

```powershell
.\tools\tests\spellcheck-corpus\run-tests.ps1
```

Соберите probe (нужны Visual Studio Build Tools/MSBuild и C++ toolset):

```powershell
.\tools\tests\spellcheck-corpus\build-hunspell-probe.ps1 -Configuration Release
```

Исполняемый файл создаётся в `out\tools\hunspell-probe.exe` и не
коммитится. Его режим `-l FILE` печатает только отклонённые слова.
Также поддерживаются `-a` (stdin), `--benchmark FILE` и `--suggest-file FILE`.
Сборка всегда сначала вызывает incremental Build Hunspell с теми же
Configuration и PlatformToolset (по умолчанию `Release` / `v143`), без clean/rebuild.

Probe использует `main(char**)` и narrow file API Hunspell. Пути словаря и
входных файлов должны представляться в системной Windows ANSI code page;
ASCII-пути наиболее переносимы. UTF-8 содержимое файлов от этого не зависит.
Полная поддержка Unicode-путей требует отдельной доработки. Недоступная пара
`.aff`/`.dic` вызывает ошибку до создания Hunspell, а не пустую проверку.

## Подготовка и сравнение корпусов

Для загрузки публичных opt-in наборов нужны Python, PowerShell и пакеты из
`requirements.txt`. Скрипт создаёт локальное `.venv`; можно указать отдельный
каталог результатов.

```powershell
.\tools\tests\spellcheck-corpus\prepare-corpora.ps1 -Profile smoke `
  -Output .\tools\tests\spellcheck-corpus\data\prepared-smoke

.\tools\tests\spellcheck-corpus\compare-dictionaries.ps1 `
  -HunspellExe .\out\tools\hunspell-probe.exe `
  -CurrentDictionary .\out\Release\Dict\ru_RU `
  -CandidateDictionary <candidate-dictionary>\ru_RU `
  -Prepared .\tools\tests\spellcheck-corpus\data\prepared-smoke `
  -Output .\tools\tests\spellcheck-corpus\results-local
```

`CurrentDictionary` и `CandidateDictionary` принимают базовое имя либо путь к
`.aff`/`.dic`. Настройки источников находятся в `corpora.json`; пути к локальным
данным передаются аргументами, а не хранятся в конфигурации.
Comparison не требует download-пакетов или предварительного создания `.venv`:
выбирается project `.venv`, затем `py.exe -3`, затем Windows `python.exe`
(MSYS/devkitPro исключаются); пакеты автоматически не устанавливаются.

Acceptance считается по всем typo cases. `-SuggestionLimit` ограничивает число
уникальных ошибочных форм для подсказок (порядок первого появления);
`-1` проверяет все формы, `0` отключает оценку подсказок. Все случаи с выбранной
формой, включая повторы с разными исправлениями, входят в оценку.
`suggestion_unique_words_evaluated` и `suggestion_cases_evaluated` фиксируют объём;
Top-N/Any — числа попаданий, MRR делится на `suggestion_cases_evaluated`.
При отсутствии оценки метрики равны `null` в JSON и `N/A` в отчёте.
CSV содержит `suggestion_evaluated`; suggestion delta учитывает только случаи,
оценённые у обоих словарей. Acceptance delta сохраняет полный benchmark.
В annotated-таблицах classes/POS/source groups отклонения и доли показаны
парами `tokens / unique`, рядом с полными объёмами словоупотреблений и слов.

## НКРЯ

Harness поддерживает локально установленные датасеты НКРЯ: Морфологический
стандарт, СинТагРус, диахронические датасеты и мультиязычный датасет НКРЯ.
Получите их самостоятельно и используйте только по условиям своей лицензии,
передав корень через `-RncRoot`:

```powershell
.\tools\tests\spellcheck-corpus\prepare-corpora.ps1 -Profile smoke `
  -RncRoot <rnc-dataset-root> -Only rnc_morphological_standard
```

Данные НКРЯ, подготовленные JSONL, контексты, построчные результаты и
производные CSV/FB2 в репозиторий не входят и не должны коммититься. Большие
внешние corpus runs остаются ручными opt-in проверками; в обычный CI они не
добавлены. Это не относится к малым фиксированным RU/UK/EN regression-fixtures:
они входят в обязательный `verify-release.ps1` contour.

## Зависимости и источники

Unit-тестам требуются только Python 3.10+ и PowerShell. Для скачивания
публичных корпусов используются `datasets`, `huggingface_hub`, `pyarrow` и
`requests` (см. `requirements.txt`). `pymorphy3` не является штатной
зависимостью. Происхождение и лицензионные заметки публичных и локальных
источников приведены в [SOURCES.md](SOURCES.md).
