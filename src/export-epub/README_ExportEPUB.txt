ExportEPUB — плагин экспорта EPUB для FictionBook Editor

Версия: 0.2.2-beta

Практические улучшения качества конвертации в этой версии:
- более аккуратный вывод `empty-line`, `subtitle`, `poem`, `epigraph`, `cite`, `annotation`;
- улучшенный CSS для читалок: стихи/цитаты/аннотации/таблицы не получают обычный абзацный отступ;
- сохранение структуры аннотации на отдельной XHTML-странице;
- меньше визуального дублирования заголовков глав;
- устойчивее обработка картинок с отсутствующим или ошибочным `content-type`;
- исправлен `BUILD_BATCH_WIN32_RELEASE.cmd`, теперь он обязан создавать и `ExportEPUB.dll`, и `ExportEPUBBatch.exe`.

===========================================================


Изменения 0.2.2-beta
----------------------
- Исправлены актуальные предупреждения PVS-Studio в ExportEPUBBatch.cpp и ExportEPUBPlugin.cpp.
- Убраны ложноположительные PVS-предупреждения по фиксированным ATL/COM-сигнатурам через точечные подавляющие комментарии.
- Логика экспорта EPUB не менялась, кроме безопасных косметических/диагностических правок вокруг SVG, URL и fallback XML id.


Изменения 0.1.15-beta
----------------------
- Добавлен TEST_EXPORT_AND_PACK.cmd для автоматического прогона тестового набора FB2.
- Командный файл конвертирует указанную папку в EPUB 2 и EPUB 3, собирает CSV/console-логи, при наличии epubcheck.jar запускает EPUBCheck и упаковывает исходные FB2, полученные EPUB и логи в ZIP для анализа.

Изменения 0.1.14-beta
----------------------
- Исправлены внутренние ссылки на верхнеуровневые FB2-секции, вынесенные в отдельные XHTML-главы.
- Улучшено восстановление пробелов вокруг inline-разметки: после двоеточий/запятых пробел восстанавливается, вокруг sub/sup не добавляется.

Назначение
----------
ExportEPUB добавляет в FictionBook Editor экспорт книги FB2 в формат EPUB.

Поддерживаются два варианта результата:

    EPUB 2.0.1
    EPUB 3.3

Плагин рассчитан на 32-битный FB Editor, поэтому основная рабочая конфигурация сборки:

    Release | Win32

После сборки основной файл плагина:

    out\Release\ExportEPUB.dll

Дополнительно собирается консольная утилита массового экспорта:

    out\Release\ExportEPUBBatch.exe


Состав папки проекта
--------------------
В папке export-epub должны находиться только исходники, ресурсы, командные файлы и документация:

    ExportEPUB.cpp
    ExportEPUBPlugin.cpp
    ExportEPUBPlugin.h
    ExportEPUBBatch.cpp
    FbeEpubExport.cpp
    FbeEpubExport.h
    StdAfx.cpp
    StdAfx.h
    dllmain.cpp
    dllmain.h
    ExportEPUB.idl
    ExportEPUB.def
    ExportEPUB.rc
    ExportEPUBPlugin.rgs
    ExportEPUB.sln
    ExportEPUB.vcxproj
    ExportEPUB.vcxproj.filters
    ExportEPUBBatch.vcxproj
    ExportEPUBBatch.vcxproj.filters
    resource.h
    targetver.h
    fbe.h
    ExportEPUB_i.c
    ExportEPUB_i.h

    res\ExportEPUB.ico

    BUILD_WIN32_RELEASE.cmd
    BUILD_BATCH_WIN32_RELEASE.cmd
    CLEAN_AFTER_BUILD.cmd
    REGISTER_EXPORTEPUB.cmd
    UNREGISTER_EXPORTEPUB.cmd
    REBUILD_REGISTER_WIN32_RELEASE.cmd
    EXPORT_FOLDER.cmd
    TEST_EXPORT_AND_PACK.cmd

    README_ExportEPUB.txt
    TODO_ExportEPUB.txt
    CHANGELOG_ExportEPUB.txt


Возможности плагина
-------------------
Плагин поддерживает:

- экспорт FB2 в EPUB 2.0.1 и EPUB 3.3;
- создание корректного EPUB/OCF ZIP-контейнера;
- запись mimetype первым файлом архива без сжатия;
- генерацию META-INF/container.xml;
- генерацию content.opf;
- генерацию toc.ncx для EPUB 2;
- генерацию nav.xhtml для EPUB 3;
- NCX fallback для EPUB 3 по настройке;
- экспорт текста FB2 в XHTML;
- разбиение книги на главы по body/section;
- вложенное оглавление по вложенным section;
- настройку максимальной глубины оглавления;
- перенос section/@id в XHTML-якоря;
- внутренние ссылки и защиту от битых ссылок;
- сноски и примечания из body name="notes";
- comments из body name="comments";
- прямые ссылки на примечания и обратные ссылки из примечаний;
- обложку из title-info/coverpage;
- отдельную страницу обложки по настройке;
- использование первой картинки как обложки, если coverpage отсутствует;
- аннотацию как metadata и отдельную XHTML-страницу по настройке;
- перенос изображений из binary;
- удаление неиспользуемых binary-изображений по настройке;
- сохранение картинок внутри абзацев без невалидного вложенного p;
- перенос таблиц, colspan, rowspan, align, valign;
- перенос стихов: poem, stanza, v;
- перенос эпиграфов, цитат, text-author;
- перенос emphasis, strong, sub, sup, code, strikethrough;
- перенос date как time datetime;
- перенос email как mailto-ссылки;
- перенос home-page как внешней ссылки;
- перенос stylesheet и style из FB2;
- базовые и расширенные metadata: title, authors, translators, genres, keywords, publisher, date, ISBN, series/sequence;
- EPUB 3 collection metadata для нескольких sequence;
- базовые accessibility metadata для EPUB 3;
- настройки CSS: выравнивание, отступ первой строки, CSS-переносы;
- всплывающие подсказки для настроек;
- пресеты настроек: По умолчанию, Совместимость, Подробный EPUB;
- встроенную preflight-проверку без запуска EPUBCheck;
- итоговое окно экспорта;
- открытие EPUB или папки после экспорта;
- массовый экспорт FB2 через ExportEPUBBatch.exe.


Сборка
------
Для обычной сборки DLL и batch-утилиты запустите:

    BUILD_WIN32_RELEASE.cmd

После успешной сборки должны появиться:

    out\Release\ExportEPUB.dll
    out\Release\ExportEPUBBatch.exe

Для сборки batch-утилиты можно использовать:

    BUILD_BATCH_WIN32_RELEASE.cmd

Файл BUILD_BATCH_WIN32_RELEASE.cmd собирает решение Release | Win32, включая ExportEPUB.dll и ExportEPUBBatch.exe.


Регистрация DLL
---------------
Для 32-битного FB Editor DLL нужно регистрировать именно через 32-битный regsvr32:

    C:\Windows\SysWOW64\regsvr32.exe

После сборки запустите:

    REGISTER_EXPORTEPUB.cmd

Или выполните вручную:

    C:\Windows\SysWOW64\regsvr32.exe "out\Release\ExportEPUB.dll"

После регистрации перезапустите FictionBook Editor. В меню экспорта должен появиться пункт экспорта в EPUB.

Для снятия регистрации:

    UNREGISTER_EXPORTEPUB.cmd

Для пересборки и регистрации одной командой:

    REBUILD_REGISTER_WIN32_RELEASE.cmd

Перед регистрацией закрывайте FB Editor, чтобы DLL не была заблокирована.


Командные файлы
---------------

    BUILD_WIN32_RELEASE.cmd
        Сборка ExportEPUB.dll и ExportEPUBBatch.exe в конфигурации Release | Win32.

    BUILD_BATCH_WIN32_RELEASE.cmd
        Сборка batch-утилиты и DLL-зависимости.

    CLEAN_AFTER_BUILD.cmd
        Удаление build/out/build_logs и временных файлов Visual Studio.

    REGISTER_EXPORTEPUB.cmd
        Регистрация собранной 32-битной COM DLL.

    UNREGISTER_EXPORTEPUB.cmd
        Снятие регистрации COM DLL.

    REBUILD_REGISTER_WIN32_RELEASE.cmd
        Пересборка Release | Win32 и регистрация DLL.

    EXPORT_FOLDER.cmd
        Упрощённый запуск ExportEPUBBatch.exe для папки FB2.

    TEST_EXPORT_AND_PACK.cmd
        Автоматическая конвертация тестовой папки FB2 в EPUB 2/3, сбор логов и упаковка архива для анализа.

Примечание:
    Для автоматических запусков без пауз можно задать переменную окружения:

        set NO_PAUSE=1
        BUILD_WIN32_RELEASE.cmd


Массовый экспорт
----------------
После сборки можно запускать:

    out\Release\ExportEPUBBatch.exe --input "D:\Books" --output "D:\EPUB" --version 3 --recursive --overwrite

Через командный файл:

    EXPORT_FOLDER.cmd "D:\Books" "D:\EPUB"

По умолчанию EXPORT_FOLDER.cmd использует:

    --version 3 --recursive --overwrite

Дополнительные параметры можно передавать после двух путей:

    EXPORT_FOLDER.cmd "D:\Books" "D:\EPUB" --version both --report-csv "D:\EPUB\report.csv"

Подготовка архива для анализа
------------------------------
Для проверки тестового набора можно использовать:

    TEST_EXPORT_AND_PACK.cmd

В начале файла задаются основные переменные:

    FB2_INPUT_DIR=D:\fb2_test_suite
    TEST_OUTPUT_ROOT=D:\fb2_test_suite_out
    ARCHIVE_PATH=D:\fb2_epub_test_result.zip
    EPUBCHECK_JAR=D:\Tools\epubcheck.jar

Командный файл выполняет:

- конвертацию исходной папки FB2 в EPUB 2;
- конвертацию исходной папки FB2 в EPUB 3;
- запись CSV-отчётов и console-логов batch-утилиты;
- опциональный запуск EPUBCheck, если RUN_EPUBCHECK=1 и найден epubcheck.jar;
- копирование исходных FB2, полученных EPUB и логов в staging-папку;
- создание ZIP-архива, который можно отправить для анализа.

Отрицательные тесты с заведомо битым XML могут завершаться FAIL без создания EPUB. Это нормально, если batch-обработка продолжается дальше.


Проверка EPUB
-------------
Автоматический запуск EPUBCheck из GUI-плагина не выполняется.

Ручная проверка:

    java -jar epubcheck.jar "Книга.epub"

Текущая версия проверялась на EPUB 2.0.1 и EPUB 3.3.


Настройки экспорта
------------------
Окно настроек открывается кнопкой "Настройки экспорта..." в диалоге сохранения файла.

Основные настройки:

- добавлять NCX fallback для EPUB 3;
- создавать отдельную страницу обложки;
- создавать отдельную страницу аннотации;
- создавать диагностический log;
- показывать итоговое окно после экспорта;
- открывать EPUB после экспорта;
- открывать папку после экспорта;
- показывать предупреждения встроенной проверки;
- настройки CSS;
- настройки оглавления;
- обратные ссылки из примечаний;
- использование первой картинки как обложки;
- удаление неиспользуемых изображений.

Настройки сохраняются в реестр пользователя:

    HKCU\Software\FBETeam\FictionBook Editor\Plugins\ExportEPUB


Выходные файлы
--------------
Для GUI-экспорта имя файла по умолчанию берётся от исходного FB2:

    Саган. Космос.fb2 -> Саган. Космос.epub

Для EPUB 2 и EPUB 3 формат выбирается в фильтре диалога сохранения.


Типовые проблемы
----------------

1. Плагин не появляется в FBE.

   Проверьте, что собрана Release | Win32 версия и DLL зарегистрирована через:

       C:\Windows\SysWOW64\regsvr32.exe

2. regsvr32 сообщает ошибку.

   Закройте FBE, пересоберите проект и повторите регистрацию через REGISTER_EXPORTEPUB.cmd.

3. EPUBCheck пишет File not found.

   Проверяйте полный путь к EPUB или перейдите в папку с файлом командой cd /d.

4. EPUB открывается в одной читалке, но не открывается в другой.

   Сначала проверьте файл через EPUBCheck. Затем проверьте, не отключены ли NCX fallback, обложка или аннотация в настройках.


## Версия консольной утилиты

Для проверки версии пакетного экспортёра:

```cmd
ExportEPUBBatch.exe --version-info
```

Начиная с версии 0.1.11-beta, `ExportEPUBBatch.exe` также содержит VERSIONINFO и иконку в свойствах файла Windows.
Примечание для массового тестирования
-----------------------------------
Начиная с версии 0.1.13-beta ExportEPUBBatch.exe не должен прекращать весь прогон на первом невалидном FB2/XML-файле. Некорректный файл получает статус FAIL, ошибка записывается в консоль/отчёт, после чего обработка остальных файлов продолжается.

Для тестовых наборов с negative_tests рекомендуется запускать с отчётом:

out\Release\ExportEPUBBatch.exe --input "D:\fb2_test_suite" --output "D:\fb2_test_suite_out\epub3" --version 3 --recursive --overwrite --report-log "D:\fb2_test_suite_out\logs\export_epub3.log"


Примечание по датам:
- Для OPF metadata dc:date используется нормализованная дата W3CDTF.
- Если в FB2 есть <date value="YYYY-MM-DD">dd.mm.yyyy</date>, в OPF записывается value, а не локализованный текст.
- Это устраняет ошибки EPUBCheck для EPUB 2 и предупреждения для EPUB 3.


TEST_EXPORT_AND_PACK.cmd note:
Do not put a trailing backslash in FB2_INPUT_DIR. Correct: set "FB2_INPUT_DIR=D:\Books". The script additionally protects calls by passing the input as "<path>\.".
The command file is saved as UTF-8 without BOM to avoid the "я╗┐@echo off" error in cmd.exe.


Примечание по тестам 0.1.17-beta
--------------------------------
В версии 0.1.17-beta исправлены проблемы, найденные на большом наборе из 74 FB2:
вложенные блочные элементы внутри inline-разметки, некорректные media-type изображений,
устаревшие табличные атрибуты в EPUB 3, невалидные fallback UUID и ссылки на главы после удаления пустых секций.

Примечание по тестам 0.1.18-beta
--------------------------------
В версии 0.1.18-beta исправлены оставшиеся ошибки из повторного прогона большого набора из 74 FB2 после 0.1.17-beta:
- <subtitle><image .../></subtitle> больше не создаёт недопустимый XHTML <h3><p>...</p></h3>;
- id из <title><p id="...">...</p></title> сохраняются в XHTML-заголовках, поэтому ссылки оглавления #BdToc_... становятся валидными;
- главы больше не отбрасываются второй раз после построения карты ссылок, чтобы не появлялись ссылки на отсутствующие chapter_XXX.xhtml.

Focused problem-file test package
---------------------------------

Для повторной проверки только ранее проблемных файлов используется:

    TEST_EXPORT_PROBLEM_FILES.cmd

В начале файла задаются переменные:

    FB2_INPUT_DIR=D:\fb2_test_suite
    TEST_OUTPUT_ROOT=D:\fb2_problem_cases_out
    ARCHIVE_PATH=D:\fb2_epub_problem_cases.zip
    EPUBCHECK_JAR=D:\Tools\epubcheck.jar

Командный файл конвертирует выбранные FB2 в EPUB 2 и EPUB 3, запускает EPUBCheck и собирает малый архив с исходниками, EPUB и логами.
Список файлов вынесен в отдельный файл PROBLEM_FILES_ExportEPUB.txt, чтобы менять набор без правки логики .cmd.

## Практические режимы пакетной конвертации

### Конвертация только выбранных FB2

Для повторной проверки небольшого набора файлов используется список:

```text
PROBLEM_FILES_ExportEPUB.txt
```

Каждая строка — путь к `.fb2` относительно папки, указанной в `--input`.
Пустые строки, строки с `#` и `;` игнорируются. Файл списка должен быть сохранён в UTF-8.

Пример запуска напрямую:

```cmd
out\Release\ExportEPUBBatch.exe ^
  --input "D:\fb2_test_suite" ^
  --from-list "PROBLEM_FILES_ExportEPUB.txt" ^
  --output "D:\fb2_problem_cases_out\epub3" ^
  --version 3 ^
  --keep-folder-structure ^
  --overwrite ^
  --report-csv "D:\fb2_problem_cases_out\logs\export_problem_epub3.csv"
```

Быстро посмотреть, какие файлы будут обработаны, без конвертации:

```cmd
out\Release\ExportEPUBBatch.exe --input "D:\fb2_test_suite" --from-list "PROBLEM_FILES_ExportEPUB.txt" --check-input
```

### Малый архив для отправки на анализ

```cmd
TEST_EXPORT_PROBLEM_FILES.cmd
```

Командный файл конвертирует только FB2 из `PROBLEM_FILES_ExportEPUB.txt`, запускает EPUBCheck при наличии `EPUBCHECK_JAR` и создаёт компактный архив:

```text
D:\fb2_epub_problem_cases.zip
```

В архив попадают только выбранные исходные FB2, соответствующие EPUB 2/EPUB 3 и логи.

### Расширенный CSV-отчёт

CSV-отчёт batch-утилиты теперь содержит дополнительные диагностические поля:

```text
Input;Output;Version;Status;ExitCode;DurationMs;SourceSize;OutputSize;SkipReason;Message
```

Это помогает быстро увидеть медленные книги, слишком маленькие EPUB, ошибки экспорта и размер исходного/полученного файла.

### Имена выходных EPUB из метаданных FB2

По умолчанию имя EPUB берётся из имени исходного `.fb2`. Для реальных библиотек можно формировать имя из метаданных:

```cmd
out\Release\ExportEPUBBatch.exe ^
  --input "D:\fb2" ^
  --output "D:\epub" ^
  --version 3 ^
  --recursive ^
  --filename author-title
```

Доступные режимы:

```text
--filename source              исходное имя FB2; режим по умолчанию
--filename title               название книги из <book-title>
--filename author-title        Автор - Название
--filename title-author        Название - Автор
--filename series-number-title Серия 001 Название
```

Особенности:

```text
- читаются book-title, первый author и первая sequence из title-info;
- запрещённые для Windows символы заменяются пробелами;
- слишком длинное имя обрезается;
- если метаданных нет, используется имя исходного FB2;
- при одинаковых именах внутри одного запуска добавляются суффиксы _2, _3, _4.
```

### Продолжение большого экспорта

Для больших папок можно не пересоздавать уже готовые EPUB:

```cmd
out\Release\ExportEPUBBatch.exe ^
  --input "D:\fb2" ^
  --output "D:\epub" ^
  --version 3 ^
  --recursive ^
  --skip-existing
```

`--skip-existing` делает поведение явным: если EPUB уже есть, файл получает статус `SKIP`.

Более удобный режим для повторных запусков:

```cmd
out\Release\ExportEPUBBatch.exe ^
  --input "D:\fb2" ^
  --output "D:\epub" ^
  --version 3 ^
  --recursive ^
  --newer-only
```

`--newer-only` пропускает EPUB, если он новее или того же возраста, что исходный FB2. Если FB2 изменился позже EPUB, файл пересоздаётся. В CSV причина пропуска записывается в поле `SkipReason`.



Практические улучшения 0.1.24-beta
----------------------------------
- В окне настроек появилась опция "Добавлять титульный блок в начало книги".
  Она добавляет в начало первой текстовой главы аккуратный блок с названием, авторами, серией, издателем, датой, ISBN и краткой аннотацией.
  Блок не создаёт отдельный chapter_*.xhtml, поэтому не сдвигает существующие главы и не ломает внутренние ссылки.
- В ExportEPUBBatch.exe добавлены параметры:
    --title-page
    --no-title-page
- Исправлено изменение подписи стандартной кнопки "Сохранить" на "OK" после открытия окна настроек из диалога сохранения.
- Дополнительно улучшено качество XHTML/CSS: image-only абзацы, date, code, strikethrough, lang/xml:lang и annotation.

Тестирование на большом объёме FB2
==================================

Для больших наборов FB2 используйте:

    TEST_EXPORT_LARGE_FB2.cmd

Этот файл конвертирует EPUB 2/EPUB 3, пишет CSV/console-логи, может запускать EPUBCheck
и создаёт компактный ZIP-архив с логами и только проблемными файлами.

Основные переменные внутри TEST_EXPORT_LARGE_FB2.cmd:

    FB2_INPUT_DIR      - папка с исходными FB2;
    TEST_OUTPUT_ROOT   - папка результатов;
    ARCHIVE_PATH       - итоговый ZIP для анализа;
    EPUBCHECK_JAR      - путь к epubcheck.jar;
    NEWER_ONLY=1       - безопасное продолжение прерванного теста;
    START_INDEX        - номер первого FB2 для частичного прогона;
    MAX_FILES          - сколько FB2 обработать, 0 = без ограничения.

Пример:

    cd /d D:\Download\FBeditor\src\export-epub
    BUILD_BATCH_WIN32_RELEASE.cmd
    TEST_EXPORT_LARGE_FB2.cmd

Для подробностей см. LARGE_TEST_ExportEPUB.txt.

Новые параметры ExportEPUBBatch.exe для больших прогонов:

    --start-index N
    --max-files N
    --progress-every N
    --keep-failed-output

По умолчанию частично созданные EPUB после ошибки удаляются, чтобы при следующем запуске
--newer-only не принял пустой или битый файл за готовый результат.

Примечание к 0.2.2-beta
-----------------------
По результатам большого тестового прогона добавлены дополнительные защиты XHTML 1.1/EPUB 2: смешанное текстовое и блочное содержимое внутри div/blockquote переводится в валидные блоки, а заведомо недопустимые file:// ссылки не записываются как EPUB-href.


Примечание 0.2.2-beta: base64-данные FB2 binary декодируются внутренним декодером. Это снижает риск непрозрачных MSXML-ошибок ExitCode 100 на отдельных валидных FB2.
