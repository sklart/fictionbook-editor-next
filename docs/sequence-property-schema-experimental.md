# Экспериментальная регистрация FBE-specific schema

Этот документ описывает текущий экспериментальный контур для FBE-specific
property schema книжных полей.

## Что уже подготовлено

В проект добавлены:

- schema-файл:
  [FBE.Sequence.propdesc](D:\Download\FBeditor\packaging\property-schema\FBE.Sequence.propdesc)
- скрипт регистрации:
  [register-sequence-property-schema.ps1](D:\Download\FBeditor\tools\build\register-sequence-property-schema.ps1)
- скрипт отката:
  [unregister-sequence-property-schema.ps1](D:\Download\FBeditor\tools\build\unregister-sequence-property-schema.ps1)

Сейчас через этот schema-файл публикуются:

- `FBE:Жанр`
- `FBE:Серия`
- `FBE:Версия документа`
- `FBE:Дата документа`

## Что это пока НЕ делает

Пока этот шаг:

- не обещает немедленное появление колонки серии в Проводнике.

Сейчас это уже безопасная основа для следующего шага:

1. зарегистрировать schema;
2. убедиться, что система принимает FBE-specific свойство;
3. использовать уже подключённый published `PKEY_FBE_Sequence` в modern property handler;
4. вручную проверить отображение FBE-specific полей в Проводнике.

Текущий статус:

- `PKEY_FBE_Sequence`, `PKEY_FBE_Genre`,
  `PKEY_FBE_DocumentVersion`, `PKEY_FBE_DocumentDate` уже подключены в
  published-набор modern property handler;
- отдельная experimental-сборка `FBShell.dll` с этими ключами уже успешно
  собирается;
- отображение `Sequence` уже подтверждено на реальной машине в Проводнике;
- schema-файл уже включён в общий staging и в штатный NSIS-установщик;
- без отдельной регистрации `.propdesc`-schema это свойство всё равно нельзя
  считать полноценной GUI-функцией.

## Что изменилось дальше

После успешной ручной проверки experimental-контура следующий шаг уже внесён в
кодовую базу:

- `FBE.Sequence.propdesc` теперь попадает в общий staging `out\package`;
- обычный NSIS-установщик готов ставить и снимать эту schema вместе с modern
  property handler.

То есть этот документ теперь нужен в основном как памятка по ручной отладке и
точечной проверке schema вне полного установщика.

## Как запускать

Регистрация:

```powershell
.\tools\build\register-sequence-property-schema.ps1
```

Откат:

```powershell
.\tools\build\unregister-sequence-property-schema.ps1
```

Оба скрипта требуют PowerShell с правами администратора.

## Ограничение текущего черновика

В `.propdesc` сейчас зашиты labels с префиксом `FBE:` прямо в XML.

Это осознанный текущий шаг. Для полного соответствия локализации Windows позже
лучше перейти на ресурсно-локализуемые имена свойств, а не на прямые строки
внутри schema-файла.
