# Системная интеграция Windows

> Тип: Действующая инструкция. Документационная база: FBE Next 3.0.9, `master` / `1ad7c507d1ed`; сверка 19 октября 2026 года.

## Что входит в текущий контур

Штатная современная системная интеграция использует `Fb2PropertyHandler` для свойств FB2 и `Fb2ThumbnailProvider` для миниатюр. В активной карте классов FBShell нет прежних `ColumnProvider` и `ContextMenu`. Условные определения в исходнике относятся к выбранной сборочной конфигурации: они не доказывают, что штатная поставка «экспериментально отключена».

В проекте `ShellIntegration` включает современные property/thumbnail определения. Окончательная поставка определяется build/staging/manifest, а не наличием старого DLL или `.rgs` в дереве. Старый IconExtractor/ImageLoader и старый графический контур не следует возвращать для «починки» современных миниатюр.

## Компоненты и ответственность

| Компонент | Назначение |
| --- | --- |
| `FBShell.dll` / `FBShell64.dll` | COM-компоненты под разрядность Проводника. |
| Property handler | Передача FB2-метаданных Windows Shell. |
| FBE-specific property schema | Книжные поля, которым не подходит стандартное свойство Windows. |
| Thumbnail provider | Извлечение встроенной обложки и WIC-конвейер миниатюры. |
| Статические shell-команды и ассоциации | Открытие/валидация и другие операции без возврата legacy COM context menu. |

[Маппинг свойств](reference/fb2-property-mapping.md), [property schema](reference/sequence-property-schema.md), [миниатюры](reference/thumbnail-provider.md) и [обслуживание установщиком](reference/installer-shell-maintenance.md) являются отдельными подробными справочниками.

## Установка, обновление и удаление

Выбор installed/current-user/all-users и компонентов выполняется штатным установщиком. Portable не должен самовольно устанавливать системные обработчики. Наличие x64 shell DLL не превращает основной редактор в x64-приложение.

Рабочая упаковка использует Core и Integration stages. Временные каталоги готовятся и проверяются до NSIS. Не выполняйте случайную ручную COM-регистрацию поверх чужой installed-копии, чтобы «проверить, появились ли колонки».

При upgrade/uninstall важны режим установки, владение компонентами, их разрядность и состояние занятых DLL. Статусы `ok/pending/skipped/error`, recovery и receipt описаны в сопровождаемом installer-документе. Не удаляйте пользовательские ассоциации `.zip`/`.rar` вместе со старой интеграцией ArchHandler.

## Кэш Проводника

Устаревшая миниатюра не обязательно означает отказ провайдера: Explorer кэширует и положительные, и отрицательные результаты. Сначала различите прямой вызов провайдера и отображение результата в Explorer. Очистка или prime кэша — адресная диагностика, а не часть алгоритма построения миниатюры и не обязательный шаг каждого запуска приложения.

## Проверка и статус

[Shell-матрица](testing/shell-integration.md) объединяет property, thumbnail и регистрацию. PASS отдельного COM smoke не доказывает GUI-поведение, установку всех режимов или совместимость каждой Windows. Исторические планы сохранены в [архиве Shell](archive/shell/README.md), но не являются текущими инструкциями включения экспериментальной реализации.

## Основания

- [src/fbshell/FBShell.cpp](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/src/fbshell/FBShell.cpp)
- [src/fbshell/FBShell.vcxproj](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/src/fbshell/FBShell.vcxproj)
- [docs/shell-extension-modernization.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/shell-extension-modernization.md)
- [docs/architecture/runtime-packaging.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/architecture/runtime-packaging.md)
- [docs/installer-shell-maintenance.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/installer-shell-maintenance.md)
