# Обновление библиотек: 2 ноября 2026

| Компонент | Было → стало | Официальный источник |
| --- | --- | --- |
| Scintilla | 5.6.7 → 5.6.9 | Архив `scintilla569.zip`, SHA-256 `FDDC637AFA6E3A9205A5AD67C777D711A921ED2797D792F3034010C1BE1539D6`, Mercurial `rel-5-6-9`, node `69bed9304d290ba16399e493ab6fe4b1a3fb4b5f`. По просьбе пользователя выбран выпуск 5.6.9 вместо 5.6.8. |
| Lexilla | 5.5.4 → 5.5.5 | `rel-5-5-5`, commit `e67537693bfb1ae4e6f643dcab725a38ffeac3e3` |
| libaom | 3.15.2 → 3.15.3 | `v3.15.3`, commit `b141d9c45db236335351c8a4b2a14b3ade27c937` |
| libarchive | 3.8.10 → 3.8.11 | `v3.8.11`, commit `03036ffa409a2aa1c5bbad35be2202641a040fda` |
| UPX | 5.2.1 → 5.3.0 | Официальный `upx-5.3.0-win32.zip`, SHA-256 `36A740F077637D66DA90A133A2A166E90DD692BE5D08B2350FEC97ED23809D53`, сверено с GitHub release API. |

Исходная отслеживаемая Scintilla 5.6.7 совпала с официальным архивом после нормализации переводов строк. Изменения 5.6.8 и 5.6.9 перенесены из официальных архивов; FBE-специфичных отличий в отслеживаемых файлах старого дерева не обнаружено. Версии и лицензии приведены в THIRD-PARTY-NOTICES.md; UPX распространяется только как инструмент сборки, тексты лицензии находятся в tools/upx.

Конфигурация сборки libaom: static, `AOM_TARGET_CPU=generic`, `CONFIG_AV1_DECODER=1`, `CONFIG_AV1_ENCODER=0`. libarchive собран штатно со статическим zlib, без включения дополнительных кодеков. Scintilla/Lexilla собраны v143/VC 14.44 Win32. Хеши обновлённых файлов: Scintilla.dll `AFC2139852E87D81614B200DCD71CB3ADA13427BCE85CF923FF066ECEF908BD5`, Lexilla.dll `99A205F5116D8BF82EBEA71243C8BC92DF4FC3E3E6A3C743249BA22E5D7CD0CA`, UPX `F001B2019610EA340663101D8E9054333104FAE7F4210D20682C23D07E99D236`. Сборка FBE с `-SkipUpx` не меняет отслеживаемые runtime DLL.

UPX 5.3.0 запущен; тестовый PE упакован с `--exact`, проверен `-t`, распакован и совпал побайтно по SHA-256. Тестовый PE с подтверждённым `CF instrumented` отвергнут: `GUARD_CF enabled PE files are not supported`; политика стандартной сборки не изменена.

Release Win32 сборка `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` прошла. Прямые импорты основных EXE/DLL и пяти плагинов проверены `check-win7-imports.ps1`: PASS; FBV использует UCRT API sets и требует соответствующий runtime. Smoke `test-scintilla.ps1`, `test-image-import-native.ps1`, `test-archive-zip-functional.ps1`, `test-archive-metadata-numbers.ps1`, `test-fbe-archive-document-support.ps1`, `test-editor-runtime-fingerprint.ps1` и `test-third-party-notices.ps1` прошли. ZIP-тест проверяет кириллицу, вложенные каталоги, запрет `..`, сохранность остальных записей, выбор второго дубликата и отсутствие изменения архива при ошибке. Проверки по тексту контракта не подменяют поведенческие тесты.

Замеры производительности до/после не выполнены. Расширенные негативные RAR/AVIF и полная проверка всех состояний редактора остаются ручными или отдельными проверками. `verify-release.ps1` и FullValidation не запускались по требованию пользователя; это не release-gate.

Интеграционный test-fbe-archive-runtime.ps1 прошёл с фактическим FBE: ZIP с одной и несколькими книгами, RAR5, сохранение выбранной записи и восстановление. RAR4 на реальной книге и дополнительные негативные RAR остаются непроверенными.

На итоговой сборке 5.6.9 прошли test-fbe-source-editor-ui-runtime.ps1 (включая UTF-8, стили, сворачивание, перенос, курсор и Undo) и test-fbe-xml-source-theme-v1-runtime.ps1. Стандартная сборка повторно собрала DLL из тех же исходников: версии совпадают, но бинарные хеши отличаются от эталонных runtime-файлов. Хеши файлов out/Release: Scintilla.dll 053FA91D9C1AEBD1D7FBE7430F1BDA57B7A0AF8962BF92C4FFE72CAEC185CFB6; Lexilla.dll AD939CB94074645B5F43197BFBAB916175807A1C0ED1BCD3FFA2B64992D6C6BB. Битовая воспроизводимость не заявляется.

В изолированной виртуальной Windows 7 SP1 6.1.7601 запущена Release-сборка FBE.exe SHA-256 641CEAD051359A408E04EB6BD14CCDADCC1DFBE83165E423CFDE008BC6216008; переход в режим «Код» отобразил XML с подсветкой и кириллицей. Снимок и журналы: build/dependency-update-20261102/win7-evidence. На этой Windows 7 с IE8 при старте зафиксированы WB205/SD100, наблюдавшиеся и до обновления зависимостей; их причина не установлена. Архивы и AVIF в гостевой системе не проверены.
