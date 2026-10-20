# Настройки, темы и каталоги данных

> Тип: Действующая инструкция. Документационная база: FBE Next 3.0.9, `master` / `1ad7c507d1ed`; сверка 19 октября 2026 года.

## Installed и portable

Пути определяет общий `DeploymentContext`, а не произвольная запись в старом отчёте. По умолчанию используются:

| Данные | Installed | Portable |
| --- | --- | --- |
| Настройки | `%LOCALAPPDATA%\FBE Next\` | `<каталог FBE>\Data\Settings\` |
| Диагностика | `%LOCALAPPDATA%\FBE Next\Diagnostics\` | `<каталог FBE>\Data\Diagnostics\` |
| Recovery | `%LOCALAPPDATA%\FBE Next\Recovery\` | `<каталог FBE>\Data\Recovery\` |
| Пользовательские скрипты | `%LOCALAPPDATA%\FBE Next\Scripts\` | `<каталог FBE>\Data\Scripts\` |
| Пользовательские словари | `%LOCALAPPDATA%\FBE Next\Dictionaries\` | `<каталог FBE>\Data\Dictionaries\` |
| Пользовательские темы | `%LOCALAPPDATA%\FBE Next\Themes\` | `<каталог FBE>\Data\Themes\` |

Если `portable.ini` задаёт допустимый `DataPath`, вместо `Data` используется соответствующий локальный каталог этой копии. Не пытайтесь задавать через этот параметр произвольный абсолютный путь: код проверяет допустимость значения и при нарушении возвращается к `Data`.

`Settings.xml` и `Hotkeys.xml` создаются штатной инфраструктурой; их нельзя считать поставляемыми файлами рядом с EXE. Начальные `Words.xml` берутся из сопровождаемых ресурсов и копируются в активную директорию настроек. Обратная совместимость поиска seed-файлов не меняет основного текущего пути.

## Редактирование настроек

Проверьте, какая копия FBE запущена. Измените параметры и подтвердите применение. При ошибке записи диалог должен оставаться доступным и сообщать причину, а не молча объявлять настройки сохранёнными. Для проверки сохранения закройте и повторно запустите именно эту копию.

Предпросмотр темы или цвета не равнозначен сохранению всех связанных файлов. При проверке «Отмена» сравнивайте реальные пользовательские файлы и состояние интерфейса. Техническая граница транзакции описана в [архитектуре настроек](../architecture/settings.md).

## Темы

Тема интерфейса приложения, оформление визуального редактора и цветовая схема XML — связанные, но не идентичные настройки. Формат `.fbetheme` и логические роли описаны в [справочнике тем Source](../reference/xml-source-themes.md), происхождение палитр — в [списке источников](../reference/xml-source-theme-sources.md).

## Перенос и восстановление

Перед ручной правкой сделайте копию активного каталога данных. Закройте редактор, чтобы он не перезаписал файлы при выходе. Не удаляйте старые словари и настройки, пока не проверите результат миграции. Каталог классического FBE и каталог FBE Next нельзя считать взаимозаменяемыми.

## Основания

- [src/common/DeploymentContext.h](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/src/common/DeploymentContext.h)
- [docs/architecture/runtime-packaging.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/architecture/runtime-packaging.md)
- [docs/settings-followup-report.txt](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/settings-followup-report.txt)
