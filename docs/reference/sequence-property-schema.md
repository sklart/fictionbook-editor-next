# FBE-specific property schema

> Тип: Действующая инструкция. Документационная база: FBE Next 3.0.9, `master` / `1ad7c507d1ed`; сверка 19 октября 2026 года.

## Назначение

Книжная серия и номер книги не должны подменяться случайным стандартным Windows-полем вроде жанра или категории. Для таких книжных свойств используется FBE-specific схема; историческое имя `FBE.Sequence.propdesc` не означает, что весь современный контур ограничен единственным полем.

Сопоставление конкретных FB2-полей, Windows canonical names и собственных свойств хранится в [таблице mapping](fb2-property-mapping.md) и соответствующей реализации `Fb2ShellProperties`.

## Поставка и регистрация

Схема входит в подготовку Integration stage и обслуживается текущим installer-контуром. Это уже не инструкция по добавлению произвольного эксперимента, не включённого в обычную поставку.

Файл схемы, MUI-строки, COM-handler и разрядность Проводника проверяются совместно. Для эксплуатации используйте штатный установщик; ручная регистрация допустима лишь как осознанная диагностика в изолированной среде с известным исходным состоянием и откатом.

Точные команды регистрации, receipt, статусы, восстановление и удаление определяет [installer maintenance](installer-shell-maintenance.md). Этот справочник не создаёт второй независимый сценарий записи реестра.

## Проверка

Сопоставьте серию без номера, серию с номером, несколько авторов, пустые значения, кириллицу и локализованные подписи. Отсутствие поля в видимой колонке может быть связано с регистрацией схемы или настройкой представления Explorer, а не с чтением FB2.

Исходные план и экспериментальная инструкция сохранены в [архиве Shell](../archive/README.md). Их утверждения «пока не входит» не применяются к текущей штатной поставке автоматически.

## Основания

- [docs/sequence-property-schema-plan.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/sequence-property-schema-plan.md)
- [docs/sequence-property-schema.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/sequence-property-schema.md)
- [docs/fb2-property-mapping.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/fb2-property-mapping.md)
- [docs/installer-shell-maintenance.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/installer-shell-maintenance.md)
