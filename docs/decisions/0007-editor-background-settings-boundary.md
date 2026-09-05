# 0007. Каталог фонов является сервисом настроек

## Контекст

`EditorBackgrounds` читает сопровождаемый manifest фонов, валидирует имена
файлов и локальные изображения. Он вызывается FBDoc, main frame и settings
page, но не владеет document/view, HWND или диалогом.

## Решение

Сервис расположен в `src/fbe/settings`. Callers сохраняют свои обязанности:
FBDoc применяет фон документа, main frame обновляет отображение, settings page
предоставляет выбор. Сервис использует только localization и file helpers.

## Последствия

Не меняются runtime manifest, путь `EditorBackgrounds`, сохранённые settings
или fallback при отсутствующем ресурсе. `test-fbe-settings-background-boundary.ps1`
фиксирует путь и отсутствие coordinator-зависимостей, а assets/regression
tests проверяют поведение.
