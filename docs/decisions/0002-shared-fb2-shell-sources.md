# 0002. Общая FB2/shell-реализация остаётся в исходниках без новой DLL

## Контекст

FBE и FBShell компилировали четыре одинаковых исходника из `src/fbe`. Shell
тем самым зависел от private расположения GUI-кода; thumbnail дополнительно
получал FBE PCH и `atlimage.h` неявно.

## Решение

`Fb2Metadata`, `Fb2CoverImage`, `Fb2CoverThumbnail` и
`Fb2ShellProperties` перенесены в `src/common/fb2`. Обёртка ATL/GDI+ перенесена
в `src/common/win32/atlimage.h`. Оба проекта по-прежнему компилируют один
список исходников, поэтому не появляется новая DLL, регистрация или ABI.

`Fb2CoverThumbnail` не использует PCH FBE. Он сохраняет явные Windows/COM
зависимости и потому не объявляется платформонезависимым core.

## Последствия

`test-fb2-common-boundary.ps1` запрещает возврат общей реализации в `src/fbe`
и private include/PCH-зависимость. Негативный WIC-путь теперь всегда отдаёт
диагностическое сообщение, что закреплено существующим smoke-тестом.

