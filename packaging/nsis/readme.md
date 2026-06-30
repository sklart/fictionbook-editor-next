# Инструкция по созданию setup-версии FBE

## Пререквизиты:

1. Установите систему инсталляции NSIS: [nsis-3.09-setup.exe](https://sourceforge.net/projects/nsis/files/NSIS%203/3.09/nsis-3.09-setup.exe/download)
2. UAC-плагин теперь хранится прямо в репозитории:
   - рабочие файлы для сборки лежат в `packaging\nsis\NSIS`;
   - полный комплект исходников и примеров лежит в `third_party\uac`.

При вызове `.\tools\build\prepare-installer.ps1` нужные рабочие файлы
`UAC.nsh` и `UAC.dll` автоматически синхронизируются из `third_party\uac`
в `packaging\nsis\NSIS`, поэтому вручную раскладывать их по системной папке
NSIS больше не нужно.

## Актуальный поток сборки

Папка `packaging\nsis\Installer\Input` больше не используется и удалена из
репозитория. Основной поток использует `out\package\FictionBookEditor`
напрямую. Официальный поток такой:

1. Собрать и проверить релиз:

```powershell
.\tools\build\verify-release.ps1 -Configuration Release
```

2. Подготовить portable staging:

```powershell
.\tools\build\package-portable.ps1 -Configuration Release
```

Это создаёт папку `out\package\FictionBookEditor`.

Дополнительно в staging автоматически попадает:

- основной `Win32`-набор приложения;
- `ShellExtensions\x64\FBShell.dll` для современного `64-bit Explorer`.

3. Подготовить NSIS input:

```powershell
.\tools\build\prepare-installer.ps1 -Configuration Release
```

Эта команда:

- проверяет, что staging-папка `out\package\FictionBookEditor` уже готова для
  прямого чтения NSIS-скриптом;
- синхронизирует UAC-плагин из `third_party\uac` в
  `packaging\nsis\NSIS`.

4. Собрать установщик:

```powershell
.\packaging\nsis\Installer\MakeInstaller.bat
```

`MakeInstaller.bat` сам:

- валидирует версии через `sync-version.ps1`;
- вызывает `prepare-installer.ps1`;
- запускает `makensis` с `INPUTDIR=..\..\..\out\package\FictionBookEditor`.

## Полный релизный сценарий

Для обычного выпуска версии лучше использовать не ручные шаги выше, а единый
сценарий:

```powershell
.\tools\build\create-release.ps1 -Configuration Release -Platform Win32 -ValidateUpdateManifest
```

Он автоматически:

- собирает проект;
- отдельно собирает `x64 FBShell.dll` для modern property handler в Проводнике;
- проверяет релизные smoke/regression-тесты;
- создаёт portable ZIP;
- создаёт setup.exe;
- собирает symbols ZIP;
- записывает `SHA256SUMS.txt`;
- синхронизирует поля в `update.xml`.

Текущий `setup.exe` также включает bundled `x64 FBShell.dll` и на
`64-bit Windows` пытается автоматически зарегистрировать modern property
handler для `.fb2`.

Повышение прав теперь запрашивается не всегда, а только если пользователь
оставляет включённой опцию `Системная интеграция`.

Итоговые артефакты попадают в `out\artifacts`.

## Что больше не нужно делать вручную

- вручную синхронизировать `update.xml` перед обычным локальным релизом;
- вручную собирать portable-набор из `out\Release`.

## Примечание

Утилита `update_fbe.exe` остаётся историческим вспомогательным инструментом.
Программку `update_fbe.cs` можно скомпилировать в исполняемый файл командой:

```powershell
C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe update_fbe.cs
```

Visual Studio для этого не нужна
