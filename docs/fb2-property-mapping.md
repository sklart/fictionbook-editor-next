# Маппинг свойств `.fb2` на Windows Shell Properties

Этот документ фиксирует текущее решение по тому, как внутренние свойства FB2
соотносятся со стандартными Windows Shell properties.

Важно: это не финальная реализация property handler, а рабочая таблица решений,
на которую должен опираться будущий modern shell-контур.

## Статусы

- `confirmed` — сопоставление выглядит корректным и осмысленным;
- `fbe-specific` — стандартного Windows-свойства пока нет или оно выглядит
  хуже, чем отдельное FBE-специфичное поле.

## Таблица

| Свойство FB2/FBE | Для пользователя | Windows canonical name | Статус | Комментарий |
| --- | --- | --- | --- | --- |
| `author` | Автор | `System.Author` | `confirmed` | Хорошо соответствует авторам книги/документа. |
| `title` | Название | `System.Title` | `confirmed` | Естественное сопоставление для названия книги. |
| `language` | Язык | `System.Language` | `confirmed` | Подходит для `title-info/lang`. |
| `keywords` | FBE:Ключевые слова | — | `fbe-specific` | Ключевые слова вынесены в отдельное FBE-specific свойство, потому что UI Проводника Windows 11 непредсказуемо показывает `System.Keywords`. |
| `genre` | FBE:Жанр | — | `fbe-specific` | Книжный жанр вынесен в отдельное FBE-specific свойство, чтобы не использовать музыкальный системный ключ Windows. |
| `sequence` | FBE:Серия | — | `fbe-specific` | Книжная серия публикуется как отдельное FBE-specific свойство. |
| `documentId` | FBE:Идентификатор документа | — | `fbe-specific` | Идентификатор документа вынесен в отдельное FBE-specific свойство, потому что UI Проводника Windows 11 непредсказуемо показывает `System.Document.DocumentID`. |
| `documentVersion` | FBE:Версия документа | — | `fbe-specific` | Версия FB2-документа вынесена в отдельное FBE-specific свойство, чтобы не смешивать её с общим Windows-полем версии документа. |
| `documentDate` | FBE:Дата документа | — | `fbe-specific` | Дата FB2-документа вынесена в отдельное FBE-specific свойство, чтобы не смешивать её с системной датой создания файла или документа Windows. |

## Правило для отображаемых названий колонок

Если свойство маппится на стандартное Windows property и используется его
canonical name, итоговое отображаемое имя поля должно браться из локализации
самой Windows, а не из жёстко зашитой строки FBE.

То есть для таких полей:

- `Author`
- `Title`
- `Language`

FBE хранит только:

- внутренний `debugName`;
- `fallbackDisplayName` для диагностики, тестов и резервного сценария;
- canonical name свойства Windows.

Но в пользовательском интерфейсе Проводника приоритет должен быть у
локализованного имени, которое отдаёт Windows.

Для `fbe-specific` полей (`Keywords`, `Genre`, `Sequence`, `DocumentId`,
`DocumentVersion`, `DocumentDate` и возможных будущих собственных свойств)
отдельная локализация FBE по-прежнему понадобится. Для них теперь принято
явное отображение с префиксом `FBE:`.

## Практические выводы

### Уже можно считать устойчивыми

- `Author -> System.Author`
- `Title -> System.Title`
- `Language -> System.Language`

Именно эти поля лучше брать в первый MVP будущего property handler.

В коде это правило уже должно выражаться явно, а не через догадки по строкам:

- `UsesWindowsProperty(...)` — у свойства есть canonical name Windows;
- `IsConfirmedWindowsProperty(...)` — это подтверждённое стандартное свойство;
- `IsMvpWindowsProperty(...)` — свойство входит в первый безопасный MVP
  property handler.

Для уже подтверждённых MVP-полей полезно также держать отдельный слой прямого
маппинга на `PROPERTYKEY`, чтобы будущий handler не восстанавливал его через
строки canonical name. Этот слой уже можно строить только для:

- `Author -> PKEY_Author`
- `Title -> PKEY_Title`
- `Language -> PKEY_Language`

### Уже переведены в FBE-specific schema

- `Keywords`
- `Genre`
- `Sequence`
- `DocumentId`
- `DocumentVersion`
- `DocumentDate`

Для этих полей принято явное решение:

- не использовать спорные или семантически чужие системные Windows-свойства;
- публиковать собственные FBE-specific поля;
- визуально помечать их в Проводнике префиксом `FBE:`.

Для этих полей разумнее заранее готовиться к собственному FBE-контуру свойств,
чем пытаться силой вписать их в неудачные стандартные ключи Windows.

## Рекомендуемый MVP для modern property handler

Первый безопасный MVP уже опирается на такие поля:

1. `Author`
2. `Title`
3. `Language`

Следующим опубликованным кольцом теперь уже считаются FBE-specific поля:

4. `Keywords`
5. `Genre`
6. `Sequence`
7. `DocumentId`
8. `DocumentVersion`
9. `DocumentDate`

## Связанные файлы

- кодовая таблица дескрипторов: `src/fbe/Fb2ShellProperties.h`
- реализация текущих статусов и кандидатов: `src/fbe/Fb2ShellProperties.cpp`
- общий roadmap shell-переработки: `docs/fbshell-roadmap.md`
