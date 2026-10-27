# Hunspell 1.7.6 и libheif 1.23.7: обновление и проверка

База: `ca261fab3e3ab04f416c399a37038c2f5b8e98ba` (`origin/master`).
Работа выполнена в отдельном worktree; параллельные изменения основного checkout
в обновление не включены. Toolchain: MSVC v143 / VC Tools 14.44.35207, Win32
Release, C++17 для first-party кода. C++20 libheif остаётся vendor-настройкой.

| Зависимость | Было | Стало | Старый SHA | Новый SHA |
|---|---|---|---|---|
| libheif | 1.23.6 | 1.23.7 | `f81f28ac014b1c28dee483ac45b72c6b05bc421a` | `12a259a5882dd99d45ec5fc77e296a3b620d1e2e` |
| Hunspell | 1.7.5 | 1.7.6 | `e8342700b4dde06130cdbba75f7e6500ffa58864` | `15faca15532b34db3106d50f162e5edc2503d6d6` |

Официальные источники: [libheif v1.23.7](https://github.com/strukturag/libheif/releases/tag/v1.23.7),
[Hunspell v1.7.6](https://github.com/hunspell/hunspell/releases/tag/v1.7.6).
Внутренние изменения vendor-кода не вносились. libaom 3.15.2 и libde265 1.1.3
сохранены; словари, их provenance, лицензии, corpus baseline и CI не изменены.

## Изменения интеграции

- `DecodeHeif()` до чтения контейнера ужесточает существующие ограничения:
  32 млн пикселей, 128 MiB на блок, 256 MiB общего бюджета libheif, 4 MiB на ICC.
  Более строгие upstream-значения сохраняются. Лимиты трансформаций и coded
  pictures libheif 1.23.7 не отключаются и не увеличиваются.
- Размер ICC проверяется также перед копированием в `std::vector`.
  WCS-профили и transform освобождаются через RAII, в том числе при исключении
  выделения памяти. Ошибки NCLX не игнорируются; HRESULT преобразования цветов,
  включая `E_OUTOFMEMORY`, не заменяется ошибкой неподдерживаемого профиля.
- Статическая сборка явно отключает OpenH264 и GDK loader. Подключены только
  существующие AV1/libaom и HEVC/libde265; загрузка плагинов выключена.
- Оба production entry points добавления слова используют
  `FbePrepareDictionaryWord()` перед кодированием, как spell/suggest.
  Произвольные ICONV-правила применяет сама Hunspell. В пользовательский файл
  по-прежнему сохраняется исходная форма слова.
- `libheif` добавлен в оба update orchestration scripts. Маршруты `libheif` и
  `aom` используют общий build/test код, применяя только выбранный gitlink.
  Сохраняются `WhatIf`, `SkipApply` и native failure propagation.
- Apply больше не выполняет `submodule update` для уже инициализированного
  checkout перед проверкой dirty state. Повторный checkout целевого SHA
  пропускается, а неожиданный локальный HEAD требует рассмотрения.
  Статусы `LocalNewer`/`NeedsReview` не приводят к автоматическому downgrade.

## Целевые проверки

| Проверка | Результат |
|---|---|
| libheif Release/v143 static build | PASS |
| Native image import; linked/header version equality | PASS, 1.23.7 |
| HEIC, AVIF 8/10-bit, alpha, ICC/NCLX/P3, PQ/HLG | PASS |
| Повороты 90/180/270 с контрольными пикселями | PASS |
| Повреждённые файлы, oversized extent и ICC, duplicate irot | PASS |
| FB2 binary save/reload round-trip (MSXML) | PASS |
| Hunspell Release/v143 static build | PASS |
| RU/UK/EN/DE native dictionary regressions | PASS |
| Synthetic non-idempotent ICONV add/remove/add-with-affix | PASS |
| Custom words: добавление, создание/замена файла, reload/new handle | PASS |
| Fixed corpus quality/performance baseline | PASS, baseline без изменений |
| Update pipeline: 17 synthetic cases | PASS |
| Реальный libheif update WhatIf | PASS, изменений не применяет |
| Notices и third-party update-report contracts | PASS |
| Общая Release Win32 сборка, first-party /W4 /WX | PASS |
| verify-release.ps1 | FAIL: UI defects light/ru-RU, активация commandbar/menu |
| Production spellcheck direct/script edits, large paragraph, scroll | PASS, последовательные запуски; 2000/4000 абзацев |
| Win7 direct imports FBE.exe | PASS |
| Новые DLL imports относительно старых библиотек | PASS: отсутствуют |

Первый общий build выявил неинициализированный вложенный PCRE2/sljit в новом
worktree. Инициализирована закреплённая ревизия; повторная сборка прошла.
Это не изменение PCRE2 или sljit.

Default release gate остановился на `test-fbe-ui-defects-runtime.ps1`
(`light/ru-RU`, `menu_ready_observed=0`, `commandbar_popup=0`). Отдельный
последовательный повтор также завершился FAIL. Контрольный FBE, скомпонованный
из тех же объектов с прежними Hunspell/libheif, воспроизвёл этот UI FAIL.
Код меню и assertions не менялись; общий release gate **не считается пройденным**.
Проверки после остановившего его вызова внутри gate не выполнены, кроме явно
перечисленных отдельных целевых запусков.

Первый дополнительный local-edit запуск совпал с GUI-частью gate и завершился
`0xC000041D`. Сохранённый dump локализует null document в startup document-tree
script callback до сценария spellcheck. Последовательные direct/script,
large-paragraph и scroll запуски прошли без изменения кода/порогов этих тестов.

`abc_color_irot_alpha_irot.avif` остаётся неизменной: два разных items имеют по
одной ассоциации одного свойства irot. Отрицательный тест меняет только копию,
добавляя повторную ассоциацию одному item. Повороты и oversized ICC также
генерируются в тесте; новые бинарные fixtures в Git не добавлены.

Дополнительный reproducer прямого `Hunspell_remove()` для русского custom word
показал одинаковое поведение старой и новой библиотек: add принимается, remove
возвращает 0, однако форма продолжает приниматься. Это не новая регрессия;
production FBE не вызывает данный API. Отдельный synthetic ICONV regression
проверяет фактическое удаление на контролируемом `.aff/.dic`.

## Границы доказательства

Ручные действия в диалоге орфографии и контекстном меню, визуальная проверка
подчёркиваний в «Дизайн»/«Код», отображение повторно открытого FB2 в MSHTML и
реальный запуск Windows 7: **PENDING MANUAL**. Native round-trip проверяет
сохранение и чтение binary через MSXML, а не визуальную отрисовку редактором.

Полный `-FullValidation` GUI/stress/table contour: **NOT RUN** после повторяемого
UI FAIL обычного gate; целевые проверки перечислены отдельно. Большие внешние корпуса не
скачивались и не включались в проверку.

Размер production FBE.exe: 11 760 640 байт (`-SkipUpx`). Контрольное сравнение
на одинаковых объектах FBE и остальных библиотеках, с заменой только
`libhunspell.lib` / `heif.lib` и равной длиной output/PDB paths:
старые библиотеки — 11 728 384 байт, новые — 11 761 152 байт;
разница **+32 768 байт (+0,279%)**. Это сравнение вклада библиотек при одном и
том же коде интеграции FBE, а не сравнение двух полных выпусков редактора.
Контрольные relink binaries не являются release artifacts.
