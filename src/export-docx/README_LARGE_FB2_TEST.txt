ExportDOCX: тестирование на большом объёме FB2
================================================

Рекомендуемый порядок проверки
------------------------------

1. Собрать Release|Win32:

   BUILD_WIN32_RELEASE.cmd

2. Быстро проверить список входных файлов без конвертации:

   RUN_LARGE_FB2_DRYRUN.cmd

   Перед запуском откройте CMD-файл и задайте:
   SOURCE_DIR=D:b2_large_input
   OUTPUT_DIR=D:b2_large_output_docx

3. Полный прогон:

   RUN_LARGE_FB2_TEST.cmd

   Скрипт по умолчанию запускает ExportDOCXBatch.exe с параметрами:

   -Recurse
   -Resume
   -SkipIfNewer
   -Retries 2
   -PreserveDates
   -ValidateDocx
   -ProgressEvery 25
   -HtmlReport
   -Summary
   -FailedList
   -OkList
   -PrintFailed

Что смотреть после большого прогона
-----------------------------------

В папке OUTPUT_DIR\_logs создаются:

- ExportDOCX_batch_log.csv        полный CSV-журнал;
- summary.txt                     краткая сводка;
- report.html                     HTML-отчёт;
- failed.txt                      список проблемных FB2;
- ok.txt                          список успешно обработанных FB2;
- warnings_summary.txt            сводка предупреждений по всем книгам;
- large_test_analysis.txt         анализ CSV: статусы, проблемные и самые медленные файлы;
- ExportDOCX_current_file.txt     текущий/последний обрабатываемый файл;
- ExportDOCX_progress_state.txt   состояние прогона на случай зависания/аварии;
- ExportDOCX_batch_console.log    полный вывод batch-утилиты.

Возобновление после остановки
----------------------------

RUN_LARGE_FB2_TEST.cmd уже использует -Resume и -SkipIfNewer. Поэтому после
перезагрузки или ручной остановки можно запустить тот же CMD повторно: готовые
DOCX будут пропущены, недостающие будут обработаны.

Ожидаемая оценка результата
---------------------------

Хороший результат большого прогона:

- Unexpected FAIL = 0;
- DOCX_INVALID = 0;
- FAIL = только ожидаемые отрицательные тесты, если они есть;
- OK_WITH_WARNINGS допустимы, но их нужно просмотреть в warnings_summary.txt.

Если есть проблемы
------------------

Для анализа достаточно прислать:

- summary.txt;
- failed.txt;
- warnings_summary.txt;
- large_test_analysis.txt;
- 2-3 проблемных FB2 и соответствующие *_export_report.txt.
