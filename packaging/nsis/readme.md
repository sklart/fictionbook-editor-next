# Сборка setup FBE Next

## Готовые Core и Integration

Требуется NSIS 3.13 или новее. `tools/build/resolve-nsis.ps1` выбирает компилятор;
для явного выбора можно задать `FBE_MAKENSIS`. Рабочие UAC-файлы синхронизируются
из `third_party/uac` автоматически; устанавливать плагин в системный NSIS не нужно.

Основная команда сборки из подготовленных stages:

```powershell
.\tools\build\build-installer.ps1 -CoreDirectory .\out\stage\Core -IntegrationDirectory .\out\stage\Integration
```

Она проверяет версии и stages, экспортирует NSIS-ресурсы, формирует input,
вычисляет SHA-256 его DLL/схемы и запускает NSIS с независимой проверкой payload.
Параметры `-OutputDirectory` и `-OutputFile` задают отдельные input и setup.
По умолчанию input — `out/package/InstallerInput`, setup — `out/artifacts`.
`packaging/nsis/Installer/MakeInstaller.bat` — оболочка той же команды и принимает
те же параметры. Отдельно запускать `prepare-installer.ps1` перед сборкой не нужно.

Если stages ещё не подготовлены, после соответствующих сборок Core и обоих
вариантов shell handler:

```powershell
.\tools\build\stage-core.ps1 -Configuration Release -OutputDirectory .\out\stage\Core
.\tools\build\stage-integration.ps1 -Configuration Release -OutputDirectory .\out\stage\Integration
```

`stage-integration.ps1` использует готовые x86/x64 shell artifacts и ресурсы FBV;
он не собирает их сам. Для полного выпуска используется
`tools/build/create-release.ps1 -Configuration Release -Platform Win32`.
Короткая пересборка setup из готовых stages не требует полного release gate.

## Режимы

- **Current User** — Core в `%LOCALAPPDATA%\Programs\FictionBook Editor Next`.
  Системная интеграция запрашивает повышение прав, включая её удаление при обновлении.
- **All Users** — Core в Program Files (x86), требуется повышение прав.
- **Portable** — автономный Core с `portable.ini` и Data; без системной интеграции,
  uninstall-записей и ярлыков.

Для тихого режима передать `/S` и один из `/CURRENTUSER`, `/ALLUSERS`, `/PORTABLE`.
Для смены scope сначала удалить существующую установленную копию. Обычное
удаление сохраняет пользовательские настройки.

## Обновление и удаление интеграции

Общие DLL и схема находятся в защищённом Common Files. Обновление сохраняет
снимок файлов, Owner, COM/schema и маркеров до подтверждённой регистрации.
Новый деинсталлятор использует фактически установленный пакет, включая старые
хеши после неудачного обновления. При блокировке файлов сведения о владельце
и возможность повторного удаления сохраняются. Чужие пакеты не удаляются.

Порядок операций, статусы `ok/pending/skipped/error`, действия при занятых DLL,
явное восстановление (`/RECOVERSHELL` с прежним режимом установки), очистка
остатков без COM, временных Maintenance-сеансов и границы выполненных проверок:
[Обслуживание системной интеграции](../../docs/installer-shell-maintenance.md).

Перед выпуском настоящую установку/обновление/удаление проверять на чистой VM.
Для широкого lifecycle-теста предназначен
`tools/tests/test-nsis-install-lifecycle-e2e.ps1 -InstallerPath <setup.exe>`;
он требует elevated PowerShell и не является обязательной короткой проверкой
при каждой правке установщика.
