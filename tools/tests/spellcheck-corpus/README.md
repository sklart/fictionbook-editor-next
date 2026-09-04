# Проверка русских Hunspell-словарей

`spellcheck-corpus` — opt-in regression и coverage harness для русских
Hunspell-словарей FBE Next. Он сравнивает словари на подготовленных текстовых
корпусах, размеченных наборах и benchmark опечаток, а также собирает native
`hunspell-probe` с той же библиотекой Hunspell, что и проект.

В репозитории находятся только код, конфигурация и маленькие синтетические
fixtures. Корпуса, кэши загрузок, подготовленные JSONL, результаты сравнений,
FB2-review и локальное виртуальное окружение намеренно игнорируются Git.

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

## Подготовка и сравнение корпусов

Для загрузки публичных opt-in наборов нужны Python, PowerShell и пакеты из
`requirements.txt`. Скрипт создаёт локальное `.venv`; можно указать отдельный
каталог результатов.

```powershell
.\tools\tests\spellcheck-corpus\prepare-corpora.ps1 -Profile smoke `
  -Output .\tools\tests\spellcheck-corpus\data\prepared-smoke

.\tools\tests\spellcheck-corpus\compare-dictionaries.ps1 `
  -HunspellExe .\out\tools\hunspell-probe.exe `
  -CurrentDictionary .\out\Release\dict\ru_RU `
  -CandidateDictionary <candidate-dictionary>\ru_RU `
  -Prepared .\tools\tests\spellcheck-corpus\data\prepared-smoke `
  -Output .\tools\tests\spellcheck-corpus\results-local
```

`CurrentDictionary` и `CandidateDictionary` принимают базовое имя либо путь к
`.aff`/`.dic`. Настройки источников находятся в `corpora.json`; пути к локальным
данным передаются аргументами, а не хранятся в конфигурации.

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
производные CSV/FB2 в репозиторий не входят и не должны коммититься. Полные
corpus runs остаются ручными opt-in проверками; в обычный CI они не добавлены.

## Зависимости и источники

Unit-тестам требуются только Python 3.10+ и PowerShell. Для скачивания
публичных корпусов используются `datasets`, `huggingface_hub`, `pyarrow` и
`requests` (см. `requirements.txt`). `pymorphy3` не является штатной
зависимостью. Происхождение и лицензионные заметки публичных и локальных
источников приведены в [SOURCES.md](SOURCES.md).
