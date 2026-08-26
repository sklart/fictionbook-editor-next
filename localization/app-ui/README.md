# Локализация FBE и FBV

Этот каталог — production source of truth для пользовательских строк FBE.

FBE.exe содержит английские Win32 structural/fallback resources (DIALOGEX, MENU,
TOOLBAR, BITMAP, ICON и STRINGTABLE). Для каждого из 12 интерфейсных языков
пользовательский текст загружается из `Lang/<locale>/fbe.json`; fallback идёт
через `Lang/en-US/fbe.json`, затем встроенную английскую строку. FBE locale
resource DLL не используются.

FBV и поставляемые плагины используют тот же runtime JSON-контур. Windows Shell
остаётся отдельной MUI-системой: `Lang/Shell/FBVVerbResources.dll` и его
`<locale>/*.mui` не относятся к локализации FBE и не должны заменяться JSON.

## Будущие языковые пакеты

Базовый английский JSON обязателен; остальные языковые JSON и связанные MUI
assets устанавливаются по плану language packs.


## Fallback при отсутствии внешних файлов

Даже после перехода на JSON отсутствие `Lang/<язык>/fbe.json` или `Lang/<язык>/fbv.json` не должно ломать запуск. FBE и FBV должны использовать встроенные строки из `exe`/ресурсов, а внешний JSON рассматривать как переопределение.
