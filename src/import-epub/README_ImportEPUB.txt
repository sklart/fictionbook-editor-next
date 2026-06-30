ImportEPUB — плагин импорта EPUB для FictionBook Editor
========================================================

Назначение
----------
ImportEPUB добавляет в FictionBook Editor импорт книг EPUB с преобразованием во внутренний FB2/DOM-документ FBE.

Плагин рассчитан на 32-битный FB Editor, поэтому основная рабочая конфигурация сборки:

    Release | Win32

После сборки основной файл плагина:

    out\Release\ImportEPUB.dll

Дополнительно собирается консольная утилита для проверки и пакетной конвертации EPUB в FB2:

    out\Release\ImportEPUBBatch.exe


Состав папки проекта
--------------------
В папке import-epub должны находиться только исходники, ресурсы, командные файлы и документация:

    ImportEPUBPlugin.cpp
    ImportEPUBPlugin.h
    ImportEPUBModule.h
    ImportEPUBGuids.h
    FBEPluginInterfaces.h
    EpubImport.cpp
    EpubImport.h
    ImportOptionsDialog.cpp
    ImportOptionsDialog.h
    MsxmlImport.h
    Registry.cpp
    ImportEPUBBatch.cpp
    stdafx.cpp
    stdafx.h
    dllmain.cpp
    ImportEPUB.def
    ImportEPUB.rc
    ImportEPUB.sln
    ImportEPUB.vcxproj
    ImportEPUB.vcxproj.filters
    ImportEPUBBatch.vcxproj
    ImportEPUBLunaSVG.vcxproj
    ImportEPUBLunaSVG.cpp
    ImportEPUBLunaSVG.def
    resource.h

    res\ImportEPUB.ico

    BUILD_WIN32_RELEASE.cmd
    BUILD_BATCH_WIN32_RELEASE.cmd
    CLEAN_AFTER_BUILD.cmd
    REGISTER_IMPORTEPUB.cmd
    UNREGISTER_IMPORTEPUB.cmd
    REBUILD_REGISTER_WIN32_RELEASE.cmd
    IMPORT_FOLDER.cmd
    CONVERT_AND_PACK_TEST_RESULTS.cmd
    BUILD_LUNASVG_ADAPTER_WIN32_RELEASE.cmd
    CHECK_REGISTRATION.cmd

    README_ImportEPUB.txt
    TODO_ImportEPUB.txt
    CHANGELOG_ImportEPUB.txt

    thirdparty\lunasvg\README_PLACE_LUNASVG_HERE.txt


Возможности плагина
-------------------
Плагин поддерживает:

- подключение к FBE как COM-плагин импорта через IFBEImportPlugin;
- пункт меню Файл -> Импорт -> из EPUB...;
- выбор EPUB через системный диалог;
- кнопку «Настройки импорта...» в окне выбора EPUB;
- всплывающие подсказки ко всем настройкам импорта;
- окно настроек сгруппировано по разделам: содержимое и структура, изображения и SVG, ссылки и сноски, очистка и фильтрация, проверка/логирование/отладка;
- сохранение настроек в HKCU\Software\FBETeam\FictionBook Editor\ImportEPUB;
- распаковку EPUB через встроенный Windows ZIP provider без PowerShell Expand-Archive и без tar.exe;
- проверку временной распаковки на выход за пределы рабочей папки и reparse-point/symlink;
- чтение META-INF/container.xml;
- поиск OPF-пакета;
- чтение metadata, manifest, spine;
- проверку mimetype и предупреждение о META-INF/encryption.xml;
- учёт META-INF/rights.xml и META-INF/signatures.xml в диагностике;
- поддержку EPUB 2 OPF guide/reference;
- импорт XHTML строго в порядке spine;
- использование nav.xhtml и toc.ncx для заголовков глав;
- создание секций по h1/h2/h3;
- пропуск служебных страниц cover/nav/toc/notes/titlepage/landmarks/page-list/copyright/calibre;
- удаление служебных и пустых разделов;
- импорт метаданных EPUB в description/title-info, document-info, publish-info и custom-info;
- нормализацию языка ru-RU / ru_RU -> ru;
- перенос авторов и переводчиков с разбиением на first-name, middle-name, last-name;
- перенос серии calibre:series / calibre:series_index и EPUB 3 collection metadata в sequence;
- перенос ISBN, publisher, date, rights, source, modified, subjects;
- сопоставление dc:subject с несколькими FB2-жанрами;
- исправление типичных UTF-8/Windows-1251 кракозябр;
- очистку типографического мусора: soft hyphen, NBSP, zero-width marks;
- импорт обложки в coverpage;
- поиск обложки через cover-image, meta name="cover", OPF guide и cover.xhtml;
- импорт изображений как image + binary;
- определение content-type изображений по OPF media-type и расширению файла;
- поддержку src, href, xlink:href и srcset для изображений;
- импорт сносок в body name="notes";
- поддержку epub:type и DPUB-ARIA ролей doc-noteref/doc-footnote/doc-endnote/doc-backlink;
- удаление обратных ссылок из сносок;
- сохранение обычных ссылок и якорей;
- импорт таблиц table/tr/td/th, caption, colspan, rowspan, align, valign;
- импорт списков ul/ol/li, включая start, value, type и CSS list-style-type;
- импорт списков определений dl/dt/dd;
- распознавание подзаголовков как subtitle;
- базовое распознавание стихов, эпиграфов и цитат;
- перенос подписей автора как text-author;
- импорт figure/picture/figcaption;
- импорт pre/code/kbd/samp/var;
- поддержку abbr/acronym, ruby/rt/rp, q, del/s/strike;
- импорт pagebreak/hr как empty-line и, при наличии, номера страницы;
- пропуск скрытых элементов hidden/aria-hidden/display:none/visibility:hidden;
- проверку итогового FB2 перед открытием;
- диагностику отсутствующих binary, битых note-ссылок, дублей id и подозрительных внешних ссылок;
- лог импорта рядом с EPUB по настройке или автоматически при ошибках/предупреждениях;
- сохранение промежуточного FB2 рядом с EPUB по настройке;
- консольную утилиту ImportEPUBBatch.exe для одиночной и пакетной конвертации;
- CSV-отчёт пакетной конвертации.
- опциональную растеризацию SVG через отдельную ImportEPUBLunaSVG.dll рядом с основными бинарниками;
- автоматическую PNG/JPEG-заглушку, если SVG-библиотека отсутствует или рендеринг не удался.


Автоматический тест и упаковка результатов
------------------------------------------
Для прогона тестового набора EPUB, сбора логов и упаковки материалов для анализа добавлен файл:

    CONVERT_AND_PACK_TEST_RESULTS.cmd

Перед запуском откройте его в текстовом редакторе и при необходимости измените переменные в начале файла:

    set "INPUT_DIR=D:\epub_test_suite"
    set "OUTPUT_DIR=D:\epub_test_suite_fb2"
    set "PACKAGE_DIR=D:\epub_import_test_packages"
    set "IMPORT_OPTIONS=--recursive --preserve-tree --overwrite --stats --log --svg png"

Скрипт выполняет:

- проверку наличия ImportEPUBBatch.exe;
- при необходимости автоматическую сборку через BUILD_BATCH_WIN32_RELEASE.cmd;
- пакетную конвертацию EPUB -> FB2;
- создание CSV-отчёта ImportEPUBBatch_report.csv;
- сбор исходных EPUB, сконвертированных FB2 и *.ImportEPUB.log;
- создание ZIP-архива вида ImportEPUB_test_YYYYMMDD_HHMMSS.zip.

Запуск:

    CONVERT_AND_PACK_TEST_RESULTS.cmd

Полученный ZIP можно отправлять для анализа качества импорта.


SVG при импорте
---------------
По умолчанию ImportEPUB пытается преобразовать SVG в PNG, потому что FictionBook Editor и многие FB2-читалки не отображают SVG-бинарники напрямую.

Важно: сам ImportEPUB.dll и ImportEPUBBatch.exe не содержат LunaSVG и не зависят от неё при запуске. Для растеризации SVG используется необязательная дополнительная DLL:

    ImportEPUBLunaSVG.dll

Её нужно положить рядом с ImportEPUB.dll и/или ImportEPUBBatch.exe. Если DLL отсутствует, имеет неправильную разрядность или не смогла отрисовать конкретный SVG, ImportEPUB создаёт видимую PNG/JPEG-заглушку. Это лучше, чем оставлять пустую страницу или битую ссылку на отсутствующий binary.

Параметр командной строки:

    --svg keep|png|jpg|skip

Режимы:

    keep  — оставить SVG как image/svg+xml;
    png   — преобразовать SVG в PNG через ImportEPUBLunaSVG.dll, при неудаче вставить PNG-заглушку;
    jpg   — преобразовать SVG в JPEG через ImportEPUBLunaSVG.dll + GDI+, при неудаче вставить JPEG-заглушку;
    skip  — пропустить SVG.

Сборка необязательной библиотеки LunaSVG:

    BUILD_LUNASVG_ADAPTER_WIN32_RELEASE.cmd

Перед сборкой см. файл:

    thirdparty\lunasvg\README_PLACE_LUNASVG_HERE.txt

Сборка
------
Для обычной сборки DLL и консольной утилиты запустите:

    BUILD_WIN32_RELEASE.cmd

После успешной сборки должны появиться:

    out\Release\ImportEPUB.dll
    out\Release\ImportEPUBBatch.exe

Для сборки только консольной утилиты можно использовать:

    BUILD_BATCH_WIN32_RELEASE.cmd

Файл BUILD_BATCH_WIN32_RELEASE.cmd сначала пересобирает ImportEPUB.dll, затем ImportEPUBBatch.exe. Это важно, потому что batch-утилита использует общее ядро импорта и должна поставляться вместе с актуальной DLL.


Регистрация DLL
---------------
FB Editor использует 32-битный COM-плагин. Для текущей сборки FBE рабочая регистрация выполняется в 32-битной ветке HKLM, поэтому скрипт нужно запускать от имени администратора:

    REGISTER_IMPORTEPUB.cmd

Скрипт ищет DLL в типовых местах:

    out\Release\ImportEPUB.dll
    out\ImportEPUB.dll
    ImportEPUB.dll

Для сборки и регистрации одной командой используйте:

    REBUILD_REGISTER_WIN32_RELEASE.cmd

Для удаления регистрации:

    UNREGISTER_IMPORTEPUB.cmd

Для проверки регистрации:

    CHECK_REGISTRATION.cmd

Важно: плагин предназначен для Win32/x86 FBE. 64-битный FBE не сможет загрузить 32-битную DLL.


Очистка после сборки
--------------------
Для удаления результатов сборки и временных файлов запустите:

    CLEAN_AFTER_BUILD.cmd

Обычно удаляются каталоги:

    build\
    out\

а также промежуточные файлы Visual Studio, если они появились в папке проекта.


Как импортировать из FB Editor
------------------------------
1. Соберите проект в конфигурации Release | Win32.
2. Зарегистрируйте ImportEPUB.dll через REGISTER_IMPORTEPUB.cmd.
3. Запустите FB Editor.
4. Выберите Файл -> Импорт -> из EPUB...
5. При необходимости нажмите «Настройки импорта...».
6. Укажите EPUB-файл и нажмите «Импорт».
7. FBE получит готовый FB2 DOM и откроет книгу как обычный FB2-документ.


Настройки импорта
-----------------
Окно настроек вызывается кнопкой «Настройки импорта...» в диалоге выбора EPUB. Для каждого пункта есть всплывающая подсказка.

Основные настройки:

- импортировать обложку;
- импортировать изображения;
- импортировать сноски;
- использовать nav.xhtml / toc.ncx для заголовков;
- исправлять типичные ошибки кодировки;
- пропускать служебные страницы;
- импортировать таблицы;
- импортировать списки;
- распознавать стихи, эпиграфы и цитаты;
- распознавать подзаголовки;
- сохранять ссылки и якоря;
- очищать типографику текста;
- импортировать разделители и номера страниц;
- пропускать скрытые элементы;
- проверять итоговый FB2 перед открытием;
- использовать CSS-классы для структуры;
- удалять обратные ссылки из сносок;
- удалять служебные и пустые разделы;
- создавать лог при ошибках и предупреждениях;
- сохранять промежуточный FB2 при ошибке;
- сохранять FB2-копию рядом с EPUB.


Консольная утилита ImportEPUBBatch.exe
-------------------------------
Одиночная конвертация:

    out\Release\ImportEPUBBatch.exe "D:\Books\book.epub" "D:\Books\book.fb2" --log

Пакетная конвертация папки:

    IMPORT_FOLDER.cmd "D:\Books_EPUB" "D:\Books_FB2" --recursive --preserve-tree --log

Прямой запуск пакетного режима:

    out\Release\ImportEPUBBatch.exe --batch "D:\Books_EPUB" "D:\Books_FB2" --recursive --preserve-tree --log

Поддерживаемые ключи:

    --batch
    --recursive
    --preserve-tree
    --overwrite
    --log / --no-log
    --stats
    --dry-run
    --quiet
    --no-report
    --report <file.csv>
    --skip-existing
    --fail-fast
    --profile full|text|minimal
    --cover / --no-cover
    --images / --no-images
    --notes / --no-notes
    --tables / --no-tables
    --lists / --no-lists
    --links / --no-links
    --subtitles / --no-subtitles
    --poems / --no-poems
    --pagebreaks / --no-pagebreaks
    --split-sections / --no-split-sections
    --css-semantic / --no-css-semantic
    --clean-typography / --no-clean-typography
    --skip-hidden / --include-hidden
    --skip-service-pages / --keep-service-pages
    --remove-service-sections / --keep-service-sections
    --remove-footnote-backlinks / --keep-footnote-backlinks
    --validate / --no-validate
    --diagnostic-section
    --save-copy
    --save-on-warning
    --keep-temp-on-error

Профили импорта:

    --profile full      полный импорт, профиль по умолчанию
    --profile text      текстовый импорт: без обложки, изображений, таблиц и pagebreak
    --profile minimal   минимальный текст: без изображений, сносок, таблиц, списков, ссылок и семантических блоков

Профиль можно комбинировать с отдельными переключателями. Например:

    out\Release\ImportEPUBBatch.exe book.epub book.fb2 --profile text --tables --notes

В пакетном режиме создаётся отчёт:

    ImportEPUBBatch_report.csv


PVS-Studio
----------
Проект подготовлен так, чтобы PVS-Studio мог анализировать его из чистого состояния без отдельного подготовительного скрипта.

Для MSXML используется стабильный заголовок Windows SDK <msxml6.h>, а не #import <msxml6.dll>. Поэтому при анализе не требуются заранее сгенерированные msxml6.tlh / msxml6.tli в промежуточной папке.

Рекомендуемый запуск анализа: выбрать конфигурацию Release | Win32 или Debug | Win32 и запустить анализ всего решения ImportEPUB.sln.


Ограничения текущей версии
--------------------------
- DRM не поддерживается; при наличии META-INF/encryption.xml выводится предупреждение.
- Fixed-layout EPUB, JavaScript, MathML, аудио и видео не поддерживаются.
- CSS не переносится как визуальное оформление; используются только смысловые классы и epub:type.
- Распознавание стихов, эпиграфов, цитат и служебных страниц эвристическое.
- ZIP читается через Windows Shell ZIP provider с временной распаковкой. Полностью потоковый miniz/minizip-ридер можно добавить следующим этапом.

Примечание по PVS-Studio/MSXML:
Проект не использует #import <msxml6.dll> и не требует предварительной генерации msxml6.tlh/msxml6.tli. MSXML подключается через <msxml6.h>, а создание DOM выполняется через ProgID Msxml2.DOMDocument.6.0, поэтому отдельный подготовительный CMD-файл не нужен.
Дополнительные примеры ImportEPUBBatch.exe:

Текстовый импорт одной книги без изображений и обложки:

    out\Release\ImportEPUBBatch.exe D:\Books\book.epub D:\Books\book.fb2 --profile text --log

Минимальная проверка книги без записи результата:

    out\Release\ImportEPUBBatch.exe D:\Books\book.epub D:\Temp\book.fb2 --profile minimal --dry-run --stats

Пакетная конвертация с остановкой на первой ошибке:

    out\Release\ImportEPUBBatch.exe --batch D:\Books_EPUB D:\Books_FB2 --recursive --fail-fast --report D:\Books_FB2\report.csv

Пакетная конвертация с пропуском уже готовых файлов:

    out\Release\ImportEPUBBatch.exe --batch D:\Books_EPUB D:\Books_FB2 --recursive --preserve-tree --skip-existing --stats --report D:\Books_FB2\report.csv


Ресурсы ImportEPUBBatch.exe
--------------------
Консольная утилита ImportEPUBBatch.exe имеет собственный ресурс версии и иконку.
В свойствах файла должны отображаться:
- CompanyName: sklart
- FileDescription: EPUB to FB2 console converter for FictionBook Editor
- InternalName: ImportEPUBBatch
- OriginalFilename: ImportEPUBBatch.exe
- ProductName: FictionBook Editor EPUB import tools
- LegalCopyright: Copyright 2026 Artem Sklyarov aka sklart

Ресурсный файл утилиты: ImportEPUBBatch.rc.
Иконка используется общая с плагином: res\ImportEPUB.ico.


[Fix44]
BUILD_BATCH_WIN32_RELEASE.cmd исправлен: удалены управляющие символы, появившиеся из-за некорректного экранирования путей, и добавлен явный приоритет MSBuild по пути C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe. Скрипт сначала собирает ImportEPUB.dll, затем ImportEPUBBatch.exe.

Примечание по выводу в консоль
------------------------------
ImportEPUBBatch выводит текст через Unicode API WriteConsoleW. Это нужно, чтобы русские сообщения вида
"Не удалось импортировать..." корректно отображались в cmd.exe и Windows Terminal, а не превращались в
"?? ??????? ...". Если вывод перенаправлен в файл, используется UTF-8.

EPUB с изображениями напрямую в spine
-------------------------------------
Некоторые тестовые EPUB содержат в spine не XHTML-документы, а изображения напрямую, например JPEG или SVG.
ImportEPUB импортирует такие элементы как отдельные FB2-секции с изображением и добавляет соответствующий
ресурс в <binary>. Это важно для файлов вида svg-in-spine.epub, sous-le-vent_svg-in-spine.epub,
haruko-jpeg.epub и page-blanche-bitmaps-in-spine.epub.



### SVG при импорте

FB2/FBE не всегда отображает SVG как обычные изображения. В настройках импорта добавлен пункт "SVG-изображения":

- Оставлять SVG как есть;
- Преобразовывать SVG в PNG;
- Преобразовывать SVG в JPEG;
- Пропускать SVG.

По умолчанию выбран PNG. В режимах PNG/JPEG ImportEPUB ищет рядом с ImportEPUB.dll или ImportEPUBBatch.exe необязательную библиотеку `ImportEPUBLunaSVG.dll`. Это отдельная DLL-обёртка над LunaSVG. Основной плагин не зависит от неё: если DLL нет или рендеринг не удался, вместо SVG вставляется видимая PNG/JPEG-заглушка с именем исходного SVG.

Для `ImportEPUBBatch.exe` доступен ключ:

```bat
ImportEPUBBatch.exe input.epub output.fb2 --svg png
ImportEPUBBatch.exe input.epub output.fb2 --svg jpg
ImportEPUBBatch.exe input.epub output.fb2 --svg keep
ImportEPUBBatch.exe input.epub output.fb2 --svg skip
```

Для сборки необязательной SVG-библиотеки используйте:

```bat
BUILD_LUNASVG_ADAPTER_WIN32_RELEASE.cmd
```

Перед этим положите заголовки и `lunasvg.lib` согласно инструкции в `thirdparty\lunasvg\README_PLACE_LUNASVG_HERE.txt`.


Проверка ресурсов версии
------------------------
После сборки можно проверить, что в ImportEPUB.dll и ImportEPUBBatch.exe реально попали сведения VersionInfo:

    CHECK_VERSIONINFO.cmd

Скрипт выводит CompanyName, FileDescription, FileVersion, InternalName, OriginalFilename, ProductName и ProductVersion для обоих файлов из out\Release.

Если в свойствах файла Windows эти поля пустые, убедитесь, что собирается актуальный проект и что проверяется файл именно из out\Release. В этой версии ресурс VS_VERSION_INFO явно определён как ресурс версии №1, чтобы Windows Explorer корректно показывал сведения о файле.


VersionInfo ресурсов
---------------------
ImportEPUB.dll и ImportEPUBBatch.exe имеют разные наборы VersionInfo-строк.

ImportEPUB.dll:
  FileDescription  = FictionBook Editor EPUB import plugin
  InternalName     = ImportEPUB
  OriginalFilename = ImportEPUB.dll
  ProductName      = FictionBook Editor EPUB import plugin

ImportEPUBBatch.exe:
  FileDescription  = FictionBook Editor EPUB batch import utility
  InternalName     = ImportEPUBBatch
  OriginalFilename = ImportEPUBBatch.exe
  ProductName      = FictionBook Editor EPUB batch import utility

Проверка после сборки:
  CHECK_VERSIONINFO.cmd


LunaSVG в составе проекта
-------------------------
В архив проекта добавлены исходники LunaSVG 3.5.0 и PlutoVG в папку:

  thirdparty\lunasvg

Основной импортёр не зависит от них напрямую. Для SVG используется отдельная DLL:

  out\Release\ImportEPUBLunaSVG.dll

Сборка библиотеки и адаптера:

  BUILD_LUNASVG_LIBRARY_WIN32_RELEASE.cmd   - собирает plutovg.lib и lunasvg.lib
  BUILD_LUNASVG_ADAPTER_WIN32_RELEASE.cmd   - собирает ImportEPUBLunaSVG.dll
  BUILD_LUNASVG_ALL_WIN32_RELEASE.cmd       - собирает библиотеку и адаптер одним запуском

Если ImportEPUBLunaSVG.dll лежит рядом с ImportEPUB.dll / ImportEPUBBatch.exe,
режим --svg png или --svg jpg пытается преобразовать SVG через LunaSVG.
Если DLL отсутствует, не той разрядности или не смогла отрисовать SVG,
создаётся видимая PNG/JPEG-заглушка.

Лицензии сторонних компонентов лежат здесь:

  thirdparty\lunasvg\LICENSE
  thirdparty\lunasvg\plutovg\LICENSE


LunaSVG build note (fix54): command files are UTF-8 without BOM; PlutoVG project paths point to thirdparty\lunasvg\plutovg\source.

fix55 notes
-----------
When ImportEPUBBatch.exe is launched with --stats, ImportEPUBBatch_report.csv contains separate columns for generated FB2 statistics and quality checks:

- Sections, Paragraphs, Images, Binaries, Tables, NoteLinks, Chars
- MissingImageBinaries, MissingInternalLinks, MissingNoteTargets, DuplicateIds, ExternalLinks, Warnings

Files with successful conversion but non-zero quality warnings are marked as OK_WITH_WARNINGS. They should be opened in FBE and checked manually, but they are still written to disk.

CONVERT_AND_PACK_TEST_RESULTS.cmd now includes the built binaries in the package:
- ImportEPUB.dll
- ImportEPUBBatch.exe
- ImportEPUBLunaSVG.dll, when present


Build note fix56:
- If ImportEPUBBatch.cpp fails with `std::wstring is not a member of std`, use fix56 or add `#include <string>` to ImportEPUBBatch.cpp.
- Warnings from thirdparty\lunasvg and thirdparty\lunasvg\plutovg are upstream warnings and do not block creation of lunasvg.lib/plutovg.lib.

fix57 notes
-----------
External HTTP/HTTPS links are treated as informational counters only. They remain visible in the ExternalLinks CSV column but do not affect Status and do not increase Warnings.

The importer now tries harder to preserve EPUB anchors. When preserve-links is enabled, id/name anchors from XHTML block elements and common nested inline anchors are transferred to generated FB2 elements. If the same anchor id appears more than once, the later generated FB2 ids receive numeric suffixes, while links keep targeting the first generated anchor.

This mainly reduces MissingInternalLinks for technical EPUBs and poetry/test EPUBs that use anchors on h1-h6, span, a, div, section, p, li, dt/dd and similar elements.

fix58: отчеты и упаковка проблемных файлов
------------------------------------------
После пакетной конвертации ImportEPUBBatch теперь создает не только CSV, но и HTML-отчет рядом с ним:

    ImportEPUBBatch_report.csv
    ImportEPUBBatch_report.html

HTML-отчет удобен для быстрой визуальной проверки: строки OK, OK_WITH_WARNINGS, FAILED и SKIPPED подсвечиваются разными цветами, а основные колонки качества повторяют CSV.

Командный файл:

    CONVERT_AND_PACK_TEST_RESULTS.cmd

теперь кладет в test package оба отчета: CSV и HTML.

Для уменьшения архива после большого тестового прогона добавлен:

    PACK_PROBLEM_FILES.cmd

В начале файла можно задать:

    INPUT_DIR=D:\epub_test_suite
    OUTPUT_DIR=D:\epub_test_suite_fb2
    REPORT_CSV=D:\epub_test_suite_fb2\ImportEPUBBatch_report.csv
    PACKAGE_DIR=D:\epub_import_test_packages

По умолчанию он пакует только FAILED и OK_WITH_WARNINGS: исходные EPUB, соответствующие FB2, логи, отчеты и бинарники.

Для проверки состава сборки добавлен:

    CHECK_DEPENDENCIES.cmd

Он проверяет наличие и разрядность:

    out\Release\ImportEPUB.dll
    out\Release\ImportEPUBBatch.exe
    out\Release\ImportEPUBLunaSVG.dll

а также показывает основные VersionInfo-поля и наличие:

    thirdparty\lunasvg\lib\Win32\Release\plutovg.lib
    thirdparty\lunasvg\lib\Win32\Release\lunasvg.lib

fix60: регрессионные проверки и расширенная диагностика
--------------------------------------------------------
Добавлен командный файл:

    RUN_IMPORT_REGRESSION_TESTS.cmd

Он выполняет полный тестовый прогон указанной папки EPUB и проверяет базовые условия, чтобы не вернулись старые ошибки:

    - нет FAILED-строк;
    - MissingImageBinaries = 0;
    - SVG-файлы имеют Images = Binaries;
    - если ImportEPUBLunaSVG.dll найден, SVG должны конвертироваться без плейсхолдеров;
    - friedrich-engels содержит ссылки на сноски;
    - linear-algebra содержит таблицы.

Итоговый текстовый отчет сохраняется рядом с результатами:

    REGRESSION_TEST_REPORT.txt

В CSV/HTML-отчет добавлены SVG-колонки:

    SvgImages
    SvgConverted
    SvgPlaceholders
    SvgSkipped
    SvgBackend

Это позволяет сразу понять, что произошло с SVG: реальная конвертация через LunaSVG, пропуск, сохранение SVG как есть или вставка плейсхолдера.

CHECK_DEPENDENCIES.cmd теперь сохраняет подробный отчет:

    CHECK_DEPENDENCIES_REPORT.txt

Дополнительно проверяются COM-регистрация плагина и запись FBE Plugins в реестре. Скрипт предупреждает, если зарегистрированная DLL отличается от текущей out\Release\ImportEPUB.dll.

CONVERT_AND_PACK_TEST_RESULTS.cmd получил параметры:

    OPEN_REPORT_AFTER_RUN=1
    CHECK_DEPENDENCIES_BEFORE_RUN=0

Первый автоматически открывает HTML-отчет после прогона, второй позволяет перед тестом выполнить CHECK_DEPENDENCIES.cmd.


fix61 conversion-quality notes
------------------------------
- Internal EPUB links are now normalized not only for #anchor, but also for file.xhtml#anchor and file.xhtml links when the target XHTML file/anchor was imported into FB2.
- The batch CSV/HTML reports include new conversion-quality counters: AnchorsImported, HrefsResolved, HrefsUnresolved, NoteRefsDetected and NoteRefsResolved.
- Note links are stricter: links become FB2 type="note" only when the source link is explicitly a noteref and the target note was collected. This avoids converting ordinary poem line anchors, CFI anchors, or section anchors into broken FB2 note links.


Fix63 conversion notes
----------------------
- The EPUB file picker now uses the standard button caption "Открыть...".
- Table captions are imported as FB2 subtitles before tables.
- Complex table cell content is flattened more carefully so nested paragraphs, lists and <br> elements do not get glued together.
- Preformatted/code blocks preserve leading indentation.
- Poem paragraphs that use <br> for line breaks are converted into separate FB2 verse lines.


Large-volume stabilization test
===============================
For large input collections, for example 3178 EPUB files / 4.28 GB, use:

  BUILD_STABLE_TEST_RELEASE.cmd
  BIG_VOLUME_TEST.cmd

Before running BIG_VOLUME_TEST.cmd, edit variables at the top of the file:

  INPUT_DIR
  OUTPUT_DIR
  PACKAGE_DIR
  RESUME_MODE
  MAX_FILES

Recommended first pass:

  MAX_FILES=50

Then full pass:

  MAX_FILES=0
  RESUME_MODE=1

BIG_VOLUME_TEST.cmd uses ImportEPUBBatch.exe with --flush-report-each, so
ImportEPUBBatch_report.csv and ImportEPUBBatch_report.html are updated after
each processed EPUB. This protects the test report if the run is interrupted.

The big-test package does not include the whole 4+ GB source collection. It
contains reports, logs, binaries and a limited number of FAILED /
OK_WITH_WARNINGS EPUB/FB2/log pairs.

New ImportEPUBBatch options:

  --flush-report-each   write CSV/HTML report after every processed EPUB
  --max-files <N>       process only first N EPUB files for smoke tests
