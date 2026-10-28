# Интеграционная приёмка FBE Next 3.0.10

Рабочая ветка: `codex/fbe-next-integration-3010`. База `origin/master`: `9cb126e5eeae26fb802d6a91c3dcd3c890f6461e`. Включённые ветки: `codex/community-3010-closure` — `ff91662c641c8aa55b1b0a2a8705c568a03e0abe`, `codex/fix-ci-validation-gates` — `9d0c2cd82ffe5e861452cf1ed41d201fca491237`. Merge-коммиты: `fbce06338eaa80b80daf085283d4e68098e4605d` и `367f85aad2417fac3b5b6a74f65caac5e3a653f8`; обе указанные исходные ревизии проверены как предки второго merge.

## Проверка объединённого HEAD до новых исправлений

Чистая Release Win32 сборка `367f85aa` с v143 / VC 14.44 и `/W4 /WX` прошла; CommonCore и Runtime provenance проверены. Все результаты ниже относятся к этой сборке, а не к ранее построенным worktree. Журналы находятся в `out/integration-evidence/` интеграционного worktree.

| Контур | Результат |
|---|---|
| EPUB fidelity и независимая XSD-проверка | PASS: 71 тест. Первый прогон выполнил тесты и XSD, но не записал итог из-за отсутствия `out/tests`; после создания каталога полный повтор PASS. |
| Пустые якоря Save/Reopen | PASS |
| Реальная MSHTML/OLE вставка | PASS: 28 сценариев |
| Body/Source | PASS: 252 сценария выделения |
| Редактирование href | PASS |
| Тематические диалоги и задержанный arrow-space | PASS в обоих вариантах |
| Runtime dialog coverage, localization preflight, ссылки Markdown | PASS |
| ASan infrastructure | PASS, включая реальный dump и отрицательный UAF probe |
| UBSan infrastructure | PASS по физическому пути. Запуск через `subst R:` сначала FAIL из-за сравнения символьного и физического путей; исправление сравнивает конечный путь открытого файла и проверено на том же `R:`. |
| Search Core | PASS |
| File Close portable/FB2 | PASS, исходный сбой не воспроизвёлся в данном варианте |
| Image Inserter | RED: binary создан, но ссылка изображения отсутствует |
| Split | RED: `split-section-middle`, после Undo остаётся дублированный раздел |

## Изменения после объединения

`8d58ded4`: Image Inserter определяет фактический родитель `_descriptionBody`, в который `apiAddBinary` добавляет узел; раньше проверялся только `binobj`, поэтому операция возвращала `0x8000FFFF` и оставляла binary без изображения. Release-тесты block/inline, двух имён, совпадающего ID, неизменности JPEG, Undo/Redo и Save/Reopen прошли; соседний native ImageImport и его round-trip также PASS.

`5298bb44`: test-only probe нативной команды `InsertHTML` для Split. На MSHTML команда вернула `0x80040100`, Undo не восстановил исходный DOM. Старый custom Undo prototype также не сохранял Redo последующего ввода. Безопасное production-исправление Split не подтверждено: **BLOCKED**. Диагностика: `out/integration-evidence/split-exec-probe.log` и соответствующий TSV в `out/integration-temp/`.

`295ae0d2`: тест UBSan сопоставляет конечные пути открытых файлов, а не строки с разными буквами дисков; отклонение стороннего процесса, тайм-аут и отрицательные sanitizer-пробы сохранены. Infrastructure PASS на `R:` и по физическому пути.

File Close portable/FBD один раз завершился `0xC0000409` в первой фазе сохранения после импорта. `Application Error` указывает смещение в `FBE.exe`, символизированное как `abort`, но стек и код fast-fail недоступны, поэтому тип повреждения и связь с прежним UBSan-исключением не установлены. Повторы под отладчиком, а также installed/FB2, installed/FBD и повтор portable/FBD прошли; считать это исправлением нельзя. Нужен дамп при повторении отказа: **INCONCLUSIVE**.

## Итоговая приёмка

UBSan Fast на ревизии 295ae0d2: **FAIL**, 84 из 85 сценариев прошли, единственный отказ — Split; ошибок undefined behavior в остальных сценариях не обнаружено. ASan Fast на той же ревизии: **FAIL / INCOMPLETE**, после двух успешных сценариев editor-background завершился переполнением стека в диспетчере тестов; прогон остановлен из-за диагностического дампа размером более 1 ГБ. Дамп сохранён в out/integration-evidence/asan-editor-background-stack-overflow.dmp, SHA-256 FAC8E3FDBBCE83A931D25BEA876146ED6E3A4C45BDB52EA55506C355FB4E8BBB. Ручная проверка Windows GUI: **PENDING MANUAL**. Внешний CI для конечного SHA: **NOT RUN** до отдельного разрешения на публикацию. Исторические прогоны не заменяют текущую приёмку. Общая готовность к релизу остаётся **BLOCKED** до устранения Split и завершения остальных gate.

`4d919ebe`: разделён тестовый диспетчер сценариев, ранее резервировавший около 986 КБ стека до выбора сценария. Строгая Release-сборка и тесты editor-background, File Close portable/FB2, selection и Image Inserter block/inline прошли. ASan infrastructure и отдельный ранее падавший editor-background на этой ревизии — PASS; полный ASan Fast — **FAIL**, 84/85 PASS; единственный отказ — release.fbe-split-container-production.1, split-section-middle. Отчёт: out/tests/asan/20261028-152106-a822621924844f5292d51d21b1bfb055/summary.json.

Portable для ручной проверки: `out/integration-portable-4d919ebe`, собран из Release ревизии `4d919ebe1dc13bdb12f6fffd50886f7246800485`. Проверки состава и Core identity — PASS. SHA-256 `FBE.exe`: `CE71A5271143BAC4DCA5D98D4D39071E9DAAA994736E79EB3368DB456D7481D6`; полный список EXE/DLL — `out/integration-evidence/portable-4d919ebe-sha256.txt`. Каталог является диагностической сборкой, релизный gate не пройден.

Ручная матрица — **PENDING MANUAL**: Copy/Paste с Undo/Redo; Split и последующий ввод с Undo/Redo; block/inline изображения; открытие, закрытие и замена документов; импорт EPUB; Save/Reopen; Body/Source и дерево; редактирование href; диагностический пакет после ошибки. Проверять только копии книг.

Первая попытка `verify-release` после изменения диспетчера остановилась на provenance Scintilla, поскольку параллельная ASan-сборка изменила общий промежуточный runtime. Обычный runtime и Release повторно собраны; результат этого прерванного запуска не является оценкой выпускного gate.
## SHA-256 файлов portable

```text
0C5A461BD113E8FFAC2186064C970EE5600C74012BA272C279A4C75EE30DDAA7  ExportDOCXBatch.exe
86BC2ACB82669E1BB4EC55807AB7506437C1B2DB4ECC3058B79C74D67DA189E0  ExportEPUBBatch.exe
CE71A5271143BAC4DCA5D98D4D39071E9DAAA994736E79EB3368DB456D7481D6  FBE.exe
BFE17317002A86A3CFCA6436ED1CA8347BC8730044866C16421C6A2694462CE5  FBV.exe
963BA4076BC6D72195052BC84E3CC46407DED8D742086C07213FA831A6022D5D  ImportEPUBBatch.exe
BFD0A7DCE1C4AFCCBD2B479F0A63A89F82F6AB7D344CFF0B36E512AC54F552E4  Lexilla.dll
04D8335370099BC261868E6E5890B618C48C3C968CE2F81395BDDE66EF831FC0  Scintilla.dll
967B931E00A6B60B8CC25A8798DC6A243614723D54B6575DE93E6C461FA3B053  Plugins\ExportDOCX.dll
45A0460D309EB85428F7046DDC57B10845A9C88F2A1E750159756514A484E540  Plugins\ExportEPUB.dll
65DBE30B7FFC7D0114EE8A80334EB132958DEB1856F36604318305368D03C4A0  Plugins\ExportHTML.dll
8DB10A37C9923C082D386A84CCD63292E3E9EAE641EADA350C573169FEA78821  Plugins\ImportEPUB.dll
4498793954C8616149383FC79AC25EDD49B17E8AFA1A8E1011CEAA319E74117B  Plugins\ImportEPUBLunaSVG.dll
```

## Итоговая таблица статусов

| Проверка | Статус и доказательство |
|---|---|
| Интеграция двух веток | PASS: оба merge-коммита и предки проверены. |
| Строгая Release-сборка, v143 / 14.44 | PASS на кодовой ревизии `4d919ebe1dc13bdb12f6fffd50886f7246800485`. |
| Image Inserter | PASS: block/inline, имена, коллизия ID, Undo/Redo и Save/Reopen. |
| File Close | INCONCLUSIVE: четыре варианта прошли под ASan и UBSan; исходный единичный abort portable/FBD без доступного стека не объяснён. |
| Split/Undo | BLOCKED: `split-section-middle` после Undo дублирует раздел; безопасное исправление не подтверждено. |
| ASan Fast | FAIL: 84/85 PASS, один FAIL Split; стековый overflow диспетчера устранён, self-test и адресный повтор PASS. |
| UBSan Fast | FAIL: 84/85 PASS на `295ae0d2`; один FAIL Split. После test-only изменения диспетчера полный UBSan не повторялся: NOT RUN на `4d919ebe`. |
| verify-release | FAIL: физический путь, журнал `out/integration-evidence/verify-release-physical.log`, отказ `split-section-middle`. Запуск через `subst R:` ранее завершился на сравнении путей MSBuild из-за двойного разделителя. |
| FullValidation | NOT RUN: стандартный gate не прошёл, предварительное условие ТЗ не выполнено. |
| Внешний CI/OLE текущей ревизии | NOT RUN: ветка не опубликована. |
| Ручная GUI-матрица | PENDING MANUAL. |
| Итоговая готовность | BLOCKED: Split, причина File Close и незавершённые gates. |

Итоговый HEAD документации получается командой `git rev-parse HEAD`; артефакты и проверки выше привязаны к указанным кодовым ревизиям, а не к последующему коммиту только документации.