> **Исторический материал.** Сохранён из `docs/README.md` на ревизии `1ad7c507d1ed`. Указанные ниже даты, версии, PASS/FAIL и планы относятся к описанным прогонам, а не к проверке текущего релиза. Изменены навигация и оформление архивного раздела; результаты испытаний не повышались.

[Исходная редакция без документационных изменений](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/README.md).

# Документация FictionBook Editor Next

## Актуальные инструкции

- [Сборка](../../building.md) — требования, официальные команды и расположение результатов.
- [Контуры тестирования](../../test-contours.md) — FAST, FULL и диагностические проверки.
- [Чек-лист выпуска](../../release-checklist.md) — подготовка и верификация артефактов.
- [Карта структуры репозитория](../../architecture/repository-layout.md) — компоненты, границы и граф сборки.
- [Runtime и упаковка](../../architecture/runtime-packaging.md) — происхождение файлов и staging-карта.
- [Статус верификации](../../testing/verification-status.md) — проведённые проверки и ограничения среды.

## Тематические документы

- Плагины и type library: [scripting-api.md](../../scripting-api.md), [fbelib-typelib-versioning.md](../../reference/fbelib-typelib-versioning.md).
- Shell-интеграция: [property-handler.md](../shell/property-handler.md), [fb2-property-mapping.md](../../reference/fb2-property-mapping.md).
- Режим исходного XML: [xml-source-themes.md](../../reference/xml-source-themes.md), [regex-regression.md](../../testing/regex-regression.md).
- Локализация: [localization.md](../../reference/localization.md).

Исторические планы и результаты ручных проверок не являются заменой действующим
инструкциям. При расхождении приоритет имеют сценарии `tools/build`, активный
workflow CI и документы из раздела «Актуальные инструкции».
