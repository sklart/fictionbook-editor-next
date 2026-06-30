# Экспериментальный modern property handler

Этот документ описывает безопасный способ собирать и проверять новый
property-handler контур для `.fb2`, не подмешивая его в обычную сборку по
умолчанию.

## Текущий принцип

Новый класс `Fb2PropertyHandler` уже существует в исходниках:

- `src/fbshell/Fb2PropertyHandler.h`
- `src/fbshell/Fb2PropertyHandler.cpp`
- `src/fbshell/Fb2PropertyHandler.rgs`

Но по умолчанию он **не включён** в `OBJECT_MAP` и поэтому не участвует в
обычной COM-регистрации `FBShell.dll`.

Это сделано специально, чтобы:

- не ломать legacy-поставку;
- не включать experimental handler случайно;
- иметь отдельный управляемый путь для ручной проверки на GUI.

## Как включить experimental handler в сборку

Для этого добавлен отдельный build-скрипт:

```powershell
.\tools\build\build-experimental-property-handler.ps1 -Configuration Release -Platform x64
```

Он собирает проект с MSBuild-свойством:

- `ExperimentalPropertyHandler=true`

И важно: этот скрипт собирает **только**
`src/fbshell/FBShell.vcxproj`, а не весь `FBE.sln`.

Это сделано специально, чтобы experimental-проверка property handler не
зависела от отдельных старых ошибок в других проектах решения.

Для реальной проверки в современном 64-битном Проводнике приоритетен именно
вариант:

- `-Platform x64`

При таком режиме в `src/fbshell/FBShell.cpp` активируется compile-time флаг:

- `FBE_ENABLE_EXPERIMENTAL_PROPERTY_HANDLER`

И новый класс попадает в `OBJECT_MAP` DLL.

## Важное ограничение

Даже после такой сборки handler ещё не считается частью стандартной поставки.

Его нужно рассматривать как отдельный экспериментальный контур для ручной
проверки:

1. собрать DLL с включённым флагом;
2. отдельно решить сценарий регистрации;
3. вручную проверить поведение на Windows GUI;
4. только потом думать о включении в инсталлятор или в обычную поставку.

Для управляемого эксперимента добавлены отдельные скрипты:

- `tools/build/register-experimental-property-handler.ps1`
- `tools/build/unregister-experimental-property-handler.ps1`

И отдельный чеклист GUI-проверки:

- `docs/experimental-property-handler-manual-test.md`

Сами скрипты используют схему регистрации, описанную в Microsoft Learn:

- COM-регистрация CLSID обработчика;
- ассоциация расширения `.fb2` через
  `HKLM\Software\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers`.

Это требует прав администратора на время эксперимента.

## Зачем нужен этот промежуточный шаг

Он даёт управляемую развилку:

- обычная сборка остаётся стабильной;
- experimental-сценарий можно включить только явно;
- дальнейшие проверки modern property handler не смешиваются с legacy
  `ColumnProvider`.
