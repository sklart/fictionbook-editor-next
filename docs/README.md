# Документация FictionBook Editor Next

## Актуальные инструкции

- [Сборка](building.md) — требования, официальные команды и расположение результатов.
- [Контуры тестирования](test-contours.md) — FAST, FULL и диагностические проверки.
- [Чек-лист выпуска](release-checklist.md) — подготовка и верификация артефактов.
- [Карта структуры репозитория](architecture/repository-layout.md) — компоненты, границы и граф сборки.

## Тематические документы

- Плагины и type library: [scripting-api.md](scripting-api.md), [fbelib-typelib-versioning.md](fbelib-typelib-versioning.md).
- Shell-интеграция: [property-handler.md](property-handler.md), [fb2-property-mapping.md](fb2-property-mapping.md).
- Режим исходного XML: [xml-source-themes.md](xml-source-themes.md), [regex-regression.md](regex-regression.md).
- Локализация: [localization.md](localization.md).

Исторические планы и результаты ручных проверок не являются заменой действующим
инструкциям. При расхождении приоритет имеют сценарии `tools/build`, активный
workflow CI и документы из раздела «Актуальные инструкции».
